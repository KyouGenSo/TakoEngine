#pragma once
#include <d3d12.h>
#include <vector>
#include <wrl.h>

#include "GpuResource.h"
#include "PipelineCache.h"
#include "ShaderBindingLayout.h"
#include "UploadAllocator.h"

namespace Tako {

  /// <summary>
  /// コマンドリスト 1 本の記録窓口。バリアは溜めておき、GPU がリソースを触るコマンドの直前にまとめて発行する。
  /// パイプラインは描画先に依存しない設定だけを受け取り、PSO は描画時に現在の描画先フォーマットで解決する
  /// </summary>
  class CommandContext {
  private: //構造体
    /// <summary>
    /// グラフィックス / コンピュートそれぞれのルートシグネチャと束縛状況
    /// </summary>
    struct BindState {
      const ShaderBindingLayout* layout        = nullptr;
      ID3D12RootSignature*       rootSignature = nullptr;
      uint64_t                   boundMask     = 0;  ///< ルートパラメータ番号のビット。ルートシグネチャが変わると全て外れる
      bool                       psoDirty      = true;
    };

  public: //構造体
    /// <summary>
    /// 現在のパイプラインのシェーダーリソースへ名前で値を渡す窓口。シェーダーが使っていない名前は無視する
    /// </summary>
    class ShaderBinder {
    public: //メンバー関数
      void BindCBV(ShaderParamId id, D3D12_GPU_VIRTUAL_ADDRESS address);

      /// <summary>
      /// SrvManager のヒープ上の SRV を渡す
      /// </summary>
      void BindSRV(ShaderParamId id, uint32_t srvIndex);

      /// <summary>
      /// SrvManager のヒープ上の UAV を渡す
      /// </summary>
      void BindUAV(ShaderParamId id, uint32_t uavIndex) { BindSRV(id, uavIndex); }

      /// <summary>
      /// 定数をこのフレームのアップロード領域に書き、その CBV を渡す
      /// </summary>
      template<class T>
      void BindConstants(ShaderParamId id, const T& data) { BindCBV(id, context_.uploadAllocator_->Push(data)); }

    private: //メンバー関数
      friend class CommandContext;
      ShaderBinder(CommandContext& context, BindState& state, bool isCompute) : context_(context), state_(state), isCompute_(isCompute) {}

    private: //メンバー変数
      CommandContext& context_;
      BindState&      state_;
      bool            isCompute_;
    };

  public: //メンバー関数
    /// <summary>
    /// コマンドアロケータとコマンドリストを生成する。生成直後から記録できる
    /// </summary>
    /// <param name="device">D3D12 デバイス</param>
    /// <param name="pipelineCache">PSO とルートシグネチャの取得元</param>
    /// <param name="uploadAllocator">BindConstants の書き込み先</param>
    void Initialize(ID3D12Device* device, PipelineCache* pipelineCache, UploadAllocator* uploadAllocator);

    /// <summary>
    /// 状態遷移を予約する。現在と同じ状態なら何もしない
    /// </summary>
    /// <param name="resource">対象リソース</param>
    /// <param name="newState">遷移後の状態</param>
    void Transition(GpuResource& resource, D3D12_RESOURCE_STATES newState);

    /// <summary>
    /// UAV 書き込みの完了待ちを予約する
    /// </summary>
    /// <param name="resource">対象リソース</param>
    void UavBarrier(GpuResource& resource);

    /// <summary>
    /// 予約済みのバリアを発行する
    /// </summary>
    void FlushBarriers();

    /// <summary>
    /// 描画先を設定し、以降の PSO の解決に使うフォーマットを記録する
    /// </summary>
    /// <param name="rtv">カラーターゲット（nullptr ならなし）</param>
    /// <param name="rtvFormat">カラーターゲットのフォーマット</param>
    /// <param name="dsv">深度ターゲット（nullptr ならなし）</param>
    /// <param name="dsvFormat">深度ターゲットのフォーマット</param>
    void SetRenderTarget(const D3D12_CPU_DESCRIPTOR_HANDLE* rtv, DXGI_FORMAT rtvFormat, const D3D12_CPU_DESCRIPTOR_HANDLE* dsv, DXGI_FORMAT dsvFormat);

    /// <summary>
    /// グラフィックスパイプラインを切り替える。ルートシグネチャが変わった場合は束縛をやり直す必要がある
    /// </summary>
    void SetGraphicsPipeline(const GraphicsPipelineDesc& desc);

    /// <summary>
    /// コンピュートパイプラインを切り替える。ルートシグネチャが変わった場合は束縛をやり直す必要がある
    /// </summary>
    void SetComputePipeline(const ComputePipelineDesc& desc);

    ShaderBinder Graphics() { return ShaderBinder(*this, graphics_, false); }

    ShaderBinder Compute() { return ShaderBinder(*this, compute_, true); }

    void DrawInstanced(UINT vertexCount, UINT instanceCount, UINT startVertex, UINT startInstance);

    void DrawIndexedInstanced(UINT indexCount, UINT instanceCount, UINT startIndex, INT baseVertex, UINT startInstance);

    void Dispatch(UINT threadGroupCountX, UINT threadGroupCountY, UINT threadGroupCountZ);

    void ExecuteIndirect(ID3D12CommandSignature* commandSignature, UINT maxCommandCount, GpuResource& argumentBuffer, UINT64 argumentOffset);

    /// <summary>
    /// コマンドリストを直接操作したコード（ImGui など）の後に呼び、記憶しているパイプラインと束縛を捨てる
    /// </summary>
    void InvalidateState();

    /// <summary>
    /// 予約済みのバリアを発行してから記録を閉じる
    /// </summary>
    void Close();

    /// <summary>
    /// GPU の完了後に呼び、アロケータとコマンドリストを記録できる状態に戻す
    /// </summary>
    void Reset();

    //=========================================
    //Getter
    //=========================================
    /// <summary>
    /// 予約済みのバリアを発行してからコマンドリストを返す
    /// </summary>
    ID3D12GraphicsCommandList* GetCommandList();

    const RenderTargetFormats& GetRenderTargetFormats() const { return renderTargetFormats_; }

    /// <summary>
    /// これまでの描画命令の数（増え続ける。差を取って区間の描画回数にする）
    /// </summary>
    uint32_t GetDrawCount() const { return drawCount_; }

  private: //非公開関数
    /// <summary>
    /// 描画直前に、現在の設定と描画先に合う PSO を有効にする
    /// </summary>
    void PrepareDraw();

    /// <summary>
    /// ディスパッチ直前に、コンピュート PSO を有効にする
    /// </summary>
    void PrepareDispatch();

    /// <summary>
    /// 束縛していないルートパラメータがあれば名前を出して止める（Debug のみ）
    /// </summary>
    void ValidateBindings(const BindState& state) const;

  private: //メンバー変数
    Microsoft::WRL::ComPtr<ID3D12CommandAllocator>    allocator_;
    Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> commandList_;
    std::vector<D3D12_RESOURCE_BARRIER>               pendingBarriers_;
    PipelineCache*                                    pipelineCache_   = nullptr;
    UploadAllocator*                                  uploadAllocator_ = nullptr;

    //パイプライン
    GraphicsPipelineDesc   graphicsDesc_{};
    ComputePipelineDesc    computeDesc_{};
    BindState              graphics_;
    BindState              compute_;
    RenderTargetFormats    renderTargetFormats_{};
    ID3D12PipelineState*   activePso_      = nullptr;  ///< コマンドリストに設定中の PSO（グラフィックスとコンピュートで共有の枠）
    ID3D12PipelineState*   graphicsPso_    = nullptr;
    ID3D12PipelineState*   computePso_     = nullptr;
    D3D_PRIMITIVE_TOPOLOGY activeTopology_ = D3D_PRIMITIVE_TOPOLOGY_UNDEFINED;
    uint32_t               drawCount_      = 0;
  };

} // namespace Tako

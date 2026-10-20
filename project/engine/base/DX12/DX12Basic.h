#pragma once
#include "WinApp.h"
#include <array>
#include<string>
#include<fstream>
#include<sstream>
#include<chrono>
#include<wrl.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <utility>

#include"Vector4.h"
#include "CommandContext.h"
#include "GpuResource.h"
#include "PipelineCache.h"
#include "ShaderLibrary.h"
#include "UploadAllocator.h"

namespace Tako {

  /// <summary>
  /// DirectX 12基盤システムクラス
  /// デバイス、コマンドキュー、スワップチェーン管理
  /// </summary>
  class DX12Basic {
  public: //定数
    static constexpr float       kMinRenderScale  = 0.25f;
    static constexpr DXGI_FORMAT kColorFormat     = DXGI_FORMAT_R8G8B8A8_UNORM;  ///< スワップチェインとシーン描画先のカラー
    static constexpr DXGI_FORMAT kDepthFormat     = DXGI_FORMAT_D32_FLOAT;       ///< シーン深度の DSV
    static constexpr DXGI_FORMAT kDepthReadFormat = DXGI_FORMAT_R32_FLOAT;       ///< シーン深度を SRV で読むとき

  private: //定数
    static constexpr size_t kUploadAllocatorCapacity = 4 * 1024 * 1024;  ///< 1 フレームの一時定数の総量（バイト）

  public: //構造体
    /// <summary>
    /// ComPtr のエイリアス
    /// </summary>
    template<class T> using ComPtr = Microsoft::WRL::ComPtr<T>;

    /// <summary>
    /// シーン描画先。シャドウ/デカール等の途中パスが抜けるときの復帰先として記録する
    /// </summary>
    struct SceneRenderTarget {
      D3D12_CPU_DESCRIPTOR_HANDLE rtv{};
      D3D12_CPU_DESCRIPTOR_HANDLE dsv{};                       ///< ptr == 0 なら深度なし
      uint32_t                    width     = 0;
      uint32_t                    height    = 0;
      DXGI_FORMAT                 rtvFormat = kColorFormat;
      DXGI_FORMAT                 dsvFormat = kDepthFormat;
    };

  public: //メンバー関数

    /// <summary>
    /// デストラクタ
    /// </summary>
    ~DX12Basic();

    /// <summary>
    /// 初期化
    /// </summary>
    /// <param name="winApp">ウィンドウ管理クラスのポインタ</param>
    void Initialize(WinApp* winApp);

    /// <summary>
    /// 終了処理
    /// </summary>
    void Finalize();

    /// <summary>
    /// swapChain を設定
    /// </summary>
    void SetSwapChain();

    /// <summary>
    /// 描画後の処理
    /// </summary>
    void EndDraw();

    /// <summary>
    /// コマンド完了まで待機
    /// </summary>
    void WaitForGPU();

    /// <summary>
    /// バッファリソースの生成
    /// </summary>
    /// <param name="sizeInBytes">バッファサイズ（バイト）</param>
    /// <returns>生成されたバッファリソース</returns>
    ComPtr<ID3D12Resource> MakeBufferResource(size_t sizeInBytes);

    /// <summary>
    /// バッファリソースの生成（参照版）
    /// </summary>
    /// <param name="bufferResource">出力先のバッファリソース</param>
    /// <param name="sizeInBytes">バッファサイズ（バイト）</param>
    void CreateBufferResource(ComPtr<ID3D12Resource>& bufferResource, size_t sizeInBytes);

    /// <summary>
    /// UAV リソースの生成（COMMON 状態で作られる）
    /// </summary>
    /// <param name="uavResource">出力先の UAV リソース</param>
    /// <param name="sizeInBytes">リソースサイズ（バイト）</param>
    void CreateResourceForUAV(GpuResource& uavResource, UINT sizeInBytes);

    /// <summary>
    /// レンダーテクスチャリソースの生成（RENDER_TARGET 状態で作られる）
    /// </summary>
    /// <param name="rendertextureResource">出力先のレンダーテクスチャリソース</param>
    /// <param name="width">テクスチャの幅（ピクセル）</param>
    /// <param name="height">テクスチャの高さ（ピクセル）</param>
    /// <param name="format">ピクセルフォーマット（例: DXGI_FORMAT_R8G8B8A8_UNORM）</param>
    /// <param name="clearColor">クリアカラー（RGBA、各要素0.0-1.0）</param>
    void CreateRenderTextureResource(GpuResource& rendertextureResource, uint32_t width, uint32_t height, DXGI_FORMAT format, const Vector4& clearColor);

    /// <summary>
    /// ビューポートとシザリング矩形をセット
    /// </summary>
    void SetViewPort();

    /// <summary>
    /// シーン描画先をバインドしてビューポート/シザーを合わせ、途中パスの復帰先として記録する
    /// </summary>
    /// <param name="target">バインドする描画先</param>
    void BindSceneRenderTarget(const SceneRenderTarget& target);

    /// <summary>
    /// 記録済みのシーン描画先とビューポート/シザーを再バインドする
    /// </summary>
    void RestoreSceneRenderTarget();

    /// <summary>
    /// RTV,DepthBuffer のリサイズ
    /// </summary>
    /// <param name="width">新しい幅（ピクセル）</param>
    /// <param name="height">新しい高さ（ピクセル）</param>
    void ResizeBuffers(uint32_t width, uint32_t height);

    /// <summary>
    /// ビューポートとシザー矩形の更新
    /// </summary>
    void UpdateViewportAndScissorRect() {
      InitViewport();
      InitScissorRect();
    }

    /// <summary>
    /// 描画先の再構築を次フレーム先頭に要求する（記録中のコマンドが参照する RT を解放しないため）
    /// </summary>
    void RequestRenderTargetRebuild() { isRebuildRequested_ = true; }

    /// <summary>
    /// 再構築要求を取り出し、要求フラグを下ろす
    /// </summary>
    /// <returns>要求があれば true</returns>
    bool ConsumeRenderTargetRebuildRequest() { return std::exchange(isRebuildRequested_, false); }

    //============================================================
    //Setter
    //============================================================
    void SetVSync(bool vsync) { vsync_ = vsync; }

    /// <summary>
    /// フレームレート上限を設定（0 以下で上限なし）
    /// </summary>
    void SetTargetFPS(int fps) { targetFPS_ = fps; }

    /// <summary>
    /// 3D シーンを描く内部解像度の倍率を設定し、描画先の再構築を要求する（UI は常にウィンドウ解像度）
    /// </summary>
    void SetRenderScale(float scale);

    //============================================================
    //Getter
    //============================================================
    bool IsVSync() const { return vsync_; }
    int GetTargetFPS() const { return targetFPS_; }
    float GetRenderScale() const { return renderScale_; }

    /// <summary>
    /// 3D シーンの内部解像度（ウィンドウ解像度 × 倍率、最小 1）
    /// </summary>
    uint32_t GetSceneWidth() const;
    uint32_t GetSceneHeight() const;

    ID3D12Device* GetDevice() {
      return device_.Get();
    }

    /// <summary>
    /// 予約済みのバリアを発行してからコマンドリストを返す
    /// </summary>
    ID3D12GraphicsCommandList* GetCommandList() {
      return context_.GetCommandList();
    }

    CommandContext& GetContext() { return context_; }

    ShaderLibrary& GetShaderLibrary() { return shaderLibrary_; }

    UploadAllocator& GetUploadAllocator() { return uploadAllocator_; }

    ID3D12CommandQueue* GetCommandQueue() {
      return commandQueue_.Get();
    }

    size_t GetSwapChainBufferCount() {
      return swapChainResources_.size();
    }

    /// <summary>
    /// メイン深度バッファの DSV ハンドルの取得
    ///	</summary>
    /// <returns>メイン DSV の CPU ディスクリプタハンドル</returns>
    D3D12_CPU_DESCRIPTOR_HANDLE GetMainDSVHandle() const;

    /// <summary>
    /// ウィンドウ解像度で描く非エフェクトパス用の DSV（等倍ならメイン深度、縮小解像度中は専用の深度）
    /// </summary>
    D3D12_CPU_DESCRIPTOR_HANDLE GetNonEffectDSVHandle() const;

    GpuResource& GetDepthStencilResource() {
      return depthStencilResource_;
    }

    IDXGISwapChain4* GetSwapChain() {
      return swapChain_.Get();
    }

    /// <summary>
    /// スワップチェインの RTVHandle を取得
    ///	</summary>
    /// <returns>現在のバックバッファの RTV ハンドル</returns>
    D3D12_CPU_DESCRIPTOR_HANDLE GetSwapChainRTVHandle() {
      UINT backBufferIndex = swapChain_->GetCurrentBackBufferIndex();
      return rtvHandle_[backBufferIndex];
    }

    D3D12_VIEWPORT GetViewport() {
      return viewport_;
    }

    D3D12_RECT GetScissorRect() {
      return scissorRect_;
    }

    const SceneRenderTarget& GetSceneRenderTarget() const { return sceneRenderTarget_; }
  private: //非公開関数
    /// <summary>
    /// device の初期化
    /// </summary>
    void InitDevice();

    /// <summary>
    /// コマンド関連の初期化
    /// </summary>
    void InitCommand();

    /// <summary>
    /// スワップチェインの生成
    /// </summary>
    void CreateSwapChain();

    /// <summary>
    /// スワップチェインの生成とリサイズで揃えて渡すフラグ
    /// </summary>
    UINT SwapChainFlags() const { return isTearingSupported_ ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING : 0u; }

    /// <summary>
    /// 深度バッファの生成
    /// </summary>
    void CreateDepthStencilResource();

    /// <summary>
    /// DEPTH_WRITE 状態の D32 深度バッファを生成する
    /// </summary>
    GpuResource MakeDepthBuffer(uint32_t width, uint32_t height);

    /// <summary>
    /// 縮小解像度中だけ、非エフェクトパス用にウィンドウ解像度の深度を作る（等倍なら解放する）
    /// </summary>
    void RecreateNonEffectDepthBuffer();

    /// <summary>
    /// デスクリプタヒープの初期化
    /// </summary>
    void InitDescriptorHeap();

    /// <summary>
    /// レンダーターゲットビューの初期化
    /// </summary>
    void InitSwapChainRTV();

    /// <summary>
    /// 深度ステンシルビューの初期化
    /// </summary>
    void InitDSV();

    /// <summary>
    /// フェンスの初期化
    /// </summary>
    void InitFence();

    /// <summary>
    ///　ビューポート矩形の初期化
    /// </summary>
    void InitViewport();

    /// <summary>
    /// シザリング矩形の初期化
    /// </summary>
    void InitScissorRect();

    /// <summary>
    /// FPS 制御初期化
    /// </summary>
    void InitFPSLimiter();

    /// <summary>
    /// FPS 制御更新
    /// </summary>
    void UpdateFPSLimiter();

    /// <summary>
    /// 深度バッファの再作成
    /// </summary>
    void RecreateDepthBuffer();

    /// <summary>
    /// 指定番号の CPU ディスクリプタハンドルを取得
    /// </summary>
    /// <param name="descriptorHeap">デスクリプタヒープ</param>
    /// <param name="descriptorSize">デスクリプタのサイズ（バイト）</param>
    /// <param name="index">インデックス番号</param>
    /// <returns>CPU ディスクリプタハンドル</returns>
    static D3D12_CPU_DESCRIPTOR_HANDLE GetCPUDescriptorHandle(ID3D12DescriptorHeap* descriptorHeap, uint32_t descriptorSize, uint32_t index);

    /// <summary>
    /// 指定番号の GPU ディスクリプタハンドルを取得
    /// </summary>
    /// <param name="descriptorHeap">デスクリプタヒープ</param>
    /// <param name="descriptorSize">デスクリプタのサイズ（バイト）</param>
    /// <param name="index">インデックス番号</param>
    /// <returns>GPU ディスクリプタハンドル</returns>
    static D3D12_GPU_DESCRIPTOR_HANDLE GetGPUDescriptorHandle(ID3D12DescriptorHeap* descriptorHeap, uint32_t descriptorSize, uint32_t index);

  private: //メンバー変数

    //FPS 制御
    std::chrono::steady_clock::time_point referenceTime_;               ///< 記録時間（FPS 制御用の基準時刻）
    bool                                  vsync_              = true;
    int                                   targetFPS_          = 60;     ///< 0 以下で上限なし
    bool                                  isTearingSupported_ = false;  ///< VSync 無効時にティアリング付きで Present できるか

    //描画先の再構築
    bool  isRebuildRequested_ = false;  ///< TakoFramework::Update の先頭で消費する
    float renderScale_        = 1.0f;   ///< kMinRenderScale〜1.0

    static const UINT kRtvHandleCount = 2;  ///< RTV ハンドルの要素数（スワップチェイン用バックバッファ数）

    WinApp* winApp_ = nullptr;  ///< ウィンドウクラスポインター（ウィンドウサイズ等の取得に使用）

    ComPtr<ID3D12Device> device_;  ///< DirectX 12デバイス（リソース生成の中心オブジェクト）

    ComPtr<IDXGIFactory7> dxgiFactory_;  ///< DXGI ファクトリ（スワップチェイン生成に使用）

    ComPtr<ID3D12CommandQueue> commandQueue_;  ///< コマンドキュー（GPU 実行キュー）

    ShaderLibrary shaderLibrary_;  ///< シェーダーのコンパイルとキャッシュ

    PipelineCache pipelineCache_;  ///< ルートシグネチャと PSO のキャッシュ（context_ が描画時に引く）

    CommandContext context_;  ///< 唯一のコマンドリストとバリアの予約

    UploadAllocator uploadAllocator_;  ///< 1 フレームだけ使う定数の置き場（EndDraw で空に戻す）

    ComPtr<IDXGISwapChain4> swapChain_;  ///< スワップチェイン（ダブルバッファリング管理）

    GpuResource depthStencilResource_;  ///< 深度バッファリソース（深度テスト用）

    uint32_t mainDsvIndex_ = 0;  ///< メイン深度バッファの DSV インデックス（DsvManager 発行）

    GpuResource nonEffectDepthResource_;  ///< 縮小解像度中だけ作るウィンドウ解像度の深度

    uint32_t nonEffectDsvIndex_ = 0;  ///< nonEffectDepthResource_ の DSV インデックス（初回作成時に確保）

    std::array<GpuResource, 2> swapChainResources_;  ///< スワップチェインのバッファ（ダブルバッファリング用2枚）

    UINT swapChainBufferCount_;  ///< スワップチェインのバッファのカウント（通常2）

    D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle_[kRtvHandleCount];  ///< RTV ハンドル配列（各バックバッファ用）

    uint32_t swapChainRtvIndices_[kRtvHandleCount] = {};  ///< スワップチェーン用 RTV インデックス（RtvManager 発行）

    ComPtr<ID3D12Fence> fence_;  ///< フェンスオブジェクト（GPU 同期用）

    HANDLE fenceEvent_;  ///< フェンスイベントハンドル（CPU 待機用）

    UINT64 fenceValue_;  ///< フェンスの値（同期カウンター）

    D3D12_VIEWPORT viewport_;  ///< ビューポート（描画領域の定義）

    D3D12_RECT scissorRect_;  ///< シザリング矩形（描画範囲の制限）

    SceneRenderTarget sceneRenderTarget_{};  ///< 現在のシーン描画先（BindSceneRenderTarget で更新）

  };

} // namespace Tako

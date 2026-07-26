#pragma once
#include <unordered_map>

#include "IPostEffect.h"
#include "Vector2.h"

namespace Tako {

  class WinApp;

  /// <summary>
  /// ブルームエフェクト - 明るい領域を抽出してぼかし、元の画像に合成して光の溢れを表現
  /// </summary>
  class Bloom : public IPostEffect
  {
  private: //定数
    static constexpr uint32_t kBlurTextureParam = 2;  ///< t1: ブラー結果テクスチャ（BloomCombine 用）

  public: //メンバー関数
    /// <summary>
    /// デストラクタ
    /// </summary>
    ~Bloom();

    /// <summary>
    /// 初期化
    /// </summary>
    /// <param name="dx12">DirectX12基盤</param>
    /// <param name="shaderName">シェーダー名</param>
    void Initialize(DX12Basic* dx12, const std::string& shaderName) override;

    /// <summary>
    /// エフェクトを適用
    /// </summary>
    /// <param name="inputSrvIndex">入力テクスチャの SRV インデックス</param>
    /// <param name="outputRtvHandle">出力先の RTV ハンドル</param>
    /// <param name="depthSrvIndex">深度バッファの SRV インデックス</param>
    /// <param name="clearColor">出力先の RTV のクリアカラー</param>
    void Apply(
      uint32_t                    inputSrvIndex,
      D3D12_CPU_DESCRIPTOR_HANDLE outputRtvHandle,
      uint32_t                    depthSrvIndex,
      const Vector4& clearColor
    ) override;

    /// <summary>
    /// ImGui でデバッグ UI を描画
    /// </summary>
    void DrawImgui() override;

    /// <summary>
    /// 汎用パラメータを設定
    /// </summary>
    /// <param name="param">エフェクトパラメータ</param>
    /// <returns>設定成功の場合 true</returns>
    bool SetGenericParam(const EffectParam& param) override;

    /// <summary>
    /// ウィンドウリサイズ時の処理
    /// </summary>
    /// <param name="newSize">新しいウィンドウサイズ</param>
    void OnResize(const Vector2& newSize);

    //==========================================
    //Setter
    //==========================================
    void SetParam(const HighLumExtrcatParam& param);
    void SetParam(const GaussianBlurParam& param);
    void SetParam(const BloomCombineParam& param);

  private: //非公開関数
    /// <summary>
    /// ルートシグネチャを作成
    /// </summary>
    void CreateRootSignature() override;

    /// <summary>
    /// ルートシグネチャを作成（シェーダー名指定）
    /// </summary>
    /// <param name="shaderName">シェーダー名</param>
    void CreateRootSignature(const std::string& shaderName);

    /// <summary>
    /// パイプラインステートオブジェクトを作成
    /// </summary>
    void CreatePSO() override;

    /// <summary>
    /// パイプラインステートオブジェクトを作成（シェーダー名指定）
    /// </summary>
    /// <param name="shaderName">シェーダー名</param>
    void CreatePSO(const std::string& shaderName);

    /// <summary>
    /// 定数バッファビューを作成
    /// </summary>
    void CreateCBV();

    /// <summary>
    /// レンダーテクスチャを作成
    /// </summary>
    void CreateRenderTexture();

    /// <summary>
    /// レンダーテクスチャを再作成
    /// </summary>
    void RecreateRenderTexture();

    /// <summary>
    /// リソースバリアを設定
    /// </summary>
    /// <param name="resource">対象リソース</param>
    /// <param name="stateBefore">遷移前の状態</param>
    /// <param name="stateAfter">遷移後の状態</param>
    void SetBarrier(ID3D12Resource* resource, D3D12_RESOURCE_STATES stateBefore, D3D12_RESOURCE_STATES stateAfter);

  private: //メンバー変数

    std::unordered_map <std::string, ComPtr<ID3D12RootSignature>> rootSignatures_;  ///< ルートシグネチャ

    std::unordered_map<std::string, ComPtr<ID3D12PipelineState>> pipelineStates_;  ///< パイプラインステート

    ComPtr<ID3D12Resource> extractCBufferRes_;
    HighLumExtrcatParam*   extractData_       = nullptr;

    ComPtr<ID3D12Resource> blurCBufferRes1_;
    GaussianBlurParam*     blurData1_       = nullptr;

    ComPtr<ID3D12Resource> blurCBufferRes2_;
    GaussianBlurParam*     blurData2_       = nullptr;

    ComPtr<ID3D12Resource> combineCBufferRes_;
    BloomCombineParam*     combineData_       = nullptr;

    RenderTexture highLumRT_{};
    RenderTexture blurRT_{};
    RenderTexture resultRT_{};

    //リサイズコールバック管理
    WinApp*  winApp_     = nullptr;
    uint32_t onResizeId_ = 0;
  };

} // namespace Tako

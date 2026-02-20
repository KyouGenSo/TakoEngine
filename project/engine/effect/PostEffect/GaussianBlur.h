#pragma once
#include "IPostEffect.h"
#include "Vector2.h"

namespace Tako {

  class WinApp;

  /// <summary>
  /// ガウシアンブラーエフェクト - 2パスでガウス分布に基づくぼかし効果を適用
  /// </summary>
  class GaussianBlur : public IPostEffect
  {
  public:
    /// <summary>
    /// デストラクタ
    /// </summary>
    ~GaussianBlur();

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
    /// ガウシアンブラーパラメータを設定
    /// </summary>
    /// <param name="param">ガウシアンブラーパラメータ</param>
    void SetParam(const GaussianBlurParam& param);

    /// <summary>
    /// ウィンドウリサイズ時の処理
    /// </summary>
    /// <param name="newSize">新しいウィンドウサイズ</param>
    void OnResize(const Vector2& newSize);

  private:
    /// <summary>
    /// ルートシグネチャを作成
    /// </summary>
    void CreateRootSignature() override;

    /// <summary>
    /// パイプラインステートオブジェクトを作成
    /// </summary>
    void CreatePSO() override;

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

  private:

    ComPtr<ID3D12Resource> cBufferResource1_;
    GaussianBlurParam* cBufferData1_ = nullptr;

    ComPtr<ID3D12Resource> cBufferResource2_;
    GaussianBlurParam* cBufferData2_ = nullptr;

    RenderTexture resultRT_{};

    // リサイズコールバック管理
    WinApp* winApp_ = nullptr;
    uint32_t onResizeId_ = 0;
  };

} // namespace Tako

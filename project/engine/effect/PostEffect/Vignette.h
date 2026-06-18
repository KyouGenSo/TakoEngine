#pragma once
#include "IPostEffect.h"
#include "PostEffectStruct.h"

namespace Tako {

  /// <summary>
  /// ビネットエフェクト - 画面の周辺部を暗くして中心に視線を誘導
  /// </summary>
  class Vignette : public IPostEffect
  {
  public: //メンバー関数
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

    //========================================
    //Setter
    //========================================
    void SetParam(const VignetteParam& param);

  private: //非公開関数
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

  private: //メンバー変数

    ComPtr<ID3D12Resource> cBufferResource_;
    VignetteParam*         cBufferData_     = nullptr;

  };

} // namespace Tako

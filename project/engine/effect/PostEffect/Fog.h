#pragma once
#include "IPostEffect.h"

namespace Tako {

/// <summary>
/// フォグエフェクト - 距離に基づいて霧を表現し、遠景をぼかす
/// </summary>
class Fog : public IPostEffect
{
public:
  /// <summary>
  /// 初期化
  /// </summary>
  /// <param name="dx12">DirectX12基盤</param>
  /// <param name="shaderName">シェーダー名</param>
  void Initialize(DX12Basic* dx12, const std::string& shaderName) override;

  /// <summary>
  /// エフェクトを適用
  /// </summary>
  /// <param name="inputSrvIndex">入力テクスチャのSRVインデックス</param>
  /// <param name="outputRtvHandle">出力先のRTVハンドル</param>
  /// <param name="depthSrvIndex">深度バッファのSRVインデックス</param>
  /// <param name="clearColor">出力先のRTVのクリアカラー</param>
  void Apply(
    uint32_t                    inputSrvIndex,
    D3D12_CPU_DESCRIPTOR_HANDLE outputRtvHandle,
    uint32_t                    depthSrvIndex,
    const Vector4&              clearColor
  ) override;

  /// <summary>
  /// ImGuiでデバッグUIを描画
  /// </summary>
  void DrawImgui() override;

  /// <summary>
  /// 汎用パラメータを設定
  /// </summary>
  /// <param name="param">エフェクトパラメータ</param>
  /// <returns>設定成功の場合true</returns>
  bool SetGenericParam(const EffectParam& param) override;

  /// <summary>
  /// フォグパラメータを設定
  /// </summary>
  /// <param name="param">フォグパラメータ</param>
  void SetParam(const FogParam& param);

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

private:

  ComPtr<ID3D12Resource> fogParamResource_;
  ComPtr<ID3D12Resource> cameraResource_;
  FogParam* fogData_ = nullptr;
  CameraForGPU* cameraData_ = nullptr;

};

} // namespace Tako


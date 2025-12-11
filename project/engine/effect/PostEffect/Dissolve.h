#pragma once
#include "IPostEffect.h"

namespace Tako {

/// <summary>
/// ディゾルブエフェクト - マスクテクスチャを使用して徐々に消える/現れる遷移を表現
/// </summary>
class Dissolve : public IPostEffect
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
  /// <param name="maskSrvIndex">マスクテクスチャのSRVインデックス</param>
  /// <param name="clearColor">出力先のRTVのクリアカラー</param>
  void Apply(
    uint32_t                    inputSrvIndex,
    D3D12_CPU_DESCRIPTOR_HANDLE outputRtvHandle,
    uint32_t                    maskSrvIndex,
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
  /// ディゾルブパラメータを設定
  /// </summary>
  /// <param name="param">ディゾルブパラメータ</param>
  void SetParam(const DissolveParam& param);

  /// <summary>
  /// 背景テクスチャのSRVインデックスを設定
  /// </summary>
  /// <param name="srvIndex">SRVインデックス</param>
  void SetBaseTextureSrvIndex(uint32_t srvIndex){
    baseTexSrvIndex_ = srvIndex;
  }

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
  ComPtr<ID3D12Resource> cBufferResource_;
  DissolveParam* cBufferData_ = nullptr;

  uint32_t baseTexSrvIndex_ = 0; // 背景テクスチャのSRVインデックス

};

} // namespace Tako


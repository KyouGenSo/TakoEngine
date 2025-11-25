#pragma once
#include "IPostEffect.h"

/// <summary>
/// エフェクトなし - 入力をそのまま出力に渡すパススルーエフェクト
/// </summary>
class NoEffect : public IPostEffect
{
public:
  /// <summary>
  /// 初期化
  /// </summary>
  /// <param name="dx12">DirectX12基盤</param>
  /// <param name="shaderName">シェーダー名</param>
  void Initialize(DX12Basic* dx12, std::string shaderName) override;

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
    Vector4                     clearColor
  ) override;

  /// <summary>
  /// バックバッファに直接適用
  /// </summary>
  /// <param name="inputSrvIndex">入力テクスチャのSRVインデックス</param>
  void ApplyToBackBuffer(uint32_t inputSrvIndex);

  /// <summary>
  /// ImGuiでデバッグUIを描画
  /// </summary>
  void DrawImgui() override;

private:
  /// <summary>
  /// ルートシグネチャを作成
  /// </summary>
  void CreateRootSignature() override;

  /// <summary>
  /// パイプラインステートオブジェクトを作成
  /// </summary>
  void CreatePSO() override;

};


#pragma once
#include "IPostEffect.h"
#include "Matrix4x4.h"

/// <summary>
/// 深度ベースアウトラインエフェクト - 深度バッファを使用してオブジェクトの輪郭を検出・描画
/// </summary>
class DepthBasedOutline : public IPostEffect
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
  /// 深度ベースアウトラインパラメータを設定
  /// </summary>
  /// <param name="param">深度アウトラインパラメータ</param>
  void SetParam(const DepthOutlineParam& param);

  /// <summary>
  /// 逆プロジェクション行列を設定
  /// </summary>
  /// <param name="invProjectionMatrix">逆プロジェクション行列</param>
  void SetInvProjectionMatrix(const Matrix4x4& invProjectionMatrix);

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
  DepthOutlineParam* cBufferData_ = nullptr;

};


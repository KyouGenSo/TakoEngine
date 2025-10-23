#pragma once  
#include <d3d12.h>  
#include <string>  
#include <variant>
#include <wrl.h>  

#include "PostEffectStruct.h"

struct Vector4;
class DX12Basic;

/// <summary>
/// ポストエフェクト基底インターフェースクラス
/// Bloom、Vignette、RadialBlurなど各種ポストエフェクトの共通インターフェースを定義
/// 各エフェクトはこのクラスを継承し、Apply()メソッドで独自の画像処理を実装
/// ルートシグネチャとPSOの管理、ImGuiデバッグUI統合をサポート
/// </summary>
class IPostEffect
{
public: // メンバー関数
  /// <summary>
  /// デストラクタ
  /// </summary>
  virtual ~IPostEffect() = default;

  /// <summary>
  /// 初期化
  /// </summary>
  /// <param name="dx12">DirectX12基盤</param>
  /// <param name="shaderName">シェーダー名</param>
  virtual void Initialize(DX12Basic* dx12, std::string shaderName);

  /// <summary>
  /// エフェクトを適用
  /// </summary>
  /// <param name="inputSrvIndex">入力テクスチャのSRVインデックス</param>
  /// <param name="outputRtvHandle">出力先のRTVハンドル</param>
  /// <param name="depthSrvIndex">深度バッファのSRVインデックス</param>
  /// <param name="clearColor">出力先のRTVのクリアカラー</param>
  virtual void Apply(
    uint32_t                    inputSrvIndex,
    D3D12_CPU_DESCRIPTOR_HANDLE outputRtvHandle,
    uint32_t                    depthSrvIndex,
    Vector4                     clearColor
  ) = 0;

  /// <summary>
  /// ImGuiでデバッグUIを描画
  /// </summary>
  virtual void DrawImgui() = 0;

  /// <summary>
  /// 汎用パラメータを設定
  /// </summary>
  /// <param name="param">エフェクトパラメータ</param>
  /// <returns>設定成功の場合true</returns>
  virtual bool SetGenericParam(const EffectParam& param) { param; return false; }

  /// <summary>
  /// 深度バッファが必要か判定
  /// </summary>
  /// <returns>必要な場合true</returns>
  virtual bool RequiresDepthBuffer() const { return false; }

protected: // プライベートメンバー関数

  // ComPtrのエイリアス
  template<class T> using ComPtr = Microsoft::WRL::ComPtr<T>;

  /// <summary>
  /// ルートシグネチャを作成
  /// </summary>
  virtual void CreateRootSignature() = 0;

  /// <summary>
  /// パイプラインステートオブジェクトを作成
  /// </summary>
  virtual void CreatePSO() = 0;  

protected: // メンバー変数

  DX12Basic* m_dx12_ = nullptr;  ///< DirectX12基盤システムへの参照

  std::string shaderName_;  ///< 使用するシェーダーのファイル名（拡張子なし）

  ComPtr<ID3D12RootSignature> rootSignature_;  ///< このエフェクト用のルートシグネチャ

  ComPtr<ID3D12PipelineState> pipelineState_;  ///< このエフェクト用のパイプラインステート
};

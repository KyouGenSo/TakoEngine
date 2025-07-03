#pragma once  
#include <d3d12.h>  
#include <string>  
#include <variant>
#include <wrl.h>  

#include "PostEffectStruct.h"

struct Vector4;  
class DX12Basic;  

class IPostEffect  
{
protected: // エフェクトパラメーターのvariant型定義

  using EffectParam = std::variant<
    VignetteParam,
    BloomParam,
    NewBloomParam,
    FogParam,
    RadialBlurParam,
    BWFilterParam,
    RGBSplitParam,
    LuminanceOutlineParam,
    DepthOutlineParam
  >;

public: // メンバー関数  

  // デストラクタ  
  virtual ~IPostEffect() = default;  

  // 初期化  
  virtual void Initialize(DX12Basic* dx12, std::string shaderName);  

  // エフェクト適用  
  virtual void Apply(  
    uint32_t                    inputSrvIndex,   // 入力テクスチャのSRVインデックス  
    D3D12_CPU_DESCRIPTOR_HANDLE outputRtvHandle, // 出力先のRTVハンドル  
    uint32_t                    depthSrvIndex,   // 深度バッファが必要なエフェクト用  
    Vector4                     clearColor       // 出力先のRTVのクリアカラー  
  ) = 0;  

  // Debug用のパラメータを設定  
  virtual void DrawImgui() = 0;

  // 汎用パラメーター設定用の仮想関数
  virtual bool SetGenericParam(const EffectParam& param) { param; return false; }

  // 深度バッファが必要か  
  virtual bool RequiresDepthBuffer() const { return false; }  

protected: // プライベートメンバー関数  

  // ComPtrのエイリアス  
  template<class T> using ComPtr = Microsoft::WRL::ComPtr<T>;  

  virtual void CreateRootSignature() = 0;  
  virtual void CreatePSO() = 0;  
  virtual void CreateCBV() = 0;  

protected: // メンバー変数  

  // DX12の基本情報  
  DX12Basic* m_dx12_ = nullptr;  

  // シェーダー名  
  std::string shaderName_;  

  // ルートシグネチャ  
  ComPtr<ID3D12RootSignature> rootSignature_;  

  // パイプラインステート  
  ComPtr<ID3D12PipelineState> pipelineState_;  
};

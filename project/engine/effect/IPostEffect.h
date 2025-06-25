#pragma once
#include <d3d12.h>
#include <string>
#include<wrl.h>

struct Vector4;
class DX12Basic;

class IPostEffect
{
public: // メンバー関数

  // デストラクタ
  virtual ~IPostEffect() = default;

  // 初期化
  virtual void Initialize(DX12Basic* dx12, std::string shaderName);

  // エフェクト適用
  virtual void Apply(
    uint32_t inputSrvIndex,
    D3D12_CPU_DESCRIPTOR_HANDLE outputRtvHandle,
    uint32_t depthSrvIndex, // 深度バッファが必要なエフェクト用
    Vector4 clearColor
  ) = 0;

  // Debug用のパラメータを設定
  virtual void DrawImgui() = 0;

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


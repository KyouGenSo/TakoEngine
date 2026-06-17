#include "DepthBasedOutline.h"

#include "DX12Basic.h"
#ifdef _DEBUG
#include "DebugUIManager.h"
#endif
#include "SrvManager.h"
#include "StringUtility.h"
#include "Mat4x4Func.h"
#include "EnginePaths.h"

#ifdef _DEBUG
#include "ImGuiManager.h"
#endif

namespace Tako {

  void DepthBasedOutline::Initialize(DX12Basic* dx12, const std::string& shaderName)
  {
    IPostEffect::Initialize(dx12, shaderName);
    CreateCBV();
  }

  void DepthBasedOutline::Apply(uint32_t inputSrvIndex, D3D12_CPU_DESCRIPTOR_HANDLE outputRtvHandle, uint32_t depthSrvIndex, [[maybe_unused]] const Vector4& clearColor)
  {
    m_dx12_->TransitionResourceState(D3D12_RESOURCE_STATE_DEPTH_WRITE,
      D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
      m_dx12_->GetDepthStencilResource());

    D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = m_dx12_->GetDSVHeapHandleStart();
    m_dx12_->GetCommandList()->OMSetRenderTargets(1,
      &outputRtvHandle,
      false,
      &dsvHandle);

    // エフェクト適用シェーダーの設定
    m_dx12_->GetCommandList()->SetGraphicsRootSignature(rootSignature_.Get());
    m_dx12_->GetCommandList()->SetPipelineState(pipelineState_.Get());

    // プリミティブトポロジーの設定（フルスクリーン三角形用）
    m_dx12_->GetCommandList()->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

    // パラメータリソースの設定
    m_dx12_->GetCommandList()->SetGraphicsRootConstantBufferView(1, cBufferResource_->GetGPUVirtualAddress());

    // 深度テクスチャをシェーダーリソースとして設定
    SrvManager::GetInstance()->SetGraphicsRootDescriptorTable(2, depthSrvIndex);

    // レンダーテクスチャ A をシェーダーリソースとして設定
    SrvManager::GetInstance()->SetGraphicsRootDescriptorTable(0, inputSrvIndex);

    // フルスクリーン三角形描画
    m_dx12_->GetCommandList()->DrawInstanced(3, 1, 0, 0);

    m_dx12_->TransitionResourceState(D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
      D3D12_RESOURCE_STATE_DEPTH_WRITE,
      m_dx12_->GetDepthStencilResource());
  }

  void DepthBasedOutline::DrawImgui()
  {
#ifdef _DEBUG
    ImGui::DragFloat("DepthBasedOutline Thickness", &cBufferData_->outlineThickness, 0.01f, 0.0f, 100.0f, "%.2f");
#endif
  }

  bool DepthBasedOutline::SetGenericParam(const EffectParam& param)
  {
    if (auto* DepthBasedOutlineParam = std::get_if<DepthOutlineParam>(&param)) {
      SetParam(*DepthBasedOutlineParam);
      return true;
    }
    return false;
  }

  void DepthBasedOutline::SetParam(const DepthOutlineParam& param)
  {
    if (cBufferData_ == nullptr) {
      return;
    }
    cBufferData_->outlineThickness = param.outlineThickness;
  }

  void DepthBasedOutline::SetInvProjectionMatrix(const Matrix4x4& invProjectionMatrix)
  {
    cBufferData_->projectionInverse = invProjectionMatrix;
  }

  void DepthBasedOutline::CreateRootSignature()
  {
    BuildRootSignature({ {RootParam::SrvTable, 0}, {RootParam::Cbv, 0}, {RootParam::SrvTable, 1} }, SamplerFilterMode::Point);
  }

  void DepthBasedOutline::CreatePSO()
  {
    BuildFullScreenPSO();
  }

  void DepthBasedOutline::CreateCBV()
  {
    cBufferResource_ = m_dx12_->MakeBufferResource(sizeof(DepthOutlineParam));

    // map
    cBufferResource_->Map(0, nullptr, reinterpret_cast<void**>(&cBufferData_));

    // 初期値を設定
    cBufferData_->outlineThickness = 1.0f;
    cBufferData_->projectionInverse = Mat4x4::MakeIdentity();
  }

} // namespace Tako
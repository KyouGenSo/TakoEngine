#include "Fog.h"

#include "DX12Basic.h"
#ifdef _DEBUG
#include "DebugUIManager.h"
#endif
#include "SrvManager.h"
#include "StringUtility.h"
#include "Object3dBasic.h"
#include "Camera.h"
#include "EnginePaths.h"

#ifdef _DEBUG
#include "ImGuiManager.h"
#endif

namespace Tako {

  void Fog::Initialize(DX12Basic* dx12, const std::string& shaderName)
  {
    IPostEffect::Initialize(dx12, shaderName);
    CreateCBV();
  }

  void Fog::Apply(uint32_t inputSrvIndex, D3D12_CPU_DESCRIPTOR_HANDLE outputRtvHandle, uint32_t depthSrvIndex, [[maybe_unused]] const Vector4& clearColor)
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
    m_dx12_->GetCommandList()->SetGraphicsRootConstantBufferView(1, fogParamResource_->GetGPUVirtualAddress());
    m_dx12_->GetCommandList()->SetGraphicsRootConstantBufferView(3, cameraResource_->GetGPUVirtualAddress());

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

  void Fog::DrawImgui()
  {
#ifdef _DEBUG
    ImGui::ColorEdit4("FogColor", &fogData_->color.x);
    ImGui::DragFloat("Density", &fogData_->density, 0.001f, 0.0f, 1.0f);
#endif
  }

  bool Fog::SetGenericParam(const EffectParam& param)
  {
    if (auto* fogParam = std::get_if<FogParam>(&param)) {
      SetParam(*fogParam);
      return true;
    }
    return false;
  }

  void Fog::SetParam(const FogParam& param)
  {
    if (fogData_ == nullptr) {
      return; // cBufferData_が初期化されていない場合は何もしない
    }
    // パラメータを更新
    fogData_->color = param.color;
    fogData_->density = param.density;
  }

  void Fog::CreateRootSignature()
  {
    BuildRootSignature(
      { {RootParam::SrvTable, 0}, {RootParam::Cbv, 0}, {RootParam::SrvTable, 1}, {RootParam::Cbv, 1} },
      SamplerFilterMode::Point);
  }

  void Fog::CreatePSO()
  {
    BuildFullScreenPSO();
  }

  void Fog::CreateCBV()
  {
    fogParamResource_ = m_dx12_->MakeBufferResource(sizeof(FogParam));

    // map
    fogParamResource_->Map(0, nullptr, reinterpret_cast<void**>(&fogData_));

    // 初期値を設定
    fogData_->color = Vector4(1.f, 1.f, 1.f, 1.0f);
    fogData_->density = 0.0f;


    // camera resource の生成--------------------------------------------------------------------------------
    cameraResource_ = m_dx12_->MakeBufferResource(sizeof(CameraForGPU));

    // map
    cameraResource_->Map(0, nullptr, reinterpret_cast<void**>(&cameraData_));

    // 初期値を設定
    cameraData_->farPlane = (*Object3dBasic::GetInstance()->GetCamera())->GetFarClip();
    cameraData_->nearPlane = (*Object3dBasic::GetInstance()->GetCamera())->GetNearClip();

    // unmap
    cameraResource_->Unmap(0, nullptr);
  }

} // namespace Tako
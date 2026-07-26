#include "GaussianBlur.h"
#include "Bloom.h"

#include "DX12Basic.h"
#include "WinApp.h"
#ifdef _DEBUG
#include "DebugUIManager.h"
#endif
#include "SrvManager.h"
#include "StringUtility.h"
#include "EnginePaths.h"

#ifdef _DEBUG
#include "ImGuiManager.h"
#endif

namespace Tako {

  GaussianBlur::~GaussianBlur()
  {
    if (winApp_ && onResizeId_ != 0) {
      winApp_->UnregisterOnResizeFunc(onResizeId_);
    }

    // RT の RTV/SRV を返却
    resultRT_.Release();
  }

  void GaussianBlur::Initialize(DX12Basic* dx12, const std::string& shaderName)
  {
    IPostEffect::Initialize(dx12, shaderName);
    CreateCBV();
    CreateRenderTexture();

    // WinApp のインスタンスを取得してリサイズコールバックを登録
    winApp_ = WinApp::GetInstance();
    if (winApp_) {
      onResizeId_ = winApp_->RegisterOnResizeFunc(
        std::bind(&GaussianBlur::OnResize, this, std::placeholders::_1)
      );
    }
  }

  void GaussianBlur::Apply(uint32_t inputSrvIndex, D3D12_CPU_DESCRIPTOR_HANDLE outputRtvHandle, [[maybe_unused]] uint32_t depthSrvIndex, [[maybe_unused]] const Vector4& clearColor)
  {
    // Pass1: 入力テクスチャを1方向にブラーして resultRT_ へ
    DrawFullScreenPass(rootSignature_.Get(), pipelineState_.Get(), resultRT_.rtvHandle,
      cBufferResource1_->GetGPUVirtualAddress(), inputSrvIndex);

    // Pass2: resultRT_ を直交方向にブラーして出力先へ
    SetBarrier(resultRT_.resource.Get(),
      D3D12_RESOURCE_STATE_RENDER_TARGET,
      D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

    DrawFullScreenPass(rootSignature_.Get(), pipelineState_.Get(), outputRtvHandle,
      cBufferResource2_->GetGPUVirtualAddress(), resultRT_.srvIndex);

    SetBarrier(resultRT_.resource.Get(),
      D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
      D3D12_RESOURCE_STATE_RENDER_TARGET);
  }

  void GaussianBlur::DrawImgui()
  {
#ifdef _DEBUG
    ImGui::DragFloat("Blur Sigma", &cBufferData1_->sigma, 0.01f, 0.0f, 50.0f);
    cBufferData2_->sigma = cBufferData1_->sigma; // Pass2でも同じ値を使用
    ImGui::DragInt("Blur Kernel Size", reinterpret_cast<int*>(&cBufferData1_->kernelSize), 1.0f, 1, 100);
    cBufferData2_->kernelSize = cBufferData1_->kernelSize; // Pass2でも同じ値を使用
#endif
  }

  bool GaussianBlur::SetGenericParam(const EffectParam& param)
  {
    if (auto* blurParam = std::get_if<GaussianBlurParam>(&param)) {
      SetParam(*blurParam);
      return true;
    }
    return false;
  }

  void GaussianBlur::SetParam(const GaussianBlurParam& param)
  {
    if (cBufferData1_ == nullptr) {
      return;
    }
    cBufferData1_->sigma = param.sigma;
    cBufferData1_->kernelSize = param.kernelSize;

    if (cBufferData2_ == nullptr) {
      return;
    }
    cBufferData2_->sigma = param.sigma;
    cBufferData2_->kernelSize = param.kernelSize;
  }

  void GaussianBlur::CreateRootSignature()
  {
    BuildRootSignature({ {RootParam::SrvTable, 0}, {RootParam::Cbv, 0} }, SamplerFilterMode::Linear, SamplerAddressMode::Clamp);
  }

  void GaussianBlur::CreatePSO()
  {
    BuildFullScreenPSO();
  }

  void GaussianBlur::CreateCBV()
  {
    cBufferResource1_ = dx12_->MakeBufferResource(sizeof(GaussianBlurParam));
    cBufferResource2_ = dx12_->MakeBufferResource(sizeof(GaussianBlurParam));

    // データの設定
    cBufferResource1_->Map(0, nullptr, reinterpret_cast<void**>(&cBufferData1_));
    cBufferResource2_->Map(0, nullptr, reinterpret_cast<void**>(&cBufferData2_));

    // データの初期化
    cBufferData1_->sigma = 2.0f;
    cBufferData1_->direction = { 0.0f, 1.0f };
    cBufferData1_->kernelSize = 9;

    cBufferData2_->sigma = 2.0f;
    cBufferData2_->direction = { 1.0f, 0.0f };
    cBufferData2_->kernelSize = 9;
  }

  void GaussianBlur::CreateRenderTexture()
  {
    resultRT_.Create(dx12_, WinApp::clientWidth, WinApp::clientHeight, DXGI_FORMAT_R8G8B8A8_UNORM, Vector4(0.0f, 0.0f, 0.0f, 1.0f));
  }

  void GaussianBlur::OnResize(const Vector2& newSize)
  {
    newSize; // 未使用の警告を抑制

    // RenderTexture を再作成
    RecreateRenderTexture();
  }

  void GaussianBlur::RecreateRenderTexture()
  {
    resultRT_.Release();

    CreateRenderTexture();
  }

  void GaussianBlur::SetBarrier(ID3D12Resource* resource, D3D12_RESOURCE_STATES stateBefore, D3D12_RESOURCE_STATES stateAfter)
  {
    D3D12_RESOURCE_BARRIER barrier{};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    barrier.Transition.pResource = resource;
    barrier.Transition.StateBefore = stateBefore;
    barrier.Transition.StateAfter = stateAfter;

    dx12_->GetCommandList()->ResourceBarrier(1, &barrier);
  }

} // namespace Tako

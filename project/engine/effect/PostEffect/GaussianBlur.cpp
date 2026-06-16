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
      return; // cBufferData_が初期化されていない場合は何もしない
    }
    // パラメータを設定する
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
    // BloomParam のリソース生成
    cBufferResource1_ = m_dx12_->MakeBufferResource(sizeof(GaussianBlurParam));
    cBufferResource2_ = m_dx12_->MakeBufferResource(sizeof(GaussianBlurParam));

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
    auto createRT = [this](RenderTexture& rt, int rtvIndex, const Vector4& clearColor) {
      // リソース作成
      m_dx12_->CreateRenderTextureResource(
        rt.resource,
        WinApp::clientWidth,
        WinApp::clientHeight,
        DXGI_FORMAT_R8G8B8A8_UNORM,
        clearColor
      );

      // RTV 作成
      rt.rtvHandle = m_dx12_->GetRenderTextureRTVHandle(rtvIndex);
      D3D12_RENDER_TARGET_VIEW_DESC rtvDesc{};
      rtvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
      rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
      m_dx12_->GetDevice()->CreateRenderTargetView(
        rt.resource.Get(), &rtvDesc, rt.rtvHandle
      );

      // SRV 作成
      rt.srvIndex = SrvManager::GetInstance()->Allocate();
      SrvManager::GetInstance()->CreateSRVForTexture2D(
        rt.srvIndex, rt.resource.Get(), DXGI_FORMAT_R8G8B8A8_UNORM, 1
      );
      };

    createRT(resultRT_, 9, Vector4(0.0f, 0.0f, 0.0f, 1.0f));
  }

  void GaussianBlur::OnResize(const Vector2& newSize)
  {
    newSize; // 未使用の警告を抑制

    // RenderTexture を再作成
    RecreateRenderTexture();
  }

  void GaussianBlur::RecreateRenderTexture()
  {
    resultRT_.resource.Reset();
    SrvManager::GetInstance()->Free(resultRT_.srvIndex);

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

    m_dx12_->GetCommandList()->ResourceBarrier(1, &barrier);
  }

} // namespace Tako

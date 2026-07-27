#include "PreviewViewport.h"

#ifdef _DEBUG

#include "DX12Basic.h"
#include "SrvManager.h"
#include "RtvManager.h"
#include "DsvManager.h"
#include "Object3dBasic.h"
#include "ImGuiManager.h"

#include <cassert>
#include <string>

namespace Tako {

  namespace {
    constexpr DXGI_FORMAT kColorFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
    constexpr DXGI_FORMAT kDepthFormat = DXGI_FORMAT_D32_FLOAT;
  }

  PreviewViewport::~PreviewViewport()
  {
    Finalize();
  }

  void PreviewViewport::Initialize(const wchar_t* debugName, uint32_t width, uint32_t height, const Vector4& clearColor)
  {
    if (renderTexture_) {
      return;
    }

    dx12_ = Object3dBasic::GetInstance()->GetDX12Basic();
    width_ = width;
    height_ = height;
    clearColor_ = clearColor;

    const std::wstring namePrefix = debugName;

    // RT
    dx12_->CreateRenderTextureResource(renderTexture_, width_, height_, kColorFormat, clearColor_);
    renderTexture_->SetName((namePrefix + L"RT").c_str());
    dx12_->SetInitialResourceState(renderTexture_.Get(), D3D12_RESOURCE_STATE_RENDER_TARGET);

    rtvIndex_ = RtvManager::GetInstance()->Allocate();
    RtvManager::GetInstance()->CreateRTV(rtvIndex_, renderTexture_.Get(), kColorFormat);
    rtvHandle_ = RtvManager::GetInstance()->GetCpuHandle(rtvIndex_);

    srvIndex_ = SrvManager::GetInstance()->Allocate();
    // 半透明描画で RT のアルファが 1 未満になると ImGui::Image がウィンドウ背景とブレンドしてしまうため、アルファ強制 1.0 で作る
    SrvManager::GetInstance()->CreateSRVForTexture2D(srvIndex_, renderTexture_.Get(), kColorFormat, 1, true);

    // 深度バッファ
    D3D12_RESOURCE_DESC depthDesc{};
    depthDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    depthDesc.Width = width_;
    depthDesc.Height = height_;
    depthDesc.DepthOrArraySize = 1;
    depthDesc.MipLevels = 1;
    depthDesc.Format = kDepthFormat;
    depthDesc.SampleDesc.Count = 1;
    depthDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    depthDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

    D3D12_HEAP_PROPERTIES heapProps{};
    heapProps.Type = D3D12_HEAP_TYPE_DEFAULT;

    D3D12_CLEAR_VALUE depthClear{};
    depthClear.Format = kDepthFormat;
    depthClear.DepthStencil.Depth = 1.0f;

    HRESULT hr = dx12_->GetDevice()->CreateCommittedResource(
      &heapProps,
      D3D12_HEAP_FLAG_NONE,
      &depthDesc,
      D3D12_RESOURCE_STATE_DEPTH_WRITE,
      &depthClear,
      IID_PPV_ARGS(&depthBuffer_));
    assert(SUCCEEDED(hr));
    depthBuffer_->SetName((namePrefix + L"Depth").c_str());

    dsvIndex_ = DsvManager::GetInstance()->Allocate();
    DsvManager::GetInstance()->CreateDSV(dsvIndex_, depthBuffer_.Get(), kDepthFormat);
    dsvHandle_ = DsvManager::GetInstance()->GetCpuHandle(dsvIndex_);
  }

  void PreviewViewport::Finalize()
  {
    if (!renderTexture_) {
      return;
    }

    SrvManager* srvManager = SrvManager::GetInstance();
    if (srvManager && srvManager->IsAllocated(srvIndex_)) {
      srvManager->Free(srvIndex_);
    }
    RtvManager::GetInstance()->Free(rtvIndex_);
    DsvManager::GetInstance()->Free(dsvIndex_);
    // 解放済みアドレスが別リソースに再利用された際の誤った状態遷移を防ぐ
    if (dx12_) {
      dx12_->RemoveResourceState(renderTexture_.Get());
    }
    renderTexture_.Reset();
    depthBuffer_.Reset();
    rtvIndex_ = 0;
    dsvIndex_ = 0;
    srvIndex_ = 0;
    dx12_ = nullptr;
  }

  void PreviewViewport::BeginPass()
  {
    ID3D12GraphicsCommandList* commandList = dx12_->GetCommandList();

    dx12_->TransitionResourceWithTracking(renderTexture_.Get(), D3D12_RESOURCE_STATE_RENDER_TARGET);
    commandList->OMSetRenderTargets(1, &rtvHandle_, FALSE, &dsvHandle_);
    const float clearColor[4] = { clearColor_.x, clearColor_.y, clearColor_.z, clearColor_.w };
    commandList->ClearRenderTargetView(rtvHandle_, clearColor, 0, nullptr);
    commandList->ClearDepthStencilView(dsvHandle_, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

    // RT サイズに合わせたビューポート/シザー（画面サイズとは独立）
    D3D12_VIEWPORT viewport{ 0.0f, 0.0f, static_cast<float>(width_), static_cast<float>(height_), 0.0f, 1.0f };
    D3D12_RECT scissorRect{ 0, 0, static_cast<LONG>(width_), static_cast<LONG>(height_) };
    commandList->RSSetViewports(1, &viewport);
    commandList->RSSetScissorRects(1, &scissorRect);
  }

  void PreviewViewport::EndPass()
  {
    dx12_->TransitionResourceWithTracking(renderTexture_.Get(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
  }

  bool PreviewViewport::DrawImGuiImage() const
  {
    const ImVec2 availableSize = ImGui::GetContentRegionAvail();
    const float aspectRatio = GetAspect();
    ImVec2 imageSize;
    if (availableSize.x / availableSize.y > aspectRatio) {
      imageSize.y = availableSize.y;
      imageSize.x = imageSize.y * aspectRatio;
    }
    else {
      imageSize.x = availableSize.x;
      imageSize.y = imageSize.x / aspectRatio;
    }
    ImVec2 cursorPos = ImGui::GetCursorPos();
    cursorPos.x += (availableSize.x - imageSize.x) * 0.5f;
    cursorPos.y += (availableSize.y - imageSize.y) * 0.5f;
    ImGui::SetCursorPos(cursorPos);

    D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle = SrvManager::GetInstance()->GetGPUDescriptorHandle(srvIndex_);
    ImGui::Image((ImTextureID)gpuHandle.ptr, imageSize);
    return ImGui::IsItemHovered();
  }

} // namespace Tako

#endif // _DEBUG

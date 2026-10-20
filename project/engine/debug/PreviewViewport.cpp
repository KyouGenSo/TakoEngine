#include "PreviewViewport.h"

#ifdef _DEBUG

#include "DX12Basic.h"
#include "SrvManager.h"
#include "RtvManager.h"
#include "DsvManager.h"
#include "PostProcess.h"
#include "Renderer.h"
#include "RenderPipeline.h"
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
    if (color_.resource) {
      return;
    }

    dx12_ = Renderer::GetInstance()->GetDX12Basic();
    width_ = width;
    height_ = height;
    clearColor_ = clearColor;

    const std::wstring namePrefix = debugName;

    // RT。半透明描画で RT のアルファが 1 未満になると ImGui::Image がウィンドウ背景とブレンドしてしまうため、SRV はアルファ強制 1.0 で作る
    color_.Create(dx12_, width_, height_, kColorFormat, clearColor_, true);
    color_.resource->SetName((namePrefix + L"RT").c_str());

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
      IID_PPV_ARGS(&depthBuffer_.resource));
    assert(SUCCEEDED(hr));
    depthBuffer_->SetName((namePrefix + L"Depth").c_str());
    depthBuffer_.state = D3D12_RESOURCE_STATE_DEPTH_WRITE;

    dsvIndex_ = DsvManager::GetInstance()->Allocate();
    DsvManager::GetInstance()->CreateDSV(dsvIndex_, depthBuffer_.Get(), kDepthFormat);
    dsvHandle_ = DsvManager::GetInstance()->GetCpuHandle(dsvIndex_);

    depthSrvIndex_ = SrvManager::GetInstance()->Allocate();
    SrvManager::GetInstance()->CreateSRVForTexture2D(depthSrvIndex_, depthBuffer_.Get(), DXGI_FORMAT_R32_FLOAT, 1);
  }

  void PreviewViewport::Finalize()
  {
    if (!color_.resource) {
      return;
    }

    SrvManager* srvManager = SrvManager::GetInstance();
    if (srvManager && srvManager->IsAllocated(color_.srvIndex)) {
      srvManager->Free(color_.srvIndex);
    }
    if (srvManager && srvManager->IsAllocated(depthSrvIndex_)) {
      srvManager->Free(depthSrvIndex_);
    }
    RtvManager::GetInstance()->Free(color_.rtvIndex);
    DsvManager::GetInstance()->Free(dsvIndex_);
    color_ = {};
    depthBuffer_.Reset();
    dsvIndex_ = 0;
    depthSrvIndex_ = 0;
    dx12_ = nullptr;
  }

  void PreviewViewport::BeginPass(bool postProcess)
  {
    // ポストエフェクトを掛けるときはシーンを一時 RT へ描き、EndPass で color_ へ書き出す
    sceneTarget_ = postProcess ? &RenderPipeline::GetInstance()->GetGraph().Acquire({ width_, height_, kColorFormat, clearColor_ }) : &color_;

    dx12_->GetContext().Transition(sceneTarget_->resource, D3D12_RESOURCE_STATE_RENDER_TARGET);
    dx12_->GetContext().Transition(depthBuffer_, D3D12_RESOURCE_STATE_DEPTH_WRITE);
    // RT サイズのビューポートで記録し、途中のシャドウ/デカールパスがここへ戻れるようにする
    dx12_->BindSceneRenderTarget({ sceneTarget_->rtvHandle, dsvHandle_, width_, height_ });
    ID3D12GraphicsCommandList* commandList = dx12_->GetCommandList();
    commandList->ClearRenderTargetView(sceneTarget_->rtvHandle, &clearColor_.x, 0, nullptr);
    commandList->ClearDepthStencilView(dsvHandle_, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);
  }

  void PreviewViewport::EndPass(const PostProcessStack* stack, const Camera* camera)
  {
    if (sceneTarget_ != &color_) {
      RenderGraph& graph = RenderPipeline::GetInstance()->GetGraph();
      const PostEffectFrame frame{ dx12_, &graph, camera, &depthBuffer_, depthSrvIndex_, nullptr, width_, height_ };
      stack->Execute(frame, *sceneTarget_, color_);
      graph.Release(*sceneTarget_);
    }
    sceneTarget_ = nullptr;
    dx12_->GetContext().Transition(color_.resource, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
  }

  void PreviewViewport::DrawImGuiImage() const
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

    D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle = SrvManager::GetInstance()->GetGPUDescriptorHandle(color_.srvIndex);
    // ボタンにしておくとドラッグ中はマウスを掴み続け、ウィンドウ移動にも奪われない
    ImGui::InvisibleButton("##PreviewImage", imageSize,
      ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight | ImGuiButtonFlags_MouseButtonMiddle);
    ImGui::GetWindowDrawList()->AddImage((ImTextureID)gpuHandle.ptr, ImGui::GetItemRectMin(), ImGui::GetItemRectMax());
  }

} // namespace Tako

#endif // _DEBUG

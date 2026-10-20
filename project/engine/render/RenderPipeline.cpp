#include "RenderPipeline.h"

#include <algorithm>

#include "DecalManager.h"
#include "DX12Basic.h"
#include "GPUParticle.h"
#include "LineRenderer.h"
#include "PostEffectManager.h"
#include "PostProcess.h"
#include "Renderer.h"
#include "SrvManager.h"
#include "WinApp.h"

namespace Tako {

  std::unique_ptr<RenderPipeline> RenderPipeline::instance_ = nullptr;

  RenderPipeline* RenderPipeline::GetInstance()
  {
    if (!instance_) {
      instance_ = std::make_unique<RenderPipeline>(Token{});
    }
    return instance_.get();
  }

  void RenderPipeline::Initialize(DX12Basic* dx12)
  {
    dx12_ = dx12;
    graph_.Initialize(dx12_);
    depthSrvIndex_ = SrvManager::GetInstance()->Allocate();
    CreateRenderTargets();
  }

  void RenderPipeline::Finalize()
  {
    ReleaseRenderTargets();
    graph_.Finalize();
    SrvManager::GetInstance()->Free(depthSrvIndex_);
    instance_.reset();
  }

  void RenderPipeline::Render()
  {
    graph_.BeginFrame();

    Renderer*                         renderer = Renderer::GetInstance();
    const Renderer::RenderView&       view     = renderer->GetMainView();
    const DrawList&                   scene    = renderer->GetSceneList();
    CommandContext&                   context  = dx12_->GetContext();
    GpuResource&                      depth    = dx12_->GetDepthStencilResource();
    const D3D12_CPU_DESCRIPTOR_HANDLE sceneDsv = dx12_->GetMainDSVHandle();
    const DX12Basic::SceneRenderTarget sceneTarget{ sceneColor_.rtvHandle, sceneDsv, sceneColor_.width, sceneColor_.height };

    //------------------------------ シーン ------------------------------//
    // 影・デカール・パーティクルは途中で描画先や深度の状態を変え、sceneTarget へ戻して抜ける
    context.Transition(sceneColor_.resource, D3D12_RESOURCE_STATE_RENDER_TARGET);
    context.Transition(depth, D3D12_RESOURCE_STATE_DEPTH_WRITE);
    dx12_->BindSceneRenderTarget(sceneTarget);
    ID3D12GraphicsCommandList* commandList = context.GetCommandList();
    commandList->ClearRenderTargetView(sceneColor_.rtvHandle, &clearColor_.x, 0, nullptr);
    commandList->ClearDepthStencilView(sceneDsv, D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);

    RunPasses(RenderEvent::BeforeShadow);
    renderer->RenderShadows(scene);
    renderer->RenderList(scene, view, INT32_MIN, ShaderAsset::kQueueTransparent);
    RunPasses(RenderEvent::AfterOpaque);

    DecalManager::GetInstance()->DrawAll();
#ifdef _DEBUG
    DecalManager::GetInstance()->DrawAllDebug();
#endif
    RunPasses(RenderEvent::BeforeTransparent);

    renderer->RenderList(scene, view, ShaderAsset::kQueueTransparent);
    GPUParticle::GetInstance()->Draw();
    LineRenderer::GetInstance()->Draw();
    RunPasses(RenderEvent::AfterTransparent);

    //------------------------------ マスク ------------------------------//
    // シーンの深度で隠れた部分を除いて maskId を書く。マスク付きのエフェクトが無いフレームは作らない
    PostEffectManager* postEffect = PostEffectManager::GetInstance();
    RenderTexture*     objectMask = nullptr;
    if (postEffect->UsesObjectMask()) {
      objectMask = &graph_.Acquire({ sceneColor_.width, sceneColor_.height, DXGI_FORMAT_R8_UINT });
      graph_.AddPass({ RenderGraph::Write(objectMask->resource), RenderGraph::DepthWrite(depth) }, [&] {
        dx12_->BindSceneRenderTarget({ objectMask->rtvHandle, sceneDsv, objectMask->width, objectMask->height, DXGI_FORMAT_R8_UINT });
        const float zero[4] = {};
        context.GetCommandList()->ClearRenderTargetView(objectMask->rtvHandle, zero, 0, nullptr);
        renderer->RenderMask(scene, view);
      });
      dx12_->BindSceneRenderTarget(sceneTarget);
    }

    //------------------------------ ポストエフェクト ------------------------------//
    RunPasses(RenderEvent::BeforePostProcess);
    const PostEffectFrame frame{ dx12_, &graph_, *renderer->GetCamera(), &depth, depthSrvIndex_, objectMask, sceneColor_.width, sceneColor_.height };
    postEffect->Execute(frame, sceneColor_, final_);
    if (objectMask) {
      graph_.Release(*objectMask);
    }

    //------------------------------ UI ------------------------------//
    // 縮小解像度でなければ UI もシーンの深度を使う（縮小中はウィンドウ解像度の専用深度）
    const D3D12_CPU_DESCRIPTOR_HANDLE uiDsv = dx12_->GetNonEffectDSVHandle();
    context.Transition(final_.resource, D3D12_RESOURCE_STATE_RENDER_TARGET);
    context.Transition(depth, D3D12_RESOURCE_STATE_DEPTH_WRITE);
    dx12_->BindSceneRenderTarget({ final_.rtvHandle, uiDsv, final_.width, final_.height });
    RunPasses(RenderEvent::AfterPostProcess);

    if (uiDsv.ptr != sceneDsv.ptr) {
      context.GetCommandList()->ClearDepthStencilView(uiDsv, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);
    }
    RunPasses(RenderEvent::BeforeUI);
    renderer->RenderList(renderer->GetOverlayList(), view);
    RunPasses(RenderEvent::AfterUI);
  }

  void RenderPipeline::Present(bool copyFinalResult)
  {
    CommandContext& context = dx12_->GetContext();
    context.Transition(final_.resource, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    dx12_->SetSwapChain();
    if (copyFinalResult) {
      context.SetGraphicsPipeline(MakeFullScreenPipeline(dx12_->GetShaderLibrary(), L"NoEffect.PS.hlsl"));
      context.Graphics().BindSRV("gTexture"_id, final_.srvIndex);
      context.DrawInstanced(3, 1, 0, 0);
    }
  }

  void RenderPipeline::OnResize()
  {
    ReleaseRenderTargets();
    CreateRenderTargets();
  }

  uint32_t RenderPipeline::AddRenderPass(RenderEvent event, std::function<void()> execute)
  {
    passes_.push_back({ nextPassId_, event, std::move(execute) });
    return nextPassId_++;
  }

  void RenderPipeline::RemoveRenderPass(uint32_t id)
  {
    std::erase_if(passes_, [id](const InjectedPass& pass) { return pass.id == id; });
  }

  void RenderPipeline::SetClearColor(const Vector4& color)
  {
    clearColor_ = color;
    // 作成時と異なる色でクリアするとデバッグレイヤーの警告で止まるため、次フレーム先頭で RT ごと作り直す
    dx12_->RequestRenderTargetRebuild();
  }

  void RenderPipeline::CreateRenderTargets()
  {
    sceneColor_.Create(dx12_, dx12_->GetSceneWidth(), dx12_->GetSceneHeight(), DX12Basic::kColorFormat, clearColor_);
    sceneColor_.resource->SetName(L"SceneColor");

    // 最終表示用なので SRV は α=1 固定で読ませ、ImGui 表示での背景透けを防ぐ
    final_.Create(dx12_, WinApp::clientWidth, WinApp::clientHeight, DX12Basic::kColorFormat, Vector4(0.0f, 0.0f, 0.0f, 0.0f), true);
    final_.resource->SetName(L"FinalResult");

    SrvManager::GetInstance()->CreateSRVForTexture2D(depthSrvIndex_, dx12_->GetDepthStencilResource().Get(), DXGI_FORMAT_R32_FLOAT, 1);
  }

  void RenderPipeline::ReleaseRenderTargets()
  {
    sceneColor_.Release();
    final_.Release();
  }

  void RenderPipeline::RunPasses(RenderEvent event)
  {
    for (const InjectedPass& pass : passes_) {
      if (pass.event == event) {
        pass.execute();
      }
    }
  }

} // namespace Tako

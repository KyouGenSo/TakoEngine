#include "RenderPipeline.h"

#include <algorithm>
#include <filesystem>
#include <format>
#include <fstream>
#include <iterator>

#include <json.hpp>

#include "DecalManager.h"
#include "DX12Basic.h"
#include "GPUParticle.h"
#include "LineRenderer.h"
#include "Logger.h"
#include "MaterialLibrary.h"
#include "PostEffectManager.h"
#include "PostProcess.h"
#include "Renderer.h"
#include "SrvManager.h"
#include "WinApp.h"

namespace Tako {

  using json = nlohmann::json;

  namespace {

    const std::filesystem::path kObjectPassPath = "resources/Json/RenderPasses.json";

    template<class T, size_t N>
    T FromName(const char* const (&names)[N], const std::string& name, T fallback)
    {
      for (size_t i = 0; i < N; ++i) {
        if (name == names[i]) {
          return static_cast<T>(i);
        }
      }
      return fallback;
    }

  } // anonymous namespace

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
#ifdef _DEBUG
    frameSteps_.clear();
#endif

    Renderer*                         renderer = Renderer::GetInstance();
    const Renderer::RenderView&       view     = renderer->GetMainView();
    const DrawList&                   scene    = renderer->GetSceneList();
    CommandContext&                   context  = dx12_->GetContext();
    GpuResource&                      depth    = dx12_->GetDepthStencilResource();
    const D3D12_CPU_DESCRIPTOR_HANDLE sceneDsv = dx12_->GetMainDSVHandle();
    const DX12Basic::SceneRenderTarget sceneTarget{ sceneColor_.rtvHandle, sceneDsv, sceneColor_.width, sceneColor_.height };

    // 手順を 1 つ行い、名前と描画回数を記録する
    const auto step = [&](std::string_view label, const auto& draw) {
      const uint32_t firstDraw = context.GetDrawCount();
      draw();
      RecordStep(label, firstDraw);
    };

    //------------------------------ シーン ------------------------------//
    // 影・デカール・パーティクルは途中で描画先や深度の状態を変え、sceneTarget へ戻して抜ける
    context.Transition(sceneColor_.resource, D3D12_RESOURCE_STATE_RENDER_TARGET);
    context.Transition(depth, D3D12_RESOURCE_STATE_DEPTH_WRITE);
    dx12_->BindSceneRenderTarget(sceneTarget);
    ID3D12GraphicsCommandList* commandList = context.GetCommandList();
    commandList->ClearRenderTargetView(sceneColor_.rtvHandle, &clearColor_.x, 0, nullptr);
    commandList->ClearDepthStencilView(sceneDsv, D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);

    RunPasses(RenderEvent::BeforeShadow);
    step("Shadows", [&] { renderer->RenderShadows(scene); });
    step("Opaque", [&] { renderer->RenderList(scene, view, { .maxQueue = ShaderAsset::kQueueTransparent, .excludeNameIds = replacedIds_ }); });
    RunPasses(RenderEvent::AfterOpaque);

    step("Decals", [&] {
      DecalManager::GetInstance()->DrawAll();
#ifdef _DEBUG
      DecalManager::GetInstance()->DrawAllDebug();
#endif
    });
    RunPasses(RenderEvent::BeforeTransparent);

    step("Transparent", [&] { renderer->RenderList(scene, view, { .minQueue = ShaderAsset::kQueueTransparent, .excludeNameIds = replacedIds_ }); });
    step("Particles", [&] { GPUParticle::GetInstance()->Draw(); });
    step("Lines", [&] { LineRenderer::GetInstance()->Draw(); });
    RunPasses(RenderEvent::AfterTransparent);

    //------------------------------ マスク ------------------------------//
    // シーンの深度で隠れた部分を除いて maskId を書く。マスク付きのエフェクトが無いフレームは作らない
    PostEffectManager* postEffect = PostEffectManager::GetInstance();
    RenderTexture*     objectMask = nullptr;
    if (postEffect->UsesObjectMask()) {
      objectMask = &graph_.Acquire({ sceneColor_.width, sceneColor_.height, DXGI_FORMAT_R8_UINT });
      step("Object Mask", [&] {
        graph_.AddPass({ RenderGraph::Write(objectMask->resource), RenderGraph::DepthWrite(depth) }, [&] {
          dx12_->BindSceneRenderTarget({ objectMask->rtvHandle, sceneDsv, objectMask->width, objectMask->height, DXGI_FORMAT_R8_UINT });
          const float zero[4] = {};
          context.GetCommandList()->ClearRenderTargetView(objectMask->rtvHandle, zero, 0, nullptr);
          renderer->RenderMask(scene, view);
        });
      });
      dx12_->BindSceneRenderTarget(sceneTarget);
    }

    //------------------------------ ポストエフェクト ------------------------------//
    RunPasses(RenderEvent::BeforePostProcess);
    const PostEffectFrame frame{ dx12_, &graph_, *renderer->GetCamera(), &depth, depthSrvIndex_, objectMask, sceneColor_.width, sceneColor_.height, view.constants };
    std::string postLabel = "Post Process";
#ifdef _DEBUG
    // 掛けたエフェクトを手順の名前に並べる（エフェクトごとの描画回数は数えない）
    const char* separator = ": ";
    for (const PostEffectInstance& effect : postEffect->GetStack().GetEffects()) {
      if (effect.enabled) {
        postLabel += separator + effect.name + (effect.maskId != 0 ? std::format(" [mask {}]", effect.maskId) : "");
        separator = ", ";
      }
    }
#endif
    step(postLabel, [&] { postEffect->Execute(frame, sceneColor_, final_); });
    if (objectMask) {
      graph_.Release(*objectMask);
    }

    //------------------------------ UI ------------------------------//
    // 縮小解像度でなければ UI もシーンの深度を使う（縮小中はウィンドウ解像度の専用深度を、差し込んだパスより前にクリアする）
    const D3D12_CPU_DESCRIPTOR_HANDLE uiDsv = dx12_->GetNonEffectDSVHandle();
    context.Transition(final_.resource, D3D12_RESOURCE_STATE_RENDER_TARGET);
    context.Transition(depth, D3D12_RESOURCE_STATE_DEPTH_WRITE);
    dx12_->BindSceneRenderTarget({ final_.rtvHandle, uiDsv, final_.width, final_.height });
    if (uiDsv.ptr != sceneDsv.ptr) {
      context.GetCommandList()->ClearDepthStencilView(uiDsv, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);
    }
    RunPasses(RenderEvent::AfterPostProcess);
    RunPasses(RenderEvent::BeforeUI);
    step("UI", [&] { renderer->RenderList(renderer->GetOverlayList(), view); });
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

  uint32_t RenderPipeline::AddRenderPass(RenderEvent event, std::string name, std::function<void()> execute)
  {
    passes_.push_back({ nextPassId_, event, std::move(name), std::move(execute) });
    return nextPassId_++;
  }

  void RenderPipeline::RemoveRenderPass(uint32_t id)
  {
    std::erase_if(passes_, [id](const InjectedPass& pass) { return pass.id == id; });
  }

  void RenderPipeline::LoadObjectPasses()
  {
    std::ifstream ifs(kObjectPassPath);
    if (ifs) {
      ApplyObjectPasses(std::string(std::istreambuf_iterator<char>(ifs), std::istreambuf_iterator<char>()));
    }
  }

  bool RenderPipeline::SaveObjectPasses() const
  {
    std::error_code ec;
    std::filesystem::create_directories(kObjectPassPath.parent_path(), ec);
    std::ofstream ofs(kObjectPassPath);
    if (!ofs) {
      Logger::Log("Render passes save failed: %s", kObjectPassPath.string().c_str());
      return false;
    }
    ofs << SerializeObjectPasses() << std::endl;
    return true;
  }

  std::string RenderPipeline::SerializeObjectPasses() const
  {
    json passes = json::array();
    for (const ObjectPassSettings& pass : objectPassSettings_) {
      passes.push_back({
        { "Name",     pass.name },
        { "Event",    kRenderEventNames[static_cast<size_t>(pass.event)] },
        { "Enabled",  pass.enabled },
        { "Queue",    kQueueFilterNames[static_cast<size_t>(pass.queue)] },
        { "Objects",  pass.objects },
        { "Material", pass.material },
        { "Replace",  pass.replace },
      });
    }
    return json{ { "Passes", passes } }.dump(4);
  }

  bool RenderPipeline::ApplyObjectPasses(const std::string& text)
  {
    std::vector<ObjectPassSettings> settings;
    try {
      const json root = json::parse(text);
      for (const json& entry : root.value("Passes", json::array())) {
        ObjectPassSettings& pass = settings.emplace_back();
        pass.name     = entry.value("Name", std::string());
        pass.event    = FromName(kRenderEventNames, entry.value("Event", std::string()), RenderEvent::AfterTransparent);
        pass.enabled  = entry.value("Enabled", true);
        pass.queue    = FromName(kQueueFilterNames, entry.value("Queue", std::string()), QueueFilter::All);
        pass.objects  = entry.value("Objects", std::vector<std::string>{});
        pass.material = entry.value("Material", std::string());
        pass.replace  = entry.value("Replace", false);
      }
    }
    catch (const json::exception& e) {
      Logger::Log("Render passes load failed: %s", e.what());
      return false;
    }
    objectPassSettings_ = std::move(settings);
    RefreshObjectPasses();
    return true;
  }

  void RenderPipeline::RefreshObjectPasses()
  {
    objectPasses_.clear();
    replacedIds_.clear();
    for (const ObjectPassSettings& settings : objectPassSettings_) {
      ObjectPass& pass = objectPasses_.emplace_back();
      if (!settings.material.empty()) {
        pass.material = MaterialLibrary::GetInstance()->Load(settings.material);
      }
      for (const std::string& object : settings.objects) {
        pass.objectIds.push_back(MakeShaderParamId(object));
      }
      // パスが描けるときだけ本来の描画から外す（マテリアルを読めなかったパスは描かないので、消えないよう外さない）
      if (settings.enabled && settings.replace && (settings.material.empty() || pass.material)) {
        replacedIds_.insert(replacedIds_.end(), pass.objectIds.begin(), pass.objectIds.end());
      }
    }
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
    CommandContext& context = dx12_->GetContext();
    RecordStep(kRenderEventNames[static_cast<size_t>(event)], context.GetDrawCount(), 0, true);

    for (const InjectedPass& pass : passes_) {
      if (pass.event == event) {
        const uint32_t firstDraw = context.GetDrawCount();
        pass.execute();
        RecordStep(pass.name, firstDraw, 1);
      }
    }

    // オブジェクトパスは本編の視点で、シーンの描画物を描き直す
    Renderer* renderer = Renderer::GetInstance();
    for (size_t i = 0; i < objectPassSettings_.size() && i < objectPasses_.size(); ++i) {
      const ObjectPassSettings& settings = objectPassSettings_[i];
      const ObjectPass&         pass     = objectPasses_[i];
      // マテリアルを読めなかったパスは、各描画物のマテリアルで描き直さないよう飛ばす
      if (!settings.enabled || settings.event != event || (!settings.material.empty() && !pass.material)) {
        continue;
      }
      DrawFilter filter{ .nameIds = pass.objectIds, .overrideMaterial = pass.material.get() };
      if (settings.queue == QueueFilter::Opaque) {
        filter.maxQueue = ShaderAsset::kQueueTransparent;
      }
      else if (settings.queue == QueueFilter::Transparent) {
        filter.minQueue = ShaderAsset::kQueueTransparent;
      }
      const uint32_t firstDraw = context.GetDrawCount();
      renderer->RenderList(renderer->GetSceneList(), renderer->GetMainView(), filter);
      RecordStep(settings.name, firstDraw, 1);
    }
  }

  void RenderPipeline::RecordStep([[maybe_unused]] std::string_view label, [[maybe_unused]] uint32_t firstDraw, [[maybe_unused]] uint8_t depth, [[maybe_unused]] bool isEvent)
  {
#ifdef _DEBUG
    frameSteps_.push_back({ std::string(label), dx12_->GetContext().GetDrawCount() - firstDraw, depth, isEvent });
#endif
  }

} // namespace Tako

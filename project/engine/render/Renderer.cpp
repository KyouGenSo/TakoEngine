#include "Renderer.h"

#include <algorithm>
#include <cassert>
#include <tuple>

#include "Camera.h"
#include "DX12Basic.h"
#include "Mat4x4Func.h"
#include "Material.h"
#include "ShadowRenderer.h"
#include "WinApp.h"

namespace Tako {

  std::unique_ptr<Renderer> Renderer::instance_ = nullptr;

  Renderer* Renderer::GetInstance()
  {
    if (!instance_) {
      instance_ = std::make_unique<Renderer>(Token{});
    }
    return instance_.get();
  }

  void Renderer::Initialize(DX12Basic* dx12)
  {
    dx12_ = dx12;

    light_ = std::make_unique<Light>();
    light_->Initialize(dx12_);

    screenView_ = Mat4x4::MakeIdentity();
    OnResize(Vector2(static_cast<float>(WinApp::clientWidth), static_cast<float>(WinApp::clientHeight)));

    RegisterBuiltinShaders();
  }

  void Renderer::Finalize()
  {
    light_.reset();
    instance_.reset();
  }

  void Renderer::Update()
  {
    camera_->SetViewProjectionMatrix(camera_->GetViewMatrix() * camera_->GetProjectionMatrix());
    mainView_ = MakeView(*camera_);

    light_->Update();
  }

  void Renderer::BeginFrame()
  {
    sceneList_.Clear();
    overlayList_.Clear();
  }

  void Renderer::Submit(const DrawItem& item)
  {
    assert(current_ && "Renderer: Submit outside BeginSubmit/EndSubmit");
    assert(item.material && "Renderer: DrawItem without material");
    if (current_) {
      current_->Add(item);
    }
  }

  void Renderer::RenderShadows(const DrawList& list)
  {
    ShadowRenderer* shadow = ShadowRenderer::GetInstance();
    if (!shadow->IsEnabled()) {
      return;
    }

    shadow->BeginShadowPass();
    CommandContext& context = dx12_->GetContext();
    GraphicsPipelineDesc current{};
    bool hasCurrent = false;
    for (const DrawItem& item : list.GetItems()) {
      GraphicsPipelineDesc pipeline;
      if (!item.castShadow || !item.material->GetPipeline(ShaderPass::ShadowCaster, item.instanceSrv != 0, pipeline)) {
        continue;
      }
      if (!hasCurrent || !(pipeline == current)) {
        context.SetGraphicsPipeline(pipeline);
        context.Graphics().BindCBV("gShadowConstants"_id, shadow->GetShadowConstantsAddress());
        current = pipeline;
        hasCurrent = true;
      }
      DrawItemGeometry(item, false);
    }
    shadow->EndShadowPass();
  }

  void Renderer::RenderList(const DrawList& list, const RenderView& view, int32_t minQueue, int32_t maxQueue)
  {
    const std::span<const DrawItem> items = list.GetItems();

    sortEntries_.clear();
    for (uint32_t i = 0; i < items.size(); ++i) {
      const DrawItem& item = items[i];
      int32_t queue = item.material->GetRenderQueue();
      if (item.transparent) {
        queue = (std::max)(queue, ShaderAsset::kQueueTransparent);
      }
      if (queue < minQueue || queue >= maxQueue) {
        continue;
      }
      // 半透明は奥から手前へ。不透明は登録順のまま描く
      float depthKey = 0.0f;
      if (queue >= ShaderAsset::kQueueTransparent && item.depthSort) {
        const float dx = item.sortPosition.x - view.position.x;
        const float dy = item.sortPosition.y - view.position.y;
        const float dz = item.sortPosition.z - view.position.z;
        depthKey = -(dx * dx + dy * dy + dz * dz);
      }
      sortEntries_.push_back({ queue, item.sortingOrder, depthKey, i });
    }
    std::stable_sort(sortEntries_.begin(), sortEntries_.end(), [](const SortEntry& a, const SortEntry& b) {
      return std::tie(a.queue, a.sortingOrder, a.depthKey) < std::tie(b.queue, b.sortingOrder, b.depthKey);
    });

    CommandContext& context = dx12_->GetContext();
    GraphicsPipelineDesc current{};
    bool hasCurrent = false;
    for (const SortEntry& entry : sortEntries_) {
      const DrawItem& item = items[entry.index];
      GraphicsPipelineDesc pipeline;
      if (!item.material->GetPipeline(ShaderPass::Forward, item.instanceSrv != 0, pipeline)) {
        continue;
      }
      if (item.transparent) {
        pipeline.renderState.cull = CullMode::None;
        pipeline.renderState.depthWrite = false;
      }
      if (!hasCurrent || !(pipeline == current)) {
        context.SetGraphicsPipeline(pipeline);
        BindGlobals(view);
        current = pipeline;
        hasCurrent = true;
      }
      DrawItemGeometry(item, true);
    }
  }

  void Renderer::RenderMask(const DrawList& list, const RenderView& view)
  {
    CommandContext& context = dx12_->GetContext();
    GraphicsPipelineDesc current{};
    bool hasCurrent = false;
    for (const DrawItem& item : list.GetItems()) {
      GraphicsPipelineDesc pipeline;
      if (item.maskId == 0 || !item.material->GetPipeline(ShaderPass::Mask, item.instanceSrv != 0, pipeline)) {
        continue;
      }
      if (!hasCurrent || !(pipeline == current)) {
        context.SetGraphicsPipeline(pipeline);
        context.Graphics().BindCBV("gView"_id, view.constants);
        current = pipeline;
        hasCurrent = true;
      }
      context.Graphics().BindConstants("gMaskParam"_id, item.maskId);
      DrawItemGeometry(item, false);
    }
  }

  void Renderer::RenderImmediate(const RenderView& view, const std::function<void()>& submit)
  {
    DrawList list;
    DrawList* previous = current_;
    current_ = &list;
    submit();
    current_ = previous;

    RenderList(list, view);
  }

  Renderer::RenderView Renderer::MakeView(const Camera& camera)
  {
    const ViewConstants constants{ camera.GetViewMatrix() * camera.GetProjectionMatrix(), camera.GetTranslate(), 0.0f };
    return { dx12_->GetUploadAllocator().Push(constants), camera.GetTranslate() };
  }

  ShaderAsset& Renderer::RegisterShader(const std::string& name)
  {
    std::unique_ptr<ShaderAsset>& slot = shaders_[name];
    if (!slot) {
      slot = std::make_unique<ShaderAsset>(name);
    }
    return *slot;
  }

  void Renderer::OnResize(const Vector2& size)
  {
    screenProjection_ = Mat4x4::MakeOrtho(0.0f, 0.0f, size.x, size.y, 0.0f, 100.0f);
  }

  const ShaderAsset* Renderer::FindShader(std::string_view name) const
  {
    auto it = shaders_.find(std::string(name));
    return it != shaders_.end() ? it->second.get() : nullptr;
  }

  void Renderer::RegisterBuiltinShaders()
  {
    ShaderLibrary& library = dx12_->GetShaderLibrary();

    // ライティング付きのメッシュ
    GraphicsPipelineDesc lit;
    lit.vs = library.Get(L"Object3D.VS.hlsl", ShaderStage::Vertex);
    lit.ps = library.Get(L"Object3d.PS.hlsl", ShaderStage::Pixel);
    lit.vertexLayout = VertexLayout::Mesh;
    lit.renderState.blend = BlendMode::Alpha;

    GraphicsPipelineDesc litInstanced = lit;
    litInstanced.vs = library.Get(L"Object3dInstanced.VS.hlsl", ShaderStage::Vertex);
    litInstanced.ps = library.Get(L"Object3dInstanced.PS.hlsl", ShaderStage::Pixel);

    // フロントフェースカリングでピーターパニングを緩和する。深度だけを書くのでピクセルシェーダーは使わない
    GraphicsPipelineDesc shadowCaster;
    shadowCaster.vs = library.Get(L"ShadowMap.VS.hlsl", ShaderStage::Vertex);
    shadowCaster.vertexLayout = VertexLayout::Position;
    shadowCaster.renderState.cull = CullMode::Front;
    shadowCaster.renderState.depthFunc = D3D12_COMPARISON_FUNC_LESS;

    GraphicsPipelineDesc shadowCasterInstanced = shadowCaster;
    shadowCasterInstanced.vs = library.Get(L"ShadowMapInstanced.VS.hlsl", ShaderStage::Vertex);

    // シーンの深度で隠れた画素には書かない。整数の RT なのでブレンドしない
    // ponytail: インスタンシング版は無い。InstancedObject3d に maskId を付けるときに足す
    GraphicsPipelineDesc mask;
    mask.vs = library.Get(L"Mask.VS.hlsl", ShaderStage::Vertex);
    mask.ps = library.Get(L"Mask.PS.hlsl", ShaderStage::Pixel);
    mask.vertexLayout = VertexLayout::Position;
    mask.renderState = { .blend = BlendMode::Opaque, .depthWrite = false };

    ShaderAsset& litShader = RegisterShader("Lit");
    litShader.SetPass(ShaderPass::Forward, lit);
    litShader.SetPass(ShaderPass::Forward, litInstanced, true);
    litShader.SetPass(ShaderPass::ShadowCaster, shadowCaster);
    litShader.SetPass(ShaderPass::ShadowCaster, shadowCasterInstanced, true);
    litShader.SetPass(ShaderPass::Mask, mask);

    // 最遠面として描くので深度は書き込まない
    GraphicsPipelineDesc skyBox;
    skyBox.vs = library.Get(L"SkyBox.VS.hlsl", ShaderStage::Vertex);
    skyBox.ps = library.Get(L"SkyBox.PS.hlsl", ShaderStage::Pixel);
    skyBox.vertexLayout = VertexLayout::Position;
    skyBox.renderState.depthWrite = false;

    ShaderAsset& skyBoxShader = RegisterShader("SkyBox");
    skyBoxShader.SetPass(ShaderPass::Forward, skyBox);
    skyBoxShader.SetDefaultQueue(ShaderAsset::kQueueBackground);

    // スクリーン空間の矩形。変換行列にスクリーン射影まで含める
    GraphicsPipelineDesc sprite;
    sprite.vs = library.Get(L"Sprite.VS.hlsl", ShaderStage::Vertex);
    sprite.ps = library.Get(L"Sprite.PS.hlsl", ShaderStage::Pixel);
    sprite.vertexLayout = VertexLayout::Sprite;
    sprite.renderState = { .blend = BlendMode::Alpha, .cull = CullMode::None, .depthTest = false, .depthWrite = false };

    ShaderAsset& spriteShader = RegisterShader("Sprite");
    spriteShader.SetPass(ShaderPass::Forward, sprite);
    spriteShader.SetDefaultQueue(ShaderAsset::kQueueOverlay);
  }

  void Renderer::BindGlobals(const RenderView& view)
  {
    dx12_->GetContext().Graphics().BindCBV("gView"_id, view.constants);
    light_->PreDraw();
    ShadowRenderer::GetInstance()->SetShadowForMainPass();
  }

  void Renderer::DrawItemGeometry(const DrawItem& item, bool bindMaterial)
  {
    CommandContext& context = dx12_->GetContext();
    ID3D12GraphicsCommandList* commandList = context.GetCommandList();
    commandList->IASetVertexBuffers(0, 1, item.vertexBuffer);
    commandList->IASetIndexBuffer(item.indexBuffer);

    CommandContext::ShaderBinder binder = context.Graphics();
    if (item.transform != 0) {
      binder.BindCBV("gTransformationMatrix"_id, item.transform);
    }
    if (item.instanceSrv != 0) {
      binder.BindSRV("gInstanceData"_id, item.instanceSrv);
    }
    if (bindMaterial) {
      item.material->Bind(binder);
      if (item.textureOverride != 0) {
        binder.BindSRV("gTexture"_id, item.textureOverride);
      }
    }
    context.DrawIndexedInstanced(item.indexCount, item.instanceCount, item.startIndex, 0, 0);
  }

} // namespace Tako

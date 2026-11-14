#include "PostProcess.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <type_traits>

#include "DX12Basic.h"
#include "MaterialPropertyLayout.h"

namespace Tako {

  namespace {

    struct Registry {
      std::vector<std::string>          names;
      std::vector<PostEffectDefinition> definitions;
    };

    Registry& GetRegistry()
    {
      static Registry registry;
      return registry;
    }

    /// <summary>
    /// 実行するエフェクト 1 つ分（スタック外の FXAA・拡大コピーも同じ形で扱う）
    /// </summary>
    struct Step {
      const PostEffectDefinition* definition;
      const EffectParam*          param;
    };

    const EffectParam          kNoParam;
    const PostEffectDefinition kAntiAliasing{ .pixelShader = L"FXAA.PS.hlsl" };
    const PostEffectDefinition kCopy{ .pixelShader = L"NoEffect.PS.hlsl" };

    void Run(const PostEffectFrame& frame, const PostEffectDefinition& definition, const EffectParam& param, RenderTexture& input, RenderTexture& output)
    {
      if (definition.execute) {
        definition.execute(frame, param, input, output);
        return;
      }

      const auto draw = [&] {
        const Shader&                ps     = *frame.dx12->GetShaderLibrary().Get(definition.pixelShader, ShaderStage::Pixel);
        CommandContext::ShaderBinder binder = BeginFullScreen(frame, ps, output);
        binder.BindSRV("gTexture"_id, input.srvIndex);
        if (definition.usesDepth) {
          binder.BindSRV("gDepthTexture"_id, frame.depthSrv);
        }
        // 宣言していないシェーダーには何もしない
        if (frame.view != 0) {
          binder.BindCBV("gView"_id, frame.view);
        }
        EffectParam value = param;
        if (definition.setup) {
          definition.setup(frame, value, binder);
        }
        // 色の値（名前に color を含む float3/float4）は sRGB で持つので、シーンの色に掛けるときはリニアに直す
        std::visit([&]<class T>(T& constants) {
          if constexpr (std::is_same_v<T, PropertyBlock>) {
            if (!constants.bytes.empty()) {
              if (frame.linear && constants.layout) {
                constants.layout->ToLinearColors(constants.bytes);
              }
              const UploadAllocator::Allocation allocation = frame.dx12->GetUploadAllocator().Allocate(constants.bytes.size());
              std::memcpy(allocation.cpu, constants.bytes.data(), constants.bytes.size());
              binder.BindCBV("gParam"_id, allocation.gpu);
            }
          }
          else if constexpr (!std::is_same_v<T, std::monostate>) {
            if (frame.linear) {
              MaterialPropertyLayout::ForShader(ps, "gParam")->ToLinearColors(std::span(reinterpret_cast<uint8_t*>(&constants), sizeof(T)));
            }
            binder.BindConstants("gParam"_id, constants);
          }
        }, value);
        DrawFullScreen(frame);
      };

      RenderGraph& graph = *frame.graph;
      if (definition.usesDepth) {
        graph.AddPass({ RenderGraph::Read(input.resource), RenderGraph::Read(*frame.depth), RenderGraph::Write(output.resource) }, draw);
      }
      else {
        graph.AddPass({ RenderGraph::Read(input.resource), RenderGraph::Write(output.resource) }, draw);
      }
    }

    /// <param name="event">maskId 0 のときに選ぶ定義の event（マスク付きは位置を問わない）</param>
    std::vector<Step> CollectSteps(std::span<const PostEffectInstance> effects, uint32_t maskId, std::optional<RenderEvent> event = std::nullopt)
    {
      std::vector<Step> steps;
      for (const PostEffectInstance& effect : effects) {
        const PostEffectDefinition* definition = FindPostEffect(effect.name);
        if (effect.enabled && definition && effect.maskId == maskId && (maskId != 0 || definition->event == event)) {
          steps.push_back({ definition, &effect.param });
        }
      }
      return steps;
    }

    /// <summary>
    /// steps を順に掛ける。input の解像度・形式の一時 RT へ交互に書く
    /// </summary>
    /// <returns>最後に書いた RT（手順が無ければ input）。input でなければ呼び出し側が Release する</returns>
    RenderTexture* RunSteps(const PostEffectFrame& frame, std::span<const Step> steps, RenderTexture& input)
    {
      RenderGraph&   graph  = *frame.graph;
      RenderTexture* source = &input;
      for (size_t i = 0; i < steps.size(); ++i) {
        RenderTexture& target = graph.Acquire(RenderGraph::DescOf(input));
        Run(frame, *steps[i].definition, *steps[i].param, *source, target);
        if (source != &input) {
          graph.Release(*source);
        }
        source = &target;
      }
      return source;
    }

  } // anonymous namespace

  void RegisterPostEffect(const std::string& name, PostEffectDefinition definition)
  {
    Registry& registry = GetRegistry();
    const auto it = std::ranges::find(registry.names, name);
    if (it != registry.names.end()) {
      registry.definitions[it - registry.names.begin()] = std::move(definition);
      return;
    }
    registry.names.push_back(name);
    registry.definitions.push_back(std::move(definition));
  }

  void UnregisterPostEffect(std::string_view name)
  {
    Registry& registry = GetRegistry();
    const auto it = std::ranges::find(registry.names, name);
    if (it == registry.names.end()) {
      return;
    }
    registry.definitions.erase(registry.definitions.begin() + (it - registry.names.begin()));
    registry.names.erase(it);
  }

  const PostEffectDefinition* FindPostEffect(std::string_view name)
  {
    const Registry& registry = GetRegistry();
    const auto it = std::ranges::find(registry.names, name);
    return it != registry.names.end() ? &registry.definitions[it - registry.names.begin()] : nullptr;
  }

  std::span<const std::string> GetPostEffectNames()
  {
    return GetRegistry().names;
  }

  GraphicsPipelineDesc MakeFullScreenPipeline(ShaderLibrary& library, std::wstring_view psFile)
  {
    return MakeFullScreenPipeline(library, *library.Get(psFile, ShaderStage::Pixel));
  }

  GraphicsPipelineDesc MakeFullScreenPipeline(ShaderLibrary& library, const Shader& ps)
  {
    GraphicsPipelineDesc pipeline;
    pipeline.vs = library.Get(L"FullScreen.VS.hlsl", ShaderStage::Vertex);
    pipeline.ps = &ps;
    pipeline.renderState.cull = CullMode::None;
    pipeline.renderState.depthTest = false;
    pipeline.renderState.depthWrite = false;
    pipeline.topology = D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP;
    return pipeline;
  }

  CommandContext::ShaderBinder BeginFullScreen(const PostEffectFrame& frame, std::wstring_view psFile, RenderTexture& output, BlendMode blend)
  {
    return BeginFullScreen(frame, *frame.dx12->GetShaderLibrary().Get(psFile, ShaderStage::Pixel), output, blend);
  }

  CommandContext::ShaderBinder BeginFullScreen(const PostEffectFrame& frame, const Shader& ps, RenderTexture& output, BlendMode blend)
  {
    GraphicsPipelineDesc pipeline = MakeFullScreenPipeline(frame.dx12->GetShaderLibrary(), ps);
    pipeline.renderState.blend = blend;

    CommandContext& context = frame.dx12->GetContext();
    context.SetRenderTarget(&output.rtvHandle, output.format, nullptr, DXGI_FORMAT_UNKNOWN);
    const D3D12_VIEWPORT viewport{ 0.0f, 0.0f, static_cast<float>(output.width), static_cast<float>(output.height), 0.0f, 1.0f };
    const D3D12_RECT     scissor{ 0, 0, static_cast<LONG>(output.width), static_cast<LONG>(output.height) };
    ID3D12GraphicsCommandList* commandList = context.GetCommandList();
    commandList->RSSetViewports(1, &viewport);
    commandList->RSSetScissorRects(1, &scissor);
    context.SetGraphicsPipeline(pipeline);
    return context.Graphics();
  }

  void DrawFullScreen(const PostEffectFrame& frame)
  {
    frame.dx12->GetContext().DrawInstanced(3, 1, 0, 0);
  }

  RenderTexture& CopyToTemporary(const PostEffectFrame& frame, RenderTexture& source)
  {
    RenderTexture& copy = frame.graph->Acquire(RenderGraph::DescOf(source));
    Run(frame, kCopy, kNoParam, source, copy);
    CommandContext& context = frame.dx12->GetContext();
    context.Transition(copy.resource, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    context.Transition(source.resource, D3D12_RESOURCE_STATE_RENDER_TARGET);
    return copy;
  }

  void ToneMapToDisplay(const PostEffectFrame& frame, RenderTexture& source, RenderTexture& output)
  {
    // ToneMap.PS の ToneMapParam と同じ並び
    struct ToneMapConstants {
      float    exposureScale;
      uint32_t mode;
      float    padding[2];
    };
    frame.graph->AddPass({ RenderGraph::Read(source.resource), RenderGraph::Write(output.resource) }, [&] {
      CommandContext::ShaderBinder binder = BeginFullScreen(frame, L"ToneMap.PS.hlsl", output);
      binder.BindSRV("gTexture"_id, source.srvIndex);
      binder.BindConstants("gParam"_id, ToneMapConstants{ std::exp2(frame.toneMap.exposure), static_cast<uint32_t>(frame.toneMap.mode), {} });
      DrawFullScreen(frame);
    });
  }

  uint32_t PostProcessStack::Add(std::string_view name, uint32_t maskId)
  {
    const PostEffectDefinition* definition = FindPostEffect(name);
    return definition ? Add(name, definition->defaultParam, maskId) : 0;
  }

  uint32_t PostProcessStack::Add(std::string_view name, const EffectParam& param, uint32_t maskId)
  {
    if (!FindPostEffect(name)) {
      return 0;
    }
    effects_.push_back({ nextId_, std::string(name), param, maskId });
    return nextId_++;
  }

  void ResolvePostEffectValues(PostEffectInstance& effect)
  {
    if (effect.material && effect.material->shader != effect.name) {
      effect.name = effect.material->shader;
    }
    const PostEffectDefinition* definition = FindPostEffect(effect.name);
    const PropertyBlock*        defaults   = definition ? std::get_if<PropertyBlock>(&definition->defaultParam) : nullptr;
    if (!defaults) {
      return;
    }
    PropertyBlock* block = std::get_if<PropertyBlock>(&effect.param);
    if (!block) {
      block = &effect.param.emplace<PropertyBlock>();
    }
    // 値を名前で重ねるので、PS のホットリロードで並びが変わっても今の並びで作り直せる。assign は 2 回目から確保しない
    block->layout = defaults->layout;
    block->bytes.assign(defaults->bytes.begin(), defaults->bytes.end());
    block->textures.clear();
    if (effect.material) {
      effect.material->values.Apply(*block->layout, block->bytes);
      block->textures.assign(effect.material->values.GetTextures().begin(), effect.material->values.GetTextures().end());
    }
    effect.overrides.Apply(*block->layout, block->bytes);
  }

  void PostProcessStack::ResolveValues()
  {
    for (PostEffectInstance& effect : effects_) {
      ResolvePostEffectValues(effect);
    }
  }

  void PostProcessStack::Remove(uint32_t id)
  {
    std::erase_if(effects_, [id](const PostEffectInstance& effect) { return effect.id == id; });
  }

  void PostProcessStack::Execute(const PostEffectFrame& frame, RenderTexture& input, RenderTexture& output, bool antiAliasing,
                                 const std::function<void(RenderTexture&)>& afterEffects) const
  {
    std::vector<Step> steps = CollectSteps(effects_, 0);
    if (antiAliasing) {
      steps.push_back({ &kAntiAliasing, &kNoParam });
    }

    // エフェクトはシーンの色（リニアの HDR）のまま掛け、表示用の output へは最後のトーンマップだけが書く
    RenderTexture* source = RunSteps(frame, steps, input);
    if (afterEffects) {
      afterEffects(*source);
    }
    ToneMapToDisplay(frame, *source, output);
    if (source != &input) {
      frame.graph->Release(*source);
    }
  }

  bool PostProcessStack::ExecuteAt(const PostEffectFrame& frame, RenderEvent event, RenderTexture& target) const
  {
    const std::vector<Step> steps = CollectSteps(effects_, 0, event);
    if (steps.empty()) {
      return false;
    }
    // 読みながら同じ RT へは書けないので、一時 RT に書いてから写す（URP の Full Screen Pass も入力をコピーする）
    RenderTexture* result = RunSteps(frame, steps, target);
    Run(frame, kCopy, kNoParam, *result, target);
    frame.graph->Release(*result);
    return true;
  }

  void PostProcessStack::ExecuteMasked(const PostEffectFrame& frame, uint32_t maskId, RenderTexture& layer, RenderTexture& target) const
  {
    RenderTexture* effected = RunSteps(frame, CollectSteps(effects_, maskId), layer);
    // 描画物の外の (0,0,0,0) は何も変えず、α=0 で色だけある画素（Bloom の光など）は加算になる
    frame.graph->AddPass({ RenderGraph::Read(effected->resource), RenderGraph::Write(target.resource) }, [&] {
      CommandContext::ShaderBinder binder = BeginFullScreen(frame, kCopy.pixelShader, target, BlendMode::Premultiplied);
      binder.BindSRV("gTexture"_id, effected->srvIndex);
      DrawFullScreen(frame);
    });
    if (effected != &layer) {
      frame.graph->Release(*effected);
    }
  }

  PostEffectInstance* PostProcessStack::Find(uint32_t id)
  {
    const auto it = std::ranges::find(effects_, id, &PostEffectInstance::id);
    return it != effects_.end() ? &*it : nullptr;
  }

  bool PostProcessStack::HasEnabledEffects() const
  {
    return std::ranges::any_of(effects_, &PostEffectInstance::enabled);
  }

  std::vector<uint32_t> PostProcessStack::GetMaskIds() const
  {
    std::vector<uint32_t> maskIds;
    for (const PostEffectInstance& effect : effects_) {
      if (effect.enabled && effect.maskId != 0) {
        maskIds.push_back(effect.maskId);
      }
    }
    std::ranges::sort(maskIds);
    maskIds.erase(std::ranges::unique(maskIds).begin(), maskIds.end());
    return maskIds;
  }

} // namespace Tako

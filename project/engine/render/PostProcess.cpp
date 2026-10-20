#include "PostProcess.h"

#include <algorithm>
#include <type_traits>

#include "DX12Basic.h"

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
      uint32_t                    maskId;
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
        CommandContext::ShaderBinder binder = BeginFullScreen(frame, definition.pixelShader, output);
        binder.BindSRV("gTexture"_id, input.srvIndex);
        if (definition.usesDepth) {
          binder.BindSRV("gDepthTexture"_id, frame.depthSrv);
        }
        EffectParam value = param;
        if (definition.setup) {
          definition.setup(frame, value, binder);
        }
        std::visit([&binder]<class T>(const T& constants) {
          if constexpr (!std::is_same_v<T, std::monostate>) {
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

    void Apply(const PostEffectFrame& frame, const Step& step, RenderTexture& input, RenderTexture& output)
    {
      if (step.maskId == 0) {
        Run(frame, *step.definition, *step.param, input, output);
        return;
      }

      // 一度画面全体に掛けてから、maskId が一致する画素だけを採る
      RenderGraph& graph = *frame.graph;
      RenderTexture& effected = graph.Acquire(RenderGraph::DescOf(input));
      Run(frame, *step.definition, *step.param, input, effected);
      graph.AddPass({ RenderGraph::Read(input.resource), RenderGraph::Read(effected.resource), RenderGraph::Read(frame.objectMask->resource), RenderGraph::Write(output.resource) }, [&] {
        CommandContext::ShaderBinder binder = BeginFullScreen(frame, L"MaskComposite.PS.hlsl", output);
        binder.BindSRV("gTexture"_id, input.srvIndex);
        binder.BindSRV("gEffected"_id, effected.srvIndex);
        binder.BindSRV("gObjectMask"_id, frame.objectMask->srvIndex);
        binder.BindConstants("gMaskParam"_id, step.maskId);
        DrawFullScreen(frame);
      });
      graph.Release(effected);
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
    GraphicsPipelineDesc pipeline;
    pipeline.vs = library.Get(L"FullScreen.VS.hlsl", ShaderStage::Vertex);
    pipeline.ps = library.Get(psFile, ShaderStage::Pixel);
    pipeline.renderState.cull = CullMode::None;
    pipeline.renderState.depthTest = false;
    pipeline.renderState.depthWrite = false;
    pipeline.topology = D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP;
    return pipeline;
  }

  CommandContext::ShaderBinder BeginFullScreen(const PostEffectFrame& frame, std::wstring_view psFile, RenderTexture& output)
  {
    const GraphicsPipelineDesc pipeline = MakeFullScreenPipeline(frame.dx12->GetShaderLibrary(), psFile);

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

  void PostProcessStack::Remove(uint32_t id)
  {
    std::erase_if(effects_, [id](const PostEffectInstance& effect) { return effect.id == id; });
  }

  void PostProcessStack::Execute(const PostEffectFrame& frame, RenderTexture& input, RenderTexture& output, bool antiAliasing) const
  {
    std::vector<Step> steps;
    for (const PostEffectInstance& effect : effects_) {
      const PostEffectDefinition* definition = FindPostEffect(effect.name);
      if (effect.enabled && definition && (effect.maskId == 0 || frame.objectMask)) {
        steps.push_back({ definition, &effect.param, effect.maskId });
      }
    }
    if (antiAliasing) {
      steps.push_back({ &kAntiAliasing, &kNoParam, 0 });
    }

    // 途中のエフェクトは入力の解像度で一時 RT へ交互に書き、出力と同じ大きさなら最後だけ出力へ直接書く
    RenderGraph&   graph    = *frame.graph;
    const bool     sameSize = input.width == output.width && input.height == output.height;
    RenderTexture* source   = &input;
    for (size_t i = 0; i < steps.size(); ++i) {
      RenderTexture& target = (sameSize && i + 1 == steps.size()) ? output : graph.Acquire(RenderGraph::DescOf(input));
      Apply(frame, steps[i], *source, target);
      if (source != &input) {
        graph.Release(*source);
      }
      source = &target;
    }

    if (source != &output) {
      Run(frame, kCopy, kNoParam, *source, output);
      if (source != &input) {
        graph.Release(*source);
      }
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

  bool PostProcessStack::UsesObjectMask() const
  {
    return std::ranges::any_of(effects_, [](const PostEffectInstance& effect) { return effect.enabled && effect.maskId != 0; });
  }

} // namespace Tako

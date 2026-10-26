#include "PostProcess.h"

#include <algorithm>
#include <cstring>
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
        // 宣言していないシェーダーには何もしない
        if (frame.view != 0) {
          binder.BindCBV("gView"_id, frame.view);
        }
        EffectParam value = param;
        if (definition.setup) {
          definition.setup(frame, value, binder);
        }
        std::visit([&]<class T>(const T& constants) {
          if constexpr (std::is_same_v<T, PropertyBlock>) {
            if (!constants.bytes.empty()) {
              const UploadAllocator::Allocation allocation = frame.dx12->GetUploadAllocator().Allocate(constants.bytes.size());
              std::memcpy(allocation.cpu, constants.bytes.data(), constants.bytes.size());
              binder.BindCBV("gParam"_id, allocation.gpu);
            }
          }
          else if constexpr (!std::is_same_v<T, std::monostate>) {
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

    std::vector<Step> CollectSteps(std::span<const PostEffectInstance> effects, uint32_t maskId)
    {
      std::vector<Step> steps;
      for (const PostEffectInstance& effect : effects) {
        const PostEffectDefinition* definition = FindPostEffect(effect.name);
        if (effect.enabled && definition && effect.maskId == maskId) {
          steps.push_back({ definition, &effect.param });
        }
      }
      return steps;
    }

    /// <summary>
    /// steps を順に掛ける。途中は input の解像度の一時 RT へ交互に書く
    /// </summary>
    /// <param name="last">nullptr でなければ最後の手順はここへ書く</param>
    /// <returns>最後に書いた RT（手順が無ければ input）。input・last 以外なら呼び出し側が Release する</returns>
    RenderTexture* RunSteps(const PostEffectFrame& frame, std::span<const Step> steps, RenderTexture& input, RenderTexture* last)
    {
      RenderGraph&   graph  = *frame.graph;
      RenderTexture* source = &input;
      for (size_t i = 0; i < steps.size(); ++i) {
        RenderTexture& target = (last && i + 1 == steps.size()) ? *last : graph.Acquire(RenderGraph::DescOf(input));
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

  bool RefreshPropertyLayout(std::string_view name, EffectParam& param)
  {
    PropertyBlock*              block      = std::get_if<PropertyBlock>(&param);
    const PostEffectDefinition* definition = FindPostEffect(name);
    const PropertyBlock*        latest     = definition ? std::get_if<PropertyBlock>(&definition->defaultParam) : nullptr;
    if (!block || !latest || block->layout == latest->layout) {
      return false;
    }
    // 新しく増えた値は定義の初期値から始める
    PropertyBlock refreshed = *latest;
    if (block->layout) {
      refreshed.layout->DeserializeValues(block->layout->SerializeValues(block->bytes), refreshed.bytes);
    }
    *block = std::move(refreshed);
    return true;
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

  CommandContext::ShaderBinder BeginFullScreen(const PostEffectFrame& frame, std::wstring_view psFile, RenderTexture& output, BlendMode blend)
  {
    GraphicsPipelineDesc pipeline = MakeFullScreenPipeline(frame.dx12->GetShaderLibrary(), psFile);
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

  void PostProcessStack::RefreshLayouts()
  {
    for (PostEffectInstance& effect : effects_) {
      RefreshPropertyLayout(effect.name, effect.param);
    }
  }

  void PostProcessStack::Remove(uint32_t id)
  {
    std::erase_if(effects_, [id](const PostEffectInstance& effect) { return effect.id == id; });
  }

  void PostProcessStack::Execute(const PostEffectFrame& frame, RenderTexture& input, RenderTexture& output, bool antiAliasing) const
  {
    std::vector<Step> steps = CollectSteps(effects_, 0);
    if (antiAliasing) {
      steps.push_back({ &kAntiAliasing, &kNoParam });
    }

    // 出力と同じ大きさなら最後のエフェクトは出力へ直接書く
    const bool     sameSize = input.width == output.width && input.height == output.height;
    RenderTexture* source   = RunSteps(frame, steps, input, sameSize ? &output : nullptr);
    if (source != &output) {
      Run(frame, kCopy, kNoParam, *source, output);
      if (source != &input) {
        frame.graph->Release(*source);
      }
    }
  }

  void PostProcessStack::ExecuteMasked(const PostEffectFrame& frame, uint32_t maskId, RenderTexture& layer, RenderTexture& target) const
  {
    RenderTexture* effected = RunSteps(frame, CollectSteps(effects_, maskId), layer, nullptr);
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

#include "IPostEffect.h"

#include "DX12Basic.h"
#include "StringUtility.h"

namespace Tako {

  void IPostEffect::Initialize(DX12Basic* dx12, const std::string& shaderName)
  {
    dx12_ = dx12;
    shaderName_ = shaderName;
  }

  GraphicsPipelineDesc IPostEffect::MakeFullScreenPipeline(std::wstring_view psFile) const
  {
    ShaderLibrary& library = dx12_->GetShaderLibrary();

    GraphicsPipelineDesc desc;
    desc.vs = library.Get(L"FullScreen.VS.hlsl", ShaderStage::Vertex);
    desc.ps = library.Get(psFile, ShaderStage::Pixel);
    desc.renderState.cull = CullMode::None;
    desc.renderState.depthTest = false;
    desc.renderState.depthWrite = false;
    desc.topology = D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP;
    return desc;
  }

  CommandContext::ShaderBinder IPostEffect::BeginFullScreenPass(const GraphicsPipelineDesc& pipeline, D3D12_CPU_DESCRIPTOR_HANDLE outputRtv, uint32_t inputSrvIndex)
  {
    CommandContext& context = dx12_->GetContext();
    // 深度無効の全画面パスなので DSV はバインドしない
    context.SetRenderTarget(&outputRtv, DX12Basic::kColorFormat, nullptr, DXGI_FORMAT_UNKNOWN);
    context.SetGraphicsPipeline(pipeline);

    CommandContext::ShaderBinder binder = context.Graphics();
    binder.BindSRV("gTexture"_id, inputSrvIndex);
    return binder;
  }

  void IPostEffect::DrawFullScreen()
  {
    dx12_->GetContext().DrawInstanced(3, 1, 0, 0);
  }

  const GraphicsPipelineDesc& IPostEffect::GetPipeline()
  {
    if (!pipeline_.ps) {
      pipeline_ = MakeFullScreenPipeline(StringUtility::ConvertString(shaderName_ + ".PS.hlsl"));
    }
    return pipeline_;
  }

} // namespace Tako

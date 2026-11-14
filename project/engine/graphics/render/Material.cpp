#include "Material.h"

#include <algorithm>

namespace Tako {

  Material::Material(const ShaderAsset* shader)
  {
    SetShader(shader);
  }

  void Material::SetShader(const ShaderAsset* shader)
  {
    assert(shader && "Material: shader is null");
    shader_ = shader;
  }

  std::span<uint8_t> Material::GetConstantBytes(size_t size)
  {
    if (size != constants_.size() && size != 0) {
      constants_.assign(size, 0);
    }
    return constants_;
  }

  std::unique_ptr<Material> Material::Clone() const
  {
    auto clone = std::make_unique<Material>(shader_);
    clone->CopyFrom(*this);
    return clone;
  }

  void Material::CopyFrom(const Material& source)
  {
    shader_ = source.shader_;
    stateOverride_ = source.stateOverride_;
    queueOverride_ = source.queueOverride_;
    textures_ = source.textures_;
    if (!source.constants_.empty()) {
      std::ranges::copy(source.constants_, GetConstantBytes(source.constants_.size()).begin());
    }
  }

  bool Material::GetPipeline(ShaderPass pass, bool instanced, GraphicsPipelineDesc& pipeline) const
  {
    const GraphicsPipelineDesc* source = shader_->GetPass(pass, instanced);
    if (!source) {
      return false;
    }
    pipeline = *source;
    if (pass == ShaderPass::Forward) {
      pipeline.renderState = stateOverride_.ApplyTo(source->renderState);
    }
    return true;
  }

  RenderState Material::GetRenderState() const
  {
    const GraphicsPipelineDesc* forward = shader_->GetPass(ShaderPass::Forward);
    return stateOverride_.ApplyTo(forward ? forward->renderState : RenderState{});
  }

  void Material::SetTexture(ShaderParamId id, uint32_t srvIndex)
  {
    auto it = std::find_if(textures_.begin(), textures_.end(), [id](const auto& texture) { return texture.first == id; });
    if (srvIndex == 0) {
      if (it != textures_.end()) {
        textures_.erase(it);
      }
      return;
    }
    if (it != textures_.end()) {
      it->second = srvIndex;
    }
    else {
      textures_.emplace_back(id, srvIndex);
    }
  }

  uint32_t Material::GetTexture(ShaderParamId id) const
  {
    auto it = std::find_if(textures_.begin(), textures_.end(), [id](const auto& texture) { return texture.first == id; });
    return it != textures_.end() ? it->second : 0;
  }

} // namespace Tako

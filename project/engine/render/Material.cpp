#include "Material.h"

#include <algorithm>
#include <cstring>

#include "DX12Basic.h"

namespace Tako {

  Material::Material(DX12Basic* dx12, const ShaderAsset* shader)
    : dx12_(dx12)
  {
    SetShader(shader);
  }

  void Material::SetShader(const ShaderAsset* shader)
  {
    assert(shader && "Material: shader is null");
    shader_ = shader;
    if (const GraphicsPipelineDesc* forward = shader_->GetPass(ShaderPass::Forward)) {
      renderState_ = forward->renderState;
    }
    renderQueue_ = shader_->GetDefaultQueue();
  }

  std::span<uint8_t> Material::GetConstantBytes(size_t size)
  {
    if (size != constantSize_ && size != 0) {
      // 記録済みのコマンドが古い定数バッファを指していることがあるので、フレームの終わりまで残す
      if (constantBuffer_) {
        dx12_->ReleaseAfterFrame(std::move(constantBuffer_));
      }
      AllocateConstants(size);
    }
    return { constants_, constantSize_ };
  }

  std::unique_ptr<Material> Material::Clone() const
  {
    auto clone = std::make_unique<Material>(dx12_, shader_);
    clone->renderState_ = renderState_;
    clone->renderQueue_ = renderQueue_;
    clone->textures_ = textures_;
    if (constants_) {
      clone->AllocateConstants(constantSize_);
      std::memcpy(clone->constants_, constants_, constantSize_);
    }
    return clone;
  }

  void Material::Bind(CommandContext::ShaderBinder& binder) const
  {
    if (constantBuffer_) {
      binder.BindCBV("gMaterial"_id, constantBuffer_->GetGPUVirtualAddress());
    }
    for (const auto& [id, srvIndex] : textures_) {
      binder.BindSRV(id, srvIndex);
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
      pipeline.renderState = renderState_;
    }
    return true;
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

  void Material::AllocateConstants(size_t size)
  {
    const size_t capacity = (size + D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT - 1) & ~static_cast<size_t>(D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT - 1);
    constantBuffer_ = dx12_->MakeBufferResource(capacity);
    constantBuffer_->Map(0, nullptr, reinterpret_cast<void**>(&constants_));
    std::memset(constants_, 0, capacity);
    constantSize_ = size;
  }

} // namespace Tako

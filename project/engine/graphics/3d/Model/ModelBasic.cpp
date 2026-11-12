#include "ModelBasic.h"
#include "DX12Basic.h"


namespace Tako {

  void ModelBasic::Initialize(DX12Basic* dx12)
  {
    dx12_ = dx12;

    directoryFolderName_ = "resources";

    modelFolderName_ = "Model";

    skinningPipeline_.cs = dx12_->GetShaderLibrary().Get(L"Skinning.CS.hlsl", ShaderStage::Compute);
  }

  void ModelBasic::SetSkinningCSSetting()
  {
    dx12_->GetContext().SetComputePipeline(skinningPipeline_);
  }

} // namespace Tako

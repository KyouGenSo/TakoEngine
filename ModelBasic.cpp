#include "ModelBasic.h"
#include "DX12Basic.h"

void ModelBasic::Initialize(DX12Basic* dx12)
{
	m_dx12_ = dx12;

	directoryFolderName_ = "resources";

	ModelFolderName_ = "Model";

}

#include"SrvManager.h"
#include"DX12Basic.h"

SrvManager* SrvManager::instance_ = nullptr;

const uint32_t SrvManager::kMaxSRVCount = 512;

SrvManager* SrvManager::GetInstance()
{
	if (instance_ == nullptr)
	{
		instance_ = new SrvManager();
	}
	return instance_;
}

void SrvManager::Initialize(DX12Basic* dx12)
{
	m_dx12_ = dx12;

	// SRVのディスクリプタのサイズを取得
	descriptorSize_ = m_dx12_->GetDevice()->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

	// SRVのディスクリプタヒープの生成
	descriptorHeap_ = m_dx12_->CreateDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, kMaxSRVCount, true);
}

void SrvManager::Finalize()
{
	if (instance_ != nullptr)
	{
		delete instance_;
		instance_ = nullptr;
	}
}

void SrvManager::BeginDraw()
{
	// SRVのディスクリプタヒープをセット
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> descriptorHeaps[] = { descriptorHeap_.Get() };
	m_dx12_->GetCommandList()->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps->GetAddressOf());
}

uint32_t SrvManager::Allocate()
{
	int index = srvIndex;

	srvIndex++;

	return index;
}

bool SrvManager::CanAllocate()
{
	return srvIndex < kMaxSRVCount;
}

void SrvManager::CreateSRVForTexture2D(uint32_t index, ID3D12Resource* pResource, DXGI_FORMAT format, UINT mipLevels)
{
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Format = format;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.Texture2D.MipLevels = mipLevels;

	m_dx12_->GetDevice()->CreateShaderResourceView(pResource, &srvDesc, GetCPUDescriptorHandle(index));
}

void SrvManager::CreateSRVForStructuredBuffer(uint32_t index, ID3D12Resource* pResource, UINT numElements, UINT structureByteStride)
{
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Format = DXGI_FORMAT_UNKNOWN;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.Buffer.NumElements = numElements;
	srvDesc.Buffer.StructureByteStride = structureByteStride;
	srvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;

	m_dx12_->GetDevice()->CreateShaderResourceView(pResource, &srvDesc, GetCPUDescriptorHandle(index));
}

void SrvManager::SetRootDescriptorTable(UINT rootParameterIndex, uint32_t index)
{
	m_dx12_->GetCommandList()->SetGraphicsRootDescriptorTable(rootParameterIndex, GetGPUDescriptorHandle(index));
}

D3D12_CPU_DESCRIPTOR_HANDLE SrvManager::GetCPUDescriptorHandle(uint32_t index)
{
	D3D12_CPU_DESCRIPTOR_HANDLE handleCPU = descriptorHeap_->GetCPUDescriptorHandleForHeapStart();
	handleCPU.ptr += descriptorSize_ * index;

	return handleCPU;
}

D3D12_GPU_DESCRIPTOR_HANDLE SrvManager::GetGPUDescriptorHandle(uint32_t index)
{
	D3D12_GPU_DESCRIPTOR_HANDLE handleGPU = descriptorHeap_->GetGPUDescriptorHandleForHeapStart();
	handleGPU.ptr += descriptorSize_ * index;

	return handleGPU;
}

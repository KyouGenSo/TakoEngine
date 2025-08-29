#include "ShadowMap.h"
#include "DX12Basic.h"
#include "SrvManager.h"
#include "Matrix4x4.h"
#include "Mat4x4Func.h"
#include <cassert>

void ShadowMap::Initialize(DX12Basic* dx12)
{
  assert(dx12);
  dx12_ = dx12;
  srvManager_ = SrvManager::GetInstance();
  assert(srvManager_);

  // シャドウマップリソースの作成
  CreateShadowMapResource();

  // DSVの作成
  CreateDepthStencilView();

  // SRVの作成
  CreateShaderResourceView();

  // 定数バッファの作成
  CreateConstantBuffer();

  // ビューポートとシザー矩形の設定
  viewport_.Width = static_cast<float>(shadowMapSize_);
  viewport_.Height = static_cast<float>(shadowMapSize_);
  viewport_.TopLeftX = 0.0f;
  viewport_.TopLeftY = 0.0f;
  viewport_.MinDepth = 0.0f;
  viewport_.MaxDepth = 1.0f;

  scissorRect_.left = 0;
  scissorRect_.top = 0;
  scissorRect_.right = shadowMapSize_;
  scissorRect_.bottom = shadowMapSize_;
}

void ShadowMap::Finalize()
{
  // nullチェックと有効なインデックスの確認
  if (srvManager_ && srvIndex_ != 0) {
    // SrvManagerが有効かつインデックスが割り当てられている場合のみ解放
    if (srvManager_->IsAllocated(srvIndex_)) {
      srvManager_->Free(srvIndex_);
    }
    srvIndex_ = 0;
  }
  
  // ポインタをクリア
  srvManager_ = nullptr;
}

void ShadowMap::BeginFrame()
{
  // 遅延リソース再作成の処理
  if (pendingRecreation_) {
    // 前フレームの描画が完了しているので、安全にリソースを再作成できる
    
    // 既存のリソースを解放
    if (srvIndex_ != 0) {
      srvManager_->Free(srvIndex_);
      srvIndex_ = 0;
    }
    shadowMapResource_.Reset();
    dsvDescriptorHeap_.Reset();
    
    // 新しいサイズを適用
    shadowMapSize_ = pendingShadowMapSize_;
    
    // リソースを再作成
    CreateShadowMapResource();
    CreateDepthStencilView();
    CreateShaderResourceView();
    
    // ビューポートとシザー矩形を更新
    viewport_.Width = static_cast<float>(shadowMapSize_);
    viewport_.Height = static_cast<float>(shadowMapSize_);
    scissorRect_.right = shadowMapSize_;
    scissorRect_.bottom = shadowMapSize_;
    
    // フラグをクリア
    pendingRecreation_ = false;
  }
}

void ShadowMap::BeginShadowMapRender()
{
  // コマンドリストの取得
  ID3D12GraphicsCommandList* commandList = dx12_->GetCommandList();

  // リソースバリアの設定（現在の状態から DEPTH_WRITE へ）
  dx12_->TransitionResourceWithTracking(
    shadowMapResource_.Get(),
    D3D12_RESOURCE_STATE_DEPTH_WRITE
  );

  // レンダーターゲットをnullに設定（深度のみ）
  commandList->OMSetRenderTargets(0, nullptr, FALSE, &dsvHandle_);

  // 深度バッファをクリア
  commandList->ClearDepthStencilView(dsvHandle_, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

  // ビューポートとシザー矩形を設定
  commandList->RSSetViewports(1, &viewport_);
  commandList->RSSetScissorRects(1, &scissorRect_);
  
  // 初回フレームフラグをクリア
  if (isFirstFrame_) {
    isFirstFrame_ = false;
  }
}

void ShadowMap::EndShadowMapRender()
{
  // DEPTH_WRITE -> PIXEL_SHADER_RESOURCE に遷移
  dx12_->TransitionResourceWithTracking(
    shadowMapResource_.Get(),
    D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE
  );
}

void ShadowMap::SetLightViewProjectionMatrix(const Matrix4x4& lightViewProj)
{
  lightViewProjectionMatrix_ = lightViewProj;

  // 定数バッファを更新
  if (constantBufferData_) {
    constantBufferData_->lightViewProjectionMatrix = lightViewProjectionMatrix_;
    constantBufferData_->depthBias = static_cast<float>(depthBias_);
    constantBufferData_->slopeScaledDepthBias = slopeScaledDepthBias_;
    constantBufferData_->normalOffsetBias = normalOffsetBias_;
    constantBufferData_->pcfKernelSize = static_cast<float>(pcfKernelSize_);
  }
}

void ShadowMap::CreateShadowMapResource()
{
  // シャドウマップ用のテクスチャリソースを作成
  D3D12_RESOURCE_DESC resourceDesc = {};
  resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
  resourceDesc.Alignment = 0;
  resourceDesc.Width = shadowMapSize_;
  resourceDesc.Height = shadowMapSize_;
  resourceDesc.DepthOrArraySize = 1;
  resourceDesc.MipLevels = 1;
  resourceDesc.Format = DXGI_FORMAT_R32_TYPELESS;
  resourceDesc.SampleDesc.Count = 1;
  resourceDesc.SampleDesc.Quality = 0;
  resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
  resourceDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

  D3D12_HEAP_PROPERTIES heapProps = {};
  heapProps.Type = D3D12_HEAP_TYPE_DEFAULT;
  heapProps.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
  heapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;

  D3D12_CLEAR_VALUE clearValue = {};
  clearValue.Format = DXGI_FORMAT_D32_FLOAT;
  clearValue.DepthStencil.Depth = 1.0f;
  clearValue.DepthStencil.Stencil = 0;

  HRESULT hr = dx12_->GetDevice()->CreateCommittedResource(
    &heapProps,
    D3D12_HEAP_FLAG_NONE,
    &resourceDesc,
    D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,  // 初期状態をPIXEL_SHADER_RESOURCEに設定
    &clearValue,
    IID_PPV_ARGS(&shadowMapResource_));

  assert(SUCCEEDED(hr));
  
  // 初期状態をDX12Basicの状態追跡マップに登録
  // これにより、最初のBeginShadowMapRenderで正しい状態遷移が行われる
  dx12_->SetInitialResourceState(
    shadowMapResource_.Get(),
    D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE
  );
}

void ShadowMap::CreateDepthStencilView()
{
  // DSV用のディスクリプタヒープを作成
  dsvDescriptorHeap_ = dx12_->CreateDescriptorHeap(
    D3D12_DESCRIPTOR_HEAP_TYPE_DSV, 1, false);
  assert(dsvDescriptorHeap_);

  // DSVの作成
  D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
  dsvDesc.Format = DXGI_FORMAT_D32_FLOAT;
  dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
  dsvDesc.Flags = D3D12_DSV_FLAG_NONE;
  dsvDesc.Texture2D.MipSlice = 0;

  dsvHandle_ = dsvDescriptorHeap_->GetCPUDescriptorHandleForHeapStart();
  dx12_->GetDevice()->CreateDepthStencilView(
    shadowMapResource_.Get(), &dsvDesc, dsvHandle_);
}

void ShadowMap::CreateShaderResourceView()
{
  // SRVインデックスを確保
  srvIndex_ = srvManager_->Allocate();
  assert(srvManager_->CanAllocate());

  // SRVの作成
  srvManager_->CreateSRVForTexture2D(
    srvIndex_, shadowMapResource_.Get(), DXGI_FORMAT_R32_FLOAT, 1);
}

void ShadowMap::CreateConstantBuffer()
{
  // 定数バッファの作成
  constantBuffer_ = dx12_->MakeBufferResource(sizeof(ShadowConstantBuffer));
  assert(constantBuffer_);

  // 定数バッファをマップ
  HRESULT hr = constantBuffer_->Map(0, nullptr, reinterpret_cast<void**>(&constantBufferData_));
  assert(SUCCEEDED(hr));

  // 初期値を設定
  constantBufferData_->lightViewProjectionMatrix = Mat4x4::MakeIdentity();
  constantBufferData_->depthBias = static_cast<float>(depthBias_);
  constantBufferData_->slopeScaledDepthBias = slopeScaledDepthBias_;
  constantBufferData_->normalOffsetBias = normalOffsetBias_;
  constantBufferData_->pcfKernelSize = static_cast<float>(pcfKernelSize_);
}

void ShadowMap::SetShadowQuality(ShadowQuality quality)
{
  currentQuality_ = quality;
  
  // 品質に応じた設定
  uint32_t newSize = DEFAULT_SHADOW_MAP_SIZE;
  switch (quality) {
    case ShadowQuality::Low:
      newSize = 512;
      pcfKernelSize_ = 1;
      break;
    case ShadowQuality::Medium:
      newSize = 1024;
      pcfKernelSize_ = 3;
      break;
    case ShadowQuality::High:
      newSize = 2048;
      pcfKernelSize_ = 5;
      break;
    case ShadowQuality::Ultra:
      newSize = 4096;
      pcfKernelSize_ = 7;
      break;
    case ShadowQuality::Super:
      newSize = 8192;
      pcfKernelSize_ = 9;
      break;
  }
  
  // サイズが変更される場合は次フレームで再作成
  if (shadowMapSize_ != newSize && shadowMapResource_) {
    pendingShadowMapSize_ = newSize;
    pendingRecreation_ = true;
  } else {
    shadowMapSize_ = newSize;
  }
  
  // 定数バッファを更新（PCFカーネルサイズはすぐに変更可能）
  if (constantBufferData_) {
    constantBufferData_->pcfKernelSize = static_cast<float>(pcfKernelSize_);
  }
}

void ShadowMap::SetShadowMapSize(uint32_t size)
{
  // 2のべき乗にクランプ
  uint32_t clampedSize = 256;
  while (clampedSize < size && clampedSize < 8192) {
    clampedSize *= 2;
  }
  
  // サイズが変更される場合は次フレームで再作成
  if (shadowMapSize_ != clampedSize && shadowMapResource_) {
    pendingShadowMapSize_ = clampedSize;
    pendingRecreation_ = true;
  } else if (!shadowMapResource_) {
    // 初回の場合はすぐに設定
    shadowMapSize_ = clampedSize;
  }
}

void ShadowMap::SetPCFKernelSize(int kernelSize)
{
  // 奇数値のみ許可（1, 3, 5, 7, 9）
  if (kernelSize >= 1 && kernelSize <= 9 && kernelSize % 2 == 1) {
    pcfKernelSize_ = kernelSize;
    
    // 定数バッファを更新
    if (constantBufferData_) {
      constantBufferData_->pcfKernelSize = static_cast<float>(pcfKernelSize_);
    }
  }
}
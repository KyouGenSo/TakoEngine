#include "ShadowMap.h"
#include "DX12Basic.h"
#include "SrvManager.h"
#include "DsvManager.h"
#include <algorithm>
#include <cassert>

namespace Tako {

namespace {
    // シャドウ深度テクスチャのフォーマット。
    // TYPELESS リソースを DSV（深度書き込み）と SRV（深度読み取り）で別ビューとして扱う。
    constexpr DXGI_FORMAT kShadowTypelessFormat = DXGI_FORMAT_R32_TYPELESS; ///< リソース本体
    constexpr DXGI_FORMAT kShadowDepthFormat    = DXGI_FORMAT_D32_FLOAT;    ///< DSV / クリア値
    constexpr DXGI_FORMAT kShadowSrvFormat      = DXGI_FORMAT_R32_FLOAT;    ///< SRV（読み取り）
}

void ShadowMap::Initialize(DX12Basic* dx12)
{
  assert(dx12);
  dx12_ = dx12;
  srvManager_ = SrvManager::GetInstance();
  assert(srvManager_);

  // シャドウマップリソース（深度テクスチャ）の作成
  CreateShadowMapResource();

  // DSV の作成
  CreateDepthStencilView();

  // SRV の作成
  CreateShaderResourceView();
}

void ShadowMap::Finalize()
{
  // SRV インデックスを解放
  if (srvManager_) {
    srvManager_->Free(srvIndex_);
    srvIndex_ = SrvManager::kInvalidIndex;
  }

  // DSV インデックスを解放
  DsvManager::GetInstance()->Free(dsvIndex_);
  dsvIndex_ = DsvManager::kInvalidIndex;

  // ポインタをクリア
  srvManager_ = nullptr;
}

void ShadowMap::BeginFrame()
{
  // 遅延リソース再作成の処理
  if (pendingRecreation_) {
    // 前フレームの描画が完了しているので、安全にリソースを再作成できる

    // 既存のリソースを解放
    srvManager_->Free(srvIndex_);
    srvIndex_ = SrvManager::kInvalidIndex;
    shadowMapResource_.Reset();

    // 新しいサイズを適用
    shadowMapSize_ = pendingShadowMapSize_;

    // リソースを再作成
    CreateShadowMapResource();
    CreateDepthStencilView();
    CreateShaderResourceView();

    // フラグをクリア
    pendingRecreation_ = false;
  }
}

void ShadowMap::BeginTile(uint32_t x, uint32_t y, uint32_t size)
{
  dx12_->GetContext().Transition(shadowMapResource_, D3D12_RESOURCE_STATE_DEPTH_WRITE);

  // カラーターゲットなしで深度だけ書く
  dx12_->GetContext().SetRenderTarget(nullptr, DXGI_FORMAT_UNKNOWN, &dsvHandle_, kShadowDepthFormat);

  ID3D12GraphicsCommandList* commandList = dx12_->GetCommandList();

  const D3D12_VIEWPORT viewport{ static_cast<float>(x), static_cast<float>(y), static_cast<float>(size), static_cast<float>(size), 0.0f, 1.0f };
  const D3D12_RECT     rect{ static_cast<LONG>(x), static_cast<LONG>(y), static_cast<LONG>(x + size), static_cast<LONG>(y + size) };

  // ほかの区画は前に描いた内容を残すので、この区画だけ消す
  commandList->ClearDepthStencilView(dsvHandle_, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 1, &rect);
  commandList->RSSetViewports(1, &viewport);
  commandList->RSSetScissorRects(1, &rect);
}

void ShadowMap::EndShadowMapRender()
{
  dx12_->GetContext().Transition(shadowMapResource_, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
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
  resourceDesc.Format = kShadowTypelessFormat;
  resourceDesc.SampleDesc.Count = 1;
  resourceDesc.SampleDesc.Quality = 0;
  resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
  resourceDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

  D3D12_HEAP_PROPERTIES heapProps = {};
  heapProps.Type = D3D12_HEAP_TYPE_DEFAULT;
  heapProps.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
  heapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;

  D3D12_CLEAR_VALUE clearValue = {};
  clearValue.Format = kShadowDepthFormat;
  clearValue.DepthStencil.Depth = 1.0f;
  clearValue.DepthStencil.Stencil = 0;

  HRESULT hr = dx12_->GetDevice()->CreateCommittedResource(
    &heapProps,
    D3D12_HEAP_FLAG_NONE,
    &resourceDesc,
    D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,  // 初期状態を PIXEL_SHADER_RESOURCE に設定
    &clearValue,
    IID_PPV_ARGS(&shadowMapResource_.resource));

  assert(SUCCEEDED(hr));

  shadowMapResource_->SetName(L"ShadowMapResource");
  shadowMapResource_.state = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
}

void ShadowMap::CreateDepthStencilView()
{
  // 初回のみ DSV 枠を確保し、再作成時は同じ枠へビューを作り直す
  if (dsvIndex_ == DsvManager::kInvalidIndex) {
    dsvIndex_ = DsvManager::GetInstance()->Allocate();
  }
  DsvManager::GetInstance()->CreateDSV(dsvIndex_, shadowMapResource_.Get(), kShadowDepthFormat);
  dsvHandle_ = DsvManager::GetInstance()->GetCpuHandle(dsvIndex_);
}

void ShadowMap::CreateShaderResourceView()
{
  // SRV インデックスを確保
  assert(srvManager_->CanAllocate());
  srvIndex_ = srvManager_->Allocate();

  // SRV の作成（深度を float として読み取る）
  srvManager_->CreateSRVForTexture2D(
    srvIndex_, shadowMapResource_.Get(), kShadowSrvFormat, 1);
}

void ShadowMap::SetShadowQuality(ShadowQuality quality)
{
  // 品質に応じた解像度と PCF カーネルサイズの設定
  uint32_t newSize = DEFAULT_SHADOW_MAP_SIZE;
  switch (quality) {
    case ShadowQuality::Low:    newSize = 512;  pcfKernelSize_ = 1; break;
    case ShadowQuality::Medium: newSize = 1024; pcfKernelSize_ = 3; break;
    case ShadowQuality::High:   newSize = 2048; pcfKernelSize_ = 5; break;
    case ShadowQuality::Ultra:  newSize = 4096; pcfKernelSize_ = 7; break;
    case ShadowQuality::Super:  newSize = 8192; pcfKernelSize_ = 7; break;
  }

  // サイズが変更される場合は次フレームで再作成（PCF カーネルサイズは即時反映）
  if (shadowMapSize_ != newSize && shadowMapResource_) {
    pendingShadowMapSize_ = newSize;
    pendingRecreation_ = true;
  } else {
    shadowMapSize_ = newSize;
  }
}

void ShadowMap::SetShadowMapSize(uint32_t size)
{
  // 2のべき乗にクランプ（256-8192）
  uint32_t clampedSize = 256;
  while (clampedSize < size && clampedSize < 8192) {
    clampedSize *= 2;
  }

  // サイズが変更される場合は次フレームで再作成
  if (shadowMapSize_ != clampedSize && shadowMapResource_) {
    pendingShadowMapSize_ = clampedSize;
    pendingRecreation_ = true;
  } else if (!shadowMapResource_) {
    // 初回（リソース未作成）の場合はすぐに設定
    shadowMapSize_ = clampedSize;
  }
}

void ShadowMap::SetPCFKernelSize(int kernelSize)
{
  // 奇数値のみ許可（1, 3, 5, 7）。古い設定の 9 は 7 として読む
  if (kernelSize >= 1 && kernelSize % 2 == 1) {
    pcfKernelSize_ = (std::min)(kernelSize, 7);
  }
}

} // namespace Tako

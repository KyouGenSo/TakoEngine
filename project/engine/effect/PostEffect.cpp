#include "PostEffect.h"
#include "DX12Basic.h"
#include "SrvManager.h"
#include "Logger.h"
#include "StringUtility.h"
#include "Object3dbasic.h"
#include "Camera.h"

PostEffect* PostEffect::instance_ = nullptr;

PostEffect* PostEffect::GetInstance()
{
  if (instance_ == nullptr)
  {
    instance_ = new PostEffect();
  }
  return instance_;
}

void PostEffect::Initialize(DX12Basic* dx12)
{
  m_dx12_ = dx12;

  currentEffectName_ = "NoEffect";

  CreateRenderTexture();

  CreateDepthBufferSRV();

  // ダウンサンプル用テクスチャの作成
  CreateBloomTextures();

  // マルチパスブルーム用のシェーダー作成
  CreatePSO("ThresholdExtract"); // 明るい部分抽出用
  CreatePSO("GaussianBlur");     // ブラー用
  CreatePSO("BloomCombine");     // 最終合成用

  CreatePSO("NoEffect");

  CreatePSO("VignetteRed");

  CreatePSO("VignetteRedBloom");

  CreatePSO("GrayScale");

  CreatePSO("VigRedGrayScale");

  CreatePSO("Bloom");

  CreatePSO("BloomFog");

  CreatePSO("RadialBlur");

  CreateVignetteParam();

  CreateVignetteRedBloomParam();

  CreateBloomParam();

  CreateNewBloomParam();

  CreateFogParam();

  CreateCameraForGPU();

  CreateRadialBlurParam();
}

void PostEffect::Finalize()
{
  if (instance_ != nullptr)
  {
    delete instance_;
    instance_ = nullptr;
  }
}

void PostEffect::BeginDrawEffectTarget()
{
  D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = m_dx12_->GetDSVHeapHandleStart();

  // 描画先のRTVを設定
  m_dx12_->GetCommandList()->OMSetRenderTargets(1, &originRenderTexRTVHandle_, false, &dsvHandle);

  float clearColor[] = { kOriginRenderTexClearColor_.x, kOriginRenderTexClearColor_.y, kOriginRenderTexClearColor_.z, kOriginRenderTexClearColor_.w };

  // 画面の色をクリア
  m_dx12_->GetCommandList()->ClearRenderTargetView(originRenderTexRTVHandle_, clearColor, 0, nullptr);
}

void PostEffect::BegineDrawNonEffectTarget()
{
  D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = m_dx12_->GetDSVHeapHandleStart();

  // 描画先のRTVを設定
  m_dx12_->GetCommandList()->OMSetRenderTargets(1, &resultRenderTexRTVHandle_, false, &dsvHandle);
}

void PostEffect::Draw()
{
  // エフェクトの描画
  DrawPostEffect(currentEffectName_);
}

void PostEffect::DrawPostEffect(const std::string& effectName)
{
  if (effectName == "NewBloom") {
    DrawMultiPassBloom();
    return;
  }

  // レンダーテクスチャAの状態をシェーダーリソースに変更
  SetBarrier(D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, originRenderTexResource_.Get());

  if (effectName == "BloomFog") {
    m_dx12_->TransitionResourceState(D3D12_RESOURCE_STATE_DEPTH_WRITE, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, m_dx12_->GetDepthStencilResource());
  }

  // レンダーテクスチャBを描画先に設定
  D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = m_dx12_->GetDSVHeapHandleStart();
  m_dx12_->GetCommandList()->OMSetRenderTargets(1, &resultRenderTexRTVHandle_, false, &dsvHandle);

  // レンダーテクスチャBをクリア
  float clearColor[] = { resultRenderTexClearColor_.x, resultRenderTexClearColor_.y, resultRenderTexClearColor_.z, resultRenderTexClearColor_.w };
  m_dx12_->GetCommandList()->ClearRenderTargetView(resultRenderTexRTVHandle_, clearColor, 0, nullptr);

  // ビューポート設定
  m_dx12_->SetViewPort();

  // エフェクト適用シェーダーの設定
  m_dx12_->GetCommandList()->SetGraphicsRootSignature(rootSignatures_[effectName].Get());
  m_dx12_->GetCommandList()->SetPipelineState(pipelineStates_[effectName].Get());

  // パラメータリソースの設定
  SetParamResource(effectName);

  // レンダーテクスチャAをシェーダーリソースとして設定
  SrvManager::GetInstance()->SetGraphicsRootDescriptorTable(0, originRtvSrvIndex_);
  if (effectName == "BloomFog") {
    SrvManager::GetInstance()->SetGraphicsRootDescriptorTable(4, dsvSrvIndex_);
  }

  // フルスクリーン三角形描画
  m_dx12_->GetCommandList()->DrawInstanced(3, 1, 0, 0);

  if (effectName == "BloomFog") {
    m_dx12_->TransitionResourceState(D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_DEPTH_WRITE, m_dx12_->GetDepthStencilResource());
  }

  // レンダーテクスチャAの状態を元に戻す
  SetBarrier(D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET, originRenderTexResource_.Get());
}

void PostEffect::DrawMultiPassBloom()
{
  newBloomParam_->texelSize = {
    1.0f / static_cast<float>(downSampleWidth_),
    1.0f / static_cast<float>(downSampleHeight_)
  };

  newBloomParam_->iteration = bloomIteration_;

  D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = m_dx12_->GetDSVHeapHandleStart();

  /// ------------------------------------------///
  /// 1. 明るい部分の抽出（閾値以上の部分を取り出す） ///
  /// ------------------------------------------///
  // レンダーテクスチャAをシェーダリソースに変更
  SetBarrier(D3D12_RESOURCE_STATE_RENDER_TARGET,
    D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
    originRenderTexResource_.Get());

  // ダウンサンプルテクスチャを描画先に設定
  m_dx12_->GetCommandList()->OMSetRenderTargets(1, &highLumRTVHandle_, false, &dsvHandle);

  // テクスチャクリア
  float clearColor[] = { kOriginRenderTexClearColor_.x, kOriginRenderTexClearColor_.y, kOriginRenderTexClearColor_.z, kOriginRenderTexClearColor_.w };
  m_dx12_->GetCommandList()->ClearRenderTargetView(highLumRTVHandle_, clearColor, 0, nullptr);

  // ビューポート設定
  m_dx12_->SetViewPort();

  // 明るい部分を抽出するシェーダー設定
  m_dx12_->GetCommandList()->SetGraphicsRootSignature(rootSignatures_["ThresholdExtract"].Get());
  m_dx12_->GetCommandList()->SetPipelineState(pipelineStates_["ThresholdExtract"].Get());

  // BloomParamをセット
  m_dx12_->GetCommandList()->SetGraphicsRootConstantBufferView(
    1, newBloomParamResource_->GetGPUVirtualAddress());

  // 元画像をシェーダーリソースとして設定
  SrvManager::GetInstance()->SetGraphicsRootDescriptorTable(0, originRtvSrvIndex_);

  // 描画
  m_dx12_->GetCommandList()->DrawInstanced(3, 1, 0, 0);

  /// ------------------------------------------///
  /// 2. 高輝度テクスチャを数回ダウンサンプルして描画-///
  /// ------------------------------------------///
  SetBarrier(D3D12_RESOURCE_STATE_RENDER_TARGET,
    D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
    highLumResource_.Get());

  // 横方向ブラーテクスチャを描画先に設定
  m_dx12_->GetCommandList()->OMSetRenderTargets(1, &highLumShrinkRTVHandle_, false, &dsvHandle);

  // テクスチャクリア
  m_dx12_->GetCommandList()->ClearRenderTargetView(highLumShrinkRTVHandle_, clearColor, 0, nullptr);

  // ガウスブラーシェーダー設定
  m_dx12_->GetCommandList()->SetGraphicsRootSignature(rootSignatures_["NoEffect"].Get());
  m_dx12_->GetCommandList()->SetPipelineState(pipelineStates_["NoEffect"].Get());

  // ダウンサンプルテクスチャをシェーダーリソースとして設定
  SrvManager::GetInstance()->SetGraphicsRootDescriptorTable(0, highLumSrvIndex_);

  // 描画
  auto desc = highLumResource_->GetDesc();

  D3D12_VIEWPORT vp = {};
  vp.MaxDepth = 1.0f;
  vp.MinDepth = 0.0f;
  vp.Height = static_cast<FLOAT>(desc.Height) / 2;
  vp.Width = static_cast<FLOAT>(desc.Width) / 2;

  D3D12_RECT sr = {};
  sr.top = 0;
  sr.left = 0;
  sr.right = static_cast<LONG>(vp.Width);
  sr.bottom = static_cast<LONG>(vp.Height);

  for (int i = 0; i < bloomIteration_; ++i)
  {
    m_dx12_->GetCommandList()->RSSetViewports(1, &vp);
    m_dx12_->GetCommandList()->RSSetScissorRects(1, &sr);
    m_dx12_->GetCommandList()->DrawInstanced(3, 1, 0, 0);

    sr.top += static_cast<LONG>(vp.Height);
    vp.TopLeftX = 0;
    vp.TopLeftY = static_cast<float>(sr.top);

    vp.Width /= 2;
    vp.Height /= 2;
    sr.bottom = sr.top + static_cast<LONG>(vp.Height);
  }
  /// ------------------------------------------------///
  /// 3. ダウンサンプルした高輝度テクスチャをブルーム処理する///
  /// ------------------------------------------------///
   // アップサンプルテクスチャをシェーダーリソースに変更
  SetBarrier(D3D12_RESOURCE_STATE_RENDER_TARGET,
    D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
    highLumShrinkResource_.Get());

  // レンダーテクスチャBを描画先に設定
  m_dx12_->GetCommandList()->OMSetRenderTargets(1, &bloomResultRTVHandle_, false, &dsvHandle);

  // テクスチャクリア
  m_dx12_->GetCommandList()->ClearRenderTargetView(bloomResultRTVHandle_, clearColor, 0, nullptr);

  // ビューポート設定
  m_dx12_->SetViewPort();

  // 合成シェーダー設定
  m_dx12_->GetCommandList()->SetGraphicsRootSignature(rootSignatures_["GaussianBlur"].Get());
  m_dx12_->GetCommandList()->SetPipelineState(pipelineStates_["GaussianBlur"].Get());

  // BloomParamをセット
  m_dx12_->GetCommandList()->SetGraphicsRootConstantBufferView(
    1, newBloomParamResource_->GetGPUVirtualAddress());

  // 元画像をシェーダーリソースとして設定（スロット0）
  SrvManager::GetInstance()->SetGraphicsRootDescriptorTable(0, highLumSrvIndex_);

  // ブラー画像をシェーダーリソースとして設定（スロット2）
  SrvManager::GetInstance()->SetGraphicsRootDescriptorTable(2, highLumShrinkSrvIndex_);

  // 描画
  m_dx12_->GetCommandList()->DrawInstanced(3, 1, 0, 0);



  /// -----------------------------------///
  /// 4. ブルーム処理したテクスチャを合成する ///
  /// -----------------------------------///
  //　ブルーム処理したテクスチャをシェーダーリソースに変更
  SetBarrier(D3D12_RESOURCE_STATE_RENDER_TARGET,
    D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
    bloomResultResource_.Get());

  // 最終結果レンダーテクスチャを描画先に設定
  m_dx12_->GetCommandList()->OMSetRenderTargets(1, &resultRenderTexRTVHandle_, false, &dsvHandle);

  // テクスチャクリア
  clearColor[0] = resultRenderTexClearColor_.x;
  clearColor[1] = resultRenderTexClearColor_.y;
  clearColor[2] = resultRenderTexClearColor_.z;
  clearColor[3] = resultRenderTexClearColor_.w;
  m_dx12_->GetCommandList()->ClearRenderTargetView(resultRenderTexRTVHandle_, clearColor, 0, nullptr);

  // ビューポート設定
  m_dx12_->SetViewPort();

  // 合成シェーダー設定
  m_dx12_->GetCommandList()->SetGraphicsRootSignature(rootSignatures_["BloomCombine"].Get());
  m_dx12_->GetCommandList()->SetPipelineState(pipelineStates_["BloomCombine"].Get());

  // BloomParamをセット
  m_dx12_->GetCommandList()->SetGraphicsRootConstantBufferView(
    1, newBloomParamResource_->GetGPUVirtualAddress());

  // 元画像をシェーダーリソースとして設定（スロット0）
  SrvManager::GetInstance()->SetGraphicsRootDescriptorTable(0, originRtvSrvIndex_);

  // ブラー画像をシェーダーリソースとして設定（スロット2）
  SrvManager::GetInstance()->SetGraphicsRootDescriptorTable(2, bloomResultSrvIndex_);

  // 描画
  m_dx12_->GetCommandList()->DrawInstanced(3, 1, 0, 0);

  // 各テクスチャの状態を元に戻す
  SetBarrier(D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
    D3D12_RESOURCE_STATE_RENDER_TARGET,
    highLumResource_.Get());

  SetBarrier(D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
    D3D12_RESOURCE_STATE_RENDER_TARGET,
    highLumShrinkResource_.Get());

  SetBarrier(D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
    D3D12_RESOURCE_STATE_RENDER_TARGET,
    bloomResultResource_.Get());

  SetBarrier(D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
    D3D12_RESOURCE_STATE_RENDER_TARGET,
    originRenderTexResource_.Get());
}

void PostEffect::DrawFinalResult()
{
  // レンダーテクスチャBの状態をシェーダーリソースに変更
  SetBarrier(D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, resultRenderTexResource_.Get());

  // スワップチェーンを描画先に設定
  m_dx12_->SetSwapChain();

  // NoEffectシェーダーを使ってレンダーテクスチャBをそのまま表示
  m_dx12_->GetCommandList()->SetGraphicsRootSignature(rootSignatures_["NoEffect"].Get());
  m_dx12_->GetCommandList()->SetPipelineState(pipelineStates_["NoEffect"].Get());

  // レンダーテクスチャBをシェーダーリソースとして設定
  SrvManager::GetInstance()->SetGraphicsRootDescriptorTable(0, resultRtvSrvIndex_);

  // フルスクリーン三角形描画
  m_dx12_->GetCommandList()->DrawInstanced(3, 1, 0, 0);

  // レンダーテクスチャBの状態を元に戻す
  SetBarrier(D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET, resultRenderTexResource_.Get());
}

void PostEffect::RecreateRenderTexture(uint32_t width, uint32_t height)
{
  // 既存のリソースを解放
  originRenderTexResource_.Reset();
  resultRenderTexResource_.Reset();

  // 新しいレンダーテクスチャを作成
  m_dx12_->CreateRenderTextureResource(originRenderTexResource_, width, height, DXGI_FORMAT_R8G8B8A8_UNORM, kOriginRenderTexClearColor_);
  originRenderTexResource_->SetName(L"PostEffectRenderTexture");

  m_dx12_->CreateRenderTextureResource(resultRenderTexResource_, width, height, DXGI_FORMAT_R8G8B8A8_UNORM, resultRenderTexClearColor_);
  resultRenderTexResource_->SetName(L"PostEffectRenderTextureB");

  // RTVの設定
  D3D12_RENDER_TARGET_VIEW_DESC rtvDesc{};
  rtvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
  rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;

  // RTVの作成
  m_dx12_->GetDevice()->CreateRenderTargetView(originRenderTexResource_.Get(), &rtvDesc, originRenderTexRTVHandle_);
  m_dx12_->GetDevice()->CreateRenderTargetView(resultRenderTexResource_.Get(), &rtvDesc, resultRenderTexRTVHandle_);

  // レンダーテクスチャのSRVを更新
  SrvManager::GetInstance()->CreateSRVForTexture2D(originRtvSrvIndex_, originRenderTexResource_.Get(), DXGI_FORMAT_R8G8B8A8_UNORM, 1);
  SrvManager::GetInstance()->CreateSRVForTexture2D(resultRtvSrvIndex_, resultRenderTexResource_.Get(), DXGI_FORMAT_R8G8B8A8_UNORM, 1);

  // 深度バッファのSRVを更新
  SrvManager::GetInstance()->CreateSRVForTexture2D(dsvSrvIndex_, m_dx12_->GetDepthStencilResource(), DXGI_FORMAT_R32_FLOAT, 1);
}

void PostEffect::SetBarrier(D3D12_RESOURCE_STATES stateBefore, D3D12_RESOURCE_STATES stateAfter)
{
  D3D12_RESOURCE_BARRIER barrier{};
  barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
  barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
  barrier.Transition.pResource = originRenderTexResource_.Get();

  barrier.Transition.StateBefore = stateBefore;
  barrier.Transition.StateAfter = stateAfter;

  m_dx12_->GetCommandList()->ResourceBarrier(1, &barrier);
}

void PostEffect::SetBarrier(D3D12_RESOURCE_STATES stateBefore, D3D12_RESOURCE_STATES stateAfter, ID3D12Resource* resource)
{
  D3D12_RESOURCE_BARRIER barrier{};
  barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
  barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
  barrier.Transition.pResource = resource;
  barrier.Transition.StateBefore = stateBefore;
  barrier.Transition.StateAfter = stateAfter;

  m_dx12_->GetCommandList()->ResourceBarrier(1, &barrier);
}

void PostEffect::SetVignettePower(float power)
{
  vignetteParam_->power = power;
  vignetteRedBloomParam_->power = power;
}

void PostEffect::SetVignetteRange(float range)
{
  vignetteParam_->range = range;
  vignetteRedBloomParam_->range = range;

}

void PostEffect::SetBloomThreshold(float threshold)
{
  vignetteRedBloomParam_->threshold = threshold;
  bloomParam_->threshold = threshold;
  newBloomParam_->threshold = threshold;
}

void PostEffect::SetBloomIntensity(float intensity)
{
  bloomParam_->intensity = intensity;
  newBloomParam_->intensity = intensity;
}

void PostEffect::SetBloomSigma(float sigma)
{
  bloomParam_->sigma = sigma;
  newBloomParam_->sigma = sigma;
}

void PostEffect::SetBloomSampleCount(int32_t count)
{
  newBloomParam_->sampleCount = count;
}

void PostEffect::SetDownSampleFactor(int factor)
{
  if (factor < 1) factor = 1;
  if (factor > 8) factor = 8; // 最大1/8まで

  // 前回と異なる場合のみテクスチャを再作成
  if (downSampleFactor_ != factor) {
    downSampleFactor_ = factor;
    CreateBloomTextures();
  }
}

void PostEffect::SetFogColor(const Vector4& color)
{
  fogParam_->color = color;
}

void PostEffect::SetFogDensity(float density)
{
  fogParam_->density = density;
}

void PostEffect::SetRadialBlurCenter(const Vector2& center)
{
  radialBlurParam_->center = center;
}

void PostEffect::SetRadialBlurWidth(float width)
{
  radialBlurParam_->blurWidth = width;
}

void PostEffect::CreateRenderTexture()
{
  // レンダーテクスチャリソースの生成
  m_dx12_->CreateRenderTextureResource(originRenderTexResource_, WinApp::clientWidth, WinApp::clientHeight, DXGI_FORMAT_R8G8B8A8_UNORM, kOriginRenderTexClearColor_);
  originRenderTexResource_->SetName(L"PostEffectRenderTexture");

  m_dx12_->CreateRenderTextureResource(resultRenderTexResource_, WinApp::clientWidth, WinApp::clientHeight,
    DXGI_FORMAT_R8G8B8A8_UNORM, resultRenderTexClearColor_);
  resultRenderTexResource_->SetName(L"PostEffectRenderTextureB");

  // RTVの設定
  D3D12_RENDER_TARGET_VIEW_DESC rtvDesc{};
  rtvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM; // フォーマット
  rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D; // 2Dテクスチャとして書き込む

  // RTVHandleのを取得
  originRenderTexRTVHandle_ = m_dx12_->GetRenderTextureRTVHandle(2);
  resultRenderTexRTVHandle_ = m_dx12_->GetRenderTextureRTVHandle(3);

  // RTVの生成
  m_dx12_->GetDevice()->CreateRenderTargetView(originRenderTexResource_.Get(), &rtvDesc, originRenderTexRTVHandle_);
  m_dx12_->GetDevice()->CreateRenderTargetView(resultRenderTexResource_.Get(), &rtvDesc, resultRenderTexRTVHandle_);

  // SRVのインデックスを取得
  originRtvSrvIndex_ = SrvManager::GetInstance()->Allocate();
  resultRtvSrvIndex_ = SrvManager::GetInstance()->Allocate();

  // SRVの生成
  SrvManager::GetInstance()->CreateSRVForTexture2D(originRtvSrvIndex_, originRenderTexResource_.Get(), DXGI_FORMAT_R8G8B8A8_UNORM, 1);
  SrvManager::GetInstance()->CreateSRVForTexture2D(resultRtvSrvIndex_, resultRenderTexResource_.Get(), DXGI_FORMAT_R8G8B8A8_UNORM, 1);
}

void PostEffect::CreateBloomTextures()
{
  downSampleWidth_ = WinApp::clientWidth / downSampleFactor_;
  downSampleHeight_ = WinApp::clientHeight / downSampleFactor_;

  // 高輝度部分抽出用テクスチャの生成
  m_dx12_->CreateRenderTextureResource(highLumResource_,
    WinApp::clientWidth, WinApp::clientHeight,
    DXGI_FORMAT_R8G8B8A8_UNORM, kOriginRenderTexClearColor_);
  highLumResource_->SetName(L"HighLumExtractTexture");

  // 高輝度部分をダウンサンプル用テクスチャの生成
  m_dx12_->CreateRenderTextureResource(highLumShrinkResource_,
    WinApp::clientWidth / 2, WinApp::clientHeight,
    DXGI_FORMAT_R8G8B8A8_UNORM, kOriginRenderTexClearColor_);
  highLumShrinkResource_->SetName(L"HighLumShrinkTexture");

  // Bloom結果用テクスチャの生成
  m_dx12_->CreateRenderTextureResource(bloomResultResource_,
    WinApp::clientWidth, WinApp::clientHeight,
    DXGI_FORMAT_R8G8B8A8_UNORM, kOriginRenderTexClearColor_);

  // RTVの設定
  D3D12_RENDER_TARGET_VIEW_DESC rtvDesc{};
  rtvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
  rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;

  // RTVハンドル取得
  highLumRTVHandle_ = m_dx12_->GetRenderTextureRTVHandle(4);
  highLumShrinkRTVHandle_ = m_dx12_->GetRenderTextureRTVHandle(5);
  bloomResultRTVHandle_ = m_dx12_->GetRenderTextureRTVHandle(6);

  // RTV作成
  m_dx12_->GetDevice()->CreateRenderTargetView(
    highLumResource_.Get(), &rtvDesc, highLumRTVHandle_);
  m_dx12_->GetDevice()->CreateRenderTargetView(
    highLumShrinkResource_.Get(), &rtvDesc, highLumShrinkRTVHandle_);
  m_dx12_->GetDevice()->CreateRenderTargetView(
    bloomResultResource_.Get(), &rtvDesc, bloomResultRTVHandle_);

  // SRV用インデックス取得
  highLumSrvIndex_ = SrvManager::GetInstance()->Allocate();
  highLumShrinkSrvIndex_ = SrvManager::GetInstance()->Allocate();
  bloomResultSrvIndex_ = SrvManager::GetInstance()->Allocate();

  // SRV作成
  SrvManager::GetInstance()->CreateSRVForTexture2D(
    highLumSrvIndex_, highLumResource_.Get(),
    DXGI_FORMAT_R8G8B8A8_UNORM, 1);
  SrvManager::GetInstance()->CreateSRVForTexture2D(
    highLumShrinkSrvIndex_, highLumShrinkResource_.Get(),
    DXGI_FORMAT_R8G8B8A8_UNORM, 1);
  SrvManager::GetInstance()->CreateSRVForTexture2D(
    bloomResultSrvIndex_, bloomResultResource_.Get(),
    DXGI_FORMAT_R8G8B8A8_UNORM, 1);
}

void PostEffect::CreateDepthBufferSRV()
{
  // SRVの生成
  dsvSrvIndex_ = SrvManager::GetInstance()->Allocate();

  SrvManager::GetInstance()->CreateSRVForTexture2D(dsvSrvIndex_, m_dx12_->GetDepthStencilResource(), DXGI_FORMAT_R32_FLOAT, 1);
}

void PostEffect::CreateRootSignature(const std::string& effectName)
{
  HRESULT hr;

  // rootSignatureの生成
  D3D12_ROOT_SIGNATURE_DESC descriptionRootSignature;
  descriptionRootSignature.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

  // Samplerの設定
  D3D12_STATIC_SAMPLER_DESC samplerDesc[1]{};
  samplerDesc[0].Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR; // テクスチャの補間方法
  samplerDesc[0].AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP; // テクスチャの繰り返し方法
  samplerDesc[0].AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP; // テクスチャの繰り返し方法
  samplerDesc[0].AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP; // テクスチャの繰り返し方法
  samplerDesc[0].ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER; // 比較しない
  samplerDesc[0].MaxLOD = D3D12_FLOAT32_MAX; // ミップマップの最大LOD
  samplerDesc[0].ShaderRegister = 0; // レジスタ番号
  samplerDesc[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL; // ピクセルシェーダーで使う
  descriptionRootSignature.pStaticSamplers = samplerDesc;
  descriptionRootSignature.NumStaticSamplers = _countof(samplerDesc);

  // DescriptorRangeの設定。
  D3D12_DESCRIPTOR_RANGE descriptorRanges[1] = {};
  descriptorRanges[0].BaseShaderRegister = 0; // レジスタ番号
  descriptorRanges[0].NumDescriptors = 1; // ディスクリプタ数
  descriptorRanges[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV; // SRVを使う
  descriptorRanges[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND; // Offsetを自動計算

  D3D12_DESCRIPTOR_RANGE descriptorRange2[1] = {};
  descriptorRange2[0].BaseShaderRegister = 1; // レジスタ番号
  descriptorRange2[0].NumDescriptors = 1; // ディスクリプタ数
  descriptorRange2[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV; // SRVを使う
  descriptorRange2[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND; // Offsetを自動計算

  // RootParameterの設定。複数設定できるので配列
  D3D12_ROOT_PARAMETER rootParameters[5] = {};
  // Texture
  rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE; // ディスクリプタテーブルを使う
  rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL; // ピクセルシェーダーで使う
  rootParameters[0].DescriptorTable.pDescriptorRanges = descriptorRanges; // ディスクリプタレンジを設定
  rootParameters[0].DescriptorTable.NumDescriptorRanges = _countof(descriptorRanges); // レンジの数

  // Param
  rootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV; // 定数バッファビューを使う
  rootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL; // ピクセルシェーダーで使う
  rootParameters[1].Descriptor.ShaderRegister = 0; // レジスタ番号とバインド

  // Param
  if (effectName == "BloomCombine" || effectName == "GaussianBlur") {
    D3D12_DESCRIPTOR_RANGE bloomTexRanges[1] = {};
    bloomTexRanges[0].BaseShaderRegister = 1; // レジスタ番号
    bloomTexRanges[0].NumDescriptors = 1; // ディスクリプタ数
    bloomTexRanges[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV; // SRVを使う
    bloomTexRanges[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND; // Offsetを自動計算

    // BloomTex
    rootParameters[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE; // ディスクリプタテーブルを使う
    rootParameters[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL; // ピクセルシェーダーで使う
    rootParameters[2].DescriptorTable.pDescriptorRanges = bloomTexRanges; // ディスクリプタレンジを設定
    rootParameters[2].DescriptorTable.NumDescriptorRanges = _countof(bloomTexRanges); // レンジの数
  } else
  {
    rootParameters[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV; // 定数バッファビューを使う
    rootParameters[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL; // ピクセルシェーダーで使う
    rootParameters[2].Descriptor.ShaderRegister = 1; // レジスタ番号とバインド
  }

  // Param
  rootParameters[3].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV; // 定数バッファビューを使う
  rootParameters[3].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL; // ピクセルシェーダーで使う
  rootParameters[3].Descriptor.ShaderRegister = 2; // レジスタ番号とバインド

  // 深度バッファテクスチャ
  if (effectName == "BloomFog") {
    rootParameters[4].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE; // ディスクリプタテーブルを使う
    rootParameters[4].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL; // ピクセルシェーダーで使う
    rootParameters[4].DescriptorTable.pDescriptorRanges = descriptorRange2; // ディスクリプタレンジを設定
    rootParameters[4].DescriptorTable.NumDescriptorRanges = _countof(descriptorRange2); // レンジの数
  } else {
    rootParameters[4].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV; // 定数バッファビューを使う
    rootParameters[4].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL; // ピクセルシェーダーで使う
    rootParameters[4].Descriptor.ShaderRegister = 3; // レジスタ番号とバインド
  }
  descriptionRootSignature.pParameters = rootParameters;
  descriptionRootSignature.NumParameters = _countof(rootParameters);

  Microsoft::WRL::ComPtr<ID3DBlob> signatureBlob = nullptr;
  Microsoft::WRL::ComPtr<ID3DBlob> errorBlob = nullptr;

  hr = D3D12SerializeRootSignature(&descriptionRootSignature, D3D_ROOT_SIGNATURE_VERSION_1, &signatureBlob, &errorBlob);
  if (FAILED(hr))
  {
    Logger::Log(reinterpret_cast<char*>(errorBlob->GetBufferPointer()));
    assert(false);
  }

  hr = m_dx12_->GetDevice()->CreateRootSignature(0, signatureBlob->GetBufferPointer(), signatureBlob->GetBufferSize(), IID_PPV_ARGS(rootSignatures_[effectName].GetAddressOf()));
  signatureBlob->GetBufferSize(), IID_PPV_ARGS(rootSignatures_[effectName].GetAddressOf());
  assert(SUCCEEDED(hr));
}

void PostEffect::CreatePSO(const std::string& effectName)
{
  HRESULT hr;

  // RootSignatureの生成
  CreateRootSignature(effectName);

  // InputLayout
  D3D12_INPUT_LAYOUT_DESC inputLayoutDesc{};
  inputLayoutDesc.pInputElementDescs = nullptr;
  inputLayoutDesc.NumElements = 0;

  // BlendState
  D3D12_BLEND_DESC blendDesc{};
  blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

  // RasterizerState
  D3D12_RASTERIZER_DESC rasterizerDesc{};
  // 三角形の中を塗りつぶす
  rasterizerDesc.FillMode = D3D12_FILL_MODE_SOLID;
  // 裏面を表示しない
  rasterizerDesc.CullMode = D3D12_CULL_MODE_NONE;

  // shaderのコンパイル
  std::wstring psPath = L"resources/shaders/" + StringUtility::ConvertString(effectName) + L".PS.hlsl";

  Microsoft::WRL::ComPtr<IDxcBlob> vertexShaderBlob = m_dx12_->CompileShader(L"resources/shaders/FullScreen.VS.hlsl", L"vs_6_0");
  assert(vertexShaderBlob != nullptr);

  Microsoft::WRL::ComPtr<IDxcBlob> pixelShaderBlob = m_dx12_->CompileShader(psPath, L"ps_6_0");
  assert(pixelShaderBlob != nullptr);

  // DepthStencilState
  D3D12_DEPTH_STENCIL_DESC depthStencilDesc{};
  // depthの機能を無効にする
  depthStencilDesc.DepthEnable = false;

  // PSOの生成
  D3D12_GRAPHICS_PIPELINE_STATE_DESC graphicsPipelineStateDesc{};
  graphicsPipelineStateDesc.pRootSignature = rootSignatures_[effectName].Get();
  graphicsPipelineStateDesc.InputLayout = inputLayoutDesc;
  graphicsPipelineStateDesc.VS = { vertexShaderBlob->GetBufferPointer(), vertexShaderBlob->GetBufferSize() };
  graphicsPipelineStateDesc.PS = { pixelShaderBlob->GetBufferPointer(), pixelShaderBlob->GetBufferSize() };
  graphicsPipelineStateDesc.BlendState = blendDesc;
  graphicsPipelineStateDesc.RasterizerState = rasterizerDesc;
  // 書き込むRTVの情報
  graphicsPipelineStateDesc.NumRenderTargets = 1;
  graphicsPipelineStateDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
  // 利用するトポロジ（形状）のタイプ。三角形
  graphicsPipelineStateDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
  // どのように画面に色を打ち込むかの設定
  graphicsPipelineStateDesc.SampleDesc.Count = 1;
  graphicsPipelineStateDesc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;
  // DepthStencilの設定
  graphicsPipelineStateDesc.DepthStencilState = depthStencilDesc;
  graphicsPipelineStateDesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;

  // 実際に生成
  hr = m_dx12_->GetDevice()->CreateGraphicsPipelineState(&graphicsPipelineStateDesc, IID_PPV_ARGS(&pipelineStates_[effectName]));
  assert(SUCCEEDED(hr));
}

void PostEffect::CreateVignetteParam()
{
  // VignetteParamのリソース生成
  vignetteParamResource_ = m_dx12_->MakeBufferResource(sizeof(VignetteParam));

  // データの設定
  vignetteParamResource_->Map(0, nullptr, reinterpret_cast<void**>(&vignetteParam_));

  // データの初期化
  vignetteParam_->power = 0.0f;
  vignetteParam_->range = 20.0f;
}

void PostEffect::CreateVignetteRedBloomParam()
{
  // VignetteRedBloomParamのリソース生成
  vignetteRedBloomParamResource_ = m_dx12_->MakeBufferResource(sizeof(VignetteRedBloomParam));

  // データの設定
  vignetteRedBloomParamResource_->Map(0, nullptr, reinterpret_cast<void**>(&vignetteRedBloomParam_));

  // データの初期化
  vignetteRedBloomParam_->power = 0.0f;
  vignetteRedBloomParam_->threshold = 1.0f;
  vignetteRedBloomParam_->range = 20.0f;
}

void PostEffect::CreateBloomParam()
{
  // BloomParamのリソース生成
  bloomParamResource_ = m_dx12_->MakeBufferResource(sizeof(BloomParam));

  // データの設定
  bloomParamResource_->Map(0, nullptr, reinterpret_cast<void**>(&bloomParam_));

  // データの初期化
  bloomParam_->intensity = 1.0f;
  bloomParam_->threshold = 0.9f;
  bloomParam_->sigma = 2.0f;
}

void PostEffect::CreateNewBloomParam()
{
  // NewBloomParamのリソース生成
  newBloomParamResource_ = m_dx12_->MakeBufferResource(sizeof(NewBloomParam));
  // データの設定
  newBloomParamResource_->Map(0, nullptr, reinterpret_cast<void**>(&newBloomParam_));
  // データの初期化
  newBloomParam_->intensity = 1.0f;
  newBloomParam_->threshold = 0.9f;
  newBloomParam_->sigma = 2.0f;
  newBloomParam_->direction = { 1.0f, 0.0f };
  newBloomParam_->texelSize = { 1.0f / downSampleWidth_, 1.0f / downSampleHeight_ };
  newBloomParam_->sampleCount = 10;
}

void PostEffect::CreateFogParam()
{
  // FogParamのリソース生成
  fogParamResource_ = m_dx12_->MakeBufferResource(sizeof(FogParam));

  // データの設定
  fogParamResource_->Map(0, nullptr, reinterpret_cast<void**>(&fogParam_));

  // データの初期化
  fogParam_->color = { 1.0f, 1.0f, 1.0f, 1.0f };
  fogParam_->density = 0.05f;
}

void PostEffect::CreateRadialBlurParam()
{
  // RadialBlurParamのリソース生成
  radialBlurParamResource_ = m_dx12_->MakeBufferResource(sizeof(RadialBlurParam));
  // データの設定
  radialBlurParamResource_->Map(0, nullptr, reinterpret_cast<void**>(&radialBlurParam_));
  // データの初期化
  radialBlurParam_->center = { .x = 0.5f, .y = 0.5f };
  radialBlurParam_->blurWidth = 0.01f;
  radialBlurParam_->sampleCount = 10;
}

void PostEffect::CreateCameraForGPU()
{
  // CameraForGPUのリソース生成
  cameraForGPUResource_ = m_dx12_->MakeBufferResource(sizeof(CameraForGPU));

  // データの設定
  cameraForGPUResource_->Map(0, nullptr, reinterpret_cast<void**>(&cameraForGPU_));

  // データの初期化
  cameraForGPU_->farPlane = (*Object3dBasic::GetInstance()->GetCamera())->GetFarClip();
  cameraForGPU_->nearPlane = (*Object3dBasic::GetInstance()->GetCamera())->GetNearClip();
}

void PostEffect::SetParamResource(const std::string& effectName)
{
  if (effectName == "VignetteRed" || effectName == "VigRedGrayScale")
  {
    m_dx12_->GetCommandList()->SetGraphicsRootConstantBufferView(1, vignetteParamResource_->GetGPUVirtualAddress());
  } else if (effectName == "VignetteRedBloom")
  {
    m_dx12_->GetCommandList()->SetGraphicsRootConstantBufferView(1, vignetteRedBloomParamResource_->GetGPUVirtualAddress());
  } else if (effectName == "Bloom")
  {
    m_dx12_->GetCommandList()->SetGraphicsRootConstantBufferView(1, bloomParamResource_->GetGPUVirtualAddress());
  } else if (effectName == "BloomFog")
  {
    m_dx12_->GetCommandList()->SetGraphicsRootConstantBufferView(1, bloomParamResource_->GetGPUVirtualAddress());
    m_dx12_->GetCommandList()->SetGraphicsRootConstantBufferView(2, cameraForGPUResource_->GetGPUVirtualAddress());
    m_dx12_->GetCommandList()->SetGraphicsRootConstantBufferView(3, fogParamResource_->GetGPUVirtualAddress());
  } else if (effectName == "RadialBlur")
  {
    m_dx12_->GetCommandList()->SetGraphicsRootConstantBufferView(1, radialBlurParamResource_->GetGPUVirtualAddress());
  } else if (effectName == "GrayScale" || effectName == "NoEffect")
  {
    // グレースケール, ノーエフェクトの場合は何もしない
  }
}

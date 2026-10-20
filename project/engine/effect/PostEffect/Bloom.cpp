#include "Bloom.h"

#include "DX12Basic.h"
#include "WinApp.h"
#include "SrvManager.h"
#include "StringUtility.h"
#include "EnginePaths.h"

#ifdef _DEBUG
#include "DebugUIManager.h"
#include "ImGuiManager.h"
#endif

namespace Tako {

  Bloom::~Bloom()
  {
    if (winApp_ && onResizeId_ != 0) {
      winApp_->UnregisterOnResizeFunc(onResizeId_);
    }

    // RT の RTV/SRV を返却
    highLumRT_.Release();
    blurRT_.Release();
    resultRT_.Release();
  }

  void Bloom::Initialize(DX12Basic* dx12, const std::string& shaderName)
  {
    IPostEffect::Initialize(dx12, shaderName);
    extractPipeline_ = MakeFullScreenPipeline(L"ThresholdExtract.PS.hlsl");
    blurPipeline_ = MakeFullScreenPipeline(L"GaussianBlur.PS.hlsl");
    combinePipeline_ = MakeFullScreenPipeline(L"BloomCombine.PS.hlsl");
    CreateCBV();
    CreateRenderTexture();

    // WinApp のインスタンスを取得してリサイズコールバックを登録
    winApp_ = WinApp::GetInstance();
    if (winApp_) {
      onResizeId_ = winApp_->RegisterOnResizeFunc(
        std::bind(&Bloom::OnResize, this, std::placeholders::_1)
      );
    }
  }

  void Bloom::Apply(uint32_t inputSrvIndex, D3D12_CPU_DESCRIPTOR_HANDLE outputRtvHandle, [[maybe_unused]] uint32_t depthSrvIndex, [[maybe_unused]] const Vector4& clearColor)
  {
    CommandContext& context = dx12_->GetContext();

    //---------------------------Pass1 HighLunExtract---------------------------//
    BeginFullScreenPass(extractPipeline_, highLumRT_.rtvHandle, inputSrvIndex)
      .BindCBV("gParam"_id, extractCBufferRes_->GetGPUVirtualAddress());
    DrawFullScreen();

    //---------------------------Pass2 HorizontalBlur---------------------------//
    context.Transition(highLumRT_.resource, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    BeginFullScreenPass(blurPipeline_, blurRT_.rtvHandle, highLumRT_.srvIndex)
      .BindCBV("gParam"_id, blurCBufferRes1_->GetGPUVirtualAddress());
    DrawFullScreen();

    //---------------------------Pass3 VerticalBlur---------------------------//
    context.Transition(blurRT_.resource, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    BeginFullScreenPass(blurPipeline_, resultRT_.rtvHandle, blurRT_.srvIndex)
      .BindCBV("gParam"_id, blurCBufferRes2_->GetGPUVirtualAddress());
    DrawFullScreen();

    //---------------------------Pass4 Combine---------------------------//
    context.Transition(resultRT_.resource, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    CommandContext::ShaderBinder binder = BeginFullScreenPass(combinePipeline_, outputRtvHandle, inputSrvIndex);
    binder.BindCBV("gParam"_id, combineCBufferRes_->GetGPUVirtualAddress());
    binder.BindSRV("gBloomTexture"_id, resultRT_.srvIndex);
    DrawFullScreen();

    //---------------------------リソースステートを戻す---------------------------//
    context.Transition(highLumRT_.resource, D3D12_RESOURCE_STATE_RENDER_TARGET);
    context.Transition(blurRT_.resource, D3D12_RESOURCE_STATE_RENDER_TARGET);
    context.Transition(resultRT_.resource, D3D12_RESOURCE_STATE_RENDER_TARGET);
  }

  void Bloom::DrawImgui()
  {
#ifdef _DEBUG
    ImGui::DragFloat("Bloom Intensity", &combineData_->intensity, 0.01f, 0.0f, 10.0f);
    ImGui::DragFloat("Bloom Threshold", &extractData_->threshold, 0.01f, 0.0f, 1.0f);
    ImGui::DragFloat("Bloom Sigma", &blurData1_->sigma, 0.01f, 0.0f, 50.0f);
    blurData2_->sigma = blurData1_->sigma; // Pass2でも同じ値を使用
    ImGui::DragInt("Bloom Kernel Size", reinterpret_cast<int*>(&blurData1_->kernelSize), 1.0f, 1, 100);
    blurData2_->kernelSize = blurData1_->kernelSize; // Pass2でも同じ値を使用
#endif
  }

  bool Bloom::SetGenericParam(const EffectParam& param)
  {
    if (auto* bloomParam = std::get_if<HighLumExtrcatParam>(&param)) {
      SetParam(*bloomParam);
      return true;
    }

    if (auto* bloomParam = std::get_if<GaussianBlurParam>(&param)) {
      SetParam(*bloomParam);
      return true;
    }

    if (auto* bloomParam = std::get_if<BloomCombineParam>(&param)) {
      SetParam(*bloomParam);
      return true;
    }
    return false;
  }

  void Bloom::SetParam(const HighLumExtrcatParam& param)
  {
    if (extractData_ == nullptr) {
      return;
    }
    extractData_->threshold = param.threshold;
  }

  void Bloom::SetParam(const GaussianBlurParam& param)
  {
    if (blurData1_ == nullptr) {
      return;
    }
    blurData1_->sigma = param.sigma;
    blurData1_->kernelSize = param.kernelSize;

    if (blurData2_ == nullptr) {
      return;
    }
    blurData2_->sigma = param.sigma;
    blurData2_->kernelSize = param.kernelSize;
  }

  void Bloom::SetParam(const BloomCombineParam& param)
  {
    if (combineData_ == nullptr) {
      return;
    }
    combineData_->intensity = param.intensity;
  }

  void Bloom::CreateCBV()
  {
    extractCBufferRes_ = dx12_->MakeBufferResource(sizeof(HighLumExtrcatParam));
    blurCBufferRes1_ = dx12_->MakeBufferResource(sizeof(GaussianBlurParam));
    blurCBufferRes2_ = dx12_->MakeBufferResource(sizeof(GaussianBlurParam));
    combineCBufferRes_ = dx12_->MakeBufferResource(sizeof(BloomCombineParam));

    // データの設定
    extractCBufferRes_->Map(0, nullptr, reinterpret_cast<void**>(&extractData_));
    blurCBufferRes1_->Map(0, nullptr, reinterpret_cast<void**>(&blurData1_));
    blurCBufferRes2_->Map(0, nullptr, reinterpret_cast<void**>(&blurData2_));
    combineCBufferRes_->Map(0, nullptr, reinterpret_cast<void**>(&combineData_));

    // データの初期化
    extractData_->threshold = 1.0f;

    blurData1_->sigma = 2.0f;
    blurData1_->kernelSize = 10;
    blurData1_->direction = Vector2(1.0f, 0.0f); // 水平方向

    blurData2_->sigma = 2.0f;
    blurData2_->kernelSize = 10;
    blurData2_->direction = Vector2(0.0f, 1.0f); // 垂直方向

    combineData_->intensity = 1.0f;
  }

  void Bloom::CreateRenderTexture()
  {
    const Vector4 clearColor(0.0f, 0.0f, 0.0f, 1.0f);
    highLumRT_.Create(dx12_, dx12_->GetSceneWidth(), dx12_->GetSceneHeight(), DXGI_FORMAT_R8G8B8A8_UNORM, clearColor);
    blurRT_.Create(dx12_, dx12_->GetSceneWidth(), dx12_->GetSceneHeight(), DXGI_FORMAT_R8G8B8A8_UNORM, clearColor);
    resultRT_.Create(dx12_, dx12_->GetSceneWidth(), dx12_->GetSceneHeight(), DXGI_FORMAT_R8G8B8A8_UNORM, clearColor);
  }

  void Bloom::OnResize(const Vector2& newSize)
  {
    newSize; // 未使用の警告を抑制

    // RenderTexture を再作成
    RecreateRenderTexture();
  }

  void Bloom::RecreateRenderTexture()
  {
    resultRT_.Release();
    highLumRT_.Release();
    blurRT_.Release();

    CreateRenderTexture();
  }


} // namespace Tako

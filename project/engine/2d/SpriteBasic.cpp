#include "SpriteBasic.h"
#include <cassert>

#include "Mat4x4Func.h"

#ifdef _DEBUG
#include "DebugUIManager.h"
#endif

namespace Tako {

  std::unique_ptr<SpriteBasic> SpriteBasic::instance_ = nullptr;

  SpriteBasic* SpriteBasic::GetInstance()
  {
    if (!instance_) {
      instance_ = std::make_unique<SpriteBasic>(Token{});
    }
    return instance_.get();
  }

  void SpriteBasic::Initialize(DX12Basic* dx12)
  {
    dx12_ = dx12;

    viewMatrixSprite_ = Mat4x4::MakeIdentity();
    projectionMatrixSprite_ = Mat4x4::MakeOrtho(
      0.0f, 0.0f,
      static_cast<float>(WinApp::clientWidth), static_cast<float>(WinApp::clientHeight),
      0.0f, 100.0f);

    CreatePipeline();
  }

  void SpriteBasic::Finalize()
  {
    instance_.reset();
  }

  void SpriteBasic::SetCommonRenderSetting()
  {
    dx12_->GetContext().SetGraphicsPipeline(pipeline_);
  }

  void SpriteBasic::OnResize(const Vector2& size)
  {
    projectionMatrixSprite_ = Mat4x4::MakeOrtho(
      0.0f, 0.0f,
      static_cast<float>(size.x), static_cast<float>(size.y),
      0.0f, 100.0f);
  }

  void SpriteBasic::CreatePipeline()
  {
    ShaderLibrary& library = dx12_->GetShaderLibrary();
    pipeline_.vs = library.Get(L"Sprite.VS.hlsl", ShaderStage::Vertex);
    pipeline_.ps = library.Get(L"Sprite.PS.hlsl", ShaderStage::Pixel);
    pipeline_.vertexLayout = VertexLayout::Sprite;
    pipeline_.renderState = { .blend = BlendMode::Alpha, .cull = CullMode::None, .depthTest = false, .depthWrite = false };
  }

} // namespace Tako

#include "Object3dBasic.h"
#include "DX12Basic.h"
#include "Camera.h"
#include "SrvManager.h"
#include "ShadowRenderer.h"
#include "EnginePaths.h"

#ifdef _DEBUG
#include "DebugUIManager.h"
#endif

namespace Tako {

std::unique_ptr<Object3dBasic> Object3dBasic::instance_ = nullptr;

Object3dBasic* Object3dBasic::GetInstance()
{
	if (!instance_)
	{
		instance_ = std::make_unique<Object3dBasic>(Token{});
	}
	return instance_.get();
}

void Object3dBasic::Initialize(DX12Basic* dx12)
{
	dx12_ = dx12;

	CreatePipelines();

	// ライトの生成と初期化
	light_ = std::make_unique<Light>();
	light_->Initialize(dx12_);
	
}

void Object3dBasic::Update()
{
	camera_->SetViewProjectionMatrix(camera_->GetViewMatrix() * camera_->GetProjectionMatrix());

	// 本編カメラを既定の視点にする
	SetView(*camera_);

  light_->Update();
}

void Object3dBasic::SetView(const Camera& camera)
{
	currentViewAddress_ = dx12_->GetUploadAllocator().Push(ViewConstants{ camera.GetViewMatrix() * camera.GetProjectionMatrix(), camera.GetTranslate(), 0.0f });
}

void Object3dBasic::Finalize()
{
	light_.reset();

	instance_.reset();
}

void Object3dBasic::SetCommonRenderSetting()
{
	BindRenderSetting(pipeline_);
}

void Object3dBasic::SetTransparentRenderSetting()
{
	BindRenderSetting(transparentPipeline_);
}

void Object3dBasic::SetInstancedRenderSetting()
{
	BindRenderSetting(instancedPipeline_);
}

void Object3dBasic::SetDirectionalLight(const Vector3& direction, const Vector4& color, int32_t lightType, float intensity)
{
	light_->SetDirectionalLight(direction, color, lightType, intensity);
}

void Object3dBasic::SetPointLight(const Vector3& position, const Vector4& color, float intensity, float radius, float decay, bool enable, int index)
{
	light_->SetPointLight(position, color, intensity, radius, decay, enable, index);
}

void Object3dBasic::SetSpotLight(const Vector3& position, const Vector3& direction, const Vector4& color, float intensity, float distance, float decay, float cosAngle, bool enable, int index)
{
	light_->SetSpotLight(position, direction, color, intensity, distance, decay, cosAngle, enable, index);
}

void Object3dBasic::CreatePipelines()
{
	ShaderLibrary& library = dx12_->GetShaderLibrary();

	pipeline_.vs = library.Get(L"Object3D.VS.hlsl", ShaderStage::Vertex);
	pipeline_.ps = library.Get(L"Object3d.PS.hlsl", ShaderStage::Pixel);
	pipeline_.vertexLayout = VertexLayout::Mesh;
	pipeline_.renderState.blend = BlendMode::Alpha;

	// 裏面も表示し、後ろの半透明を隠さないよう深度は書かない
	transparentPipeline_ = pipeline_;
	transparentPipeline_.renderState.cull = CullMode::None;
	transparentPipeline_.renderState.depthWrite = false;

	instancedPipeline_ = pipeline_;
	instancedPipeline_.vs = library.Get(L"Object3dInstanced.VS.hlsl", ShaderStage::Vertex);
	instancedPipeline_.ps = library.Get(L"Object3dInstanced.PS.hlsl", ShaderStage::Pixel);
}

void Object3dBasic::BindRenderSetting(const GraphicsPipelineDesc& pipeline)
{
	CommandContext& context = dx12_->GetContext();
	context.SetGraphicsPipeline(pipeline);

	light_->PreDraw();
	context.Graphics().BindCBV("gView"_id, currentViewAddress_);
	ShadowRenderer::GetInstance()->SetShadowForMainPass();
}

} // namespace Tako

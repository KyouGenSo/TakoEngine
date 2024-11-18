#include "Object3d.h"
#include "DX12Basic.h"
#include "Object3dbasic.h"
#include "Model.h"
#include "TextureManager.h"
#include"ModelManager.h"
#include"Camera.h"
#include <cassert>
#include<fstream>
#include<sstream>
#include <numbers>
#include "Input.h"

Object3d::~Object3d()
{
	delete light_;
}

void Object3d::Initialize()
{
	m_camera_ = Object3dBasic::GetInstance()->GetCamera();

	// トランスフォームに初期化値を設定
	transform_ = { Vector3(1.0f, 1.0f, 1.0f), Vector3(0.0f, 0.0f, 0.0f), Vector3(0.0f, 0.0f, 0.0f) };

	// 座標変換行列データの生成
	CreateTransformationMatrixData();

	// カメラデータの生成
	CreateCameraForGPUData();

	// ライトの生成と初期化
	light_ = new Light();
	light_->Initialize(Object3dBasic::GetInstance()->GetDX12Basic());
}

void Object3d::Update()
{
	m_camera_ = Object3dBasic::GetInstance()->GetCamera();

	// トランスフォームでワールド行列を作る
	Matrix4x4 worldMatrix = Mat4x4::MakeAffine(transform_.scale, transform_.rotate, transform_.translate);

	Matrix4x4 wvpMatrix;

	if (m_camera_) {
		const Matrix4x4& viewProjectionMatrix = m_camera_->GetViewProjectionMatrix();
		wvpMatrix = Mat4x4::Multiply(worldMatrix, viewProjectionMatrix);
	} else {
		wvpMatrix = worldMatrix;
	}

	// 座標変換行列データに書き込む
	transformationMatData_->WVP = wvpMatrix;
	transformationMatData_->world = worldMatrix;
	transformationMatData_->worldInvTranspose = Mat4x4::InverseTranspose(worldMatrix);
}

void Object3d::Draw()
{
	// 座標変換行列CBufferの場所を設定
	Object3dBasic::GetInstance()->GetDX12Basic()->GetCommandList()->SetGraphicsRootConstantBufferView(1, transformationMatResource_->GetGPUVirtualAddress());

	// シェーダー用カメラデータの場所を設定
	Object3dBasic::GetInstance()->GetDX12Basic()->GetCommandList()->SetGraphicsRootConstantBufferView(4, cameraForGPUResource_->GetGPUVirtualAddress());

	// ライトの描画設定
	light_->PreDraw();

	// モデルの描画
	if (m_model_)
	{
		m_model_->Draw();
	}
}

void Object3d::SetModel(const std::string& fileName)
{
	m_model_ = ModelManager::GetInstance()->FindModel(fileName);
}

void Object3d::SetShininess(float shininess)
{
	if (m_model_)
	{
		m_model_->SetShininess(shininess);
	}
}

void Object3d::SetEnableLighting(bool enableLighting)
{
	if (m_model_)
	{
		m_model_->SetEnableLighting(enableLighting);
	}
}

void Object3d::SetEnableHighlight(bool enableHighlight)
{
	if (m_model_)
	{
		m_model_->SetEnableHighlight(enableHighlight);
	}
}

void Object3d::SetDirectionalLight(const Vector3& direction, const Vector4& color, int32_t lightType, float intensity)
{
	light_->SetDirectionalLight(direction, color, lightType, intensity);
}

void Object3d::SetPointLight(const Vector3& position, const Vector4& color, float intensity, float radius, float decay, bool enable, int index)
{
	light_->SetPointLight(position, color, intensity, radius, decay, enable, index);
}

void Object3d::SetSpotLight(const Vector3& position, const Vector3& direction, const Vector4& color, float intensity, float distance, float decay, float cosAngle, bool enable)
{
	light_->SetSpotLight(position, direction, color, intensity, distance, decay, cosAngle, enable);
}

void Object3d::CreateTransformationMatrixData()
{
	// 座標変換行列リソースを生成
	transformationMatResource_ = Object3dBasic::GetInstance()->GetDX12Basic()->MakeBufferResource(sizeof(TransformationMatrix));

	// 座標変換行列リソースをマップ
	transformationMatResource_->Map(0, nullptr, reinterpret_cast<void**>(&transformationMatData_));

	// 座標変換行列データの初期値を書き込む
	transformationMatData_->WVP = Mat4x4::MakeIdentity();
	transformationMatData_->world = Mat4x4::MakeIdentity();
	transformationMatData_->worldInvTranspose = Mat4x4::MakeIdentity();
}

void Object3d::CreateCameraForGPUData()
{
	// カメラリソースを生成
	cameraForGPUResource_ = Object3dBasic::GetInstance()->GetDX12Basic()->MakeBufferResource(sizeof(CameraForGPU));

	// カメラリソースをマップ
	cameraForGPUResource_->Map(0, nullptr, reinterpret_cast<void**>(&cameraForGPUData_));

	// カメラデータの初期値を書き込む
	cameraForGPUData_->worldPos = m_camera_->GetTranslate();
}

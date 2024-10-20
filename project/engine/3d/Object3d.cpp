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
#include "Input.h"

void Object3d::Initialize()
{
	m_camera_ = Object3dBasic::GetInstance()->GetCamera();

	// トランスフォームに初期化値を設定
	transform_ = { Vector3(1.0f, 1.0f, 1.0f), Vector3(0.0f, 0.0f, 0.0f), Vector3(0.0f, 0.0f, 0.0f) };

	// 座標変換行列データの生成
	CreateTransformationMatrixData();

	// 平行光源データの生成
	CreateDirectionalLightData();
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
}

void Object3d::Draw()
{
	// 座標変換行列CBufferの場所を設定
	Object3dBasic::GetInstance()->GetDX12Basic()->GetCommandList()->SetGraphicsRootConstantBufferView(1, transformationMatResource_->GetGPUVirtualAddress());

	// 平行光源CBufferの場所を設定
	Object3dBasic::GetInstance()->GetDX12Basic()->GetCommandList()->SetGraphicsRootConstantBufferView(3, directionalLightResource_->GetGPUVirtualAddress());

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

void Object3d::CreateTransformationMatrixData()
{
	// 座標変換行列リソースを生成
	transformationMatResource_ = Object3dBasic::GetInstance()->GetDX12Basic()->MakeBufferResource(sizeof(TransformationMatrix));

	// 座標変換行列リソースをマップ
	transformationMatResource_->Map(0, nullptr, reinterpret_cast<void**>(&transformationMatData_));

	// 座標変換行列データの初期値を書き込む
	transformationMatData_->WVP = Mat4x4::MakeIdentity();
	transformationMatData_->world = Mat4x4::MakeIdentity();
}

void Object3d::CreateDirectionalLightData()
{
	// 平行光源リソースを生成
	directionalLightResource_ = Object3dBasic::GetInstance()->GetDX12Basic()->MakeBufferResource(sizeof(DirectionalLight));

	// 平行光源リソースをマップ
	directionalLightResource_->Map(0, nullptr, reinterpret_cast<void**>(&directionalLightData_));

	// 平行光源データの初期値を書き込む
	directionalLightData_->direction = Vector3(0.0f, -1.0f, 0.0f); // ライトの方向
	
	directionalLightData_->color = { 1.0f, 1.0f, 1.0f, 1.0f };     // ライトの色
	
	directionalLightData_->lightType = 1;                          // ライトのタイプ 0:Lambert 1:Half-Lambert
	
	directionalLightData_->intensity = 1.0f;                       // 輝度
}

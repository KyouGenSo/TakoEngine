#include "Object3d.h"
#include "DX12Basic.h"
#include "Object3dbasic.h"
#include "Model.h"
#include "TextureManager.h"
#include"ModelManager.h"
#include <cassert>
#include<fstream>
#include<sstream>

void Object3d::Initialize(Object3dBasic* obj3dBasic)
{
	// Object3dBasicクラスのインスタンスを参照
	m_obj3dBasic_ = obj3dBasic;

	// トランスフォームに初期化値を設定
	transform_ = { Vector3(1.0f, 1.0f, 1.0f), Vector3(0.0f, 0.0f, 0.0f), Vector3(0.0f, 0.0f, 0.0f) };
	cameraTransform_ = { Vector3(1.0f, 1.0f, 1.0f), Vector3(0.3f, 0.0f, 0.0f), Vector3(0.0f, 4.0f, -10.0f) };

	// 座標変換行列データの生成
	CreateTransformationMatrixData();

	// 平行光源データの生成
	CreateDirectionalLightData();
}

void Object3d::Update()
{
	// トランスフォームでワールド行列を作る
	Matrix4x4 worldMatrix = Mat4x4::MakeAffine(transform_.scale, transform_.rotate, transform_.translate);

	// ビュー行列を作る
	Matrix4x4 cameraMatrix = Mat4x4::MakeAffine(cameraTransform_.scale, cameraTransform_.rotate, cameraTransform_.translate);
	Matrix4x4 viewMatrix = Mat4x4::Inverse(cameraMatrix);

	// プロジェクション行列を作る
	Matrix4x4 projectionMatrix = Mat4x4::MakePerspective(0.45f, static_cast<float>(WinApp::kClientWidth) / static_cast<float>(WinApp::kClientHeight), 0.1f, 100.0f);

	// 座標変換行列データに書き込む
	transformationMatData_->WVP = Mat4x4::Multiply(worldMatrix, Mat4x4::Multiply(viewMatrix, projectionMatrix));
	transformationMatData_->world = worldMatrix;
}

void Object3d::Draw()
{
	// 座標変換行列CBufferの場所を設定
	m_obj3dBasic_->GetDX12Basic()->GetCommandList()->SetGraphicsRootConstantBufferView(1, transformationMatResource_->GetGPUVirtualAddress());

	// 平行光源CBufferの場所を設定
	m_obj3dBasic_->GetDX12Basic()->GetCommandList()->SetGraphicsRootConstantBufferView(3, directionalLightResource_->GetGPUVirtualAddress());

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
	transformationMatResource_ = m_obj3dBasic_->GetDX12Basic()->MakeBufferResource(sizeof(TransformationMatrix));

	// 座標変換行列リソースをマップ
	transformationMatResource_->Map(0, nullptr, reinterpret_cast<void**>(&transformationMatData_));

	// 座標変換行列データの初期値を書き込む
	transformationMatData_->WVP = Mat4x4::MakeIdentity();
	transformationMatData_->world = Mat4x4::MakeIdentity();
}

void Object3d::CreateDirectionalLightData()
{
	// 平行光源リソースを生成
	directionalLightResource_ = m_obj3dBasic_->GetDX12Basic()->MakeBufferResource(sizeof(DirectionalLight));

	// 平行光源リソースをマップ
	directionalLightResource_->Map(0, nullptr, reinterpret_cast<void**>(&directionalLightData_));

	// 平行光源データの初期値を書き込む
	directionalLightData_->direction = Vector3(0.0f, -1.0f, 0.0f); // ライトの方向
	
	directionalLightData_->color = { 1.0f, 1.0f, 1.0f, 1.0f };     // ライトの色
	
	directionalLightData_->lightType = 0;                          // ライトのタイプ 0:Lambert 1:Half-Lambert
	
	directionalLightData_->intensity = 1.0f;                       // 輝度
}

#include "Object3d.h"
#include "DX12Basic.h"
#include "Object3dbasic.h"
#include "Model.h"
#include"ModelManager.h"
#include"Camera.h"
#include "SrvManager.h"
#include "Logger.h"

Object3d::~Object3d()
{
  m_model_->Finalize();
  delete m_model_;
}

void Object3d::Initialize()
{
	m_camera_ = Object3dBasic::GetInstance()->GetCamera();

	// トランスフォームに初期化値を設定
	transform_ = {
	  .scale= Vector3(1.0f, 1.0f, 1.0f),
	  .rotate= Vector3(0.0f, 0.0f, 0.0f),
	  .translate= Vector3(0.0f, 0.0f, 0.0f) };

  attachmentOffset_ = {
    .scale = Vector3(1.0f, 1.0f, 1.0f),
    .rotate = Vector3(0.0f, 0.0f, 0.0f),
    .translate = Vector3(0.0f, 0.0f, 0.0f)
  };

	// 座標変換行列データの生成
	CreateTransformationMatrixData();

	// カメラデータの生成
	CreateCameraForGPUData();
}

void Object3d::Update()
{
	// モデルの更新
	if (m_model_)
	{
		m_model_->Update();
	}

	// 親Jointへのアタッチメント処理
	if (IsAttached() && parentObject_ && parentObject_->GetModel())
	{
		Model* parentModel = parentObject_->GetModel();
		if (parentModel->HasSkeleton())
		{
			// 親のワールド行列を取得
			Matrix4x4 parentWorldMatrix = parentObject_->GetWorldMatrix();
			
			// 親のJointのワールド行列を取得
			Matrix4x4 jointWorldMatrix = parentModel->GetJointWorldMatrix(parentJointName_, parentWorldMatrix);
			
			// オフセットのアフィン変換行列を作成
			Matrix4x4 offsetMatrix = Mat4x4::MakeAffine(
				attachmentOffset_.scale,
				attachmentOffset_.rotate,
				attachmentOffset_.translate
			);
			
			// 最終的なワールド行列を計算（オフセット行列 × Joint行列）
			finalWorldMatrix_ = Mat4x4::Multiply(offsetMatrix, jointWorldMatrix);

      worldMatrix_ = finalWorldMatrix_;
		}
	}
  else
	{
    // トランスフォームでワールド行列を作る
    worldMatrix_ = Mat4x4::MakeAffine(transform_.scale, transform_.rotate, transform_.translate);
	}

	// モデルのローカル行列を取得
	Matrix4x4 modelLocalMatrix = Mat4x4::MakeIdentity();
	if (m_model_)
	{
		modelLocalMatrix = m_model_->GetLocalMatrix();
	}

	Matrix4x4 wvpMatrix;

	if ((*m_camera_)) {
		const Matrix4x4& viewProjectionMatrix = (*m_camera_)->GetViewProjectionMatrix();
		wvpMatrix = Mat4x4::Multiply(worldMatrix_, viewProjectionMatrix);
	} else {
		wvpMatrix = worldMatrix_;
	}

	// 座標変換行列データに書き込む
	if (m_model_)
	{
		if (m_model_->HasSkeleton())
		{
			transformationMatData_->WVP = wvpMatrix;
			transformationMatData_->world = worldMatrix_;
			transformationMatData_->worldInvTranspose = Mat4x4::InverseTranspose(worldMatrix_);
		} else
		{
			transformationMatData_->WVP = modelLocalMatrix * wvpMatrix;
			transformationMatData_->world = modelLocalMatrix * worldMatrix_;
			transformationMatData_->worldInvTranspose = Mat4x4::InverseTranspose(modelLocalMatrix * worldMatrix_);
		}
	}
}

void Object3d::Draw()
{
	// 座標変換行列CBufferの場所を設定
	Object3dBasic::GetInstance()->GetDX12Basic()->GetCommandList()->SetGraphicsRootConstantBufferView(1, transformationMatResource_->GetGPUVirtualAddress());

	// シェーダー用カメラデータの場所を設定
	Object3dBasic::GetInstance()->GetDX12Basic()->GetCommandList()->SetGraphicsRootConstantBufferView(4, cameraForGPUResource_->GetGPUVirtualAddress());

	// モデルの描画
	if (m_model_)
	{
		m_model_->Draw(transformationMatData_->world, (*Object3dBasic::GetInstance()->GetCamera())->GetViewProjectionMatrix());
	}
}

void Object3d::DrawImGui()
{
  if (m_model_)
  {
    m_model_->DrawImGui();
  }
}

void Object3d::SetModel(const std::string& fileName)
{
  if (m_model_)
  {
    m_model_ = nullptr;
  }

  m_model_ = ModelManager::GetInstance()->GetModel(fileName);
}

void Object3d::SetMaterialColor(const Vector4& color)
{
  if (m_model_)
  {
    m_model_->SetMaterialColor(color);
  }
}

void Object3d::SetUvTransform(const Transform& uvTransform)
{
  if (m_model_)
  {
    m_model_->SetUvTransform(uvTransform);
  }
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

void Object3d::SetEnableEnvMap(bool enableEnvMap)
{
  if (m_model_)
  {
    m_model_->SetEnableEnvMap(enableEnvMap);
  }
}

void Object3d::SetEnvironmentTexture(uint32_t textureIndex)
{
  if (m_model_)
  {
    m_model_->SetEnvironmentTexture(textureIndex);
  }
}

void Object3d::SetEnvMapCoefficient(float coefficient)
{
  if (m_model_)
  {
    m_model_->SetEnvMapCoefficient(coefficient);
  }
}

void Object3d::AttachToJoint(Object3d* parent, const std::string& jointName, const Vector3& offset)
{
	// 親オブジェクトとJoint名を設定
	parentObject_ = parent;
	parentJointName_ = jointName;
  attachmentOffset_.translate = offset;
}

void Object3d::DetachFromJoint()
{
	// 親オブジェクトとJoint名をクリア
	parentObject_ = nullptr;
	parentJointName_.clear();
  attachmentOffset_.translate = Vector3(0.0f, 0.0f, 0.0f);
}

Matrix4x4 Object3d::GetWorldMatrix() const
{
	// トランスフォームからワールド行列を作成
	Matrix4x4 worldMatrix = Mat4x4::MakeAffine(transform_.scale, transform_.rotate, transform_.translate);
	
	// モデルのローカル行列を考慮
	if (m_model_ && !m_model_->HasSkeleton())
	{
		worldMatrix = m_model_->GetLocalMatrix() * worldMatrix;
	}
	
	return worldMatrix;
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
	cameraForGPUData_->worldPos = (*m_camera_)->GetTranslate();
}
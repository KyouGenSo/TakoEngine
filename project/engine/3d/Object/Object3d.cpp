#include "Object3d.h"
#include "DX12Basic.h"
#include "Object3dbasic.h"
#include "Model.h"
#include"ModelManager.h"
#include"Camera.h"
#include "SrvManager.h"
#include "ShadowRenderer.h"
#include "Logger.h"

namespace Tako {

Object3d::Object3d() = default;

Object3d::~Object3d()
{
  if (model_) {
    model_->Finalize();
  }
  // unique_ptr が自動で delete する
}

void Object3d::Initialize()
{
	camera_ = Object3dBasic::GetInstance()->GetCamera();

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
	if (model_)
	{
		model_->Update();
	}

	// 親 Joint へのアタッチメント処理
	if (IsAttached() && parentObject_ && parentObject_->GetModel())
	{
		Model* parentModel = parentObject_->GetModel();
		if (parentModel->HasSkeleton())
		{
			// 親のワールド行列を取得
			Matrix4x4 parentWorldMatrix = parentObject_->GetWorldMatrix();
			
			// 親の Joint のワールド行列を取得
			Matrix4x4 jointWorldMatrix = parentModel->GetJointWorldMatrix(parentJointName_, parentWorldMatrix);
			
			// オフセットのアフィン変換行列を作成
			Matrix4x4 offsetMatrix = Mat4x4::MakeAffine(
				attachmentOffset_.scale,
				attachmentOffset_.rotate,
				attachmentOffset_.translate
			);
			
			// 最終的なワールド行列を計算（オフセット行列 × Joint 行列）
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
	if (model_)
	{
		modelLocalMatrix = model_->GetLocalMatrix();
	}

	Matrix4x4 wvpMatrix;

	if ((*camera_)) {
		const Matrix4x4& viewProjectionMatrix = (*camera_)->GetViewProjectionMatrix();
		wvpMatrix = Mat4x4::Multiply(worldMatrix_, viewProjectionMatrix);
	} else {
		wvpMatrix = worldMatrix_;
	}

	// 座標変換行列データに書き込む
	if (model_)
	{
		if (model_->HasSkeleton())
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
	// 半透明モードなら PSO を一時的に切り替え (CullMode=NONE で両面描画 + 深度書き込み無効)
	// シャドウマップレンダリング中はシャドウ用 PSO が既にセットされているのでスキップ
	const bool useTransparentPso = isTransparent_ && !ShadowRenderer::GetInstance()->IsRenderingShadow();
	if (useTransparentPso) {
		Object3dBasic::GetInstance()->SetTransparentRenderSetting();
	}

	// シャドウマップレンダリング中は異なるルートパラメータインデックスを使用
	if (ShadowRenderer::GetInstance()->IsRenderingShadow()) {
		// シャドウ用ルートシグネチャのインデックス（パラメータ0）
		Object3dBasic::GetInstance()->GetDX12Basic()->GetCommandList()->SetGraphicsRootConstantBufferView(0, transformationMatResource_->GetGPUVirtualAddress());
		// カメラデータは不要（シャドウマップ生成時は使用しない）
	} else {
		// 通常のルートシグネチャのインデックス
		Object3dBasic::GetInstance()->GetDX12Basic()->GetCommandList()->SetGraphicsRootConstantBufferView(1, transformationMatResource_->GetGPUVirtualAddress());
		// シェーダー用カメラデータの場所を設定
		Object3dBasic::GetInstance()->GetDX12Basic()->GetCommandList()->SetGraphicsRootConstantBufferView(4, cameraForGPUResource_->GetGPUVirtualAddress());
	}

	// モデルの描画
	if (model_)
	{
		model_->Draw(transformationMatData_->world, (*Object3dBasic::GetInstance()->GetCamera())->GetViewProjectionMatrix());
	}

	// 半透明モードで PSO を切り替えていた場合、後続の不透明 Object3d への影響を防ぐため通常 PSO に戻す
	if (useTransparentPso) {
		Object3dBasic::GetInstance()->SetCommonRenderSetting();
	}
}

void Object3d::DrawImGui()
{
  if (model_)
  {
    model_->DrawImGui();
  }
}

void Object3d::SetModel(const std::string& fileName)
{
  // 既存モデルがあれば解放
  if (model_)
  {
    model_->Finalize();
    model_.reset();
  }

  // 新しいモデルをセット（ModelManager から Clone を取得）
  model_ = ModelManager::GetInstance()->GetModel(fileName);
}

void Object3d::SetModel(std::unique_ptr<Model> model)
{
  // 既存モデルがあれば解放
  if (model_)
  {
    model_->Finalize();
    model_.reset();
  }

  model_ = std::move(model);
}

void Object3d::SetMaterialColor(const Vector4& color)
{
  if (model_)
  {
    model_->SetMaterialColor(color);
  }
}

Vector4 Object3d::GetMaterialColor() const
{
  if (model_)
  {
    return model_->GetMaterialColor();
  }
  return Vector4(1.0f, 1.0f, 1.0f, 1.0f);
}

void Object3d::SetUvTransform(const Transform& uvTransform)
{
  if (model_)
  {
    model_->SetUvTransform(uvTransform);
  }
}

void Object3d::SetShininess(float shininess)
{
	if (model_)
	{
		model_->SetShininess(shininess);
	}
}

void Object3d::SetEnableLighting(bool enableLighting)
{
	if (model_)
	{
		model_->SetEnableLighting(enableLighting);
	}
}

void Object3d::SetEnableHighlight(bool enableHighlight)
{
	if (model_)
	{
		model_->SetEnableHighlight(enableHighlight);
	}
}

void Object3d::SetEnableEnvMap(bool enableEnvMap)
{
  if (model_)
  {
    model_->SetEnableEnvMap(enableEnvMap);
  }
}

void Object3d::SetEnvironmentTexture(uint32_t textureIndex)
{
  if (model_)
  {
    model_->SetEnvironmentTexture(textureIndex);
  }
}

void Object3d::SetEnvMapCoefficient(float coefficient)
{
  if (model_)
  {
    model_->SetEnvMapCoefficient(coefficient);
  }
}

void Object3d::SetMeshVisible(const std::string& name, bool visible)
{
  if (model_)
  {
    model_->SetMeshVisible(name, visible);
  }
}

bool Object3d::IsMeshVisible(const std::string& name) const
{
  return model_ ? model_->IsMeshVisible(name) : false;
}

std::vector<std::string> Object3d::GetMeshNames() const
{
  return model_ ? model_->GetMeshNames() : std::vector<std::string>{};
}

void Object3d::AttachToJoint(Object3d* parent, const std::string& jointName, const Vector3& offset)
{
	// 親オブジェクトと Joint 名を設定
	parentObject_ = parent;
	parentJointName_ = jointName;
  attachmentOffset_.translate = offset;
}

void Object3d::DetachFromJoint()
{
	// 親オブジェクトと Joint 名をクリア
	parentObject_ = nullptr;
	parentJointName_.clear();
  attachmentOffset_.translate = Vector3(0.0f, 0.0f, 0.0f);
}

Matrix4x4 Object3d::GetWorldMatrix() const
{
	// トランスフォームからワールド行列を作成
	Matrix4x4 worldMatrix = Mat4x4::MakeAffine(transform_.scale, transform_.rotate, transform_.translate);
	
	// モデルのローカル行列を考慮
	if (model_ && !model_->HasSkeleton())
	{
		worldMatrix = model_->GetLocalMatrix() * worldMatrix;
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
	cameraForGPUData_->worldPos = (*camera_)->GetTranslate();
}

} // namespace Tako
#include "Object3d.h"
#include "DX12Basic.h"
#include "Renderer.h"
#include "Model.h"
#include"ModelManager.h"
#include "SrvManager.h"
#include "ShadowRenderer.h"
#include "Logger.h"
#include "ColorFunc.h"

#include <numbers>

namespace Tako {

namespace {

constexpr float kDegToRad = std::numbers::pi_v<float> / 180.0f;

Vector4 Multiply(const Vector4& a, const Vector4& b) { return { a.x * b.x, a.y * b.y, a.z * b.z, a.w * b.w }; }

} // namespace

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
}

void Object3d::Update()
{
	if (model_)
	{
		model_->Update();
	}

	animator_.Update();
	const AnimationPose pose = animator_.Evaluate("");
	ApplyAnimationColor(pose.color);
	isAnimationVisible_ = pose.visible;

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

	// アニメーションの上乗せはローカル空間で掛ける。worldMatrix_ は上乗せ前のまま残す（アタッチ中で更新されないフレームに重ね掛けしないため）
	Matrix4x4 world = worldMatrix_;
	if (pose.translate != Vector3{ 0.0f, 0.0f, 0.0f } || pose.rotate != Vector3{ 0.0f, 0.0f, 0.0f } || pose.scale != Vector3{ 1.0f, 1.0f, 1.0f })
	{
		world = Mat4x4::MakeAffine(pose.scale, pose.rotate * kDegToRad, pose.translate) * world;
	}

	// 座標変換行列データに書き込む
	if (model_)
	{
		if (model_->HasSkeleton())
		{
			transformationMatData_->world = world;
			transformationMatData_->worldInvTranspose = Mat4x4::InverseTranspose(world);
		} else
		{
			transformationMatData_->world = modelLocalMatrix * world;
			transformationMatData_->worldInvTranspose = Mat4x4::InverseTranspose(modelLocalMatrix * world);
		}
	}
}

void Object3d::Draw()
{
	if (!isAnimationVisible_ || !model_) {
		return;
	}

	const Matrix4x4& world = transformationMatData_->world;
	DrawItem item;
	item.transform = transformationMatResource_->GetGPUVirtualAddress();
	item.sortPosition = { world.m[3][0], world.m[3][1], world.m[3][2] };
	item.sortingOrder = sortingOrder_;
	item.transparent = isTransparent_;
	item.castShadow = castShadow_;
	item.maskId = maskId_;
	model_->Draw(item, world);
}

void Object3d::SetMaterial(size_t meshIndex, std::shared_ptr<Material> material)
{
	if (model_) {
		model_->SetMaterial(meshIndex, std::move(material));
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

  // 新しいモデルには上乗せの色がまだ書かれていないので、次の Update で元の色から掛け直させる
  animColor_ = Color::kWhite;
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
  animColor_ = Color::kWhite;
}

void Object3d::SetMaterialColor(const Vector4& color)
{
  if (model_)
  {
    baseColor_ = color;
    model_->SetMaterialColor(Multiply(color, animColor_));
  }
}

Vector4 Object3d::GetMaterialColor() const
{
  if (animColor_ != Color::kWhite)
  {
    return baseColor_;
  }
  if (model_)
  {
    return model_->GetMaterialColor();
  }
  return Color::kWhite;
}

void Object3d::ApplyAnimationColor(const Vector4& color)
{
  if (!model_ || color == animColor_)
  {
    return;
  }
  // 上乗せを始める時点の色を元の色として覚える
  if (animColor_ == Color::kWhite)
  {
    baseColor_ = model_->GetMaterialColor();
  }
  animColor_ = color;
  model_->SetMaterialColor(Multiply(baseColor_, animColor_));
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

void Object3d::SetTexture(const std::string& fileName)
{
  if (model_)
  {
    model_->SetTexture(fileName);
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
	transformationMatResource_ = Renderer::GetInstance()->GetDX12Basic()->MakeBufferResource(sizeof(TransformationMatrix));

	// 座標変換行列リソースをマップ
	transformationMatResource_->Map(0, nullptr, reinterpret_cast<void**>(&transformationMatData_));

	// 座標変換行列データの初期値を書き込む
	transformationMatData_->world = Mat4x4::MakeIdentity();
	transformationMatData_->worldInvTranspose = Mat4x4::MakeIdentity();
}

} // namespace Tako
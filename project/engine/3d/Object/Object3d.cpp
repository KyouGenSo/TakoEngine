#include "Object3d.h"
#include "DX12Basic.h"
#include "Renderer.h"
#include "Model.h"
#include"ModelManager.h"
#include "SrvManager.h"
#include "ShadowRenderer.h"
#include "Logger.h"
#include "ColorFunc.h"
#include "MaterialLibrary.h"
#include "MaterialPropertyLayout.h"
#include "ObjectSettings.h"
#include "ShaderBindingLayout.h"
#include "ShaderLibrary.h"

#include <cstring>
#include <numbers>

namespace Tako {

namespace {

constexpr float kDegToRad = std::numbers::pi_v<float> / 180.0f;

Vector4 Multiply(const Vector4& a, const Vector4& b) { return { a.x * b.x, a.y * b.y, a.z * b.z, a.w * b.w }; }

// コードの上書きも ObjectSettings も無い描画フラグの値
const DrawItem kDefaultItem{};

const ResolvedMaterial kNoSettings{};

/// <summary>
/// コード ＞ ObjectSettings ＞ 既定 の順に、ある中で一番上の値
/// </summary>
template<class T>
T Pick(const std::optional<T>& code, const ObjectOverrides* settings, std::optional<T> ObjectOverrides::* field, T fallback)
{
  if (code) {
    return *code;
  }
  return settings && settings->*field ? *(settings->*field) : fallback;
}

/// <summary>
/// マテリアルの "color"（無ければ白）
/// </summary>
Vector4 ReadColor(const Material& material)
{
  Vector4 color = Color::kWhite;
  const GraphicsPipelineDesc* forward = material.GetShader()->GetPass(ShaderPass::Forward);
  if (!forward || !forward->ps) {
    return color;
  }
  const std::span<const uint8_t> constants = material.GetConstantBytes();
  const MaterialProperty*        property  = MaterialPropertyLayout::ForShader(*forward->ps)->Find(MakeShaderParamId("color"));
  if (property && property->type == MaterialProperty::Type::Float) {
    const size_t size = (std::min)(property->components, 4u) * sizeof(float);
    if (property->offset + size <= constants.size()) {
      std::memcpy(&color, constants.data() + property->offset, size);
    }
  }
  return color;
}

#ifdef _DEBUG
std::vector<Object3d*>& LiveObjects()
{
  static std::vector<Object3d*> objects;
  return objects;
}
#endif

} // namespace

Object3d::Object3d()
{
#ifdef _DEBUG
  LiveObjects().push_back(this);
#endif
}

Object3d::~Object3d()
{
#ifdef _DEBUG
  std::erase(LiveObjects(), this);
#endif
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
	const ObjectOverrides* settings = FindSettings();
	if (!isAnimationVisible_ || !model_ || (settings && !settings->visible.value_or(true))) {
		return;
	}
	if (slotsDirty_ || builtSettingsVersion_ != ObjectSettings::GetInstance()->GetVersion() || builtPropertiesVersion_ != code_.properties.GetVersion()) {
		RebuildDrawSlots(settings);
	}

	const Matrix4x4& world = transformationMatData_->world;
	DrawItem item;
	item.transform = transformationMatResource_->GetGPUVirtualAddress();
	item.sortPosition = { world.m[3][0], world.m[3][1], world.m[3][2] };
	item.sortingOrder = Pick(code_.sortingOrder, settings, &ObjectOverrides::sortingOrder, kDefaultItem.sortingOrder);
	item.transparent = Pick(code_.transparent, settings, &ObjectOverrides::transparent, kDefaultItem.transparent);
	item.castShadow = Pick(code_.castShadow, settings, &ObjectOverrides::castShadow, kDefaultItem.castShadow);
	item.maskId = Pick(code_.maskId, settings, &ObjectOverrides::maskId, kDefaultItem.maskId);
	item.nameId = nameId_;
	model_->Draw(item, world, drawSlots_);
}

void Object3d::SetName(const std::string& name)
{
	name_ = name;
	nameId_ = name.empty() ? 0 : MakeShaderParamId(name);
	slotsDirty_ = true;
}

const ObjectOverrides* Object3d::FindSettings() const
{
	return name_.empty() ? nullptr : ObjectSettings::GetInstance()->Find(name_);
}

const Material& Object3d::GetSlotMaterial(size_t meshIndex, const ObjectOverrides* settings) const
{
	if (meshIndex < code_.materials.size() && code_.materials[meshIndex]) {
		return *code_.materials[meshIndex];
	}
	const Mesh*             mesh     = model_->GetMesh(meshIndex);
	const ResolvedMaterial& resolved = settings ? settings->Resolve(mesh->GetName()) : kNoSettings;
	return resolved.material ? *resolved.material : mesh->GetMaterial();
}

void Object3d::RebuildDrawSlots(const ObjectOverrides* settings)
{
	// 登録済みの DrawItem が同じフレームの間 slotProperties_ の要素を指すので、大きさが変わらなければ確保し直さない
	const size_t meshCount = model_->GetMeshCount();
	drawSlots_.resize(meshCount);
	slotProperties_.resize(meshCount);
	slotMaterials_.resize(meshCount);
	for (size_t i = 0; i < meshCount; ++i) {
		const Mesh*             mesh     = model_->GetMesh(i);
		const ResolvedMaterial& resolved = settings ? settings->Resolve(mesh->GetName()) : kNoSettings;

		// どのマテリアルで描くか：コード ＞ ObjectSettings ＞ モデルの既定
		slotMaterials_[i] = i < code_.materials.size() && code_.materials[i] ? code_.materials[i] : resolved.material;
		const Material& material = slotMaterials_[i] ? *slotMaterials_[i] : mesh->GetMaterial();

		// 値は ObjectSettings の上にコードを重ね、アニメーションの上乗せの色を最後に掛ける
		MaterialPropertyBlock& properties = slotProperties_[i];
		properties.Clear();
		properties.Merge(resolved.properties);
		properties.Merge(code_.properties);
		if (animColor_ != Color::kWhite) {
			properties.SetVector("color", Multiply(properties.GetVector("color").value_or(ReadColor(material)), animColor_));
		}

		bool visible = mesh->IsVisible();
		if (const auto code = code_.meshVisible.find(mesh->GetName()); code != code_.meshVisible.end()) {
			visible = code->second;
		}
		else if (settings) {
			if (const auto entry = settings->meshes.find(mesh->GetName()); entry != settings->meshes.end() && entry->second.visible) {
				visible = *entry->second.visible;
			}
		}

		drawSlots_[i] = { .material = &material, .properties = properties.IsEmpty() ? nullptr : &properties, .visible = visible };
	}
	slotsDirty_ = false;
	builtSettingsVersion_ = ObjectSettings::GetInstance()->GetVersion();
	builtPropertiesVersion_ = code_.properties.GetVersion();
}

void Object3d::SetMaterial(size_t meshIndex, std::shared_ptr<Material> material)
{
	if (code_.materials.size() <= meshIndex) {
		code_.materials.resize(meshIndex + 1);
	}
	code_.materials[meshIndex] = std::move(material);
	slotsDirty_ = true;
}

bool Object3d::IsTransparent() const
{
	return Pick(code_.transparent, FindSettings(), &ObjectOverrides::transparent, kDefaultItem.transparent);
}

bool Object3d::CastsShadow() const
{
	return Pick(code_.castShadow, FindSettings(), &ObjectOverrides::castShadow, kDefaultItem.castShadow);
}

int32_t Object3d::GetSortingOrder() const
{
	return Pick(code_.sortingOrder, FindSettings(), &ObjectOverrides::sortingOrder, kDefaultItem.sortingOrder);
}

uint32_t Object3d::GetMaskId() const
{
	return Pick(code_.maskId, FindSettings(), &ObjectOverrides::maskId, kDefaultItem.maskId);
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
  modelName_ = fileName;
  slotsDirty_ = true;
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
  modelName_.clear();
  slotsDirty_ = true;
}

#ifdef _DEBUG
const std::vector<Object3d*>& Object3d::GetLiveObjects()
{
  return LiveObjects();
}
#endif

void Object3d::SetMaterialColor(const std::optional<Vector4>& color)
{
  if (color) {
    code_.properties.SetVector("color", *color);
  }
  else {
    code_.properties.Clear("color");
  }
}

Vector4 Object3d::GetMaterialColor() const
{
  if (const std::optional<Vector4> color = code_.properties.GetVector("color")) {
    return *color;
  }
  return GetBaseMaterialColor();
}

Vector4 Object3d::GetBaseMaterialColor() const
{
  if (!model_ || model_->GetMeshCount() == 0) {
    return Color::kWhite;
  }
  const ObjectOverrides*  settings = FindSettings();
  const ResolvedMaterial& resolved = settings ? settings->Resolve(model_->GetMesh(0)->GetName()) : kNoSettings;
  return resolved.properties.GetVector("color").value_or(ReadColor(GetSlotMaterial(0, settings)));
}

void Object3d::ApplyAnimationColor(const Vector4& color)
{
  if (color != animColor_) {
    animColor_ = color;
    slotsDirty_ = true;
  }
}

void Object3d::SetUvTransform(const std::optional<Transform>& uvTransform)
{
  if (uvTransform) {
    code_.properties.SetMatrix("uvTransform", Mat4x4::MakeAffine(uvTransform->scale, uvTransform->rotate, uvTransform->translate));
  }
  else {
    code_.properties.Clear("uvTransform");
  }
}

void Object3d::SetShininess(std::optional<float> shininess)
{
  if (shininess) {
    code_.properties.SetFloat("shininess", *shininess);
  }
  else {
    code_.properties.Clear("shininess");
  }
}

void Object3d::SetEnableLighting(std::optional<bool> enableLighting)
{
  if (enableLighting) {
    code_.properties.SetInt("enableLighting", *enableLighting ? 1 : 0);
  }
  else {
    code_.properties.Clear("enableLighting");
  }
}

void Object3d::SetEnableHighlight(std::optional<bool> enableHighlight)
{
  if (enableHighlight) {
    code_.properties.SetInt("enableHighlight", *enableHighlight ? 1 : 0);
  }
  else {
    code_.properties.Clear("enableHighlight");
  }
}

void Object3d::SetEnableEnvMap(std::optional<bool> enableEnvMap)
{
  if (enableEnvMap) {
    code_.properties.SetInt("enableEnvMap", *enableEnvMap ? 1 : 0);
  }
  else {
    code_.properties.Clear("enableEnvMap");
  }
}

void Object3d::SetEnvironmentTexture(uint32_t textureIndex)
{
  if (textureIndex != 0) {
    code_.properties.SetTexture("gEnvironmentMap", textureIndex);
  }
  else {
    code_.properties.Clear("gEnvironmentMap");
  }
}

void Object3d::SetEnvMapCoefficient(std::optional<float> coefficient)
{
  if (coefficient) {
    code_.properties.SetFloat("envMapCoefficient", *coefficient);
  }
  else {
    code_.properties.Clear("envMapCoefficient");
  }
}

void Object3d::SetTexture(const std::string& fileName)
{
  if (!fileName.empty()) {
    code_.properties.SetTexture("gTexture", MaterialLibrary::ResolveTexture(fileName));
  }
  else {
    code_.properties.Clear("gTexture");
  }
}

void Object3d::SetMeshVisible(const std::string& name, std::optional<bool> visible)
{
  if (visible) {
    code_.meshVisible.insert_or_assign(name, *visible);
  }
  else {
    code_.meshVisible.erase(name);
  }
  slotsDirty_ = true;
}

bool Object3d::IsMeshVisible(const std::string& name) const
{
  if (const auto code = code_.meshVisible.find(name); code != code_.meshVisible.end()) {
    return code->second;
  }
  if (const ObjectOverrides* settings = FindSettings()) {
    if (const auto entry = settings->meshes.find(name); entry != settings->meshes.end() && entry->second.visible) {
      return *entry->second.visible;
    }
  }
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
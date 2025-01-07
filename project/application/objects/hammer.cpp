#include "hammer.h"
#include "enemy.h"

#ifdef _DEBUG
#include "ImGuiManager.h"
#endif

void Hammer::Initialize(Object3d* model, Object3d* effectModel) {
	audio_ = Audio::GetInstance();

	// SEの読み込み
	seHit_ = audio_->LoadWaveFile("hammerHit.wav");

	model_ = model;

	effectModel_ = effectModel;
	effectModel_->SetAlpha(0.5f);

	Collider::Initialize();

	SetRadius(3.5f);
	Collider::SetRadius(3.5f);

	//hammerWorldTransform_.Initialize();
	//effectWorldTransform_.Initialize();

	hammerWorldTransform_.translate = Vector3(0.0f, 0.8f, 0.0f);
	effectWorldTransform_.scale = Vector3(0.0f, 0.0f, 0.0f);

	Collider::SetTypeID(static_cast<uint32_t>(CollisionTypeId::kPlayerWeapon));
}

void Hammer::Update() {
	if (isHit_) {
		effectWorldTransform_.scale.x += 0.9f;
		effectWorldTransform_.scale.y += 0.9f;
		effectWorldTransform_.scale.z += 0.9f;

		if (effectWorldTransform_.scale.x >= 8.5f) {
			effectWorldTransform_.scale = Vector3(0.0f, 0.0f, 0.0f);
			isHit_ = false;
		}
	}


	hammerWorldTransform_.Update();

	model_->SetTransform(hammerWorldTransform_);
	effectModel_->SetTransform(effectWorldTransform_);

	model_->Update();
	effectModel_->Update();

	//hammerWorldTransform_.UpdateMatrix();
	//effectWorldTransform_.UpdateMatrix();

	//ImGuiDraw();
}

void Hammer::Draw() {
	model_->Draw();

	if (isHit_) {
		effectModel_->Draw();
	}
}

void Hammer::ImGuiDraw() {
#ifdef _DEBUG
	ImGui::Begin("Hammer");

	ImGui::DragFloat3("Position", &hammerWorldTransform_.translate.x, 0.1f);
	ImGui::DragFloat3("Rotation", &hammerWorldTransform_.rotate.x, 0.1f);
	ImGui::DragFloat3("Scale", &hammerWorldTransform_.scale.x, 0.1f);

	ImGui::End();
#endif _DEBUG
}

void Hammer::OnCollision([[maybe_unused]] Collider* other) {
	// 衝突相手の種別IDを取得
	uint32_t typeID = other->GetTypeID();

	// 衝突相手が敵である場合
	if (typeID == static_cast<uint32_t>(CollisionTypeId::kEnemy) && enable_) {
		// 衝突相手を敵クラスにダウンキャスト
		Enemy* enemy = static_cast<Enemy*>(other);
		uint32_t serialNum = enemy->GetSerialNumber();

		// すでに衝突している敵である場合は処理を終了
		if (collisionRecord_.CheckRecord(serialNum)) {
			return;
		}

		// 衝突した敵のシリアルナンバーを記録
		collisionRecord_.AddRecord(serialNum);

		// 敵の位置にeffectを表示
		effectWorldTransform_.translate = enemy->GetCenter();
		isHit_ = true;

		// 衝突した敵にダメージを与える
		enemy->Damage(damege_);

		// 敵をヒットストップさせる
		enemy->HitStop(10);

		// SEを再生
		audio_->PlayWave(seHit_);
	}
}

Vector3 Hammer::GetCenter() const {
	Vector3 offset = {0.0f, 5.0f, 0.0f};
	Matrix4x4 worldMatrix = Mat4x4::MakeAffine(hammerWorldTransform_.scale, hammerWorldTransform_.rotate, hammerWorldTransform_.translate);
	if (hammerWorldTransform_.HasParent()) {
		worldMatrix = worldMatrix * hammerWorldTransform_.parentWorldMatrix_;
	}
	Vector3 worldPos = Mat4x4::TransForm(worldMatrix, offset);

	return worldPos;
}
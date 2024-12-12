#include "hammer.h"
#include "ImGuiManager.h"
#include "enemy.h"

void Hammer::Initialize(Model* model, Model* effectModel) {
	audio_ = Audio::GetInstance();

	// SEの読み込み
	seHit_ = audio_->LoadWave("hammerHit.wav");

	model_ = model;

	effectModel_ = effectModel;
	effectModel_->SetAlpha(0.5f);

	Collider::Initialize();

	SetRadius(3.5f);

	hammerWorldTransform_.Initialize();
	effectWorldTransform_.Initialize();

	hammerWorldTransform_.translation_ = Vector3(0.0f, 0.8f, 0.0f);
	effectWorldTransform_.scale_ = Vector3(0.0f, 0.0f, 0.0f);

	Collider::SetTypeID(static_cast<uint32_t>(CollisionTypeId::kPlayerWeapon));
}

void Hammer::Update() {
	if (isHit_) {
		effectWorldTransform_.scale_.x += 0.9f;
		effectWorldTransform_.scale_.y += 0.9f;
		effectWorldTransform_.scale_.z += 0.9f;

		if (effectWorldTransform_.scale_.x >= 8.5f) {
			effectWorldTransform_.scale_ = Vector3(0.0f, 0.0f, 0.0f);
			isHit_ = false;
		}
	}

	hammerWorldTransform_.UpdateMatrix();
	effectWorldTransform_.UpdateMatrix();

	//ImGuiDraw();
}

void Hammer::Draw(const ViewProjection& viewProjection) {
	model_->Draw(hammerWorldTransform_, viewProjection);

	if (isHit_) {
		effectModel_->Draw(effectWorldTransform_, viewProjection);
	}
}

void Hammer::ImGuiDraw() {
#ifdef _DEBUG
	ImGui::Begin("Hammer");

	ImGui::DragFloat3("Position", &hammerWorldTransform_.translation_.x, 0.1f);
	ImGui::DragFloat3("Rotation", &hammerWorldTransform_.rotation_.x, 0.1f);
	ImGui::DragFloat3("Scale", &hammerWorldTransform_.scale_.x, 0.1f);

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
		effectWorldTransform_.translation_ = enemy->GetCenter();
		isHit_ = true;

		// 衝突した敵にダメージを与える
		enemy->Damage(5.0f);

		// 敵をヒットストップさせる
		enemy->HitStop(10);

		// SEを再生
		audio_->PlayWave(seHit_);
	}
}

Vector3 Hammer::GetCenter() const {
	Vector3 offset = {0.0f, 5.0f, 0.0f};
	Vector3 worldPos = TransForm(hammerWorldTransform_.matWorld_, offset);

	return worldPos;
}
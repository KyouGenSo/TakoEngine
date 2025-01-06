#include "enemyBlock.h"
#include "Player.h"
#include "followCamera.h"

EnemyBlock::EnemyBlock() {}

EnemyBlock::~EnemyBlock() {

	delete model_;
}

void EnemyBlock::Initialize(Object3d* model, const Vector3& position, const Vector3& scale, const Vector3& offset) {
	assert(model);
	model_ = model;

	audio_ = Audio::GetInstance();

	// SEの読み込み
	seHandle_ = audio_->LoadWaveFile("playerDamaged.wav");

	Collider::Initialize();

	SetRadius(worldTransform_.scale.y);

	//worldTransform_.Initialize();

	worldTransform_.translate = position;
	worldTransform_.scale = scale;
	worldTransform_.rotate = {0.0f, 0.0f, 0.0f};

	offset_ = offset;

	Collider::SetTypeID(static_cast<uint32_t>(CollisionTypeId::kEnemyAttack));
}

void EnemyBlock::Update() {
	if (--deathTimer_ <= 0) {
		isDead_ = true;
	}

	SetRadius(worldTransform_.scale.x);

	//worldTransform_.UpdateMatrix();

	worldTransform_.Update();
	model_->SetTransform(worldTransform_);
	model_->Update();
}

void EnemyBlock::Draw() {
	model_->Draw();
}

void EnemyBlock::OnCollision([[maybe_unused]] Collider* other) {
	// 衝突相手の種別IDを取得
	uint32_t typeID = other->GetTypeID();

	// 衝突相手が Player の場合
	if (typeID == static_cast<uint32_t>(CollisionTypeId::kPlayer)) {
		// 衝突相手をPlayerクラスにダウンキャスト
		Player* player = static_cast<Player*>(other);
		uint32_t serialNum = player->GetSerialNumber();

		// すでに衝突している敵である場合は処理を終了
		if (collisionRecord_.CheckRecord(serialNum)) {
			return;
		}

		// 衝突した敵のシリアルナンバーを記録
		collisionRecord_.AddRecord(serialNum);

		isHit_ = true;

		// プレイヤーにダメージを与える
		if (!isHited_) {
			player->Damage(damage_);
			audio_->PlayWave(seHandle_);
		}

		if (hitOnce_)
		isHited_ = true;

		if (isColliVanish_) {
			isDead_ = true;
		}

		ClearCollisionRecord();
	}
}

Vector3 EnemyBlock::GetCenter() const { 
	Vector3 offset = { 0.0f, 0.0f, 0.0f };
	Matrix4x4 worlfMat = Mat4x4::MakeAffine(worldTransform_.scale, worldTransform_.rotate, worldTransform_.translate);
	Vector3 worldPosition = Mat4x4::TransForm(worlfMat, offset);

	return worldPosition;
}

Vector3 EnemyBlock::GetWorldPosition() { 
	Vector3 worldPos;

	worldPos.x = worldTransform_.translate.x;
	worldPos.y = worldTransform_.translate.y;
	worldPos.z = worldTransform_.translate.z;

	return worldPos;
}
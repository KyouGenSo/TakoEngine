#include "enemyBlock.h"
#include "Player.h"
#include "followCamera.h"

EnemyBlock::EnemyBlock() {}

EnemyBlock::~EnemyBlock() {}

void EnemyBlock::Initialize(Model* model, const Vector3& position, const Vector3& scale, const Vector3& offset) {
	assert(model);
	model_ = model;

	audio_ = Audio::GetInstance();

	// SEの読み込み
	seHandle_ = audio_->LoadWave("playerDamaged.wav");

	Collider::Initialize();

	SetRadius(worldTransform_.scale_.y);

	worldTransform_.Initialize();
	worldTransform_.translation_ = position;
	worldTransform_.scale_ = scale;
	worldTransform_.rotation_ = {0.0f, 0.0f, 0.0f};

	offset_ = offset;

	Collider::SetTypeID(static_cast<uint32_t>(CollisionTypeId::kEnemyAttack));
}

void EnemyBlock::Update() {
	if (--deathTimer_ <= 0) {
		isDead_ = true;
	}

	SetRadius(worldTransform_.scale_.x);

	worldTransform_.UpdateMatrix();
}

void EnemyBlock::Draw(const ViewProjection& viewProjection) {
	model_->Draw(worldTransform_, viewProjection);
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
	Vector3 worldPosition = TransForm(worldTransform_.matWorld_, offset);

	return worldPosition;
}

Vector3 EnemyBlock::GetWorldPosition() { 
	Vector3 worldPos;

	worldPos.x = worldTransform_.matWorld_.m[3][0];
	worldPos.y = worldTransform_.matWorld_.m[3][1];
	worldPos.z = worldTransform_.matWorld_.m[3][2];

	return worldPos;
}
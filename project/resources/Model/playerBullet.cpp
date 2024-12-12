#include "playerBullet.h"
#include "enemy.h"

// global serial number
uint32_t PlayerBullet::nextSerialNumber_ = 0;

PlayerBullet::PlayerBullet() {}

PlayerBullet::~PlayerBullet() {
	serialNumber_ = nextSerialNumber_;
	nextSerialNumber_++;
}

Vector3 PlayerBullet::GetCenter() const {
	Vector3 offset = {-0.8f, 0.0f, 1.5f};
	Vector3 worldPos = TransForm(worldTransform_.matWorld_, offset);

	return worldPos;
}

void PlayerBullet::Initialize(Model* model, const Vector3& position, const Vector3& velocity) {
	audio_ = Audio::GetInstance();

	// SEの読み込み
	seHit_ = audio_->LoadWave("playerBulletHit.wav");

	Collider::Initialize();

	model_ = model;

	worldTransform_.Initialize();



	worldTransform_.rotation_.y = std::atan2(velocity.x, velocity.z);

	// 解法1
	Matrix4x4 thetaYRotationMatrix = MakeRotateMatrixY(std::atan2(velocity.y, velocity.z));
	Vector3 velocityZ = TransForm(thetaYRotationMatrix, velocity);
	worldTransform_.rotation_.x = std::atan2(-velocityZ.y, velocityZ.z);

	// offset
	Vector3 offset = {-0.8f, 0.0f, 1.5f};

	Matrix4x4 yRotMat = MakeRotateMatrixY(worldTransform_.rotation_.y);
	Matrix4x4 xRotMat = MakeRotateMatrixX(worldTransform_.rotation_.x);

	offset = TransForm(yRotMat, offset);
	offset = TransForm(xRotMat, offset);

	worldTransform_.translation_ = offset + position;

	worldTransform_.scale_ = Vector3(0.f, 0.f, 0.f);

	worldTransform_.UpdateMatrix();

	velocity_ = velocity;

	// 衝突判定の種別IDを設定
	Collider::SetTypeID(static_cast<uint32_t>(CollisionTypeId::kPlayerBullet));
}

void PlayerBullet::Update() {
	if (--deathTimer_ <= 0) {
		isDead_ = true;
	}

	float speed = 1.2f;

	Vector3 toEnemy = enemy_->GetCenter() - GetCenter();

	velocity_ = Slerp(velocity_.normalize(), toEnemy.normalize(), t_) * speed;

	if (worldTransform_.scale_.x >= 1.0f) {
		worldTransform_.translation_ += velocity_;

		Matrix4x4 yRotMat = MakeRotateMatrixY(atan2f(velocity_.x, velocity_.z));
		Vector3 velocityZ = TransForm(yRotMat, velocity_);
		worldTransform_.rotation_.x = atan2f(-velocityZ.y, velocityZ.z);
		worldTransform_.rotation_.y = atan2f(velocityZ.x, velocityZ.z);
	} else {
		worldTransform_.scale_ += Vector3(0.1f, 0.1f, 0.1f);
	}

	worldTransform_.UpdateMatrix();
}

void PlayerBullet::Draw(const ViewProjection& viewProjection) { model_->Draw(worldTransform_, viewProjection); }

void PlayerBullet::OnCollision([[maybe_unused]] Collider* other) {

	// 衝突相手の種別IDを取得
	uint32_t typeID = other->GetTypeID();

	// 衝突相手が敵である場合
	if (typeID == static_cast<uint32_t>(CollisionTypeId::kEnemy)) {
		// 衝突相手を敵クラスにダウンキャスト
		Enemy* enemy = static_cast<Enemy*>(other);
		uint32_t serialNum = enemy->GetSerialNumber();

		// すでに衝突している敵である場合は処理を終了
		if (collisionRecord_.CheckRecord(serialNum)) {
			return;
		}

		// 衝突した敵のシリアルナンバーを記録
		collisionRecord_.AddRecord(serialNum);

		// 敵にダメージを与える
		enemy->Damage(0.2f);

		// SEを再生
		audio_->PlayWave(seHit_);

		isDead_ = true;
	}
}
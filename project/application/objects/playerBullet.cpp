#include "playerBullet.h"
#include "enemy.h"

// global serial number
uint32_t PlayerBullet::nextSerialNumber_ = 0;

PlayerBullet::PlayerBullet() {}

PlayerBullet::~PlayerBullet() {
	serialNumber_ = nextSerialNumber_;
	nextSerialNumber_++;

	delete model_;
}

Vector3 PlayerBullet::GetCenter() const {
	Vector3 offset = {-0.8f, 0.0f, 1.5f};
	Matrix4x4 wordMat = Mat4x4::MakeAffine(worldTransform_.scale, worldTransform_.rotate, worldTransform_.translate);
	Vector3 worldPos = Mat4x4::TransForm(wordMat, offset);

	return worldPos;
}

void PlayerBullet::Initialize(Object3d* model, const Vector3& position, const Vector3& velocity) {
	audio_ = Audio::GetInstance();

	// SEの読み込み
	seHit_ = audio_->LoadWaveFile("playerBulletHit.wav");

	Collider::Initialize();

	model_ = model;

	worldTransform_.rotate.y = std::atan2(velocity.x, velocity.z);

	// 解法1
	Matrix4x4 thetaYRotationMatrix = Mat4x4::MakeRotateY(std::atan2(velocity.y, velocity.z));
	Vector3 velocityZ = Mat4x4::TransForm(thetaYRotationMatrix, velocity);
	worldTransform_.rotate.x = std::atan2(-velocityZ.y, velocityZ.z);

	// offset
	Vector3 offset = {-0.8f, 0.0f, 1.5f};

	Matrix4x4 yRotMat = Mat4x4::MakeRotateY(worldTransform_.rotate.y);
	Matrix4x4 xRotMat = Mat4x4::MakeRotateX(worldTransform_.rotate.x);

	offset = Mat4x4::TransForm(yRotMat, offset);
	offset = Mat4x4::TransForm(xRotMat, offset);

	worldTransform_.translate = offset + position;

	worldTransform_.scale = Vector3(0.f, 0.f, 0.f);

	//worldTransform_.UpdateMatrix();

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

	velocity_ = Vec3::Slerp(velocity_.normalize(), toEnemy.normalize(), t_) * speed;

	if (worldTransform_.scale.x >= 1.0f) {
		worldTransform_.translate += velocity_;

		Matrix4x4 yRotMat = Mat4x4::MakeRotateY(atan2f(velocity_.x, velocity_.z));
		Vector3 velocityZ = Mat4x4::TransForm(yRotMat, velocity_);
		worldTransform_.rotate.x = atan2f(-velocityZ.y, velocityZ.z);
		worldTransform_.rotate.y = atan2f(velocityZ.x, velocityZ.z);
	} else {
		worldTransform_.scale += Vector3(0.1f, 0.1f, 0.1f);
	}

	model_->SetTransform(worldTransform_);
	model_->Update();
}

void PlayerBullet::Draw() { model_->Draw(); }

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
#include "Enemy.h"
#include "ImGuiManager.h"
#include "Player.h"
#include "followCamera.h"
#include "hammer.h"

// global serial number
uint32_t Enemy::nextSerialNumber_ = 0;

Enemy::Enemy() {}

Enemy::~Enemy() {
	serialNumber_ = nextSerialNumber_;
	nextSerialNumber_++;
}

void Enemy::ReSet() {
	hp_ = 100.0f;
	isHitStop_ = false;
	hitStopTime_ = 0;
	behavior_ = Behavior::kRoot;
	behaviorRequest_ = std::nullopt;
	behaviorCD_ = 0;
	isDamegeOn_ = false;
	worldTransformBody_.translation_ = offset_;

	BehaviorNearInitialize();
	BehaviorAwayInitialize();
	BehaviorFarAttack1Initialize();
	BehaviorFarAttack2Initialize();
	BehaviorFarAttack3Initialize();
	BehaviorNearAttack1Initialize();
	BehaviorNearAttack2Initialize();
	BehaviorNearAttack3Initialize();
	BehaviorRootInitialize();

}

Vector3 Enemy::GetCenter() const {
	Vector3 offset = offset_;
	Vector3 worldPos = TransForm(worldTransform_.matWorld_, offset);

	return worldPos;
}

void Enemy::PosRange() {
	Vector3 range = {500.0f, 500.0f, 500.0f};

	// 範囲を超えたら逆方向に移動
	if (worldTransform_.translation_.x > range.x) {
		worldTransform_.translation_.x -= 1.0f;
	} else if (worldTransform_.translation_.x < -range.x) {
		worldTransform_.translation_.x += 1.0f;
	}

	if (worldTransform_.translation_.y > range.y) {
		worldTransform_.translation_.y -= 1.0f;
	} else if (worldTransform_.translation_.y < 0.0f) {
		worldTransform_.translation_.y += 1.0f;
	}

	if (worldTransform_.translation_.z > range.z) {
		worldTransform_.translation_.z -= 1.0f;
	} else if (worldTransform_.translation_.z < -range.z) {
		worldTransform_.translation_.z += 1.0f;
	}
}

void Enemy::Damage(float damage) {
	if (hp_ > 0.0f) {
		hp_ -= damage;
	}
	if (hp_ <= 0.0f) {
		hp_ = 0.0f;
	}
}

void Enemy::CreateBlock(const Vector3& position, const Vector3& scale, const Vector3& offset) {
	EnemyBlock* block = new EnemyBlock();
	block->Initialize(models_[1], position, scale, offset);
	blocks_.push_back(block);
}

void Enemy::Initialize(const std::vector<Model*> models) {
	audio_ = Audio::GetInstance();

	// SEの読み込み
	seHitPlayer_ = audio_->LoadWave("playerDamaged.wav");

	Collider::SetRadius(5.f);

	BaseCharacter::Initialize(models);

	// ワールド変換データの初期化
	worldTransformBody_.Initialize();

	// ワールド変換データの初期設定
	worldTransformBody_.translation_ = offset_;

	// モデル同士の親子関係を設定
	worldTransformBody_.SetParent(&worldTransform_);

	Collider::SetTypeID(static_cast<uint32_t>(CollisionTypeId::kEnemy));

	rand_ = Rand(0.0f, 3.0f);
	randIndex_ = int(std::floor(rand_));

	// AttackRecordの初期化
	attackRecord_.farAttack1 = false;
	attackRecord_.farAttack2 = false;
	attackRecord_.farAttack3 = false;
	attackRecord_.nearAttack1 = false;
	attackRecord_.nearAttack2 = false;
	attackRecord_.nearAttack3 = false;
	attackRecord_.isNeared = false;
	attackRecord_.isAwayed = false;
}

void Enemy::Update() {

	// 死んだブロックを削除
	blocks_.remove_if([](EnemyBlock* block) {
		if (block->IsDead()) {
			delete block;
			return true;
		}
		return false;
	});

	if (hp_ <= 0) {
		hp_ = 0;
	}

	if (hitStopTime_ > 0) {
		hitStopTime_--;
		ShakeEffect();
	} else {
		worldTransformBody_.translation_.x = offset_.x;
		worldTransformBody_.translation_.z = offset_.z;
		isHitStop_ = false;
	}

	UpdateFloatAnimation();

	// playerとの距離を計算
	toPlayerV_ = player_->GetCenter() - GetCenter();
	toPlayerDis_ = toPlayerV_.length();

	// 行動遷移
	if (behaviorRequest_) {
		// behavior_を変更する
		behavior_ = behaviorRequest_.value();
		// 各行動の初期化処理
		switch (behavior_) {
		case Behavior::kRoot:
			BehaviorRootInitialize();
			break;
		case Behavior::kNear:
			BehaviorNearInitialize();
			break;
		case Behavior::kAway:
			BehaviorAwayInitialize();
			break;
		case Behavior::kFarAttack1:
			BehaviorFarAttack1Initialize();
			break;
		case Behavior::kFarAttack2:
			BehaviorFarAttack2Initialize();
			break;
		case Behavior::kFarAttack3:
			BehaviorFarAttack3Initialize();
			break;
		case Behavior::kNearAttack1:
			BehaviorNearAttack1Initialize();
			break;
		case Behavior::kNearAttack2:
			BehaviorNearAttack2Initialize();
			break;
		case Behavior::kNearAttack3:
			BehaviorNearAttack3Initialize();
			break;
		}
		// behaviorRequest_をリセット
		behaviorRequest_ = std::nullopt;
	}

	// 行動遷移の更新
	switch (behavior_) {

	case Behavior::kRoot:
		BehaviorRootUpdate();
		break;

	case Behavior::kNear:
		BehaviorNearUpdate();
		break;

	case Behavior::kAway:
		BehaviorAwayUpdate();
		break;
	case Behavior::kFarAttack1:
		BehaviorFarAttack1Update();
		break;

	case Behavior::kFarAttack2:
		BehaviorFarAttack2Update();
		break;
	case Behavior::kFarAttack3:
		BehaviorFarAttack3Update();
		break;

	case Behavior::kNearAttack1:
		BehaviorNearAttack1Update();
		break;

	case Behavior::kNearAttack2:
		BehaviorNearAttack2Update();
		break;

	case Behavior::kNearAttack3:
		BehaviorNearAttack3Update();
		break;
	}

	// blockの更新
	for (auto& block : blocks_) {
		block->Update();
	}

	// 範囲制限
	PosRange();

	BaseCharacter::Update();

	// 行列を更新
	worldTransform_.UpdateMatrix();
	worldTransformBody_.UpdateMatrix();

	if (behaviorCD_ > 0) {
		behaviorCD_--;
	}

	// ImGuiDraw();
}

void Enemy::Draw(const ViewProjection& viewProjection) {
	// body描画
	models_[0]->Draw(worldTransformBody_, viewProjection);

	// blockの描画
	for (auto& block : blocks_) {
		block->Draw(viewProjection);
	}
}

void Enemy::ImGuiDraw() {
#ifdef _DEBUG
	ImGui::Begin("Enemy");
	ImGui::DragFloat3("Body", &worldTransformBody_.translation_.x, 0.1f);
	ImGui::DragFloat("period", &period, 0.1f);
	ImGui::DragFloat("amplitude", &amplitude, 0.1f);
	// hp
	ImGui::Text("hp: %f", hp_);
	// isHitStop
	ImGui::Text("isHitStop: %d", isHitStop_);
	// hitStopTime
	ImGui::Text("hitStopTime: %d", hitStopTime_);
	// toPlayerDis
	ImGui::Text("toPlayerDis: %f", toPlayerDis_);
	// rand
	ImGui::Text("rand: %f", rand_);
	// randIndex
	ImGui::Text("randIndex: %d", randIndex_);
	// behavior
	ImGui::Text("behavior: %d", static_cast<int>(behavior_));
	// workFarAttack2_.toPlayer
	ImGui::Text("workFarAttack2_.toPlayer: %f, %f, %f", workFarAttack2_.toPlayer.x, workFarAttack2_.toPlayer.y, workFarAttack2_.toPlayer.z);
	// behaviorCD_
	ImGui::Text("behaviorCD_: %d", behaviorCD_);
	ImGui::End();
#endif _DEBUG
}

void Enemy::Move() {
	float speed = 0.1f;
	// y軸回転
	worldTransform_.rotation_.y += 0.01f;

	// 向いてる方向に進む
	worldTransform_.translation_.x += std::sin(worldTransform_.rotation_.y) * speed;
	worldTransform_.translation_.y += std::sin(worldTransform_.rotation_.x) * speed;
	worldTransform_.translation_.z += std::cos(worldTransform_.rotation_.y) * speed;

	// 行列を更新
	worldTransform_.UpdateMatrix();
}

void Enemy::InitializeFloatAnimation() { floatingParam_ = 0.0f; }

void Enemy::UpdateFloatAnimation() {
	// 1フレームでの加算量
	float add = float(2.0f * M_PI / period);

	floatingParam_ += add;
	// 2πを超えたら0に戻す
	floatingParam_ = float(std::fmod(floatingParam_, 2.0f * M_PI));

	worldTransformBody_.translation_.y += std::sin(floatingParam_) * amplitude;
}

void Enemy::ShakeEffect() {
	// ランダムな値を生成
	float random = Rand(-0.3f, 0.3f);

	worldTransformBody_.translation_.x += random;
	worldTransformBody_.translation_.z += random;

	worldTransformBody_.UpdateMatrix();
}

void Enemy::OnCollision([[maybe_unused]] Collider* other) {
	// 衝突相手の種別IDを取得
	uint32_t typeID = other->GetTypeID();

	// 衝突相手がhammerである場合
	if (typeID == static_cast<uint32_t>(CollisionTypeId::kPlayerWeapon)) {
		// 衝突相手をHammerクラスにダウンキャスト
		Hammer* hammer = static_cast<Hammer*>(other);

		// 衝突相手のシリアルナンバーを取得
		uint32_t serialNum = hammer->GetSerialNumber();

		// すでに衝突している敵である場合は処理を終了
		if (collisionRecord_.CheckRecord(serialNum)) {
			return;
		}

		// 衝突した敵のシリアルナンバーを記録
		collisionRecord_.AddRecord(serialNum);
	}

	// 衝突相手がplayerである場合
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

		// プレイヤーにダメージを与える
		if (isDamegeOn_) {
			player->Damage(30);
			followCamera_->ShakeScreen(0.5f);
			workFarAttack1_.isHit = true;
			audio_->PlayWave(seHitPlayer_);
		} else {
			ClearCollisionRecord();
		}
	}
}

// ----------------------行動遷移用---------------------
void Enemy::BehaviorRootInitialize() {
	rand_ = Rand(0.0f, 4.0f);
	randIndex_ = int(std::floor(rand_));

	if (randIndex_ > 3) {
		randIndex_ = 3;
	}

	// behaviorCD_ = 60 * 3;

	worldTransform_.rotation_.y = 0;

	followCamera_->ResetOffset();

	blocks_.clear();
}
void Enemy::BehaviorRootUpdate() {

	// 常にplayerに向かう
	worldTransform_.rotation_.y = std::atan2(toPlayerV_.x, toPlayerV_.z);

	if (toPlayerDis_ > 100.0f) {
		behaviorRequest_ = Behavior::kNear;
	} else if (toPlayerDis_ <= 5.5f) {
		behaviorRequest_ = Behavior::kAway;
	}

	if (behaviorCD_ <= 0) {

		// playerとの距離が近い場合
		if (toPlayerDis_ < 20.0f) {
			switch (randIndex_) {
			case 0:
				behaviorRequest_ = Behavior::kAway;
				break;
			case 1:
				if (!attackRecord_.nearAttack1) {
					attackRecord_.nearAttack1 = true;
					behaviorRequest_ = Behavior::kNearAttack1;
					break;
				}
				if (!attackRecord_.nearAttack2) {
					attackRecord_.nearAttack2 = true;
					behaviorRequest_ = Behavior::kNearAttack2;
					break;
				}
				if (!attackRecord_.nearAttack3) {
					attackRecord_.nearAttack3 = true;
					behaviorRequest_ = Behavior::kNearAttack3;
					break;
				}
				behaviorRequest_ = Behavior::kNearAttack1;
				break;
			case 2:
				if (!attackRecord_.nearAttack1) {
					attackRecord_.nearAttack1 = true;
					behaviorRequest_ = Behavior::kNearAttack1;
					break;
				}
				if (!attackRecord_.nearAttack2) {
					attackRecord_.nearAttack2 = true;
					behaviorRequest_ = Behavior::kNearAttack2;
					break;
				}
				if (!attackRecord_.nearAttack3) {
					attackRecord_.nearAttack3 = true;
					behaviorRequest_ = Behavior::kNearAttack3;
					break;
				}
				behaviorRequest_ = Behavior::kNearAttack2;
				break;
			case 3:
				if (!attackRecord_.nearAttack1) {
					attackRecord_.nearAttack1 = true;
					behaviorRequest_ = Behavior::kNearAttack1;
					break;
				}
				if (!attackRecord_.nearAttack2) {
					attackRecord_.nearAttack2 = true;
					behaviorRequest_ = Behavior::kNearAttack2;
					break;
				}
				if (!attackRecord_.nearAttack3) {
					attackRecord_.nearAttack3 = true;
					behaviorRequest_ = Behavior::kNearAttack3;
					break;
				}
				behaviorRequest_ = Behavior::kNearAttack3;
				break;
			}
		}

		// playerとの距離が遠い場合
		else {
			switch (randIndex_) {
			case 0:
				 behaviorRequest_ = Behavior::kNear;
				break;
			case 1:
				if (!attackRecord_.farAttack1) {
					attackRecord_.farAttack1 = true;
					behaviorRequest_ = Behavior::kFarAttack1;
					break;
				}
				if (!attackRecord_.farAttack2) {
					attackRecord_.farAttack2 = true;
					behaviorRequest_ = Behavior::kFarAttack2;
					break;
				}
				if (!attackRecord_.farAttack3) {
					attackRecord_.farAttack3 = true;
					behaviorRequest_ = Behavior::kFarAttack3;
					break;
				}
				if (!attackRecord_.isNeared) {
					attackRecord_.isNeared = true;
					behaviorRequest_ = Behavior::kNear;
					break;
				}
				behaviorRequest_ = Behavior::kFarAttack1;
				break;
			case 2:
				if (!attackRecord_.farAttack1) {
					attackRecord_.farAttack1 = true;
					behaviorRequest_ = Behavior::kFarAttack1;
					break;
				}
				if (!attackRecord_.farAttack2) {
					attackRecord_.farAttack2 = true;
					behaviorRequest_ = Behavior::kFarAttack2;
					break;
				}
				if (!attackRecord_.farAttack3) {
					attackRecord_.farAttack3 = true;
					behaviorRequest_ = Behavior::kFarAttack3;
					break;
				}
				if (!attackRecord_.isNeared) {
					attackRecord_.isNeared = true;
					behaviorRequest_ = Behavior::kNear;
					break;
				}
				behaviorRequest_ = Behavior::kFarAttack2;
				break;
			case 3:
				if (!attackRecord_.farAttack1) {
					attackRecord_.farAttack1 = true;
					behaviorRequest_ = Behavior::kFarAttack1;
					break;
				}
				if (!attackRecord_.farAttack2) {
					attackRecord_.farAttack2 = true;
					behaviorRequest_ = Behavior::kFarAttack2;
					break;
				}
				if (!attackRecord_.farAttack3) {
					attackRecord_.farAttack3 = true;
					behaviorRequest_ = Behavior::kFarAttack3;
					break;
				}
				if (!attackRecord_.isNeared) {
					attackRecord_.isNeared = true;
					behaviorRequest_ = Behavior::kNear;
					break;
				}
				behaviorRequest_ = Behavior::kFarAttack3;
				break;
			}
		}
	}
}
// ----------------------------------------------------Near
void Enemy::BehaviorNearInitialize() {
	// 行動遷移の初期化処理
	rand_ = Rand(0.0f, 4.0f);
	randIndex_ = int(std::floor(rand_));
}
void Enemy::BehaviorNearUpdate() {
	float speed = 0.5f;

	// 常にplayerに向かう
	worldTransform_.rotation_.y = std::atan2(toPlayerV_.x, toPlayerV_.z);

	if (toPlayerDis_ > 10.0f) {
		worldTransform_.translation_.x += toPlayerV_.normalize().x * speed;
		worldTransform_.translation_.z += toPlayerV_.normalize().z * speed;
	} else {
		behaviorCD_ = 60 * 0.5f;
		behaviorRequest_ = Behavior::kRoot;
	}
}
// ----------------------------------------------------Away
void Enemy::BehaviorAwayInitialize() {
	// 行動遷移の初期化処理
	rand_ = Rand(0.0f, 4.0f);
	randIndex_ = int(std::floor(rand_));
}
void Enemy::BehaviorAwayUpdate() {
	float speed = 0.9f;

	// 常にplayerに向かう
	worldTransform_.rotation_.y = std::atan2(toPlayerV_.x, toPlayerV_.z);

	// playerの逆方向に進む
	if (toPlayerDis_ < 50.0f && worldTransform_.translation_.x > -500.0f && worldTransform_.translation_.x < 500.0f && worldTransform_.translation_.z > -500.0f &&
	    worldTransform_.translation_.z < 500.0f) {

		worldTransform_.translation_.x -= toPlayerV_.normalize().x * speed;
		worldTransform_.translation_.z -= toPlayerV_.normalize().z * speed;

	} else {
		behaviorCD_ = 60 * 0.5;
		behaviorRequest_ = Behavior::kRoot;
	}

	worldTransform_.UpdateMatrix();
}
// ----------------------------------------------------Far1
void Enemy::BehaviorFarAttack1Initialize() {
	// 行動遷移の初期化処理
	rand_ = Rand(0.0f, 4.0f);
	randIndex_ = int(std::floor(rand_));

	ClearCollisionRecord();

	isDamegeOn_ = true;

	toPlayerVOnce_ = toPlayerV_;

	workFarAttack1_.speed = 5.0f;
	workFarAttack1_.sppedMin = 0.5f;
	workFarAttack1_.sppedDec = 0.1f;

	workFarAttack1_.rotationSpeed = 0.01f;
	workFarAttack1_.rotationSpeedMax = 0.25f;
	workFarAttack1_.rotationSpeedMin = 0.08f;
	workFarAttack1_.rotationSpeedInc = 0.0015f;
	workFarAttack1_.rotationSpeedDec = 0.01f;

	workFarAttack1_.maxDis = 150.0f;
	workFarAttack1_.distanceCount = 0.0f;

	workFarAttack1_.isAttack = false;
	workFarAttack1_.isHit = false;

	workFarAttack1_.shakeTime = 15.0f;
}
void Enemy::BehaviorFarAttack1Update() {
	// 回転しながらplayerに向かって進む

	if (workFarAttack1_.rotationSpeed <= workFarAttack1_.rotationSpeedMax && !workFarAttack1_.isAttack) {
		workFarAttack1_.rotationSpeed += workFarAttack1_.rotationSpeedInc;
		toPlayerVOnce_ = toPlayerV_;
	} else {
		workFarAttack1_.isAttack = true;
	}

	worldTransform_.rotation_.y += workFarAttack1_.rotationSpeed;

	if (workFarAttack1_.isAttack) {
		worldTransform_.translation_.x += toPlayerVOnce_.normalize().x * workFarAttack1_.speed;
		worldTransform_.translation_.z += toPlayerVOnce_.normalize().z * workFarAttack1_.speed;
		workFarAttack1_.distanceCount += workFarAttack1_.speed;

		if (workFarAttack1_.speed > workFarAttack1_.sppedMin) {
			workFarAttack1_.speed -= workFarAttack1_.sppedDec;
		}

		if (workFarAttack1_.rotationSpeed > workFarAttack1_.rotationSpeedMin) {
			workFarAttack1_.rotationSpeed -= workFarAttack1_.rotationSpeedDec;
		}
	}

	if (workFarAttack1_.distanceCount >= workFarAttack1_.maxDis) {
		isDamegeOn_ = false;
		workFarAttack1_.isAttack = false;
		behaviorCD_ = 60 * 1.f;
		behaviorRequest_ = Behavior::kRoot;
	}

	if (workFarAttack1_.isHit && workFarAttack1_.shakeTime > 0) {
		workFarAttack1_.shakeTime--;
		followCamera_->ShakeScreen(0.5f);
	} else {
		followCamera_->ResetOffset();
	}

	worldTransform_.UpdateMatrix();
}
// ----------------------------------------------------Far2
void Enemy::BehaviorFarAttack2Initialize() {
	// 行動遷移の初期化処理
	rand_ = Rand(0.0f, 4.0f);
	randIndex_ = int(std::floor(rand_));

	int blockNum = 3;

	Vector3 offset = {-3.0f, 5.0f, 0.0f};

	for (int i = 0; i < blockNum; i++) {

		if (i == 0) {
			offset = {0.0f, 6.0f, 0.0f};
		} else if (i == 1) {
			offset = {6.0f, 0.0f, 0.0f};
		} else if (i == 2) {
			offset = {-6.0f, 0.0f, 0.0f};
		}

		Vector3 position = GetCenter() + offset;
		Vector3 scale = {0.0f, 0.0f, 0.0f};

		CreateBlock(position, scale, offset);
	}

	workFarAttack2_.speed = 0.6f;

	workFarAttack2_.rotationSpeed = 0.001f;
	workFarAttack2_.rotationSpeedMax = 0.4f;
	workFarAttack2_.rotationSpeedInc = 0.001f;

	workFarAttack2_.scaleIncSpeed = 0.01f;
	workFarAttack2_.scaleMax = 1.0f;

	workFarAttack2_.isAttack = false;
	workFarAttack2_.isShot = false;

	workFarAttack2_.attackTime = 60 * 5;

	for (auto& block : blocks_) {
		block->SetRadius(1.0f);

		block->SetDeathTimer(60.0f * 4.0f);

		block->SetDamage(10.0f);

		block->SetColliVanish(true);

		workFarAttack2_.toPlayer = player_->GetCenter() - block->GetCenter();

		workFarAttack2_.velocity = workFarAttack2_.toPlayer.normalize() * workFarAttack2_.speed;
	}
}
void Enemy::BehaviorFarAttack2Update() {

	if (--workFarAttack2_.attackTime <= 0) {
		behaviorCD_ = 60 * 1.f;
		behaviorRequest_ = Behavior::kRoot;
	}

	// 常にplayerに向かう
	worldTransform_.rotation_.y = std::atan2(toPlayerV_.x, toPlayerV_.z);

	for (auto& block : blocks_) {

		if (block->IsHit()) {
			followCamera_->ShakeScreen(0.5f);
		}

		block->SetRotationZ(block->GetRotation().z + workFarAttack2_.rotationSpeed);
		block->SetRotationY(block->GetRotation().y + workFarAttack2_.rotationSpeed);

		if (workFarAttack2_.rotationSpeed <= workFarAttack2_.rotationSpeedMax && !workFarAttack2_.isAttack) {
			workFarAttack2_.rotationSpeed += workFarAttack2_.rotationSpeedInc;

		} else {
			workFarAttack2_.isAttack = true;
		}

		if (block->GetScale().x < workFarAttack2_.scaleMax) {
			block->SetScale(block->GetScale() + Vector3(workFarAttack2_.scaleIncSpeed, workFarAttack2_.scaleIncSpeed, workFarAttack2_.scaleIncSpeed));
		}

		if (workFarAttack2_.isAttack) {
			workFarAttack2_.toPlayer = player_->GetCenter() - block->GetCenter();
			workFarAttack2_.velocity = Slerp(workFarAttack2_.velocity.normalize(), workFarAttack2_.toPlayer.normalize(), 0.6f) * workFarAttack2_.speed;
			block->SetPosition(block->GetTranslation() + workFarAttack2_.velocity);
		} else {
			Matrix4x4 yRotMat = MakeRotateMatrixY(std::atan2(toPlayerV_.x, toPlayerV_.z));
			Vector3 offset = TransForm(yRotMat, block->GetOffset());
			block->SetPosition(GetCenter() + offset);
		}
	}

	worldTransform_.UpdateMatrix();
}
// ----------------------------------------------------Far3
void Enemy::BehaviorFarAttack3Initialize() {
	// 行動遷移の初期化処理
	rand_ = Rand(0.0f, 4.0f);
	randIndex_ = int(std::floor(rand_));

	int blockNum = 60;

	Vector3 offset = {0.0f, 0.0f, 0.0f};

	for (int i = 0; i < blockNum / 2; i++) {
		offset = {6.0f + i * 4, -2.0f, 0.0f};

		Vector3 position = GetCenter() + offset;
		Vector3 scale = {0.0f, 0.0f, 0.0f};

		CreateBlock(position, scale, offset);
	}

	for (int i = 0; i < blockNum / 2; i++) {
		offset = {-6.0f - i * 4, -2.0f, 0.0f};

		Vector3 position = GetCenter() + offset;
		Vector3 scale = {0.0f, 0.0f, 0.0f};

		CreateBlock(position, scale, offset);
	}

	workFarAttack3_.rotationSpeed = 0.000001f;
	workFarAttack3_.rotationSpeedMax = 0.0012f;
	workFarAttack3_.rotationSpeedMin = 0.000001f;
	workFarAttack3_.rotationSpeedInc = 0.00000005f;

	workFarAttack3_.scaleIncSpeed = 0.01f;
	workFarAttack3_.scaleMax = 1.0f;

	workFarAttack3_.prepareTime = 60 * 2;
	workFarAttack3_.attackTime = 60 * 13;

	workFarAttack3_.isBegin = false;
	workFarAttack3_.isFar = false;

	for (auto& block : blocks_) {
		block->SetRadius(3.5f);

		block->SetDamage(20.0f);

		block->SetColliVanish(true); // 衝突時に消えないように設定

		block->SetDeathTimer(60.0f * 25.0f);
	}
}
void Enemy::BehaviorFarAttack3Update() {
	float speed = 1.0f;

	workFarAttack3_.attackTime--;
	workFarAttack3_.prepareTime--;

	// playerの逆方向に進む
	if (toPlayerDis_ < 90.0f && !workFarAttack3_.isFar &&
		worldTransform_.translation_.x > -500.0f && 
		worldTransform_.translation_.x < 500.0f && 
		worldTransform_.translation_.z > -500.0f &&
	    worldTransform_.translation_.z < 500.0f) {

		worldTransform_.translation_.x -= toPlayerV_.normalize().x * speed;
		worldTransform_.translation_.z -= toPlayerV_.normalize().z * speed;

	} else {
		workFarAttack3_.isFar = true;
	}

	for (auto& block : blocks_) {

		block->SetRotationZ(block->GetRotation().z + 0.01f);
		block->SetRotationY(block->GetRotation().y + 0.01f);

		Matrix4x4 yRotMat = MakeRotateMatrixY(worldTransform_.rotation_.y);
		Vector3 offset = TransForm(yRotMat, block->GetOffset());
		block->SetPosition(GetCenter() + offset);

		if (workFarAttack3_.attackTime <= 0) {

			workFarAttack3_.isBegin = false;

			if (block->GetScale().x > 0.0f) {
				block->SetScale(block->GetScale() - Vector3(workFarAttack3_.scaleIncSpeed, workFarAttack3_.scaleIncSpeed, workFarAttack3_.scaleIncSpeed));
			} else if (block->GetScale().x <= 0.0f) {
				behaviorCD_ = 60 * 1.f;
				behaviorRequest_ = Behavior::kRoot;
			}
		}else {

			if (block->GetScale().x < workFarAttack3_.scaleMax && workFarAttack3_.isFar){ 
				block->SetScale(block->GetScale() + Vector3(workFarAttack3_.scaleIncSpeed, workFarAttack3_.scaleIncSpeed, workFarAttack3_.scaleIncSpeed));
			} else {
				if (workFarAttack3_.prepareTime <= 0) {
					workFarAttack3_.isBegin = true;
				}
			}

			if (workFarAttack3_.isBegin) {
				if (workFarAttack3_.rotationSpeed <= workFarAttack3_.rotationSpeedMax) {
					workFarAttack3_.rotationSpeed += workFarAttack3_.rotationSpeedInc;
				}

				worldTransform_.rotation_.y += workFarAttack3_.rotationSpeed;
			} else {
				// 常にplayerに向かう
				worldTransform_.rotation_.y = std::atan2(toPlayerV_.x, toPlayerV_.z);
			}
		}
	}
}
// ----------------------------------------------------Near1
void Enemy::BehaviorNearAttack1Initialize() {
	// 行動遷移の初期化処理
	rand_ = Rand(0.0f, 4.0f);
	randIndex_ = int(std::floor(rand_));

	workNearAttack1_.rotationSpeed = 0.18f;
	workNearAttack1_.rotationCount = 0.0f;
	workNearAttack1_.isAttack = false;

	workNearAttack1_.attackTime = 60 * 5;

	Vector3 offset = {-9.0f, -0.5f, 0.0f};

	Vector3 position = GetCenter() + offset;
	Vector3 scale = {0.0f, 0.0f, 0.0f};

	CreateBlock(position, scale, offset);

	for (auto& block : blocks_) {
		block->SetRadius(4.0f);

		block->SetDamage(20.0f);

		block->SetHitOnce(true);

		block->SetColliVanish(false); // 衝突時に消えないように設定

		block->SetDeathTimer(60.0f * 5.0f);
	}
}
void Enemy::BehaviorNearAttack1Update() {
	// 回転しながらplayerに向かって進む

	// workNearAttack1_.attackTime--;

	for (auto& block : blocks_) {

		if (block->GetScale().x < 2.5f) {
			block->SetScaleX(block->GetScale().x + 0.07f);
		} else if (block->GetScale().y < 2.5f) {
			block->SetScaleY(block->GetScale().y + 0.07f);
		} else if (block->GetScale().z < 2.5f) {
			block->SetScaleZ(block->GetScale().z + 0.07f);
		} else {
			workNearAttack1_.isAttack = true;
		}

		if (workNearAttack1_.isAttack) {
			if (workNearAttack1_.rotationCount < 3.20f) {
				worldTransform_.rotation_.y += workNearAttack1_.rotationSpeed;
				workNearAttack1_.rotationCount += workNearAttack1_.rotationSpeed;
			} else {
				behaviorCD_ = 60 * 1.f;
				behaviorRequest_ = Behavior::kRoot;
			}
		} else {
			// 常にplayerに向かう
			worldTransform_.rotation_.y = std::atan2(toPlayerV_.x, toPlayerV_.z);
		}

		Matrix4x4 yRotMat = MakeRotateMatrixY(worldTransform_.rotation_.y);
		Vector3 offset = TransForm(yRotMat, block->GetOffset());
		block->SetPosition(GetCenter() + offset);

		// 讓Block面向前進方向
		Vector3 velocity = GetCenter() + offset;
		Matrix4x4 yRotMatBlock = MakeRotateMatrixY(std::atan2(velocity.y, velocity.z));
		block->SetRotationY(std::atan2(velocity.x, velocity.z));

		if (block->IsHit()) {
			followCamera_->ShakeScreen(0.5f);
		}
	}

	worldTransform_.UpdateMatrix();
}
// ----------------------------------------------------Near2
void Enemy::BehaviorNearAttack2Initialize() {
	// 行動遷移の初期化処理
	rand_ = Rand(0.0f, 4.0f);
	randIndex_ = int(std::floor(rand_));

	workNearAttack1_.rotationSpeed = 0.55f;
	workNearAttack1_.rotationCount = 0.0f;
	workNearAttack1_.isAttack = false;

	workNearAttack1_.attackTime = 60 * 5;

	workNearAttack1_.prepareTime = 60 * 1.5;

	Vector3 offset = {0.0f, 6.5f, 0.0f};

	Vector3 position = player_->GetCenter() + offset;
	Vector3 scale = {0.0f, 0.0f, 0.0f};

	CreateBlock(position, scale, offset);

	for (auto& block : blocks_) {
		block->SetRadius(4.0f);

		block->SetDamage(30.0f);

		block->SetHitOnce(true);

		block->SetColliVanish(false); // 衝突時に消えないように設定

		block->SetDeathTimer(60.0f * 7.0f);
	}
}
void Enemy::BehaviorNearAttack2Update() {

	for (auto& block : blocks_) {

		if (block->GetScale().x < 2.f) {
			block->SetScaleX(block->GetScale().x + 0.05f);
		} else if (block->GetScale().y < 2.f) {
			block->SetScaleY(block->GetScale().y + 0.05f);
		} else if (block->GetScale().z < 2.f) {
			block->SetScaleZ(block->GetScale().z + 0.05f);
		} else {
			workNearAttack1_.isAttack = true;
		}

		if (workNearAttack1_.isAttack) {
			workNearAttack1_.prepareTime--;
		}

		if (workNearAttack1_.prepareTime <= 0) {
			workNearAttack1_.isAttack = false;
			block->SetPositionY(block->GetCenter().y - workNearAttack1_.rotationSpeed);

		} else {
			// Playerの座標に追従
			block->SetPosition(player_->GetCenter() + block->GetOffset());

			if (workNearAttack1_.prepareTime <= 60 * 0.5) {
				float random = Rand(-0.5f, 0.5f);
				block->SetPositionX(block->GetTranslation().x + random);
				block->SetPositionZ(block->GetTranslation().z + random);
			}
		}

		if (block->GetCenter().y < 0.0f) {
			behaviorCD_ = 60 * 1.f;
			behaviorRequest_ = Behavior::kRoot;
		}

		// 常にplayerに向かう
		worldTransform_.rotation_.y = std::atan2(toPlayerV_.x, toPlayerV_.z);

		if (block->IsHit()) {
			followCamera_->ShakeScreen(0.5f);
		}
	}

	worldTransform_.UpdateMatrix();
}
// ----------------------------------------------------Near3
void Enemy::BehaviorNearAttack3Initialize() {
	// 行動遷移の初期化処理
	rand_ = Rand(0.0f, 4.0f);
	randIndex_ = int(std::floor(rand_));

	ClearCollisionRecord();

	isDamegeOn_ = true;

	toPlayerVOnce_ = toPlayerV_;

	workNearAttack3_.awaySpeed = 0.5f;
	workNearAttack3_.attckSpeed = 5.0f;
	workNearAttack3_.awayDistanceCount = 0.0f;
	workNearAttack3_.attackDistanceCount = 0.0f;

	workNearAttack3_.rotationSpeed = 0.01f;
	workNearAttack3_.rotationSpeedMax = 0.2f;
	workNearAttack3_.rotationSpeedInc = 0.001f;

	workNearAttack3_.attackTime = 60 * 15;
	workNearAttack3_.isAttack = false;
}
void Enemy::BehaviorNearAttack3Update() {

	// playerの逆方向に進む
	if (workNearAttack3_.awayDistanceCount < 100.0f && worldTransform_.translation_.x > -500.0f && worldTransform_.translation_.x < 500.0f && worldTransform_.translation_.z > -500.0f &&
	    worldTransform_.translation_.z < 500.0f) {

		toPlayerVOnce_ = toPlayerV_;

		worldTransform_.translation_.x -= toPlayerV_.normalize().x * workNearAttack3_.awaySpeed;
		worldTransform_.translation_.z -= toPlayerV_.normalize().z * workNearAttack3_.awaySpeed;

		if (worldTransform_.translation_.y < 30.0f) {
			worldTransform_.translation_.y += workNearAttack3_.awaySpeed * 0.3f;
		}

		workNearAttack3_.awayDistanceCount += workNearAttack3_.awaySpeed;

	} else {
		workNearAttack3_.isAttack = true;
	}

	worldTransform_.rotation_.y += workNearAttack3_.rotationSpeed;

	if (workNearAttack3_.isAttack) {
		worldTransform_.translation_.x += toPlayerVOnce_.normalize().x * workNearAttack3_.attckSpeed;
		worldTransform_.translation_.z += toPlayerVOnce_.normalize().z * workNearAttack3_.attckSpeed;

		if (worldTransform_.translation_.y > 0.0f) {
			worldTransform_.translation_.y -= workNearAttack3_.attckSpeed * 0.4f;
		} else {
			followCamera_->ShakeScreen(0.5f);
		}

		workNearAttack3_.attackDistanceCount += workNearAttack3_.attckSpeed;
	}

	if (workNearAttack3_.rotationSpeed <= workNearAttack3_.rotationSpeedMax) {
		workNearAttack3_.rotationSpeed += workNearAttack3_.rotationSpeedInc;
	}

	if (workNearAttack3_.attackDistanceCount >= 200.0f) {
		isDamegeOn_ = false;
		behaviorCD_ = 60 * 1.f;
		worldTransform_.rotation_.x = 0.0f;
		worldTransform_.translation_.y = 0.0f;
		followCamera_->ResetOffset();
		behaviorRequest_ = Behavior::kRoot;
	}
}
// ----------------------------------------------------
#include "Player.h"
#include "ImGuiManager.h"
#include "enemy.h"
#include "lockOn.h"

// コンボの定数表
const std::array<Player::ConstAttack, Player::kComboNum> Player::kConstAttacks_ = {
    {// 1コンボ目 {振りかぶり、溜め、攻撃、硬直時間 振りかぶり、溜め、攻撃の移動速さ}
     {10, 5, 20, 5, 0.0f, 0.0f, 0.7f},
     // 2コンボ目
     {0, 0, 30, 10, 0.0f, 0.0f, 0.3f},
     // 3コンボ目
     {0, 0, 30, 10, 0.0f, 0.0f, 0.3f}}
};

Player::Player() {}

Player::~Player() {
	models_.clear();
	models_.shrink_to_fit();
}

void Player::ReSet() {
	// ワールド変換データの初期化
	worldTransform_.Initialize();
	worldTransformHead_.Initialize();
	worldTransformBody_.Initialize();
	worldTransformL_arm_.Initialize();
	worldTransformR_arm_.Initialize();

	// モデルの初期位置を設定
	worldTransform_.translation_ = {0.0f, 0.0f, -80.0f};
	worldTransformHead_.translation_ = {0.0f, 1.5f, 0.0f};
	worldTransformL_arm_.translation_ = {-0.55f, 1.3f, 0.0f};
	worldTransformR_arm_.translation_ = {0.55f, 1.3f, 0.0f};

	InitializeFloatAnimation();

	behaviorRequest_ = Behavior::kRoot;

	hp_ = 100.0f;
}

Vector3 Player::GetCenter() const {
	Vector3 offset = {0.0f, 1.5f, 0.0f};
	Vector3 worldPos = TransForm(worldTransform_.matWorld_, offset);

	return worldPos;
}

void Player::Damage(float damage) {
	// ダメージ処理
	if (hp_ > 0) {
		hp_ -= damage;
	}
	if (hp_ <= 0) {
		hp_ = 0;
	}
}

void Player::Initialize(const std::vector<Model*> models) {
	input_ = Input::GetInstance();

	SetRadius(collisionRadius_);

	BaseCharacter::Initialize(models);

	// ワールド変換データの初期化
	// worldTransform_.Initialize();
	worldTransformHead_.Initialize();
	worldTransformBody_.Initialize();
	worldTransformL_arm_.Initialize();
	worldTransformR_arm_.Initialize();

	// モデルの初期位置を設定
	worldTransform_.translation_ = {0.0f, 0.0f, -80.0f};
	worldTransformHead_.translation_ = {0.0f, 1.5f, 0.0f};
	worldTransformL_arm_.translation_ = {-0.55f, 1.3f, 0.0f};
	worldTransformR_arm_.translation_ = {0.55f, 1.3f, 0.0f};

	// モデル同士の親子関係を設定
	worldTransformBody_.SetParent(&worldTransform_);
	worldTransformHead_.SetParent(&worldTransformBody_);
	worldTransformL_arm_.SetParent(&worldTransformBody_);
	worldTransformR_arm_.SetParent(&worldTransformBody_);

	// 武器の初期化
	hammer_ = std::make_unique<Hammer>();
	hammer_->Initialize(models[4], models[6]);
	hammer_->SetParent(worldTransformBody_);

	InitializeFloatAnimation();

	// ColliderIDの設定
	Collider::SetTypeID(static_cast<uint32_t>(CollisionTypeId::kPlayer));
}

void Player::Update() {

	bullets_.remove_if([](PlayerBullet* bullet) {
		if (bullet->IsDead()) {
			delete bullet;
			return true;
		}
		return false;
	});

	if (hp_ <= 0) {
		hp_ = 0;
	}

	if (!enableWeapon_) {
		hammer_->SetRadius(0.0f);
	} else {
		hammer_->SetRadius(1.0f);
	}

	// 行動遷移
	if (behaviorRequest_) {
		// behavior_を変更する
		behavior_ = behaviorRequest_.value();
		// 各行動の初期化処理
		switch (behavior_) {
		case Behavior::kRoot:
			BehaviorRootInitialize();
			break;
		case Behavior::kAttack:
			BehaviorAttackInitialize();
			break;
		case Behavior::kDash:
			BehaviorDashInitialize();
			break;
		case Behavior::kJump:
			BehaviorJumpInitialize();
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

	case Behavior::kAttack:
		if (isHitStop_) {
			break;
		}
		BehaviorAttackUpdate();
		break;

	case Behavior::kDash:
		BehaviorDashUpdate();
		break;

	case Behavior::kJump:
		BehaviorJumpUpdate();
		break;
	}

	// ロックオンしてる時に右トリガーで弾を撃つ
	if (lockOn_->isTargetExist()) {
		if (input_->GetJoystickState(0, joyState_)) {
			if (joyState_.Gamepad.bRightTrigger > 0 && --shotCD_ <= 0.0f) {
				Shot();
			}
		}
	}

	// 弾の更新
	for (auto bullet : bullets_) {
		bullet->Update();
	}

	// 行列の更新
	BaseCharacter::Update();
	worldTransform_.UpdateMatrix();
	worldTransformBody_.UpdateMatrix();
	worldTransformHead_.UpdateMatrix();
	worldTransformL_arm_.UpdateMatrix();
	worldTransformR_arm_.UpdateMatrix();

	// 武器の更新
	hammer_->Update();

	if (enableWeapon_) {
		hammer_->SetEnable(true);
	} else {
		hammer_->SetEnable(false);
	}

	if (hammer_->IsHit()) {
		isHitStop_ = true;
		hitStopTime_ = 8;
	}

	// ダッシュのCD
	if (workDash_.dashCD > 0) {
		workDash_.dashCD--;
	}

	// ヒットストップの更新
	if (isHitStop_) {
		hitStopTime_--;
		if (hitStopTime_ <= 0) {
			isHitStop_ = false;
		}
	}
}

void Player::Draw(const ViewProjection& viewProjection) {
	// ヘッドの描画
	models_[0]->Draw(worldTransformHead_, viewProjection);

	// 胴体の描画
	models_[1]->Draw(worldTransformBody_, viewProjection);

	// 左腕の描画
	models_[2]->Draw(worldTransformL_arm_, viewProjection);

	// 右腕の描画
	models_[3]->Draw(worldTransformR_arm_, viewProjection);

	// 弾の描画
	for (auto bullet : bullets_) {
		bullet->Draw(viewProjection);
	}

	// 武器の描画
	if (enableWeapon_) {
		hammer_->Draw(viewProjection);
	}
}

void Player::ImGuiDraw() {
#ifdef _DEBUG
	ImGui::Begin("Player");
	if (ImGui::BeginTabBar("Option")) {

		if (ImGui::BeginTabItem("WorldTransform")) {
			ImGui::DragFloat3("translation", &worldTransform_.translation_.x, 0.1f);
			ImGui::DragFloat3("rotation", &worldTransform_.rotation_.x, 0.1f);
			ImGui::DragFloat3("scale", &worldTransform_.scale_.x, 0.1f);
			ImGui::EndTabItem();
		}

		if (ImGui::BeginTabItem("Body")) {
			ImGui::DragFloat3("translation", &worldTransformBody_.translation_.x, 0.1f);
			ImGui::DragFloat3("rotation", &worldTransformBody_.rotation_.x, 0.1f);
			ImGui::DragFloat3("scale", &worldTransformBody_.scale_.x, 0.1f);
			ImGui::EndTabItem();
		}

		if (ImGui::BeginTabItem("Head")) {
			ImGui::DragFloat3("translation", &worldTransformHead_.translation_.x, 0.1f);
			ImGui::DragFloat3("rotation", &worldTransformHead_.rotation_.x, 0.1f);
			ImGui::DragFloat3("scale", &worldTransformHead_.scale_.x, 0.1f);
			ImGui::EndTabItem();
		}

		if (ImGui::BeginTabItem("L_arm")) {
			ImGui::DragFloat3("translation", &worldTransformL_arm_.translation_.x, 0.1f);
			ImGui::DragFloat3("rotation", &worldTransformL_arm_.rotation_.x, 0.1f);
			ImGui::DragFloat3("scale", &worldTransformL_arm_.scale_.x, 0.1f);
			ImGui::EndTabItem();
		}

		if (ImGui::BeginTabItem("R_arm")) {
			ImGui::DragFloat3("translation", &worldTransformR_arm_.translation_.x, 0.1f);
			ImGui::DragFloat3("rotation", &worldTransformR_arm_.rotation_.x, 0.1f);
			ImGui::DragFloat3("scale", &worldTransformR_arm_.scale_.x, 0.1f);
			ImGui::EndTabItem();
		}

		if (ImGui::BeginTabItem("FloatAnim")) {
			ImGui::DragFloat("period", &period, 0.1f);
			ImGui::DragFloat("amplitude", &amplitude, 0.1f);
			ImGui::EndTabItem();
		}

		ImGui::EndTabBar();
	}
	// hp
	ImGui::Text("HP: %f", hp_);

	// ImGui::Checkbox("EnableWeapon", &enableWeapon_);

	ImGui::End();

	ImGui::Begin("ComboParm");
	ImGui::Text("ComboIndex: %d", workAttack_.comboIndex);
	ImGui::Text("inComboPhase: %d", workAttack_.inComboPhase);
	ImGui::Text("attackParam: %d", workAttack_.attackParam);
	// comnoNext
	ImGui::Text("comboNext: %d", workAttack_.comboNext);
	// preAttackTime
	ImGui::Text("preAttackTime: %d", preAttackTime);
	// chargeTime
	ImGui::Text("chargeTime: %d", chargeTime);
	// attackTime
	ImGui::Text("attackTime: %d", attackTime);
	// recoveryTime
	ImGui::Text("recoveryTime: %d", recoveryTime);
	// comboTime
	ImGui::Text("comboTime: %d", comboTime);
	ImGui::End();
#endif _DEBUG
}

void Player::Move() {
	const float speed = 0.3f;
	Matrix4x4 rotationMatrix;
	float targetAngle = 0.0f;

	if (input_->GetJoystickState(0, joyState_)) { // ゲームパッドによる移動
		const float deadzone = 0.24f;
		bool isMoving = false;

		velocity_ = {(float)joyState_.Gamepad.sThumbLX, 0.0f, (float)joyState_.Gamepad.sThumbLY};

		if (Length(velocity_) > deadzone) {
			isMoving = true;
		}

		if (isMoving) {
			velocity_ = velocity_.normalize() * speed;

			rotationMatrix = MakeRotateMatrixXYZ(cameraViewProjection_->rotation_);

			velocity_ = TransFormNormal(velocity_, rotationMatrix);

			worldTransform_.translation_ += velocity_;

			targetAngle = std::atan2(velocity_.x, velocity_.z);

			t_ = 0.0f;
		}
	}

	// キーボードによる移動
	if (input_->PushKey(DIK_W) && input_->PushKey(DIK_A)) {
		velocity_ = {-speed, 0.0f, speed};

		rotationMatrix = MakeRotateMatrixXYZ(cameraViewProjection_->rotation_);
		velocity_ = TransFormNormal(velocity_, rotationMatrix);

		worldTransform_.translation_ += velocity_.normalize() * speed;

		targetAngle = std::atan2(velocity_.x, velocity_.z);

		t_ = 0.0f;
	} else if (input_->PushKey(DIK_W) && input_->PushKey(DIK_D)) {
		velocity_ = {speed, 0.0f, speed};

		rotationMatrix = MakeRotateMatrixXYZ(cameraViewProjection_->rotation_);
		velocity_ = TransFormNormal(velocity_, rotationMatrix);

		worldTransform_.translation_ += velocity_.normalize() * speed;

		targetAngle = std::atan2(velocity_.x, velocity_.z);

		t_ = 0.0f;
	} else if (input_->PushKey(DIK_S) && input_->PushKey(DIK_A)) {
		velocity_ = {-speed, 0.0f, -speed};

		rotationMatrix = MakeRotateMatrixXYZ(cameraViewProjection_->rotation_);
		velocity_ = TransFormNormal(velocity_, rotationMatrix);

		worldTransform_.translation_ += velocity_.normalize() * speed;

		targetAngle = std::atan2(velocity_.x, velocity_.z);

		t_ = 0.0f;
	} else if (input_->PushKey(DIK_S) && input_->PushKey(DIK_D)) {
		velocity_ = {speed, 0.0f, -speed};

		rotationMatrix = MakeRotateMatrixXYZ(cameraViewProjection_->rotation_);
		velocity_ = TransFormNormal(velocity_, rotationMatrix);

		worldTransform_.translation_ += velocity_.normalize() * speed;

		targetAngle = std::atan2(velocity_.x, velocity_.z);

		t_ = 0.0f;
	} else if (input_->PushKey(DIK_W)) {
		velocity_ = {0.0f, 0.0f, speed};

		rotationMatrix = MakeRotateMatrixXYZ(cameraViewProjection_->rotation_);
		velocity_ = TransFormNormal(velocity_, rotationMatrix);

		worldTransform_.translation_ += velocity_;

		targetAngle = std::atan2(velocity_.x, velocity_.z);

		t_ = 0.0f;
	} else if (input_->PushKey(DIK_S)) {
		velocity_ = {0.0f, 0.0f, -speed};

		rotationMatrix = MakeRotateMatrixXYZ(cameraViewProjection_->rotation_);
		velocity_ = TransFormNormal(velocity_, rotationMatrix);

		worldTransform_.translation_ += velocity_;

		targetAngle = std::atan2(velocity_.x, velocity_.z);

		t_ = 0.0f;
	} else if (input_->PushKey(DIK_A)) {
		velocity_ = {-speed, 0.0f, 0.0f};

		rotationMatrix = MakeRotateMatrixXYZ(cameraViewProjection_->rotation_);
		velocity_ = TransFormNormal(velocity_, rotationMatrix);

		worldTransform_.translation_ += velocity_;

		targetAngle = std::atan2(velocity_.x, velocity_.z);

		t_ = 0.0f;
	} else if (input_->PushKey(DIK_D)) {
		velocity_ = {speed, 0.0f, 0.0f};

		rotationMatrix = MakeRotateMatrixXYZ(cameraViewProjection_->rotation_);
		velocity_ = TransFormNormal(velocity_, rotationMatrix);

		worldTransform_.translation_ += velocity_;

		targetAngle = std::atan2(velocity_.x, velocity_.z);

		t_ = 0.0f;
	}

	targetAngle_ = targetAngle;

	if (t_ < 1.0f) {
		t_ += 0.1f;
	} else {
		t_ = 1.0f;
	}
}

void Player::InitializeFloatAnimation() { floatingParam_ = 0.0f; }

void Player::UpdateFloatAnimation() {
	// 1フレームでの加算量
	float add = float(2.0f * M_PI / period);

	floatingParam_ += add;
	// 2πを超えたら0に戻す
	floatingParam_ = float(std::fmod(floatingParam_, 2.0f * M_PI));

	worldTransformBody_.translation_.y = std::sin(floatingParam_) * amplitude;

	// 腕を揺らす
	if (!lockOn_->isTargetExist()) {
		worldTransformL_arm_.rotation_.x = std::sin(floatingParam_) * amplitude;
		worldTransformL_arm_.rotation_.y = Lerp(worldTransformL_arm_.rotation_.y, 0.0f, 0.1f);
	} else {
		worldTransformL_arm_.rotation_.x = Lerp(worldTransformL_arm_.rotation_.x, 1.6f, 0.1f);
		worldTransformL_arm_.rotation_.y = Lerp(worldTransformL_arm_.rotation_.y, 2.0f, 0.1f);
	}
	worldTransformR_arm_.rotation_.x = std::sin(floatingParam_) * amplitude;
}

bool Player::IsMoveInput() {
	if (input_->PushKey(DIK_W) || input_->PushKey(DIK_A) || input_->PushKey(DIK_S) || input_->PushKey(DIK_D) || input_->PushKey(DIK_LEFT) || input_->PushKey(DIK_RIGHT)) {
		return true;
	}

	if (input_->GetJoystickState(0, joyState_)) {
		if (joyState_.Gamepad.sThumbLX != 0 || joyState_.Gamepad.sThumbLY != 0) {
			return true;
		}
	}

	return false;
}

void Player::OnCollision([[maybe_unused]] Collider* other) {
	// 衝突相手の種別IDを取得
	uint32_t typeID = other->GetTypeID();

	// 衝突相手が敵である場合
	if (typeID == static_cast<uint32_t>(CollisionTypeId::kEnemy)) {
		//// 衝突相手を敵クラスにダウンキャスト
		// Enemy* enemy = static_cast<Enemy*>(other);
		//// 衝突相手のシリアルナンバーを取得
		// uint32_t serialNum = enemy->GetSerialNumber();

		//// すでに衝突している敵である場合は処理を終了
		// if (collisionRecord_.CheckRecord(serialNum)) {
		//	return;
		// }

		//// 衝突した敵のシリアルナンバーを記録
		// collisionRecord_.AddRecord(serialNum);

		//// プレイヤーにダメージを与える
		// Damage(5);
	}
}

void Player::Shot() {
	assert(enemy_);

	float speed = 0.5f;

	Vector3 bulletVelocity = {0.0f, 0.0f, 0.0f};

	// プレイヤーの位置
	Vector3 playerPos = GetCenter();

	// 敵の位置
	Vector3 enemyPos = enemy_->GetCenter();
	// プレイヤーから敵へのベクトル
	Vector3 dir = enemyPos - playerPos;
	// ベクトルの正規化
	dir = dir.normalize();
	// 弾の速度
	bulletVelocity = dir * speed;

	// プレイヤーの位置から弾を発射
	PlayerBullet* bullet = new PlayerBullet();
	bullet->Initialize(models_[5], playerPos, bulletVelocity);
	bullet->SetEnemy(enemy_);
	bullets_.push_back(bullet);

	shotCD_ = 10.0f;
}

// ----------------------行動遷移用---------------------
// 通常状態
void Player::BehaviorRootInitialize() {
	// 浮遊アニメーション変数の初期化
	floatingParam_ = 0.0f;
	period = 130.0f;
	amplitude = 0.15f;

	// 攻撃の初期化
	workAttack_.comboIndex = 0;

	// 角度の初期化
	worldTransformL_arm_.rotation_ = {0.0f, 0.0f, 0.0f};
	worldTransformR_arm_.rotation_ = {0.0f, 0.0f, 0.0f};
	worldTransformBody_.rotation_ = {0.0f, 0.0f, 0.0f};
	hammer_->SetTranslation({0.0f, 1.3f, 0.0f});
	hammer_->SetRotation({0.0f, 0.0f, 0.0f});

	attackRecovryTime_ = 15.0f;

	hammer_->SetRotation({0.0f, 0.0f, 0.0f});
}
void Player::BehaviorRootUpdate() {
	enableWeapon_ = false;

	attackRecovryTime_ -= 1.0f;

	if (input_->GetJoystickState(0, joyState_) && input_->GetJoystickStatePrevious(0, preJoyState_)) {
		if (joyState_.Gamepad.wButtons & XINPUT_GAMEPAD_X && !(preJoyState_.Gamepad.wButtons & XINPUT_GAMEPAD_X)) {
			if (attackRecovryTime_ <= 0.0f && !lockOn_->isTargetExist()) {
				behaviorRequest_ = Behavior::kAttack;
			}
		}

		if (joyState_.Gamepad.wButtons & XINPUT_GAMEPAD_B && !(preJoyState_.Gamepad.wButtons & XINPUT_GAMEPAD_B)) {
			if (workDash_.dashCD <= 0.0f)
				behaviorRequest_ = Behavior::kDash;
		}

		if (joyState_.Gamepad.wButtons & XINPUT_GAMEPAD_A && !(preJoyState_.Gamepad.wButtons & XINPUT_GAMEPAD_A)) {
			behaviorRequest_ = Behavior::kJump;
		}
	}

	// 移動
	if (IsMoveInput()) {
		Move();
	}

	if (lockOn_ && lockOn_->isTargetExist()) { // ロックオン時ロックオン対象に向ける
		Vector3 targetPos = lockOn_->GetTargetPos();
		Vector3 dir = targetPos - GetCenter();
		targetAngle_ = std::atan2(dir.x, dir.z);
	}

	// ターゲットの角度に向かって回転
	worldTransform_.rotation_.y = LerpShortAngle(worldTransform_.rotation_.y, targetAngle_, t_);
	worldTransform_.UpdateMatrix();

	// 浮遊アニメーション
	UpdateFloatAnimation();
}

// 攻撃状態
void Player::BehaviorAttackInitialize() {

	workAttack_.attackParam = 0;
	workAttack_.inComboPhase = 0;
	workAttack_.comboNext = false;

	worldTransformL_arm_.rotation_ = {0.0f, -1.1f, -0.7f};
	hammer_->SetRotation({-1.6f, 0.0f, 0.0f});
	hammer_->SetTranslation({-1.5f, 1.3f, 0.0f});

	// 各段階の時間
	preAttackTime = kConstAttacks_[workAttack_.comboIndex].preAttackTime;
	chargeTime = kConstAttacks_[workAttack_.comboIndex].chargeTime;
	attackTime = kConstAttacks_[workAttack_.comboIndex].attackTime;
	recoveryTime = kConstAttacks_[workAttack_.comboIndex].recoveryTime;
	// 一コンボ分の合計時間
	comboTime = preAttackTime + chargeTime + attackTime + recoveryTime;

	// 各段階の移動速度
	R_armAngleY = 1.5f;
	hammerAngleX = 1.6f;
	hammerPosZ = -0.4f;
	BodyAngleY = 6.3f;
	hammerAngleZ = 1.6f;
	L_armAngleX = -3.3f;
	R_armAngleX = -3.3f;
	hammerPosY = 1.3f;

	hammer_->ClearCollisionRecord();
}
void Player::BehaviorAttackUpdate() {
	enableWeapon_ = true;

	XINPUT_STATE joyStatePrev;
	XINPUT_STATE joyState;

	// コンボの上限に達していない
	if (workAttack_.comboIndex < kComboNum - 1) {
		// ゲームパットの状態を取得
		if (input_->GetJoystickState(0, joyState) && input_->GetJoystickStatePrevious(0, joyStatePrev)) {
			// Xボタンをトリガーしたら
			if (joyState.Gamepad.wButtons & XINPUT_GAMEPAD_X && !(joyStatePrev.Gamepad.wButtons & XINPUT_GAMEPAD_X)) {
				workAttack_.comboNext = true;
			}
		}
	}

	// コンボの進行
	if (++workAttack_.attackParam >= comboTime) {
		if (workAttack_.comboNext) {
			workAttack_.comboNext = false;

			if (workAttack_.comboIndex < kComboNum) {
				workAttack_.comboIndex += 1;
			}
			workAttack_.inComboPhase = 0;
			workAttack_.attackParam = 0;
			Move();
			BehaviorAttackInitialize();

			if (workAttack_.comboIndex == 2) {
				hammer_->SetRotation({0.0f, 0.0f, 1.6f});
			}

		} else {
			behaviorRequest_ = Behavior::kRoot;
		}
	}

	// コンボ段階によってモーション分岐
	switch (workAttack_.comboIndex) {
	case 0:
		R_armAngleY = 1.5f;
		hammerAngleX = 1.6f;
		hammerPosZ = -0.4f;

		if (workAttack_.inComboPhase == 0) { // 振りかぶり
			preAttackTime--;
			if (preAttackTime <= 0) {
				workAttack_.inComboPhase++;
			}
		} else if (workAttack_.inComboPhase == 1) { // 溜め
			chargeTime--;
			if (chargeTime <= 0) {
				workAttack_.inComboPhase++;
			}

		} else if (workAttack_.inComboPhase == 2) { // 攻撃
			if (worldTransformL_arm_.rotation_.y < R_armAngleY) {
				worldTransformL_arm_.rotation_.y += kConstAttacks_[workAttack_.comboIndex].attackSpeed;
				if (worldTransformL_arm_.rotation_.y > R_armAngleY) {
					worldTransformL_arm_.rotation_.y = R_armAngleY;
				}
			}

			if (hammer_->GetRotation().x < hammerAngleX) {
				hammer_->SetRotation({hammer_->GetRotation().x + kConstAttacks_[workAttack_.comboIndex].attackSpeed, 0.0f, 0.0f});
				if (hammer_->GetRotation().x > hammerAngleX) {
					hammer_->SetRotation({hammerAngleX, 0.0f, 0.0f});
				}
			}

			if (hammer_->GetTranslation().z > hammerPosZ) {
				hammer_->SetTranslation({hammer_->GetTranslation().x, hammer_->GetTranslation().y, hammer_->GetTranslation().z - 0.1f});
				if (hammer_->GetTranslation().z < hammerPosZ) {
					hammer_->SetTranslation({hammer_->GetTranslation().x, hammer_->GetTranslation().y, hammerPosZ});
				}
			}

			attackTime--;
			if (attackTime <= 0) {
				workAttack_.inComboPhase++;
			}

		} else if (workAttack_.inComboPhase == 3) { // 硬直
			recoveryTime--;
			if (recoveryTime <= 0) {
				workAttack_.inComboPhase++;
			}
		}
		break;

	case 1:
		BodyAngleY = 6.3f;
		hammerAngleZ = 1.6f;

		if (workAttack_.inComboPhase == 0) { // 振りかぶり
			if (preAttackTime <= 0) {
				workAttack_.inComboPhase++;
			}
			preAttackTime--;
		} else if (workAttack_.inComboPhase == 1) { // 溜め
			if (chargeTime <= 0) {
				workAttack_.inComboPhase++;
			}
			chargeTime--;
		} else if (workAttack_.inComboPhase == 2) { // 攻撃
			if (attackTime <= 0) {
				workAttack_.inComboPhase++;
			}

			if (worldTransformBody_.rotation_.y < BodyAngleY) {
				worldTransformBody_.rotation_.y += kConstAttacks_[workAttack_.comboIndex].attackSpeed;
			}

			if (hammer_->GetRotation().z < hammerAngleZ) {
				hammer_->SetRotation({0.0f, 0.0f, hammer_->GetRotation().z + kConstAttacks_[workAttack_.comboIndex].attackSpeed});
				if (hammer_->GetRotation().z > hammerAngleZ) {
					hammer_->SetRotation({0.0f, 0.0f, hammerAngleZ});
				}
			}

			attackTime--;

		} else if (workAttack_.inComboPhase == 3) { // 硬直
			if (recoveryTime <= 0) {
				workAttack_.inComboPhase++;
			}
			recoveryTime--;
		}
		break;

	case 2:
		BodyAngleY = 0.0f;
		hammerAngleY = 3.2f;

		if (workAttack_.inComboPhase == 0) { // 振りかぶり
			if (preAttackTime <= 0) {
				workAttack_.inComboPhase++;
			}
			preAttackTime--;

		} else if (workAttack_.inComboPhase == 1) { // 溜め
			if (chargeTime <= 0) {
				workAttack_.inComboPhase++;
			}
			chargeTime--;

		} else if (workAttack_.inComboPhase == 2) { // 攻撃
			if (attackTime <= 0) {
				workAttack_.inComboPhase++;
			}

			if (worldTransformBody_.rotation_.y > BodyAngleY) {
				worldTransformBody_.rotation_.y -= kConstAttacks_[workAttack_.comboIndex].attackSpeed;
			}

			if (hammer_->GetRotation().z < hammerAngleZ) {
				hammer_->SetRotation({hammer_->GetRotation().x, hammer_->GetRotation().y, hammer_->GetRotation().z + kConstAttacks_[workAttack_.comboIndex].attackSpeed});
				if (hammer_->GetRotation().z > hammerAngleZ) {
					hammer_->SetRotation({hammer_->GetRotation().x, hammer_->GetRotation().y, hammerAngleZ});
				}
			}

			hammer_->SetRotation({hammer_->GetRotation().x, hammerAngleY, hammer_->GetRotation().z});

			attackTime--;

		} else if (workAttack_.inComboPhase == 3) { // 硬直
			if (recoveryTime <= 0) {
				workAttack_.inComboPhase++;
			}
			recoveryTime--;
		}
		break;
	}
}

// ダッシュ状態
void Player::BehaviorDashInitialize() {
	enableWeapon_ = false;
	workDash_.dashParam = 0;
	worldTransform_.rotation_.y = targetAngle_;
}
void Player::BehaviorDashUpdate() {
	// 今向いてる方向に移動する
	float speed = 1.3f;
	const uint32_t kDashTime = 8;

	if (lockOn_->isTargetExist()) {
		// 今移動してる方向に移動する
		if (input_->GetJoystickState(0, joyState_)) { // ゲームパッドによる移動

			velocity_ = {(float)joyState_.Gamepad.sThumbLX, 0.0f, (float)joyState_.Gamepad.sThumbLY};

			velocity_ = velocity_.normalize() * speed;

			Matrix4x4 rotationMatrix = MakeRotateMatrixXYZ(cameraViewProjection_->rotation_);

			velocity_ = TransFormNormal(velocity_, rotationMatrix);

			worldTransform_.translation_ += velocity_;
		}
	} else {
		// 今向いてる方向に移動する
		worldTransform_.translation_.x += std::sin(worldTransform_.rotation_.y) * speed;
		worldTransform_.translation_.z += std::cos(worldTransform_.rotation_.y) * speed;
	}

	// ダッシュの時間が経過したら
	if (++workDash_.dashParam >= kDashTime) {
		workDash_.dashCD = 30.f;
		behaviorRequest_ = Behavior::kRoot;
	}
}

// ジャンプ状態
void Player::BehaviorJumpInitialize() {
	enableWeapon_ = false;

	worldTransformBody_.translation_.y = 0.0f;
	worldTransformL_arm_.rotation_.x = 0.0f;
	worldTransformR_arm_.rotation_.x = 0.0f;

	// ジャンプの初速
	const float kJumpSpeed = 0.8f;

	velocity_.y = kJumpSpeed;
}
void Player::BehaviorJumpUpdate() {
	worldTransform_.translation_ += velocity_;

	// 重力
	const float kGravity = 0.05f;
	Vector3 acceleration = {0.0f, -kGravity, 0.0f};

	velocity_ += acceleration;

	if (worldTransform_.translation_.y <= 0.0f) {
		worldTransform_.translation_.y = 0.0f;
		behaviorRequest_ = Behavior::kRoot;
	}
}

// ----------------------行動遷移用---------------------//

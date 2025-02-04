#pragma once
#include "Input.h"
#include "Audio.h"
#include "Mat4x4Func.h"
#include "Model.h"
#include "Vec3Func.h"
#include "Xinput.h"
#include "cassert"
#include "hammer.h"
#include "memory"
#include "camera.h"
#include "playerBullet.h"
#include <optional>

// 親クラス
#include "BaseCharacter.h"

class LockOn;

class Enemy;

class Player : public BaseCharacter {

public:
	/// <summary>
	/// 定数
	///</summary>

	// 攻撃の定数
	struct ConstAttack {
		// 振りかぶりの時間
		uint32_t preAttackTime;
		// 溜めの時間
		uint32_t chargeTime;
		// 攻撃振りの時間
		uint32_t attackTime;
		// 硬直時間
		uint32_t recoveryTime;
		// 振りかぶりの移動速さ
		float preAttackSpeed;
		// 溜めの移動速さ
		float chargeSpeed;
		// 攻撃振りの移動速さ
		float attackSpeed;
	};

	// コンボの数
	static const int kComboNum = 3;
	// コンボ定数表
	static const std::array<ConstAttack, kComboNum> kConstAttacks_;

	// メンバ関数------------------------------------------------------
	/// <summary>
	/// コンストラクタ
	///</summary>
	Player();

	/// <summary>
	/// デストラクタ
	/// </summary>
	~Player();

	/// <summary>
	/// 初期化
	/// </summary>
	void Initialize(const std::vector<Object3d*> models) override;

	/// <summary>
	/// 毎フレーム処理
	/// </summary>
	void Update() override;

	/// <summary>
	/// 描画
	/// </summary>
	void Draw() override;

	/// <summary>
	/// Behaviors
	/// </summary>
	void BehaviorRootInitialize(); // 通常状態の初期化
	void BehaviorRootUpdate();     // 通常状態の更新

	void BehaviorAttackInitialize(); // 攻撃状態の初期化
	void BehaviorAttackUpdate();     // 攻撃状態の更新

	void BehaviorDashInitialize(); // ダッシュ状態の初期化
	void BehaviorDashUpdate();     // ダッシュ状態の更新

	void BehaviorJumpInitialize(); // ジャンプ状態の初期化
	void BehaviorJumpUpdate();     // ジャンプ状態の更新

	/// <summary>
	/// ImGuiによるデバッグ表示
	/// </summary>
	void ImGuiDraw();

	/// <summary>
	/// 移動
	/// </summary>
	void Move();

	/// <summary>
	/// 弾を撃つ
	/// </summary>
	void Shot();

	/// <summary>
	/// 浮遊アニメーションの初期化
	/// </summary>
	void InitializeFloatAnimation();

	/// <summary>
	/// 浮遊アニメーションの更新処理
	/// </summary>
	void UpdateFloatAnimation();

	/// <summary>
	/// 衝突判定
	/// </summary>
	void OnCollision([[maybe_unused]] Collider* other) override;

	/// <summary>
	/// gamepadによる移動入力があるか
	/// </summary>
	bool IsMoveInput();

	/// <summary>
	/// ダメージを受ける
	/// </summary>
	/// <param name="damage"></param>
	void Damage(float damage);

	/// <summary>
	/// リセット
	/// </summary>
	void ReSet();

	/// <summary>
	/// Getters
	/// </summary>
	const Transform& GetWorldTransformHead() const { return worldTransformHead_; }
	const Transform& GetWorldTransformBody() const { return worldTransformBody_; }
	const Transform& GetWorldTransformL_arm() const { return worldTransformL_arm_; }
	const Transform& GetWorldTransformR_arm() const { return worldTransformR_arm_; }
	Vector3 GetCenter() const override;
	Hammer* GetHammer() const { return hammer_.get(); }
	const std::list<PlayerBullet*>& GetBullets() const { return bullets_; }
	// gat behavior
	const int GetBehavior() const { return static_cast<int>(behavior_); }
	// get serial number
	const uint32_t GetSerialNumber() const { return serialNumber_; }
	// get hp
	const float GetHP() const { return hp_; }
	const bool GetEnableWeapon() const { return enableWeapon_; }
	const bool GetHammerIsHit() const { return hammer_->IsHit(); }
	// ----------------------浮遊アニメーション用---------------------
	float GetFloatingParam() const { return floatingParam_; }
	float GetPeriod() const { return period; }
	float GetAmplitude() const { return amplitude; }

	/// <summary>
	/// Setters
	/// </summary>
	void SetCamera(const Camera* cameraViewProjection) { cameraViewProjection_ = cameraViewProjection; }
	void SetLockOn(const LockOn* lockOn) { lockOn_ = lockOn; }
	void SetEnemy(const Enemy* enemy) { enemy_ = enemy; }
	void SetHP(float hp) { hp_ = hp; }

private: // メンバ変数
	struct WorkAttack {
		int32_t attackParam = 0;
		int32_t comboIndex = 0;
		int32_t inComboPhase = 0;
		bool comboNext = false;
	};

	struct WorkDash {
		// ダッシュ用の媒介変数
		uint32_t dashParam = 0;
		// ダッシュのCD
		float dashCD = 0.f;
	};

	Input* input_ = nullptr;

	// LockOn
	const LockOn* lockOn_ = nullptr;

	// Enemy
	const Enemy* enemy_ = nullptr;

	// シリアルナンバー
	uint32_t serialNumber_ = 0;

	/// <summary>
	/// プレイヤー用
	/// </summary>

	//hp
	float hp_ = 100;

	// ワールド変換データ
	Transform worldTransformHead_;
	Transform worldTransformBody_;
	Transform worldTransformL_arm_;
	Transform worldTransformR_arm_;

	std::unique_ptr<Hammer> hammer_;

	// bullet
	std::list<PlayerBullet*> bullets_;
	float shotCD_ = 0.0f;
	bool isShooting_ = false;

	Vector3 velocity_ = {};

	const Camera* cameraViewProjection_;

	float targetAngle_ = 0.0f;
	float t_ = 0.0f;

	// 衝突記録
	//CollisionRecord collisionRecord_;

	// 衝突半径
	float collisionRadius_ = 1.0f;

	// ヒットストップ用
	bool isHitStop_ = false;
	uint32_t hitStopTime_ = 8;

	// ----------------------行動遷移用---------------------
	enum class Behavior {
		kRoot,
		kAttack,
		kDash,
		kJump,
	};

	Behavior behavior_ = Behavior::kRoot;
	std::optional<Behavior> behaviorRequest_ = std::nullopt;

	//----------------------攻撃用---------------------
	WorkAttack workAttack_;
	bool enableWeapon_ = false;
	float attackRecovryTime_ = 15.0f;

	// 攻撃の角度
	float R_armAngleX = -3.3f;
	float R_armAngleY = 1.5f;
	float L_armAngleX = -3.3f;
	float hammerAngleX = 1.6f;
	float hammerAngleY = 3.2f;
	float hammerAngleZ = 1.6f;
	float hammerPosY = 1.3f;
	float hammerPosZ = -0.4f;
	float BodyAngleY = 6.3f;

	// 各段階の時間
	uint32_t preAttackTime = kConstAttacks_[workAttack_.comboIndex].preAttackTime;
	uint32_t chargeTime = kConstAttacks_[workAttack_.comboIndex].chargeTime;
	uint32_t attackTime = kConstAttacks_[workAttack_.comboIndex].attackTime;
	uint32_t recoveryTime = kConstAttacks_[workAttack_.comboIndex].recoveryTime;
	// 一コンボ分の合計時間
	int32_t comboTime = preAttackTime + chargeTime + attackTime + recoveryTime;

	//----------------------ダッシュ用---------------------
	WorkDash workDash_;

	// ----------------------浮遊アニメーション用---------------------
	float floatingParam_ = 0.0f;
	// 周期
	float period = 130.0f; // 60フレームで1周期
	// 振幅
	float amplitude = 0.15f;
};

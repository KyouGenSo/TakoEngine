#pragma once
#pragma once
#include "Audio.h"
#include "Matrix4x4Function.h"
#include "Model.h"
#include "Vector3Function.h"
#include "ViewProjection.h"
#include "WorldTransform.h"
#include "cassert"
#include "collisionTypeIdDef.h"
#include "enemyBlock.h"
#include "memory"
#include "myFunction.h"
#include <cmath>
#include <optional>

// 親クラス
#include "BaseCharacter.h"

class Player;

class FollowCamera;

class Enemy : public BaseCharacter {

public: // メンバ関数
	/// <summary>
	/// コンストラクタ
	///</summary>
	Enemy();

	/// <summary>
	/// デストラクタ
	/// </summary>
	~Enemy();

	/// <summary>
	/// 初期化
	/// </summary>
	void Initialize(const std::vector<Model*> models) override;

	/// <summary>
	/// 毎フレーム処理
	/// </summary>
	void Update() override;

	/// <summary>
	/// 描画
	/// </summary>
	void Draw(const ViewProjection& viewProjection) override;

	/// <summary>
	/// 衝突判定
	/// </summary>
	void OnCollision([[maybe_unused]] Collider* other) override;

	/// <summary>
	/// 移動
	/// </summary>
	void Move();

	/// <summary>
	/// 浮遊アニメーションの初期化
	/// </summary>
	void InitializeFloatAnimation();

	/// <summary>
	/// 浮遊アニメーションの更新処理
	/// </summary>
	void UpdateFloatAnimation();

	/// <summary>
	/// シェークエフェクト
	/// </summary>
	void ShakeEffect();

	/// <summary>
	/// ImGuiによるデバッグ表示
	/// </summary>
	void ImGuiDraw();

	/// <summary>
	/// blockを生成
	/// </summary>
	void CreateBlock(const Vector3& position, const Vector3& scale, const Vector3& offset);

	void PosRange();

	/// <summary>
	/// 衝突記録をクリア
	/// </summary>
	void ClearCollisionRecord() { collisionRecord_.Clear(); }

	void Damage(float damage);

	void HitStop(uint32_t time) {
		isHitStop_ = true;
		hitStopTime_ = time;
	}

	void ReSet();

	/// <summary>
	/// Behaviors
	/// </summary>
	void BehaviorRootInitialize(); // 通常状態の初期化
	void BehaviorRootUpdate();     // 通常状態の更新

	void BehaviorNearInitialize(); // 接近状態の初期化
	void BehaviorNearUpdate();     // 接近状態の更新

	void BehaviorAwayInitialize(); // 離脱状態の初期化
	void BehaviorAwayUpdate();     // 離脱状態の更新

	void BehaviorFarAttack1Initialize(); // 遠距離攻撃1状態の初期化
	void BehaviorFarAttack1Update();     // 遠距離攻撃1状態の更新

	void BehaviorFarAttack2Initialize(); // 遠距離攻撃2状態の初期化
	void BehaviorFarAttack2Update();     // 遠距離攻撃2状態の更新

	void BehaviorFarAttack3Initialize(); // 遠距離攻撃3状態の初期化
	void BehaviorFarAttack3Update();     // 遠距離攻撃3状態の更新

	void BehaviorNearAttack1Initialize(); // 近距離攻撃1状態の初期化
	void BehaviorNearAttack1Update();     // 近距離攻撃1状態の更新

	void BehaviorNearAttack2Initialize(); // 近距離攻撃2状態の初期化
	void BehaviorNearAttack2Update();     // 近距離攻撃2状態の更新

	void BehaviorNearAttack3Initialize(); // 近距離攻撃3状態の初期化
	void BehaviorNearAttack3Update();     // 近距離攻撃3状態の更新

	/// <summary>
	/// Getters
	/// </summary>
	Vector3 GetCenter() const override;
	uint32_t GetSerialNumber() const { return serialNumber_; }
	// get block list
	const std::list<EnemyBlock*>& GetBlocks() const { return blocks_; }
	// get hp
	float GetHp() const { return hp_; }

	/// <summary>
	/// Setters
	/// </summary>
	void SetPlayer(const Player* player) { player_ = player; }
	void SetFollowCamera(FollowCamera* followCamera) { followCamera_ = followCamera; }
	void SetHP(float hp) { hp_ = hp; }

private: // メンバ変数
	// Audio
	Audio* audio_ = nullptr;
	// SE
	uint32_t seHitPlayer_ = 0;

	// player参照
	const Player* player_ = nullptr;

	// FollowCamera参照
	FollowCamera* followCamera_ = nullptr;

	// block list
	std::list<EnemyBlock*> blocks_;

	// ボディのワールドトランスフォーム
	WorldTransform worldTransformBody_;

	// シリアルナンバー
	uint32_t serialNumber_ = 0;

	static uint32_t nextSerialNumber_;

	Vector3 offset_ = {0.0f, 3.5f, 0.0f};

	// ヒットストップ用
	bool isHitStop_ = false;
	uint32_t hitStopTime_ = 0;

	bool isDamegeOn_ = false;

	// hp
	float hp_ = 100.0f;

	CollisionRecord collisionRecord_;

	// ----------------------行動遷移用---------------------
	enum class Behavior {
		kRoot,
		kNear,
		kAway,
		kFarAttack1,
		kFarAttack2,
		kFarAttack3,
		kNearAttack1,
		kNearAttack2,
		kNearAttack3,
	};

	Behavior behavior_ = Behavior::kRoot;
	std::optional<Behavior> behaviorRequest_ = std::nullopt;

	struct AttackRecord {
		bool farAttack1;
		bool farAttack2;
		bool farAttack3;
		bool nearAttack1;
		bool nearAttack2;
		bool nearAttack3;
		bool isNeared;
		bool isAwayed;
	};
	AttackRecord attackRecord_;

	struct WorkFarAttack1 {
		float speed;
		float sppedMin;
		float sppedDec;
		float rotationSpeed;
		float rotationSpeedMax;
		float rotationSpeedMin;
		float rotationSpeedInc;
		float rotationSpeedDec;
		float maxDis;
		float distanceCount;
		bool isAttack;
		bool isHit;
		float shakeTime;
	};
	WorkFarAttack1 workFarAttack1_;

	struct WorkFarAttack2 {
		Vector3 velocity;
		Vector3 toPlayer;
		float speed;
		float rotationSpeed;
		float rotationSpeedMax;
		float rotationSpeedInc;
		float scaleIncSpeed;
		float scaleMax;
		float attackTime;
		bool isAttack;
		bool isShot;
	};
	WorkFarAttack2 workFarAttack2_;

	struct WorkFarAttack3 {
		float rotationSpeed;
		float rotationSpeedMax;
		float rotationSpeedMin;
		float rotationSpeedInc;
		float scaleIncSpeed;
		float scaleMax;
		float prepareTime;
		float attackTime;
		bool isBegin;
		bool isFar;
	};
	WorkFarAttack3 workFarAttack3_;

	struct WorkNearAttack1 {
		float rotationSpeed;
		float rotationCount;
		float attackTime;
		float prepareTime;
		bool isAttack;
	};
	WorkNearAttack1 workNearAttack1_;

	struct WorkNearAttack3 {
		float attckSpeed;
		float awaySpeed;
		float rotationSpeed;
		float rotationSpeedMax;
		float rotationSpeedInc;
		float awayDistanceCount;
		float attackDistanceCount;
		float attackTime;
		bool isAttack;
	};
	WorkNearAttack3 workNearAttack3_;

	float rand_;    // 乱数
	int randIndex_; // 乱数カウント

	Vector3 toPlayerV_;
	Vector3 toPlayerVOnce_;
	float toPlayerDis_;

	float behaviorCD_ = 60 * 3;

	// ----------------------行動遷移用---------------------

	// ----------------------浮遊アニメーション用---------------------
	float floatingParam_ = 0.0f;
	// 周期
	float period = 120.0f; // 60フレームで1周期
	// 振幅
	float amplitude = 0.04f;
};
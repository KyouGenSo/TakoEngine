#pragma once
#include "BaseScene.h"
#include"Sprite.h"
#include"Object3d.h"
#include "collisionManager.h"

// Game Object
#include "Player.h"
#include "playerBullet.h"
#include "Enemy.h"
#include "enemyBlock.h"
#include "Skydome.h"
#include "Ground.h"
#include "followCamera.h"
#include "lockOn.h"
#include "ParticleEmitter.h"
#include "AABB.h"
#include <memory>

class GameScene : public BaseScene
{
public: // メンバ関数
	/// <summary>
	/// 初期化
	/// </summary>
	void Initialize() override;

	/// <summary>
	/// UI初期化
	/// </summary>
	void InitializeUI();

	/// <summary>
	/// 終了処理
	/// </summary>
	void Finalize() override;

	/// <summary>
	/// 更新
	/// </summary>
	void Update() override;

	/// <summary>
	/// UIの更新
	/// </summary>
	void UpdateUI();

	/// <summary>
	/// 描画
	/// </summary>
	void Draw() override;

	/// <summary>
	/// playerとenemyのHPを描画
	/// </summary>
	void DrawHP();

	/// <summary>
	/// ImGuiの描画
	/// </summary>
	void DrawImGui() override;

	/// <summary>
	/// 衝突判定と応答
	/// </summary>
	void CheckAllCollisions();

	/// <summary>
	/// reset
	/// </summary>
	void ResetAllObjects();

	// get isGameClear_
	const bool GetIsGameClear() const { return isGameClear_; }
	// set isGameClear_
	void SetIsGameClear(bool isGameClear) { isGameClear_ = isGameClear; }
	// get isGameOver_
	const bool GetIsGameOver() const { return isGameOver_; }
	// set isGameOver_
	void SetIsGameOver(bool isGameOver) { isGameOver_ = isGameOver; }

	struct EmitterParam {
		std::string name_;
		Transform transform_;
		Vector3 velocity_;
		AABB range_;
		Vector4 color_;
		float lifeTime;
		int count_;
		float frequency_;
		bool isRandomColor_;
		bool isVisualize_;
	};

private: // メンバ変数

	bool isDebug_ = false;

	Input* input_ = nullptr;
	Audio* audio_ = nullptr;

	// BGM
	uint32_t bgmSH_ = 0;
	uint32_t bgmVH_ = 0;

	// UI
	std::unique_ptr<Sprite> playerHP_ = nullptr;
	Vector2 playerHP_Pos_ = { 400.0f, 650.0f };
	Vector2 playerHP_Size_ = { 300.0f, 40.0f };

	std::unique_ptr<Sprite> enemyHP_ = nullptr;
	Vector2 enemyHP_Pos_ = { 400.0f, 30.0f };
	Vector2 enemyHP_Size_ = { 300.0f, 40.0f };

	std::unique_ptr<Sprite> player_Text_ = nullptr;
	Vector2 player_Text_Pos_ = { 297.6f, 649.6f };

	std::unique_ptr<Sprite> enemy_Text_ = nullptr;
	Vector2 enemy_Text_Pos_ = { 298.2f, 33.3f };


	std::unique_ptr<Sprite> A_Icon_ = nullptr;
	Vector2 A_Icon_Pos_ = { 1105.6f, 600.0f };
	std::unique_ptr<Sprite> B_Icon_ = nullptr;
	Vector2 B_Icon_Pos_ = { 1166.1f, 516.8f };
	std::unique_ptr<Sprite> X_Icon_ = nullptr;
	Vector2 X_Icon_Pos_ = { 1027.0f, 516.8f };
	std::unique_ptr<Sprite> LT_Icon_ = nullptr;
	Vector2 LT_Icon_Pos_ = { 1033.2f, 429.7f };
	std::unique_ptr<Sprite> RT_Icon_ = nullptr;
	Vector2 RT_Icon_Pos_ = { 1163.1f, 429.7f };

	Vector2 textSize_ = { 90.0f, 30.0f };

	std::unique_ptr<Sprite> dash_Text_ = nullptr;
	Vector2 dash_Text_Pos_ = { 1154.4f, 577.6f };

	std::unique_ptr<Sprite> jump_Text_ = nullptr;
	Vector2 jump_Text_Pos_ = { 1086.0f, 653.0f };

	std::unique_ptr<Sprite> attack_Text_ = nullptr;
	Vector2 attack_Text_Pos_ = { 1013.7f, 577.6f };

	std::unique_ptr<Sprite> aim_Text_ = nullptr;
	Vector2 aim_Text_Pos_ = { 1023.7f, 394.1f };

	std::unique_ptr<Sprite> shoot_Text_ = nullptr;
	Vector2 shoot_Text_Pos_ = { 1142.6f, 394.1f };

	std::unique_ptr<Sprite> key_Text_ = nullptr;
	Vector2 key_Text_Pos_ = { 1066.0f, 324.0f };
	Vector2 key_Text_Size_ = { 150.0f, 50.0f };
	std::unique_ptr<Sprite> manual_Text_ = nullptr;
	Vector2 manual_Text_Pos_ = { 218.0f, 50.0f };

	/// <summary>
	/// ゲームシーン用
	/// </summary>

	bool isGameClear_ = false;
	bool isGameOver_ = false;

	float vignetteAlpha_ = 0.0f;
	float vignetteRadius_ = 60.0f;

	bool isPause_ = false;

	// 追従カメラ
	std::unique_ptr<FollowCamera> followCamera_ = nullptr;

	// 当たり判定用
	std::unique_ptr<CollisionManager> collisionManager_;

	// -----Game Object----- //

	// Player
	std::unique_ptr<Player> player_ = nullptr;
	std::unique_ptr<Object3d> playerHeadModel_ = nullptr;
	std::unique_ptr<Object3d> playerBodyModel_ = nullptr;
	std::unique_ptr<Object3d> playerL_armModel_ = nullptr;
	std::unique_ptr<Object3d> playerR_armModel_ = nullptr;
	std::unique_ptr<Object3d> playerWeaponModel_ = nullptr; // weapon
	std::unique_ptr<Object3d> playerBulletModel_ = nullptr; // bullet
	std::unique_ptr<Object3d> playerWeaponEffectModel_ = nullptr; // weapon effect
	std::vector<Object3d*> playerModels_;

	// LockOn system
	std::unique_ptr<LockOn> lockOn_ = nullptr;
	uint32_t lockOnMarkTexture_ = 0;

	// enemy
	std::unique_ptr<Enemy> enemy_ = nullptr;
	std::unique_ptr<Object3d> enemyBodyModel_ = nullptr;
	std::unique_ptr<Object3d> enemyBlockModel_ = nullptr;
	std::vector<Object3d*> enemyModels_;

	// 天球
	std::unique_ptr<Skydome> skydome_ = nullptr;
	std::unique_ptr<Object3d> skydomeModel_ = nullptr;

	// 地面
	std::unique_ptr<Ground> ground_ = nullptr;
	std::unique_ptr<Object3d> groundModel_ = nullptr;

	// ----------------------Emitter---------------------
	EmitterParam emitterParam_;
	std::unique_ptr<ParticleEmitter> particleEmitter_;
};

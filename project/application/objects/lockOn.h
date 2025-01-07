#pragma once
#include "Enemy.h"
#include "Player.h"
#include "Mat4x4Func.h"
#include "Sprite.h"
#include "Xinput.h"
#include "Input.h"
#include "Vec3Func.h"
#include "Camera.h"
#include <cmath>
#include "WinApp.h"

const float kDeg2Rad = float(M_PI / 180.0f);

class LockOn {
public:
	/// <summary>
	/// 初期化
	/// </summary>
	void Initialize();

	/// <summary>
	/// 毎フレーム処理
	/// </summary>
	void Update(const std::unique_ptr<Enemy>& target, const Camera& camera);

	/// <summary>
	/// 描画
	/// </summary>
	void Draw();

	/// <summary>
	/// LOCK ON対象の検索
	/// </summary>
	void SearchTarget(const std::unique_ptr<Enemy>& enemy, const Camera& camera);


	/// <summary>
	/// ロックオン対象が範囲外かどうか
	/// </summary>
	bool IsOutDistance(const std::unique_ptr<Enemy>& enemy, const Camera& camera);
	bool IsOutDistance(const Camera& camera);

	bool isTargetExist() const { return target_ ? true : false; }

	void SetPlayer(const Player* player) { player_ = player; }

	/// <summary>
	/// 　getter
	/// </summary>
	// ロックオン対象の座標を返す
	Vector3 GetTargetPos() const;

private:
	Input* input_ = nullptr;

	XINPUT_STATE joyState_;

	std::unique_ptr<Sprite> lockOnMark_ = nullptr;

	// ロックオン対象
	const Enemy* target_ = nullptr;

	// player
	const Player* player_ = nullptr;

	// ロックオン中かどうか
	bool isLockOn_ = false;

	bool isLockOnByController_ = false;
	bool isLockOnByKeyboard_ = false;

	// 最小距離
	float minDis_ = 10.0f;
	// 最大距離
	float maxDis_ = 200.0f;
	// 角度範囲
	float angleRange_ = 30.0f * kDeg2Rad;
};

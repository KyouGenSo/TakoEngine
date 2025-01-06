#include "lockOn.h"
#include"TextureManager.h"

void LockOn::Initialize() {
	input_ = Input::GetInstance();

	TextureManager::GetInstance()->LoadTexture("aim_sphere.png");

	lockOnMark_ = std::make_unique<Sprite>();
	lockOnMark_->Initialize("aim_sphere.png");
	lockOnMark_->SetSize({100.0f, 100.0f});
}

void LockOn::Update(const std::unique_ptr<Enemy>& enemy, const Camera& camera) {
	if (target_) {
		if (!IsOutDistance(camera)) {
			target_ = nullptr;
			isLockOn_ = false;
		}
	}

	if (input_->GetJoystickState(0, joyState_)) {
		// ロックオンボタンが押されたら
		if (joyState_.Gamepad.bLeftTrigger > 0 && player_->GetBehavior() != 1) {
			// ロックオン対象の検索
			SearchTarget(enemy, camera);
		} else {
			target_ = nullptr;
		}
	}

	if (input_->TriggerKey(DIK_F) && player_->GetBehavior() != 1) {
		isLockOn_ = !isLockOn_;
	}

	if (isLockOn_) {
		SearchTarget(enemy, camera);
	} else {
		target_ = nullptr;
	}

	if (target_) { // ロックオンマークの座標計算
		// ワールド座標
		Vector3 worldPos = enemy->GetCenter();
		// view projection変換
		Vector3 screenPos = Mat4x4::TransForm(camera.GetViewMatrix(), worldPos);
		screenPos = Mat4x4::TransForm(camera.GetProjectionMatrix(), screenPos);
		// viewport変換
		Matrix4x4 matViewport = Mat4x4::MakeViewport(0.f, 0.f, WinApp::kClientWidth, WinApp::kClientHeight, 0.f, 1.f);
		screenPos = Mat4x4::TransForm(matViewport, screenPos);
		// スクリーン2D座標
		Vector3 offset = {100.f / 2.0f, 100.f / 2.0f, 0.f};
		screenPos -= offset;
		Vector2 screenPos2D = {screenPos.x, screenPos.y};
		// ロックオンマークの座標設定
		lockOnMark_->SetPos(screenPos2D);
	}

	lockOnMark_->Update();
}

void LockOn::Draw() {
	if (target_) {
		lockOnMark_->Draw();
	}
}

void LockOn::SearchTarget(const std::unique_ptr<Enemy>& enemy, const Camera& camera) {

	// 角度条件チェック
	if (IsOutDistance(enemy, camera)) {
		target_ = enemy.get();
	} else {
		isLockOn_ = false;
	}
}

bool LockOn::IsOutDistance(const std::unique_ptr<Enemy>& enemy, const Camera& camera) {
	Vector3 worldPos = enemy->GetCenter();
	Vector3 viewPos = Mat4x4::TransForm(camera.GetViewMatrix(), worldPos);

	if (minDis_ <= viewPos.z && viewPos.z <= maxDis_) {

		// 角度条件チェック
		float arcTan = std::atan2(std::sqrt(viewPos.x * viewPos.x + viewPos.y * viewPos.y), viewPos.z);
		if (std::abs(arcTan) <= angleRange_) {
			return true;
		}
	}

	return false;
}

bool LockOn::IsOutDistance(const Camera& camera) {
	Vector3 worldPos = target_->GetCenter();
	Vector3 viewPos = Mat4x4::TransForm(camera.GetViewMatrix(), worldPos);

	if (minDis_ <= viewPos.z && viewPos.z <= maxDis_) {

		// 角度条件チェック
		float arcTan = std::atan2(std::sqrt(viewPos.x * viewPos.x + viewPos.y * viewPos.y), viewPos.z);
		if (std::abs(arcTan) <= angleRange_) {
			return true;
		}
	}

	return false;
}

Vector3 LockOn::GetTargetPos() const {
	if (isTargetExist()) {
		return target_->GetCenter();
	}

	return Vector3();
}
#pragma once
#include "WinApp.h"
#include<wrl.h>
#include <memory>
#include "Vector2.h"
#include "Xinput.h"
#pragma comment(lib, "XInput.lib")

#define DIRECTINPUT_VERSION 0x0800 // DirectInput のバージョン指定
#include <dinput.h>
#pragma comment(lib, "dinput8.lib")
#pragma comment(lib, "dxguid.lib")

// XINPUT_Button の GUID
static const WORD XINPUT_Buttons[] = {
	  XINPUT_GAMEPAD_A,
	  XINPUT_GAMEPAD_B,
	  XINPUT_GAMEPAD_X,
	  XINPUT_GAMEPAD_Y,
	  XINPUT_GAMEPAD_DPAD_UP,
	  XINPUT_GAMEPAD_DPAD_DOWN,
	  XINPUT_GAMEPAD_DPAD_LEFT,
	  XINPUT_GAMEPAD_DPAD_RIGHT,
	  XINPUT_GAMEPAD_LEFT_SHOULDER,
	  XINPUT_GAMEPAD_RIGHT_SHOULDER,
	  XINPUT_GAMEPAD_LEFT_THUMB,
	  XINPUT_GAMEPAD_RIGHT_THUMB,
	  XINPUT_GAMEPAD_START,
	  XINPUT_GAMEPAD_BACK
};

namespace Tako {

/// <summary>
/// XInput ボタン ID 管理構造体
/// ゲームパッドのボタン番号を格納
/// </summary>
struct XButtonIDs
{
	/// <summary>
	/// コンストラクタ
	/// </summary>
	XButtonIDs();

	//---------------------メンバー変数---------------------//
	int A, B, X, Y; ///< 'Action'ボタン

	int DPad_Up, DPad_Down, DPad_Left, DPad_Right; ///< Directional Pad(DPad) ボタン

	int L_Shoulder, R_Shoulder; ///< Shoulder ボタン

	int L_Thumbstick, R_Thumbstick; ///< Thumbstick ボタン

	int Start; ///< 'START' ボタン
	int Back;  ///< 'BACK' ボタン
};

/// <summary>
/// 統合入力管理クラス
/// DirectInput と XInput でキーボード、マウス、ゲームパッド入力を処理
/// </summary>
class Input {
private: 	// シングルトン
	static std::unique_ptr<Input> instance_;

	Input() = default;
	~Input() = default;

	friend struct std::default_delete<Input>;

public:
	Input(const Input&) = delete;
	Input& operator=(const Input&) = delete;

	template<class T> using ComPtr = Microsoft::WRL::ComPtr<T>; ///< ComPtr のエイリアス

public:
	/// <summary>
	/// インスタンスの取得
	/// </summary>
	static Input* GetInstance();

	/// <summary>
	/// 初期化
	/// </summary>
	/// <param name="winApp">ウィンドウアプリケーション</param>
	void Initialize(WinApp* winApp);

	/// <summary>
	/// 終了処理
	/// </summary>
	void Finalize();

	/// <summary>
	/// 更新
	/// </summary>
	void Update();

	/// <summary>
	/// マウスの座標を更新
	/// </summary>
	void UpdateMousePos();

	/// <summary>
	/// キーの押下状態を取得
	/// </summary>
	/// <param name="keyNum">取得したいキーコード</param>
	/// <returns>押下されている場合 true</returns>
	bool PushKey(BYTE keyNum) const;

	/// <summary>
	/// キーのトリガー状態を取得
	/// </summary>
	/// <param name="keyNum">取得したいキーコード</param>
	/// <returns>押した瞬間のみ true</returns>
	bool TriggerKey(BYTE keyNum) const;

	/// <summary>
	/// キーのリリース状態を取得
	/// </summary>
	/// <param name="keyNum">取得したいキーコード</param>
	/// <returns>離した瞬間のみ true</returns>
	bool ReleaseKey(BYTE keyNum) const;

	/// <summary>
	/// マウスの押下状態を取得
	/// </summary>
	/// <param name="button">マウスボタン番号（0:左, 1:右, 2:中央）</param>
	/// <returns>押下されている場合 true</returns>
	bool PushMouse(int button) const;

	/// <summary>
	/// マウスのトリガー状態を取得（押した瞬間のみ true）
	/// </summary>
	/// <param name="button">マウスボタン番号（0:左, 1:右, 2:中央）</param>
	/// <returns>押した瞬間のみ true</returns>
	bool TriggerMouse(int button) const;

	/// <summary>
	/// マウスのリリース状態を取得（離した瞬間のみ true）
	/// </summary>
	/// <param name="button">マウスボタン番号（0:左, 1:右, 2:中央）</param>
	/// <returns>離した瞬間のみ true</returns>
	bool ReleaseMouse(int button) const;

	/// <summary>
	/// マウスの座標を取得
	/// </summary>
	/// <returns>スクリーン座標系でのマウス位置</returns>
	Vector2 GetMousePos();

	/// <summary>
	/// マウスの座標を設定
	/// </summary>
	/// <param name="x">X 座標（スクリーン座標）</param>
	/// <param name="y">Y 座標（スクリーン座標）</param>
	void SetMousePos(int x, int y);

	/// <summary>
	/// ゲームパッドの状態を取得
	/// </summary>
	/// <returns>XInput 状態構造体</returns>
	XINPUT_STATE GetGamePadState();

	/// <summary>
	/// ゲームパッドの接続状態を取得
	/// </summary>
	/// <returns>接続されている場合 true</returns>
	bool IsConnect();

	/// <summary>
	/// ゲームパッドの状態を更新
	/// </summary>
	void RefreshGamePadState();

	/// <summary>
	/// ゲームパッドの押下状態を取得
	/// </summary>
	/// <param name="button">ボタン番号（XButtons 構造体のメンバーを使用）</param>
	/// <returns>押下されている場合 true</returns>
	bool PushButton(int button) const;

	/// <summary>
	/// ゲームパッドのトリガー状態を取得（押した瞬間のみ true）
	/// </summary>
	/// <param name="button">ボタン番号（XButtons 構造体のメンバーを使用）</param>
	/// <returns>押した瞬間のみ true</returns>
	bool TriggerButton(int button) const;

	/// <summary>
	/// ゲームパッドのリリース状態を取得（離した瞬間のみ true）
	/// </summary>
	/// <param name="button">ボタン番号（XButtons 構造体のメンバーを使用）</param>
	/// <returns>離した瞬間のみ true</returns>
	bool ReleaseButton(int button) const;

	/// <summary>
	/// ゲームパッドの左スティックがデッドゾーン内かどうか
	/// </summary>
	/// <returns>デッドゾーン内の場合 true</returns>
	bool LStickInDeadZone() const;

	/// <summary>
	/// ゲームパッドの右スティックがデッドゾーン内かどうか
	/// </summary>
	/// <returns>デッドゾーン内の場合 true</returns>
	bool RStickInDeadZone() const;

	/// <summary>
	/// ゲームパッドの左スティックの値を取得
	/// </summary>
	/// <returns>正規化された左スティックの値（-1.0 ~ 1.0）</returns>
	Vector2 GetLeftStick();

	/// <summary>
	/// ゲームパッドの右スティックの値を取得
	/// </summary>
	/// <returns>正規化された右スティックの値（-1.0 ~ 1.0）</returns>
	Vector2 GetRightStick();

	/// <summary>
	/// ゲームパッドの左トリガーの値を取得
	/// </summary>
	/// <returns>正規化されたトリガー値（0.0 ~ 1.0）</returns>
	float GetLeftTrigger();

	/// <summary>
	/// ゲームパッドの右トリガーの値を取得
	/// </summary>
	/// <returns>正規化されたトリガー値（0.0 ~ 1.0）</returns>
	float GetRightTrigger();

	/// <summary>
	/// ゲームパッドの振動を設定
	/// </summary>
	/// <param name="leftMotor">左モーターの強度（0.0 ~ 1.0）</param>
	/// <param name="rightMotor">右モーターの強度（0.0 ~ 1.0）</param>
	/// <param name="duration">振動継続時間（秒）。0以下で無限に振動</param>
	void SetVibration(float leftMotor, float rightMotor, float duration = 0.0f);

	/// <summary>
	/// ゲームパッドの振動を停止
	/// </summary>
	void StopVibration();

private:
	WinApp* winApp_ = nullptr; ///< WinApp クラスのインスタンス

	ComPtr<IDirectInput8> directInput_; ///< DirectInput オブジェクト

	ComPtr<IDirectInputDevice8> keyboardDevice_; ///< キーボードデバイス

	ComPtr<IDirectInputDevice8> mouseDevice_; ///< マウスデバイス

	DIMOUSESTATE mouseState_; ///< マウスの状態

	DIMOUSESTATE prevMouseState_; ///< 前フレームのマウスの状態

	POINT mousePos_ = {}; ///< マウスの座標

	BYTE keys_[256] = {}; ///< キーボードの入力状態

	BYTE prevKeys_[256] = {}; ///< 前フレームのキーボード入力状態

	//---------------------ゲームパット---------------------//
	XINPUT_STATE state_; ///< ゲームパッドの状態

	static const int GAMEPAD_BUTTON_NUM = 14; ///< ゲームパッドボタンの数

	bool buttonStates_[GAMEPAD_BUTTON_NUM]; ///< ゲームパッドのボタンの状態

	bool prevButtonStates_[GAMEPAD_BUTTON_NUM]; ///< 前フレームのゲームパッドのボタンの状態

	bool buttonsTriger_[GAMEPAD_BUTTON_NUM]; ///< ゲームパッドのトリガーの状態

	//---------------------振動制御---------------------//
	float vibrationDuration_ = 0.0f; ///< 振動継続時間（秒）。0以下で無限
	float vibrationTimer_ = 0.0f;    ///< 振動経過時間
	bool isVibrating_ = false;       ///< 振動中フラグ

};

extern XButtonIDs XButtons;

} // namespace Tako

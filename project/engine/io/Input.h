#pragma once
#include "WinApp.h"
#include<wrl.h>
#include "Vector2.h"
#include "Xinput.h"
#pragma comment(lib, "XInput.lib")

#define DIRECTINPUT_VERSION 0x0800 // DirectInputのバージョン指定
#include <dinput.h>
#pragma comment(lib, "dinput8.lib")
#pragma comment(lib, "dxguid.lib")

// XINPUT_ButtonのGUID
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

// XButtonIDs構造体
struct XButtonIDs
{
	XButtonIDs(); // コンストラクタ

	// メンバー変数
	//---------------------//
	int A, B, X, Y; // 'Action'ボタン

	// Directional Pad(DPad) ボタン
	int DPad_Up, DPad_Down, DPad_Left, DPad_Right;

	// Shoulder ボタン
	int L_Shoulder, R_Shoulder;

	// Thumbstick ボタン
	int L_Thumbstick, R_Thumbstick;

	int Start; // 'START' ボタン
	int Back;  // 'BACK' ボタン
};

class Input {
private: 	// シングルトン
	// インスタンス
	static Input* instance_;

	Input() = default;
	Input(const Input&) = delete;
	Input& operator=(const Input&) = delete;
	~Input() = default;
public:
	// ComPtrのエイリアス
	template<class T> using ComPtr = Microsoft::WRL::ComPtr<T>;

public:
	///<summary>
	///インスタンスの取得
	///	</summary>
	static Input* GetInstance();

	/// <summary>
	/// 初期化
	/// </summary>
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
	/// <param name="key">取得したいキー</param>
	bool PushKey(BYTE keyNum) const;

	/// <summary>
	/// キーのトリガー状態を取得
	/// </summary>
	/// <param name="key">取得したいキー</param>
	bool TriggerKey(BYTE keyNum) const;

	/// <summary>
	/// キーのリリース状態を取得
	/// </summary>
	/// <param name="key">取得したいキー</param>
	bool ReleaseKey(BYTE keyNum) const;

	/// <summary>
	/// マウスの押下状態を取得
	/// </summary>
	/// <param name="button"></param>
	/// <returns></returns>
	bool PushMouse(int button) const;

	/// <summary>
	/// マウスのトリガー状態を取得
	/// </summary>
	/// <param name="button"></param>
	/// <returns></returns>
	bool TriggerMouse(int button) const;

	/// <summary>
	/// マウスのリリース状態を取得
	/// </summary>
	/// <param name="button"></param>
	/// <returns></returns>
	bool ReleaseMouse(int button) const;

	/// <summary>
	/// マウスの座標を取得
	/// </summary>
	/// <returns></returns>
	Vector2 GetMousePos();

	/// <summary>
	/// マウスの座標を設定
	/// </summary>
	/// <param name="x"></param>
	/// <param name="y"></param>
	void SetMousePos(int x, int y);

	/// <summary>
	/// ゲームパッドの状態を取得
	/// </summary>
	XINPUT_STATE GetGamePadState();

	/// <summary>
	/// ゲームパッドの接続状態を取得
	/// </summary>
	/// <returns></returns>
	bool IsConnect();

	/// <summary>
	/// ゲームパッドの状態を更新
	/// </summary>
	void RefreshGamePadState();

	/// <summary>
	/// ゲームパッドの押下状態を取得
	/// </summary>
	/// <param name="button"></param>
	/// <returns></returns>
	bool PushButton(int button) const;

	/// <summary>
	/// ゲームパッドのトリガー状態を取得
	/// </summary>
	/// <param name="button"></param>
	/// <returns></returns>
	bool TriggerButton(int button) const;

	/// <summary>
	/// ゲームパッドのリリース状態を取得
	/// </summary>
	/// <param name="button"></param>
	/// <returns></returns>
	bool ReleaseButton(int button) const;

	/// <summary>
	/// ゲームパッドの左スティックがデッドゾーン内かどうか
	/// </summary>
	/// <returns></returns>
	bool LStickInDeadZone() const;

	/// <summary>
	/// ゲームパッドの右スティックがデッドゾーン内かどうか
	/// </summary>
	/// <returns></returns>
	bool RStickInDeadZone() const;

	/// <summary>
	/// ゲームパッドの左スティックの値を取得
	/// </summary>
	/// <returns></returns>
	Vector2 GetLeftStick();

	/// <summary>
	/// ゲームパッドの右スティックの値を取得
	/// </summary>
	/// <returns></returns>
	Vector2 GetRightStick();

	/// <summary>
	/// ゲームパッドの左トリガーの値を取得
	/// </summary>
	/// <returns></returns>
	float GetLeftTrigger();

	/// <summary>
	/// ゲームパッドの右トリガーの値を取得
	/// </summary>
	float GetRightTrigger();

	/// <summary>
	/// ゲームパッドの振動
	/// </summary>
	/// <param name="leftMotor"></param>
	/// <param name="rightMotor"></param>
	void SetVibration(float leftMotor, float rightMotor);
	void SetVibration(float leftMotor, float rightMotor, float time);

	/// <summary>
	/// ゲームパッドの振動を停止
	/// </summary>
	void StopVibration();

private:
	// WinAppクラスのインスタンス
	WinApp* winApp_ = nullptr;

	// DirectInputオブジェクト
	ComPtr<IDirectInput8> directInput_;

	// キーボードデバイス
	ComPtr<IDirectInputDevice8> keyboardDevice_;

	// マウスデバイス
	ComPtr<IDirectInputDevice8> mouseDevice_;

	// マウスの状態
	DIMOUSESTATE mouseState_;

	// 前フレームのマウスの状態
	DIMOUSESTATE prevMouseState_;

	// マウスの座標
	POINT mousePos_ = {};

	// キーボードの入力状態
	BYTE keys_[256] = {};

	// 前フレームのキーボード入力状態
	BYTE prevKeys_[256] = {};

	// -----------------------------------------------ゲームパット-----------------------------------------------//
	// ゲームパッドの状態
	XINPUT_STATE state_;

	// ゲームパッドボタンの数
	static const int GAMEPAD_BUTTON_NUM = 14;

	// ゲームパッドのボタンの状態
	bool buttonStates_[GAMEPAD_BUTTON_NUM];

	// 前フレームのゲームパッドのボタンの状態
	bool prevButtonStates_[GAMEPAD_BUTTON_NUM];

	// ゲームパッドのトリガーの状態
	bool buttonsTriger_[GAMEPAD_BUTTON_NUM];

	// 振動時間
	float vibrationTime_ = 0.0f;
};

extern XButtonIDs XButtons;

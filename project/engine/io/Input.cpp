#include "Input.h"
#include <cassert>
#include "Vector2.h"


Input* Input::instance_ = nullptr;

XButtonIDs XButtons;

XButtonIDs::XButtonIDs()
{
	A = 0;
	B = 1;
	X = 2;
	Y = 3;

	DPad_Up = 4;
	DPad_Down = 5;
	DPad_Left = 6;
	DPad_Right = 7;

	L_Shoulder = 8;
	R_Shoulder = 9;

	L_Thumbstick = 10;
	R_Thumbstick = 11;

	Start = 12;
	Back = 13;
}

Input* Input::GetInstance()
{
	if (instance_ == nullptr)
	{
		instance_ = new Input();
	}
	return instance_;
}

void Input::Initialize(WinApp* winApp) {
	// WinAppクラスのインスタンスを取得
	this->winApp_ = winApp;

	HRESULT hr;

// DirectInputの初期化
	// DirectInputオブジェクトの生成
	hr = DirectInput8Create(winApp->GetHInstance(), DIRECTINPUT_VERSION, IID_IDirectInput8, reinterpret_cast<void**>(directInput_.GetAddressOf()), nullptr);
	assert(SUCCEEDED(hr));

	// KeyboardDeviceの生成
	hr = directInput_->CreateDevice(GUID_SysKeyboard, keyboardDevice_.GetAddressOf(), NULL);
	assert(SUCCEEDED(hr));

	// KeyboardDeviceのフォーマット設定
	hr = keyboardDevice_->SetDataFormat(&c_dfDIKeyboard);
	assert(SUCCEEDED(hr));

	// KeyboardDeviceの協調レベル設定
	hr = keyboardDevice_->SetCooperativeLevel(winApp->GetHWnd(), DISCL_FOREGROUND | DISCL_NONEXCLUSIVE | DISCL_NOWINKEY);
	assert(SUCCEEDED(hr));

	// マウスデバイスの生成
	hr = directInput_->CreateDevice(GUID_SysMouse, mouseDevice_.GetAddressOf(), NULL);
	assert(SUCCEEDED(hr));

	// マウスデバイスのフォーマット設定
	hr = mouseDevice_->SetDataFormat(&c_dfDIMouse);
	assert(SUCCEEDED(hr));

	// マウスデバイスの協調レベル設定
	hr = mouseDevice_->SetCooperativeLevel(winApp->GetHWnd(), DISCL_FOREGROUND | DISCL_NONEXCLUSIVE);
	assert(SUCCEEDED(hr));

	// マウスの座標を取得
	POINT point;
	GetCursorPos(&point);
	ScreenToClient(winApp_->GetHWnd(), &point);
	mousePos_.x = point.x;
	mousePos_.y = point.y;

	// ゲームパッドの初期化
	for (int i = 0; i < GAMEPAD_BUTTON_NUM; i++)
	{
		prevButtonStates_[i] = false;
		buttonStates_[i] = false;
		buttonsTriger_[i] = false;
	}

}

void Input::Finalize()
{
	if (instance_ != nullptr)
	{
		delete instance_;
		instance_ = nullptr;
	}
}

void Input::Update() {
	// 前フレームのキーボード入力状態を保存
	memcpy(prevKeys_, keys_, sizeof(keys_));

	// 前フレームのマウス入力状態を保存
	prevMouseState_ = mouseState_;

	// キーボード情報の取得
	keyboardDevice_->Acquire();
	keyboardDevice_->GetDeviceState(sizeof(keys_), keys_);

	// マウス情報の取得
	mouseDevice_->Acquire();
	mouseDevice_->GetDeviceState(sizeof(DIMOUSESTATE), &mouseState_);

	// マウスの座標を取得
	UpdateMousePos();

	// ゲームパッドの状態を取得
	state_ = GetGamePadState();

	// ゲームパッドのボタンの状態を更新
	for (int i = 0; i < GAMEPAD_BUTTON_NUM; i++)
	{
		buttonStates_[i] = (state_.Gamepad.wButtons & XINPUT_Buttons[i]) == XINPUT_Buttons[i];
		buttonsTriger_[i] = !prevButtonStates_[i] && buttonStates_[i];
	}
}

void Input::UpdateMousePos()
{
	// マウスの座標を取得
	POINT point;
	GetCursorPos(&point);
	ScreenToClient(winApp_->GetHWnd(), &point);
	mousePos_.x = point.x;
	mousePos_.y = point.y;
}

bool Input::PushKey(BYTE keyNum) const
{
	if (keys_[keyNum])
		return true;

	return false;
}

bool Input::TriggerKey(BYTE keyNum) const
{
	if (keys_[keyNum] && !prevKeys_[keyNum])
		return true;

	return false;
}

bool Input::ReleaseKey(BYTE keyNum) const
{
	if (!keys_[keyNum] && prevKeys_[keyNum])
		return true;

	return false;
}

bool Input::PushMouse(int button) const
{
	if (mouseState_.rgbButtons[button])
		return true;

	return false;
}

bool Input::TriggerMouse(int button) const
{
	if (mouseState_.rgbButtons[button] && !prevMouseState_.rgbButtons[button])
		return true;

	return false;
}

bool Input::ReleaseMouse(int button) const
{
	if (!mouseState_.rgbButtons[button] && prevMouseState_.rgbButtons[button])
		return true;

	return false;
}

Vector2 Input::GetMousePos()
{
	return Vector2(float(mousePos_.x), float(mousePos_.y));
}

void Input::SetMousePos(int x, int y)
{
	POINT point;
	point.x = x;
	point.y = y;
	ClientToScreen(winApp_->GetHWnd(), &point);
	SetCursorPos(point.x, point.y);
}

XINPUT_STATE Input::GetGamePadState()
{
	XINPUT_STATE state;

	ZeroMemory(&state, sizeof(XINPUT_STATE));

	// ゲームパッドの状態を取得
	XInputGetState(0, &state);

	return state;
}

bool Input::IsConnect()
{
	ZeroMemory(&state_, sizeof(XINPUT_STATE));

	// ゲームパッドの状態を取得
	DWORD result = XInputGetState(0, &state_);

	return result == ERROR_SUCCESS;
}

void Input::RefreshGamePadState()
{
	memcpy(prevButtonStates_, buttonStates_, sizeof(prevButtonStates_));
}

bool Input::PushButton(int button) const
{
	if (state_.Gamepad.wButtons & XINPUT_Buttons[button])
	{
		return true;
	}

	return false;
}

bool Input::TriggerButton(int button) const
{
	return buttonsTriger_[button];
}

bool Input::ReleaseButton(int button) const
{
	if (!buttonStates_[button] && prevButtonStates_[button])
	{
		return true;
	}

	return false;
}

bool Input::LStickInDeadZone() const
{
	// 左スティックの値を取得
	short x = state_.Gamepad.sThumbLX;
	short y = state_.Gamepad.sThumbLY;

	// デッドゾーンの設定
	if (x > XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE || x < -XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE)
	{
		return false;
	}

	if (y > XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE || y < -XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE)
	{
		return false;
	}

	return true;
}

bool Input::RStickInDeadZone() const
{
	// 右スティックの値を取得
	short x = state_.Gamepad.sThumbRX;
	short y = state_.Gamepad.sThumbRY;

	// デッドゾーンの設定
	if (x > XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE || x < -XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE)
	{
		return false;
	}

	if (y > XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE || y < -XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE)
	{
		return false;
	}

	return true;
}

Vector2 Input::GetLeftStick()
{
	// 左スティックの値を取得
	short x = state_.Gamepad.sThumbLX;
	short y = state_.Gamepad.sThumbLY;

	return Vector2(static_cast<float>(x) / 32768.0f, static_cast<float>(y) / 32768.0f);
}

Vector2 Input::GetRightStick()
{
	// 右スティックの値を取得
	short x = state_.Gamepad.sThumbRX;
	short y = state_.Gamepad.sThumbRY;

	return Vector2(static_cast<float>(x) / 32768.0f, static_cast<float>(y) / 32768.0f);
}

float Input::GetLeftTrigger()
{
	// 左トリガーの値を取得
	BYTE trigger = state_.Gamepad.bLeftTrigger;

	if (trigger > XINPUT_GAMEPAD_TRIGGER_THRESHOLD)
	{
		return static_cast<float>(trigger) / 255.0f;
	}

	return 0.0f;
}

float Input::GetRightTrigger()
{
	// 右トリガーの値を取得
	BYTE trigger = state_.Gamepad.bRightTrigger;

	if (trigger > XINPUT_GAMEPAD_TRIGGER_THRESHOLD)
	{
		return static_cast<float>(trigger) / 255.0f;
	}

	return 0.0f;
}

void Input::SetVibration(float leftMotor, float rightMotor)
{
	// モーターの振動設定
	XINPUT_VIBRATION vibration;
	ZeroMemory(&vibration, sizeof(XINPUT_VIBRATION));

	vibration.wLeftMotorSpeed = static_cast<WORD>(leftMotor * 65535.0f);
	vibration.wRightMotorSpeed = static_cast<WORD>(rightMotor * 65535.0f);

	XInputSetState(0, &vibration);
}

void Input::StopVibration()
{
	// モーターの振動停止
	XINPUT_VIBRATION vibration;
	ZeroMemory(&vibration, sizeof(XINPUT_VIBRATION));

	vibration.wLeftMotorSpeed = 0;
	vibration.wRightMotorSpeed = 0;

	XInputSetState(0, &vibration);
}

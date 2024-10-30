#include "Input.h"
#include <cassert>
#include "Vector2.h"


Input* Input::instance_ = nullptr;

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



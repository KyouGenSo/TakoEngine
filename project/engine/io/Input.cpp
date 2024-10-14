#include "Input.h"
#include <cassert>

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

	// キーボード情報の取得
	keyboardDevice_->Acquire();
	keyboardDevice_->GetDeviceState(sizeof(keys_), keys_);
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

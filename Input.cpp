#include "Input.h"
#include <cassert>


void Input::Initialize(HINSTANCE hInstance, HWND hWnd) {

	HRESULT hr;

// DirectInputの初期化
	// DirectInputオブジェクトの生成
	hr = DirectInput8Create(hInstance, DIRECTINPUT_VERSION, IID_IDirectInput8, reinterpret_cast<void**>(directInput.GetAddressOf()), nullptr);
	assert(SUCCEEDED(hr));

	// KeyboardDeviceの生成
	hr = directInput->CreateDevice(GUID_SysKeyboard, keyboardDevice.GetAddressOf(), NULL);
	assert(SUCCEEDED(hr));

	// KeyboardDeviceのフォーマット設定
	hr = keyboardDevice->SetDataFormat(&c_dfDIKeyboard);
	assert(SUCCEEDED(hr));

	// KeyboardDeviceの協調レベル設定
	hr = keyboardDevice->SetCooperativeLevel(hWnd, DISCL_FOREGROUND | DISCL_NONEXCLUSIVE | DISCL_NOWINKEY);
	assert(SUCCEEDED(hr));
}

void Input::Update() {
	// 前フレームのキーボード入力状態を保存
	memcpy(prevKeys, keys, sizeof(keys));

	// キーボード情報の取得
	keyboardDevice->Acquire();
	keyboardDevice->GetDeviceState(sizeof(keys), keys);
}

bool Input::PushKey(BYTE keyNum) const
{
	if (keys[keyNum]) 
		return true;

	return false;
}

bool Input::TriggerKey(BYTE keyNum) const
{
	if (keys[keyNum] && !prevKeys[keyNum])
		return true;

	return false;
}

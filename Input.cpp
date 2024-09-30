#include "Input.h"
#include <cassert>


void Input::Initialize(HINSTANCE hInstance, HWND hWnd) {

	HRESULT hr;

// DirectInputの初期化
	ComPtr<IDirectInput8> directInput = nullptr;
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
	// キーボード情報の取得
	keyboardDevice->Acquire();
	// キーボードの入力状態を取得
	keys[256] = {};
	keyboardDevice->GetDeviceState(sizeof(keys), keys);
}
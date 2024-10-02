#pragma once
#include <Windows.h>
#include<wrl.h>

#define DIRECTINPUT_VERSION 0x0800 // DirectInputのバージョン指定
#include <dinput.h>
#pragma comment(lib, "dinput8.lib")
#pragma comment(lib, "dxguid.lib")

class Input {
public:
	// ComPtrのエイリアス
	template<class T> using ComPtr = Microsoft::WRL::ComPtr<T>;

public:
	/// <summary>
	/// 初期化
	/// </summary>
	void Initialize(HINSTANCE hInstance, HWND hWnd);

	/// <summary>
	/// 更新
	/// </summary>
	void Update();

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

private:
	// DirectInputオブジェクト
	ComPtr<IDirectInput8> directInput;

	// キーボードデバイス
	ComPtr<IDirectInputDevice8> keyboardDevice;

	// キーボードの入力状態
	BYTE keys[256] = {};

	// 前フレームのキーボード入力状態
	BYTE prevKeys[256] = {};

};
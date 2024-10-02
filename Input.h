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

private:
	// キーボードデバイス
	ComPtr<IDirectInputDevice8> keyboardDevice;	

};

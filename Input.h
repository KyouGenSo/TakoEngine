#pragma once
#include <Windows.h>

class Input {
public:
	/// <summary>
	/// 初期化
	/// </summary>
	void Initialize(HINSTANCE hInstance, HWND hWnd);

	/// <summary>
	/// 更新
	/// </summary>
	void Update();
};

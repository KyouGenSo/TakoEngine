#pragma once
#include<Windows.h>
#include<cstdint>


class WinApp {
public:
	/// <summary>
	/// 初期化
	/// </summary>
	void Initialize();

	/// <summary>
	/// 更新処理
	/// </summary>
	void Update();

	/// <summary>
	/// ウィンドウプロシージャ
	/// </summary>
	static LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wparam, LPARAM lparam);

	/// <summary>
	/// ウィンドウハンドルの取得
	/// </summary>
	HWND GetHWnd() const { return hWnd; }

	/// <summary>
	/// hInstanceの取得
	/// </summary>
	HINSTANCE GetHInstance() const { return wc.hInstance; }

public:
	//クライアント領域のサイズ
	static const int32_t kClientWidth = 1280;
	static const int32_t kClientHeight = 720;

private:
	//ウィンドウハンドル
	HWND hWnd = nullptr;

	//ウィンドウクラス
	WNDCLASS wc{};
};
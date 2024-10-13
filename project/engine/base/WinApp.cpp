#pragma comment(lib, "winmm.lib")

#include "WinApp.h"

#include <cassert>
#include"imgui_impl_win32.h"
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

void WinApp::Initialize()
{
	// システムタイマーの分解能を上げる
	timeBeginPeriod(1);

	// COMの初期化
	HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
	assert(SUCCEEDED(hr));

	//ウィンドウプロシージャ
	wc_.lpfnWndProc = WndProc;
	//クラス名
	wc_.lpszClassName = L"TakoEngineWindowClass";
	//インスタンスハンドル
	wc_.hInstance = GetModuleHandle(nullptr);
	//カーソル
	wc_.hCursor = LoadCursor(nullptr, IDC_ARROW);

	//ウィンドウクラスを登録
	RegisterClass(&wc_);

	//ウィンドウサイズを表す構造体にクライアント領域のサイズを入れる
	RECT wrc = { 0, 0, kClientWidth, kClientHeight };

	//ウィンドウサイズを補正してウィンドウのサイズを計算
	AdjustWindowRect(&wrc, WS_OVERLAPPEDWINDOW, FALSE);

	//ウィンドウの生成
	hWnd_ = CreateWindow(
		wc_.lpszClassName,	    //クラス名
		L"TakoEngine",	        //タイトルバーの文字列
		WS_OVERLAPPEDWINDOW,	//ウィンドウスタイル
		CW_USEDEFAULT,		    //表示X座標
		CW_USEDEFAULT,		    //表示Y座標
		wrc.right - wrc.left,	//ウィンドウ幅
		wrc.bottom - wrc.top,	//ウィンドウ高さ
		nullptr,		        //親ウィンドウハンドル
		nullptr,		        //メニューハンドル
		wc_.hInstance,		    //インスタンスハンドル
		nullptr);		        //追加パラメータ

	//ウィンドウを表示
	ShowWindow(hWnd_, SW_SHOW);
}

bool WinApp::ProcessMessage()
{
	MSG msg;

	//メッセージがある限りループ
	while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
	{
		//メッセージを処理
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}

	//ウィンドウが破棄されたらTrueを返す
	if (msg.message == WM_QUIT)
	{
		return true;
	}

	return false;
}



void WinApp::Finalize()
{
	//ウィンドウを破棄
	CloseWindow(hWnd_);
	// COMの終了処理
	CoUninitialize();
}

LRESULT WinApp::WndProc(HWND hWnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
	if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wparam, lparam))
	{
		return true;
	}
	//メッセージによって処理を分岐
	switch (msg)
	{
		//ウィンドウが破棄されたとき
	case WM_DESTROY:
		//メッセージループを終了
		PostQuitMessage(0);
		break;
	}
	return DefWindowProc(hWnd, msg, wparam, lparam);
}

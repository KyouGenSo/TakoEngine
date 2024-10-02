#include "WinApp.h"
#include"externals/imgui/imgui.h"
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

void WinApp::Initialize()
{
	// COMの初期化
	CoInitializeEx(nullptr, COINIT_MULTITHREADED);

	WNDCLASS wc{};
	//ウィンドウプロシージャ
	wc.lpfnWndProc = WndProc;
	//クラス名
	wc.lpszClassName = L"TakoEngineWindowClass";
	//インスタンスハンドル
	wc.hInstance = GetModuleHandle(nullptr);
	//カーソル
	wc.hCursor = LoadCursor(nullptr, IDC_ARROW);

	//ウィンドウクラスを登録
	RegisterClass(&wc);

	//クライアント領域のサイズ
	const int32_t kClientWidth = 1280;
	const int32_t kClientHeight = 720;

	//ウィンドウサイズを表す構造体にクライアント領域のサイズを入れる
	RECT wrc = { 0, 0, kClientWidth, kClientHeight };

	//ウィンドウサイズを補正してウィンドウのサイズを計算
	AdjustWindowRect(&wrc, WS_OVERLAPPEDWINDOW, FALSE);

	//ウィンドウの生成
	HWND hWnd = CreateWindow(
		wc.lpszClassName,	    //クラス名
		L"TakoEngine",	        //タイトルバーの文字列
		WS_OVERLAPPEDWINDOW,	//ウィンドウスタイル
		CW_USEDEFAULT,		    //表示X座標
		CW_USEDEFAULT,		    //表示Y座標
		wrc.right - wrc.left,	//ウィンドウ幅
		wrc.bottom - wrc.top,	//ウィンドウ高さ
		nullptr,		        //親ウィンドウハンドル
		nullptr,		        //メニューハンドル
		wc.hInstance,		    //インスタンスハンドル
		nullptr);		        //追加パラメータ

	//ウィンドウを表示
	ShowWindow(hWnd, SW_SHOW);
}

void WinApp::Update()
{

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

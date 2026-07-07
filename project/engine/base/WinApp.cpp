#pragma comment(lib, "winmm.lib")

#include "WinApp.h"

#include <algorithm>
#include <cassert>
#ifdef _DEBUG
#include"imgui_impl_win32.h"
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
#endif

namespace Tako {

  std::unique_ptr<WinApp> WinApp::instance_ = nullptr;

  std::vector<IWndProcHandler*> WinApp::handlers_;

  int32_t WinApp::clientWidth = 1280;

  int32_t WinApp::clientHeight = 720;

  std::wstring WinApp::windowTitle_ = L"TakoEngine";

  void WinApp::Initialize()
  {
    timeBeginPeriod(1);

    // COM の初期化
    HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    assert(SUCCEEDED(hr));

    wc_.lpfnWndProc = WndProc;
    wc_.lpszClassName = L"TakoEngineWindowClass";
    wc_.hInstance = GetModuleHandle(nullptr);
    wc_.hCursor = LoadCursor(nullptr, IDC_ARROW);

    RegisterClass(&wc_);

    //ウィンドウサイズを表す構造体にクライアント領域のサイズを入れる
    RECT wrc = { 0, 0, clientWidth, clientHeight };

    //ウィンドウサイズを補正してウィンドウのサイズを計算
    AdjustWindowRect(&wrc, WS_OVERLAPPEDWINDOW, FALSE);

    //ウィンドウの生成
    hWnd_ = CreateWindow(
      wc_.lpszClassName,             //クラス名
      windowTitle_.c_str(),                //タイトルバーの文字列
      WS_OVERLAPPEDWINDOW,  // サイズ変更可能で最大化ボタンも有効なウィンドウスタイル
      CW_USEDEFAULT,               //表示 X 座標
      CW_USEDEFAULT,              //表示 Y 座標
      wrc.right - wrc.left,      //ウィンドウ幅
      wrc.bottom - wrc.top,      //ウィンドウ高さ
      nullptr,                  //親ウィンドウハンドル
      nullptr,                  //メニューハンドル
      wc_.hInstance,            //インスタンスハンドル
      nullptr);                //追加パラメータ

    //ウィンドウを表示
    ShowWindow(hWnd_, SW_SHOW);
  }

  bool WinApp::ProcessMessage()
  {
    MSG msg;

    while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
      TranslateMessage(&msg);
      DispatchMessage(&msg);
    }

    if (msg.message == WM_QUIT) {
      return true;
    }

    return false;
  }

  void WinApp::Finalize()
  {
    CloseWindow(hWnd_);
    // COM の終了処理
    CoUninitialize();

    instance_.reset();
  }

  LRESULT WinApp::WndProc(HWND hWnd, UINT msg, WPARAM wparam, LPARAM lparam)
  {
    for (auto handler : handlers_) {
      handler->OnWndProc(hWnd, msg, wparam, lparam);
    }

#ifdef _DEBUG
    if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wparam, lparam)) {
      return true;
    }
#endif

    switch (msg) {
    case WM_DESTROY:
      PostQuitMessage(0);
      break;

    case WM_SIZE:
    {
      WinApp* instance = GetInstance();

      int width = LOWORD(lparam);
      int height = HIWORD(lparam);

      switch (wparam) {
      case SIZE_MAXIMIZED:
        instance->isMaximized_ = true;
        instance->SetWindowSize(width, height);

        instance->NotifyResize(width, height);
        break;

      case SIZE_RESTORED:
        if (instance->isMaximized_) {
          instance->isMaximized_ = false;
        }
        instance->SetWindowSize(width, height);

        instance->NotifyResize(width, height);
        break;

      case SIZE_MINIMIZED:
        // 最小化時はリサイズ通知しない
        break;
      }
    }
    break;

    default:;
    }

    return DefWindowProc(hWnd, msg, wparam, lparam);
  }

  void WinApp::SetWindowTitle(const std::wstring& title)
  {
    windowTitle_ = title;
  }

  void WinApp::ToggleFullScreen()
  {
    if (!isFullScreen_) {
      // ウィンドウスタイルを保存
      LONG currentStyle = GetWindowLong(hWnd_, GWL_STYLE);

      // 現在のウィンドウ位置とサイズを保存
      GetWindowRect(hWnd_, &windowedRect_);

      // フルスクリーン用のウィンドウスタイルに変更（ボーダーなし）
      SetWindowLong(hWnd_, GWL_STYLE, currentStyle & ~(WS_CAPTION | WS_THICKFRAME));

      // モニターのサイズを取得
      HMONITOR monitor = MonitorFromWindow(hWnd_, MONITOR_DEFAULTTONEAREST);
      MONITORINFO mi = { sizeof(mi) };
      GetMonitorInfo(monitor, &mi);

      // ウィンドウをモニターサイズに合わせる
      SetWindowPos(hWnd_, HWND_TOP,
        mi.rcMonitor.left, mi.rcMonitor.top,
        mi.rcMonitor.right - mi.rcMonitor.left,
        mi.rcMonitor.bottom - mi.rcMonitor.top,
        SWP_NOOWNERZORDER | SWP_FRAMECHANGED);

      // ウィンドウのサイズを取得
      RECT clientRect;
      GetClientRect(hWnd_, &clientRect);
      // クライアント領域のサイズを保存
      SetWindowSize(clientRect.right - clientRect.left, clientRect.bottom - clientRect.top);

      // 状態を更新
      isFullScreen_ = true;
    }
    else {
      // 元のウィンドウスタイルに戻す（最大化ボタンあり）
      LONG currentStyle = GetWindowLong(hWnd_, GWL_STYLE);
      SetWindowLong(hWnd_, GWL_STYLE, currentStyle | WS_OVERLAPPEDWINDOW);

      // 保存していた位置とサイズに戻す
      SetWindowPos(hWnd_, HWND_TOP,
        windowedRect_.left, windowedRect_.top,
        windowedRect_.right - windowedRect_.left,
        windowedRect_.bottom - windowedRect_.top,
        SWP_NOOWNERZORDER | SWP_FRAMECHANGED);

      // ウィンドウのサイズを取得
      RECT clientRect;
      GetClientRect(hWnd_, &clientRect);
      // クライアント領域のサイズを保存
      SetWindowSize(clientRect.right - clientRect.left, clientRect.bottom - clientRect.top);

      // 状態を更新
      isFullScreen_ = false;
    }

    NotifyResize(clientWidth, clientHeight);
  }

  void WinApp::NotifyResize(int width, int height)
  {
    if (onResizeFuncs_.empty()) return;
    Vector2 newSize = { .x = static_cast<float>(width), .y = static_cast<float>(height) };
    for (const auto& entry : onResizeFuncs_) {
      entry.callback(newSize);
    }
  }

  uint32_t WinApp::RegisterOnResizeFunc(const std::function<void(Vector2)>& onResizeFunc)
  {
    uint32_t id = nextId_++;
    onResizeFuncs_.push_back({ .callback = onResizeFunc, .id = id });
    return id;
  }

  void WinApp::UnregisterOnResizeFunc(uint32_t id)
  {
    onResizeFuncs_.erase(
      std::remove_if(onResizeFuncs_.begin(), onResizeFuncs_.end(),
        [id](const ResizeCallbackEntry& entry) { return entry.id == id; }),
      onResizeFuncs_.end());
  }

  void WinApp::MaximizeWindow()
  {
    ShowWindow(hWnd_, SW_MAXIMIZE);
  }

} // namespace Tako

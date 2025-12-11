#pragma comment(lib, "winmm.lib")

#include "WinApp.h"

#include <algorithm>
#include <cassert>
#ifdef _DEBUG
#include"imgui_impl_win32.h"
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
#endif

namespace Tako {

// instanceの初期化
WinApp* WinApp::instance_ = nullptr;

std::vector<IWndProcHandler*> WinApp::m_handlers_;

int32_t WinApp::clientWidth = 1280;

int32_t WinApp::clientHeight = 720;

std::wstring WinApp::windowTitle_ = L"TakoEngine";

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
  RECT wrc = { 0, 0, clientWidth, clientHeight };

  //ウィンドウサイズを補正してウィンドウのサイズを計算
  AdjustWindowRect(&wrc, WS_OVERLAPPEDWINDOW, FALSE);

  //ウィンドウの生成
  hWnd_ = CreateWindow(
    wc_.lpszClassName,             //クラス名
    windowTitle_.c_str(),                //タイトルバーの文字列
    WS_OVERLAPPEDWINDOW,  // サイズ変更可能で最大化ボタンも有効なウィンドウスタイル
    CW_USEDEFAULT,               //表示X座標
    CW_USEDEFAULT,              //表示Y座標
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

  // instance_削除
  if (instance_ != nullptr)
  {
    delete instance_;
    instance_ = nullptr;
  }
}

LRESULT WinApp::WndProc(HWND hWnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
  // handlerがあれば関数を呼び出す
  for (auto handler : m_handlers_)
  {
    handler->OnWndProc(hWnd, msg, wparam, lparam);
  }

#ifdef _DEBUG
  if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wparam, lparam))
  {
    return true;
  }
#endif

  //メッセージによって処理を分岐
  switch (msg)
  {
    //ウィンドウが破棄されたとき
  case WM_DESTROY:
    //メッセージループを終了
    PostQuitMessage(0);
    break;

  // ウィンドウサイズが変更されたとき
  case WM_SIZE:
    {
      // インスタンスを取得
      WinApp* instance = GetInstance();

      // クライアント領域の新しいサイズを取得
      int width = LOWORD(lparam);
      int height = HIWORD(lparam);

      // wparamに基づいて最大化状態を更新
      switch (wparam)
      {
      case SIZE_MAXIMIZED:
        // 最大化された
        instance->isMaximized_ = true;
        instance->SetWindowSize(width, height);

        // OnResize関数があれば呼び出す
        if (!instance->onResizeFuncs_.empty()) {
          Vector2 newSize = { .x = static_cast<float>(width), .y = static_cast<float>(height) };
          for (const auto& entry : instance->onResizeFuncs_) {
            entry.callback(newSize);
          }
        }
        break;

      case SIZE_RESTORED:
        // 通常状態に戻った
        if (instance->isMaximized_) {
          instance->isMaximized_ = false;
        }
        instance->SetWindowSize(width, height);

        // OnResize関数があれば呼び出す
        if (!instance->onResizeFuncs_.empty()) {
          Vector2 newSize = { .x = static_cast<float>(width), .y = static_cast<float>(height) };
          for (const auto& entry : instance->onResizeFuncs_) {
            entry.callback(newSize);
          }
        }
        break;

      case SIZE_MINIMIZED:
        // 最小化された（特に処理なし）
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
  if (!isFullScreen_)
  {
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
  } else
  {
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

  // OnResize関数があれば呼び出す
  if (!onResizeFuncs_.empty()) {
    Vector2 newSize = { .x = static_cast<float>(clientWidth), .y = static_cast<float>(clientHeight) };
    for (const auto& entry : onResizeFuncs_) {
      entry.callback(newSize);
    }
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
  // ウィンドウを最大化
  ShowWindow(hWnd_, SW_MAXIMIZE);
}

} // namespace Tako

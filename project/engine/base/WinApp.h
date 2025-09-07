#pragma once
#include<Windows.h>
#include<cstdint>
#include <functional>
#include <string>
#include <vector>

#include "IWndProcHandler.h"
#include "Vector2.h"

class WinApp {
private: // シングルトン設定
  // シングルトンインスタンス
  static WinApp* instance_;
  WinApp() = default;
  ~WinApp() = default;

public:
  // コピーコンストラクタと代入演算子を削除
  WinApp(const WinApp&) = delete;
  WinApp& operator=(const WinApp&) = delete;

  // シングルトンインスタンスの取得
  static WinApp* GetInstance() {
    if (instance_ == nullptr) {
      instance_ = new WinApp();
    }
    return instance_;
  }

public: // メンバ関数
  /// <summary>
  /// 初期化
  /// </summary>
  void Initialize();

  /// <summary>
  /// メッセージの処理
  /// </summary>
  bool ProcessMessage();

  /// <summary>
  /// 終了処理
  /// </summary>
  void Finalize();

  /// <summary>
  /// ウィンドウプロシージャ
  /// </summary>
  static LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wparam, LPARAM lparam);

  /// <summary>
  /// ウィンドウハンドルの取得
  /// </summary>
  HWND GetHWnd() const { return hWnd_; }

  /// <summary>
  /// hInstanceの取得
  /// </summary>
  HINSTANCE GetHInstance() const { return wc_.hInstance; }

  /// <summary>
  /// ハンドラの設定
  /// </summary>
  /// <param name="handler"></param>
  void SetWndProcHandler(IWndProcHandler* handler) { m_handlers_.push_back(handler); }

  /// <summary>
  /// ウィンドウのサイズを設定
  /// </summary>
  void SetWindowSize(int32_t width, int32_t height) { clientWidth = width; clientHeight = height; }

  /// <summary>
  /// ウィンドウのタイトルを設定
  /// </summary>
  /// <param name="title">新しいタイトル</param>
  void SetWindowTitle(const std::wstring& title);

  /// <summary>
  /// フルスクリーン切り替え
  /// </summary>
  void ToggleFullScreen();

  /// <summary>
  /// フルスクリーン状態の取得
  /// </summary>
  bool IsFullScreen() const { return isFullScreen_; }

  /// <summary>
  /// OnResize関数の登録
  /// <summary>
  /// <param name="onResizeFunc"></param>
  uint32_t RegisterOnResizeFunc(const std::function<void(Vector2)>& onResizeFunc);

  /// <summary>
  /// OnResize関数の削除
  /// <summary>
  /// <param name="id"></param>
  void UnregisterOnResizeFunc(uint32_t id);

public:
  //クライアント領域のサイズ
  static int32_t clientWidth;
  static int32_t clientHeight;

private:
  struct ResizeCallbackEntry {
    std::function<void(Vector2)> callback;
    uint32_t id;
  };

private:
  //ウィンドウハンドル
  HWND hWnd_ = nullptr;

  //ウィンドウクラス
  WNDCLASS wc_{};

  // handlers
  static std::vector<IWndProcHandler*> m_handlers_;

  // フルスクリーン状態を保持
  bool isFullScreen_ = false;

  // ウィンドウモード時の位置とサイズを保存
  RECT windowedRect_ = {};

  // コールバック関数のリスト
  std::vector<ResizeCallbackEntry> onResizeFuncs_;
  // コールバック関数のID
  uint32_t nextId_ = 1u;

  // タイトルバーの文字列
  static std::wstring windowTitle_;
};
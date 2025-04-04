#pragma once
#include<Windows.h>
#include<cstdint>
#include "IWndProcHandler.h"
#include <vector>


class WinApp {
public:
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
  /// フルスクリーン切り替え
  /// </summary>
  void ToggleFullScreen();


  // フルスクリーン状態の取得
  bool IsFullScreen() const { return isFullScreen_; }

public:
	//クライアント領域のサイズ
  static int32_t clientWidth;
	static int32_t clientHeight;

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
};
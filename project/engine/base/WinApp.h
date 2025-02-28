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

public:
	//クライアント領域のサイズ
	static const int32_t kClientWidth = 1280;
	static const int32_t kClientHeight = 720;

private:
	//ウィンドウハンドル
	HWND hWnd_ = nullptr;

	//ウィンドウクラス
	WNDCLASS wc_{};

  // handlers
  static std::vector<IWndProcHandler*> m_handlers_;
};
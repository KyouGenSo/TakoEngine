#pragma once
#include<Windows.h>
#include<cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "IWndProcHandler.h"
#include "Vector2.h"

namespace Tako {

/// <summary>
/// Windowsアプリケーション管理クラス
/// ウィンドウ生成とメッセージ処理を担当
/// </summary>
class WinApp {
private: // シングルトン設定
  static std::unique_ptr<WinApp> instance_;

  WinApp() = default;
  ~WinApp() = default;

  friend struct std::default_delete<WinApp>;

public:
  WinApp(const WinApp&) = delete;
  WinApp& operator=(const WinApp&) = delete;

  /// <summary>
  /// シングルトンインスタンスの取得
  /// </summary>
  /// <returns>WinAppのシングルトンインスタンス</returns>
  static WinApp* GetInstance() {
    if (!instance_) {
      instance_ = std::unique_ptr<WinApp>(new WinApp());
    }
    return instance_.get();
  }

public: // メンバ関数
  /// <summary>
  /// 初期化
  /// </summary>
  void Initialize();

  /// <summary>
  /// メッセージの処理
  /// </summary>
  /// <returns>アプリケーション続行フラグ（false: 終了、true: 継続）</returns>
  bool ProcessMessage();

  /// <summary>
  /// 終了処理
  /// </summary>
  void Finalize();

  /// <summary>
  /// ウィンドウプロシージャ
  /// </summary>
  /// <param name="hWnd">ウィンドウハンドル</param>
  /// <param name="msg">メッセージID</param>
  /// <param name="wparam">メッセージパラメータ1</param>
  /// <param name="lparam">メッセージパラメータ2</param>
  /// <returns>メッセージ処理結果</returns>
  static LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wparam, LPARAM lparam);

  /// <summary>
  /// ウィンドウハンドルの取得
  /// </summary>
  /// <returns>ウィンドウハンドル</returns>
  HWND GetHWnd() const { return hWnd_; }

  /// <summary>
  /// hInstanceの取得
  /// </summary>
  /// <returns>アプリケーションインスタンスハンドル</returns>
  HINSTANCE GetHInstance() const { return wc_.hInstance; }

  /// <summary>
  /// ハンドラの設定
  /// </summary>
  /// <param name="handler">登録するウィンドウプロシージャハンドラ</param>
  void SetWndProcHandler(IWndProcHandler* handler) { m_handlers_.push_back(handler); }

  /// <summary>
  /// ウィンドウのサイズを設定
  /// </summary>
  /// <param name="width">ウィンドウの幅（ピクセル）</param>
  /// <param name="height">ウィンドウの高さ（ピクセル）</param>
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
  /// <returns>フルスクリーン状態フラグ（true: フルスクリーン、false: ウィンドウモード）</returns>
  bool IsFullScreen() const { return isFullScreen_; }

  /// <summary>
  /// 最大化状態の取得
  /// </summary>
  /// <returns>最大化状態フラグ（true: 最大化、false: 通常サイズ）</returns>
  bool IsMaximized() const { return isMaximized_; }

  /// <summary>
  /// ウィンドウの最大化
  /// </summary>
  void MaximizeWindow();

  /// <summary>
  /// OnResize関数の登録
  /// </summary>
  /// <param name="onResizeFunc">リサイズ時に呼び出されるコールバック関数</param>
  /// <returns>登録されたコールバックの一意識別ID</returns>
  uint32_t RegisterOnResizeFunc(const std::function<void(Vector2)>& onResizeFunc);

  /// <summary>
  /// OnResize関数の削除
  /// </summary>
  /// <param name="id">削除するコールバックの識別ID</param>
  void UnregisterOnResizeFunc(uint32_t id);

public:
  static int32_t clientWidth;  ///< クライアント領域の幅（ピクセル）
  static int32_t clientHeight;  ///< クライアント領域の高さ（ピクセル）

private:
	/// <summary>
	/// リサイズコールバック登録エントリー
	/// </summary>
	struct ResizeCallbackEntry {
		std::function<void(Vector2)> callback;  ///< ウィンドウリサイズ時に呼び出されるコールバック関数
		uint32_t id;  ///< このコールバックの一意識別子（登録解除時に使用）
	};

private:
	HWND hWnd_ = nullptr;  ///< ウィンドウハンドル

	WNDCLASS wc_{};  ///< ウィンドウクラス情報

	static std::vector<IWndProcHandler*> m_handlers_;  ///< ウィンドウメッセージ処理ハンドラのリスト

	bool isFullScreen_ = false;  ///< フルスクリーン状態フラグ

	bool isMaximized_ = false;  ///< 最大化状態フラグ

	RECT windowedRect_ = {};  ///< ウィンドウモード時の位置とサイズ（フルスクリーンから戻る時に使用）

	std::vector<ResizeCallbackEntry> onResizeFuncs_;  ///< リサイズイベント時に呼び出されるコールバック関数のリスト
	uint32_t nextId_ = 1u;  ///< 次に割り当てるコールバックID（ユニーク保証用）

	static std::wstring windowTitle_;  ///< ウィンドウタイトルバーに表示される文字列
};

} // namespace Tako
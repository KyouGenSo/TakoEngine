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
  /// Windows アプリケーション管理クラス
  /// ウィンドウ生成とメッセージ処理を担当
  /// </summary>
  class WinApp {
  private: // シングルトン設定
    static std::unique_ptr<WinApp> instance_;

    struct Token {};  ///< 外部からの直接生成を防ぐ生成キー
    ~WinApp() = default;

    friend struct std::default_delete<WinApp>;

  public:
    explicit WinApp(Token) {}
    WinApp(const WinApp&) = delete;
    WinApp& operator=(const WinApp&) = delete;

    /// <summary>
    /// シングルトンインスタンスの取得
    /// </summary>
    /// <returns>WinApp のシングルトンインスタンス</returns>
    static WinApp* GetInstance() {
      if (!instance_) {
        instance_ = std::make_unique<WinApp>(Token{});
      }
      return instance_.get();
    }

  private: //構造体
    /// <summary>
    /// リサイズコールバック登録エントリー
    /// </summary>
    struct ResizeCallbackEntry {
      std::function<void(Vector2)> callback;
      uint32_t id;  ///< 登録解除時に使用する識別子
    };

  public: //メンバー関数
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
    /// <param name="msg">メッセージ ID</param>
    /// <param name="wparam">メッセージパラメータ1</param>
    /// <param name="lparam">メッセージパラメータ2</param>
    /// <returns>メッセージ処理結果</returns>
    static LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wparam, LPARAM lparam);

    /// <summary>
    /// フルスクリーン切り替え
    /// </summary>
    void ToggleFullScreen();

    /// <summary>
    /// ウィンドウの最大化
    /// </summary>
    void MaximizeWindow();

    /// <summary>
    /// OnResize 関数の登録
    /// </summary>
    /// <param name="onResizeFunc">リサイズ時に呼び出されるコールバック関数</param>
    /// <returns>登録されたコールバックの一意識別 ID</returns>
    uint32_t RegisterOnResizeFunc(const std::function<void(Vector2)>& onResizeFunc);

    /// <summary>
    /// OnResize 関数の削除
    /// </summary>
    /// <param name="id">削除するコールバックの識別 ID</param>
    void UnregisterOnResizeFunc(uint32_t id);

    //============================================================
    //Setter
    //============================================================
    /// <summary>
    /// ハンドラの設定
    /// </summary>
    /// <param name="handler">登録するウィンドウプロシージャハンドラ</param>
    void SetWndProcHandler(IWndProcHandler* handler) { handlers_.push_back(handler); }

    void SetWindowSize(int32_t width, int32_t height) { clientWidth = width; clientHeight = height; }

    /// <summary>
    /// ウィンドウのタイトルを設定
    /// </summary>
    /// <param name="title">新しいタイトル</param>
    void SetWindowTitle(const std::wstring& title);

    //============================================================
    //Getter
    //============================================================
    HWND GetHWnd() const { return hWnd_; }
    HINSTANCE GetHInstance() const { return wc_.hInstance; }
    bool IsFullScreen() const { return isFullScreen_; }
    bool IsMaximized() const { return isMaximized_; }

  public: //メンバー変数
    static int32_t clientWidth;   ///< クライアント領域の幅（ピクセル）
    static int32_t clientHeight;  ///< クライアント領域の高さ（ピクセル）

  private: //非公開関数
    /// <summary>
    /// 登録済み OnResize コールバックを width/height で一括呼び出し
    /// </summary>
    void NotifyResize(int width, int height);

  private: //メンバー変数
    HWND     hWnd_ = nullptr;
    WNDCLASS wc_{};

    static std::vector<IWndProcHandler*> handlers_;

    bool isFullScreen_ = false;
    bool isMaximized_  = false;

    RECT windowedRect_ = {};  ///< フルスクリーンから戻る時に使うウィンドウモード時の位置とサイズ

    std::vector<ResizeCallbackEntry> onResizeFuncs_;
    uint32_t                         nextId_        = 1u;  ///< 次に割り当てるコールバック ID

    static std::wstring windowTitle_;
  };

} // namespace Tako
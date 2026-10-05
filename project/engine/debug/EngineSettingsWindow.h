#pragma once

#ifdef _DEBUG

#include <functional>

namespace Tako {

  /// <summary>
  /// 表示・時間・描画・音声などエンジン全体の設定を編集するウィンドウ
  /// ゲームに影響する設定は ProjectSettings、個人設定は %APPDATA%/TakoEngine/EditorSettings.json に保存する
  /// </summary>
  class EngineSettingsWindow {
  public: //定数
    static constexpr float kTimeScalePresets[] = { 0.0f, 0.25f, 0.5f, 1.0f, 2.0f };

  private: //構造体
    enum class Category { Display, Time, Rendering, Audio, Physics, Editor, Count };

  public: //メンバー関数
    /// <summary>
    /// 初期化
    /// </summary>
    /// <param name="isOpen">ウィンドウ表示フラグ（DebugUIManager の表示状態を共有する）</param>
    void Initialize(bool* isOpen) { isOpen_ = isOpen; }

    void Draw();

    /// <summary>
    /// 個人設定（フルスクリーン・音量・デバッグ描画・UI）を読み込み適用する（ファイルが無ければ何もしない）
    /// </summary>
    void LoadEditorSettings();

    //======================================================
    //Setter
    //======================================================
    /// <summary>
    /// フルスクリーン切替関数（バッファ再生成を伴うため TakoFramework から受け取る）
    /// </summary>
    void SetToggleFullScreenFunc(std::function<void()> func) { toggleFullScreen_ = std::move(func); }

    /// <summary>
    /// 現在の状態と異なる場合のみ切替関数を呼ぶ
    /// </summary>
    void SetFullScreen(bool fullScreen);

  private: //非公開関数
    void DrawDisplay();
    void DrawTime();
    void DrawRendering();
    void DrawAudio();
    void DrawPhysics();
    void DrawEditor();

    /// <summary>
    /// 個人設定の現在値を書き出す
    /// </summary>
    void SaveEditorSettings();

    void ApplyMasterVolume();

  private: //メンバー変数
    bool*                 isOpen_           = nullptr;            ///< ウィンドウ表示フラグ（DebugUIManager 所有）
    std::function<void()> toggleFullScreen_;
    Category              selectedCategory_ = Category::Display;

    //Audio
    float masterVolume_ = 1.0f;
    bool  isMuted_      = false;  ///< true の間はマスター音量 0 を適用し masterVolume_ は保持する

    //Editor
    int themeIndex_ = 0;  ///< GetImGuiThemes() の index
  };

} // namespace Tako

#endif // _DEBUG

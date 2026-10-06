#pragma once

#ifdef _DEBUG

#include "ViewportCameraController.h"
#include "Vector4.h"

#include <array>
#include <functional>
#include <map>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace Tako {

  /// <summary>
  /// エディタビューに表示する床グリッドの設定
  /// </summary>
  struct GridSettings {
    float size;      ///< 全長
    float cellSize;  ///< 1 マスの幅
  };

  /// <summary>
  /// 表示・時間・描画・音声などエンジン全体の設定を編集するウィンドウ
  /// ゲームに影響する設定は ProjectSettings、個人設定は %APPDATA%/TakoEngine/EditorSettings.json に保存する
  /// </summary>
  class EngineSettingsWindow {
  public: //定数
    static constexpr float kTimeScalePresets[] = { 0.0f, 0.25f, 0.5f, 1.0f, 2.0f };

  private: //構造体
    enum class Category { Application, Display, Time, Rendering, Audio, Physics, Input, Editor, Count };

    /// <summary>
    /// 次に押されたキー/ボタンを割り当てる先
    /// </summary>
    enum class CaptureSlot { ActionKey, ActionButton, AxisUp, AxisDown, AxisLeft, AxisRight };

    struct BindingCapture {
      std::string name;  ///< アクション名または軸名
      CaptureSlot slot;
    };

    using NameOrigins = std::map<std::string, std::string, std::less<>>;  ///< 今の名前 → 前回 Save 時の名前（Save 後に足した名前は含まない）

  public: //メンバー関数
    /// <summary>
    /// 初期化
    /// </summary>
    /// <param name="isOpen">ウィンドウ表示フラグ（DebugUIManager の表示状態を共有する）</param>
    /// <param name="startupWindows">起動時に開くウィンドウのフラグ（DebugUIManager 所有、DebugUIManager::Window で添字）</param>
    void Initialize(bool* isOpen, std::span<bool> startupWindows) {
      isOpen_         = isOpen;
      startupWindows_ = startupWindows;
    }

    void Draw();

    /// <summary>
    /// 個人設定（フルスクリーン・音量・デバッグ描画・UI・開発用起動シーン）を読み込み適用する（ファイルが無ければ何もしない）
    /// 起動時に開くウィンドウは読み込むだけで、現在の表示状態は変えない
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

    //======================================================
    //Getter
    //======================================================
    const GridSettings& GetDebugViewportGrid() const { return debugViewportGrid_; }
    const GridSettings& GetParticleEditorGrid() const { return particleEditorGrid_; }
    const ViewportCameraSettings& GetViewportCameraSettings() const { return viewportCamera_; }
    bool IsRebuildRequired() const { return isRebuildRequired_; }

  private: //非公開関数
    void DrawApplication();
    void DrawDisplay();
    void DrawTime();
    void DrawRendering();
    void DrawAudio();
    void DrawPhysics();
    void DrawInput();
    void DrawEditor();

    /// <summary>
    /// 衝突レイヤー名の編集リストと、Unity の Layer Collision Matrix 相当の三角行列
    /// </summary>
    void DrawCollisionLayers();

    /// <summary>
    /// 衝突層・入力アクション・入力軸の名前からヘッダを生成し、前回 Save 以降の改名をソース中の使用箇所へ反映する（生成先が空のものは何もしない）
    /// </summary>
    void SaveGeneratedHeaders();

    /// <summary>
    /// 名前を表示し、クリックで改名用のポップアップを開く
    /// </summary>
    /// <param name="isTaken">同じ種類の名前として既に使われているか</param>
    /// <returns>Enter で確定した新しい名前（確定していなければ nullopt）</returns>
    std::optional<std::string> DrawRenamableName(const std::string& name, const std::function<bool(const std::string&)>& isTaken);

    /// <summary>
    /// 待ち受け中なら今フレーム押されたキー/ボタンを割り当てて待ち受けを終える
    /// </summary>
    void UpdateBindingCapture();

    /// <summary>
    /// 押すと割当の待ち受けを開始/取消するボタン（待ち受け中は "..." 表示）
    /// </summary>
    void DrawCaptureButton(const char* label, const std::string& name, CaptureSlot slot);

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

    //Rendering（再構築を伴うのでドラッグ中は手元の値だけ変え、操作を終えてから適用する）
    Vector4 clearColorEdit_     = {};
    float   renderScaleEdit_    = 1.0f;
    bool    isClearColorDirty_  = false;
    bool    isRenderScaleDirty_ = false;

    //Physics
    std::optional<std::vector<std::string>> layerOrigins_;              ///< 層ごとの前回 Save 時の名前（新しく足した層は空）。改名の検出に使う
    bool                                    isLayerIdChanged_ = false;  ///< 層の削除・並べ替えをした。再ビルド・再起動まで実行中のコライダーは古い ID のまま

    //Input
    std::optional<BindingCapture> capture_;
    std::array<char, 32>          newActionName_{};
    std::array<char, 32>          newAxisName_{};
    std::array<char, 32>          renameBuffer_{};   ///< 開いている改名ポップアップの入力
    std::optional<NameOrigins>    actionOrigins_;    ///< 改名の検出に使う
    std::optional<NameOrigins>    axisOrigins_;

    //Editor
    int                    themeIndex_         = 0;                 ///< GetImGuiThemes() の index
    GridSettings           debugViewportGrid_  = { 200.0f, 2.0f };
    GridSettings           particleEditorGrid_ = { 500.0f, 1.0f };
    ViewportCameraSettings viewportCamera_;
    std::span<bool>        startupWindows_;                         ///< 起動時に開くウィンドウ（DebugUIManager 所有）

    std::map<std::string, std::string> playFromSceneByProject_;  ///< 作業ディレクトリ → 開発用起動シーン。個人設定ファイルは全プロジェクト共通のため分けて持つ

    //コード生成
    bool isRebuildRequired_ = false;  ///< Save の改名でソースを書き換えた。実行中のコードは再ビルド・再起動まで古い名前のまま
  };

} // namespace Tako

#endif // _DEBUG

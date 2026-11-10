#pragma once

#ifdef _DEBUG

#include <filesystem>
#include <functional>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>
#include "imgui.h"
#include "ShortcutTable.h"

namespace Tako {

  /// <summary>
  /// resources / Script / EngineResources 配下のファイルをフォルダツリーとサムネイルで閲覧するウィンドウ
  /// </summary>
  class AssetBrowser {
  public: //定数
    static constexpr const char* kAssetPayload = "TAKO_ASSET";  ///< ファイルの D&D ペイロード型。データは作業ディレクトリからの終端付き UTF-8 パス（区切りは /）

  private: //構造体
    /// <summary>
    /// フォルダツリーのノード
    /// </summary>
    struct Folder {
      std::filesystem::path path;
      std::string           name;      ///< 表示名（UTF-8）
      std::vector<Folder>   children;
    };

    /// <summary>
    /// 表示中フォルダ内の 1 項目
    /// </summary>
    struct Entry {
      std::filesystem::path path;
      std::string           name;         ///< 表示名（UTF-8）
      std::string           textureKey;   ///< TextureManager のキー。サムネイル対象外は空
      bool                  isDirectory;
      bool                  isSelected;
    };

  public: //メンバー関数
    AssetBrowser() = default;
    ~AssetBrowser();
    AssetBrowser(const AssetBrowser&) = delete;
    AssetBrowser& operator=(const AssetBrowser&) = delete;

    /// <summary>
    /// 初期化（外部ファイルのドロップ受け付けと imgui.ini への状態保存を有効にする）
    /// </summary>
    /// <param name="isOpen">ウィンドウ表示フラグ（DebugUIManager の表示状態を共有する）</param>
    void Initialize(bool* isOpen);

    void Draw();

    /// <summary>
    /// キー操作とマウス操作の表。Draw のキー処理と About の一覧が使う
    /// </summary>
    static std::span<const ShortcutBinding<AssetBrowser>> Shortcuts();

  private: //非公開関数
    /// <summary>
    /// path 配下のサブフォルダを再帰的に集めたツリーを作る
    /// </summary>
    static Folder BuildFolder(const std::filesystem::path& path);

    /// <summary>
    /// フォルダツリー・一覧・変更監視を作り直す（表示中フォルダが消えていれば存在する親へ戻る）
    /// </summary>
    void Refresh();

    void ScanEntries();
    void CloseWatches();

    /// <summary>
    /// 表示中フォルダを変え、ツリーの展開と imgui.ini の保存を要求する
    /// </summary>
    void SetCurrentDir(std::filesystem::path dir);

    /// <summary>
    /// 外部からドロップされたファイル/フォルダを表示中フォルダへコピーする（同名は " (n)" を付ける）
    /// </summary>
    void ImportFiles(const std::vector<std::filesystem::path>& sources);

    /// <summary>
    /// Refresh ボタン・パンくずリスト・検索欄
    /// </summary>
    void DrawToolbar();

    void DrawFolderTree(const Folder& folder);
    void DrawGrid();

    /// <summary>
    /// ImGui のマルチセレクトが出した選択要求を entries_ に反映する（検索で隠れている項目は選択しない）
    /// </summary>
    void ApplySelectionRequests(const ImGuiMultiSelectIO* io);

    std::vector<std::filesystem::path> SelectedPaths() const;

    /// <summary>
    /// グリッドの 1 項目を描画する。未ロードの画像は loadBudget が残っている間だけ読み込む
    /// </summary>
    void DrawItem(const Entry& entry, int& loadBudget);

    /// <summary>
    /// 直前のアイテム（グリッドの項目・ツリーのノード）の右クリックメニュー
    /// </summary>
    /// <param name="isSelected">true なら Delete は選択中の全項目が対象（ツリーのノードは常に false）</param>
    void DrawItemContextMenu(const std::filesystem::path& path, bool isDirectory, const std::string& textureKey, bool isSelected);

    /// <summary>
    /// グリッドの空き領域の右クリックメニュー
    /// </summary>
    void DrawBackgroundContextMenu();

    /// <summary>
    /// Rename / Delete のモーダル（OpenPopup と同じ ID スコープが必要なためウィンドウ直下で呼ぶ）
    /// </summary>
    void DrawPopups();

    void RequestRename(const std::filesystem::path& path);
    void ApplyRename();

    /// <summary>
    /// 削除の確認モーダルを開く。Shift を押している間は確認を省いてすぐ削除する
    /// </summary>
    void RequestDelete(std::vector<std::filesystem::path> targets);

    /// <summary>
    /// deleteTargets_ をごみ箱へ移動する
    /// </summary>
    void ApplyDelete();

    /// <summary>
    /// parent 直下に "New Folder" を作り、続けて名前変更を開く
    /// </summary>
    void CreateFolder(const std::filesystem::path& parent);

  private: //メンバー変数
    bool* isOpen_ = nullptr;  ///< ウィンドウ表示フラグ（DebugUIManager 所有）

    //一覧
    std::vector<Folder>   roots_;
    std::vector<Entry>    entries_;
    std::filesystem::path currentDir_;
    std::filesystem::path pendingDir_;            ///< 次の Draw 冒頭で移動するフォルダ（走査中の entries_ / currentDir_ を書き換えないため）
    bool                  needsRefresh_ = true;
    bool                  revealInTree_ = false;  ///< 次のツリー描画で表示中フォルダまで展開する

    //表示
    ImGuiTextFilter filter_;
    float           thumbnailSize_ = 80.0f;

    //外部連携
    std::vector<void*> watchHandles_;  ///< ルートごとの FindFirstChangeNotification ハンドル（Windows.h をヘッダに持ち込まないため void*）
    ImVec2             windowMin_;     ///< 直近フレームのウィンドウ矩形（外部ドロップの当たり判定。非表示なら空）
    ImVec2             windowMax_;

    //ポップアップ
    std::filesystem::path              renameTarget_;
    std::vector<std::filesystem::path> deleteTargets_;
    char                               renameBuffer_[256] = "";
    bool                               openRenamePopup_   = false;
    bool                               openDeletePopup_   = false;
  };

  /// <summary>
  /// OS の関連付けアプリで開く（フォルダはエクスプローラー）
  /// </summary>
  void OpenWithShell(const std::filesystem::path& path);

  /// <summary>
  /// ソースファイルを、そのファイルをプロジェクトに含むソリューションを開いている VS で開く。そういう VS が無ければ OpenWithShell
  /// </summary>
  void OpenSourceFile(const std::filesystem::path& path);

  /// <summary>
  /// Script フォルダから「型名.h」を探して OpenSourceFile で開く
  /// </summary>
  void OpenScriptFile(std::string_view typeName);

  // アセットの種類（kind）："Texture" "Cubemap" "Model" "Material" "Scene" "Prefab" "ParticlePreset" "AnimationClip" "UILayout"
  // "PostEffectProfile" "Sound" "Script"。値（value）は、その種類を読む関数に渡す文字列（テクスチャのキー・モデルのパス・名前・型名など）

  /// <summary>
  /// path が kind のファイルなら値。違えば nullopt
  /// </summary>
  std::optional<std::string> ToAssetValue(std::string_view kind, const std::filesystem::path& path);

  /// <summary>
  /// 値のファイルのパス。見つからなければ空
  /// </summary>
  std::filesystem::path ToAssetPath(std::string_view kind, std::string_view value);

  /// <summary>
  /// kind のファイルの値の一覧（名前順）
  /// </summary>
  std::vector<std::string> ListAssets(std::string_view kind);

  /// <summary>
  /// 開いているドロップ先（BeginDragDropTarget の中）で、kind のファイルを受け取る。違う種類は受け取らず、ドロップ先も光らせない。落とされたフレームだけ値を返す
  /// </summary>
  std::optional<std::string> AcceptAssetPayload(std::string_view kind);

  /// <summary>
  /// 直前のアイテムへの kind のファイルのドロップ。落とされたフレームだけ値を返す
  /// </summary>
  std::optional<std::string> AcceptAssetDrop(std::string_view kind);

  /// <summary>
  /// kind のファイルを開く処理を登録する（Assets のダブルクリックとアセットの欄の Open が使う）
  /// </summary>
  void SetAssetOpener(std::string_view kind, std::function<void(const std::string& value)> opener);

  /// <summary>
  /// 値のファイルを開く。kind の処理が登録されていなければ、ソースファイルは VS で、他は関連付けで開く
  /// </summary>
  void OpenAsset(std::string_view kind, const std::string& value);

} // namespace Tako

#endif // _DEBUG

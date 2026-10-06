#pragma once

#ifdef _DEBUG

#include <filesystem>
#include <string>
#include <vector>
#include "imgui.h"

namespace Tako {

  /// <summary>
  /// resources / EngineResources 配下のファイルをフォルダツリーとサムネイルで閲覧するウィンドウ
  /// </summary>
  class AssetBrowser {
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
    /// グリッドの 1 項目を描画する。未ロードの画像は loadBudget が残っている間だけ読み込む
    /// </summary>
    void DrawItem(const Entry& entry, int& loadBudget);

    /// <summary>
    /// 直前のアイテム（グリッドの項目・ツリーのノード）の右クリックメニュー
    /// </summary>
    void DrawItemContextMenu(const std::filesystem::path& path, bool isDirectory, const std::string& textureKey);

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
    std::filesystem::path selectedPath_;
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
    std::filesystem::path renameTarget_;
    std::filesystem::path deleteTarget_;
    char                  renameBuffer_[256] = "";
    bool                  openRenamePopup_   = false;
    bool                  openDeletePopup_   = false;
  };

} // namespace Tako

#endif // _DEBUG

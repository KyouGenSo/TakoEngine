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
    /// <summary>
    /// 初期化
    /// </summary>
    /// <param name="isOpen">ウィンドウ表示フラグ（DebugUIManager の表示状態を共有する）</param>
    void Initialize(bool* isOpen) { isOpen_ = isOpen; }

    void Draw();

  private: //非公開関数
    /// <summary>
    /// path 配下のサブフォルダを再帰的に集めたツリーを作る
    /// </summary>
    static Folder BuildFolder(const std::filesystem::path& path);

    /// <summary>
    /// フォルダツリーと表示中フォルダの一覧を作り直す（表示中フォルダが消えていれば先頭ルートへ戻る）
    /// </summary>
    void Refresh();

    void ScanEntries();

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

  private: //メンバー変数
    bool* isOpen_ = nullptr;  ///< ウィンドウ表示フラグ（DebugUIManager 所有）

    //一覧
    std::vector<Folder>   roots_;
    std::vector<Entry>    entries_;
    std::filesystem::path currentDir_;
    std::filesystem::path pendingDir_;           ///< 次の Draw 冒頭で移動するフォルダ（走査中の entries_ / currentDir_ を書き換えないため）
    std::filesystem::path selectedPath_;
    bool                  needsRefresh_ = true;

    //表示
    ImGuiTextFilter filter_;
    float           thumbnailSize_ = 80.0f;
  };

} // namespace Tako

#endif // _DEBUG

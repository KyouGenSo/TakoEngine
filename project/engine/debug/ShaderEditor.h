#pragma once

#ifdef _DEBUG

#include <array>
#include <filesystem>
#include <memory>
#include <span>
#include <string>
#include <vector>

#include <json.hpp>

#include "EditTracker.h"
#include "ShortcutTable.h"
#include "UnsavedChangesModal.h"

namespace Tako {

  struct MaterialPropertyLayout;

  /// <summary>
  /// .shader.json の作成・編集ウィンドウ。新規作成ではひな形の HLSL も書き、HLSL は外部エディタで開く
  /// </summary>
  class ShaderEditor {
  private: //構造体
    enum class Kind : int { Object, PostEffect };

  public: //メンバー関数
    /// <summary>
    /// 初期化
    /// </summary>
    /// <param name="isOpen">ウィンドウ表示フラグ（DebugUIManager の表示状態を共有する）</param>
    void Initialize(bool* isOpen) { isOpen_ = isOpen; }

    void Draw();

    /// <summary>
    /// 編集して未保存なら out に追加する（終了時の確認用）
    /// </summary>
    void CollectUnsavedChanges(std::vector<std::string>& out) const;

    /// <summary>
    /// キー操作の表。Draw のキー処理と About の一覧が使う
    /// </summary>
    static std::span<const ShortcutBinding<ShaderEditor>> Shortcuts();

  private: //非公開関数
    /// <summary>
    /// path を読み込んで編集対象にする（Undo の履歴は捨てる）
    /// </summary>
    void Select(const std::string& path);

    bool Save();

    /// <summary>
    /// ひな形の .shader.json と HLSL を書いて登録する
    /// </summary>
    void Create(const std::string& name, Kind kind);

    /// <summary>
    /// 編集中の内容でシェーダーを登録し直す（保存はしない）
    /// </summary>
    void ApplyDocument();

    void DrawListPane();

    void DrawShaderPane();

    /// <summary>
    /// 描画物のシェーダーの項目（Forward・Queue・Fallback）
    /// </summary>
    /// <returns>内容を変えたら true</returns>
    bool DrawForward();

    /// <summary>
    /// ポストエフェクトの項目（PS・追加のテクスチャ）
    /// </summary>
    /// <returns>内容を変えたら true</returns>
    bool DrawPostEffect(const MaterialPropertyLayout* layout);

    /// <summary>
    /// Properties（値の範囲）と Defaults（値の初期値）を、PS をリフレクションした並びで編集する
    /// </summary>
    /// <returns>内容を変えたら true</returns>
    bool DrawProperties(const MaterialPropertyLayout& layout);

    /// <summary>
    /// 編集中のファイルの PS（無ければ空）
    /// </summary>
    std::string FindPixelShader() const;

    /// <summary>
    /// エンジンのファイル（ビルド時のコピーなので保存しない）
    /// </summary>
    bool IsReadOnly() const;

  private: //メンバー変数
    bool*                           isOpen_ = nullptr;   ///< ウィンドウ表示フラグ（DebugUIManager 所有）
    std::string                     selected_;           ///< 編集中の .shader.json のパス
    nlohmann::json                  document_;           ///< 編集中の内容
    std::filesystem::file_time_type fileTime_{};         ///< 読み込み・保存したときの更新時刻（外で保存されたら読み直す）
    std::array<char, 64>            newName_{};
    Kind                            newKind_ = Kind::Object;

    //未保存と Undo
    EditTracker<>        tracker_{ { [this] { return document_.dump(4); },
                                     [this](const std::string& text) { document_ = nlohmann::json::parse(text, nullptr, false); ApplyDocument(); } } };
    UnsavedChangesPrompt unsavedPrompt_;  ///< 未保存のまま別のファイルを開くときの確認
  };

} // namespace Tako

#endif // _DEBUG

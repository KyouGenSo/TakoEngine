#pragma once

#ifdef _DEBUG

#include <span>
#include <string>
#include <vector>

#include "EditTracker.h"
#include "RenderPipeline.h"
#include "ShortcutTable.h"

namespace Tako {

  /// <summary>
  /// 描画パスの編集ウィンドウ。オブジェクトパス（RenderPasses.json）の追加・並べ替えと、前のフレームの描画の手順の一覧を出す
  /// </summary>
  class RenderPipelineWindow {
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
    static std::span<const ShortcutBinding<RenderPipelineWindow>> Shortcuts();

  private: //非公開関数
    bool Save();

    void DrawPassesTab();

    /// <summary>
    /// オブジェクトパス 1 つの行
    /// </summary>
    /// <returns>設定を変えたら true</returns>
    bool DrawPass(ObjectPassSettings& pass, const std::vector<std::string>& objectNames, const std::vector<std::string>& materials);

    void DrawFrameTab() const;

  private: //メンバー変数
    bool* isOpen_ = nullptr;  ///< ウィンドウ表示フラグ（DebugUIManager 所有）

    //未保存と Undo
    EditTracker<> tracker_{ { [] { return RenderPipeline::GetInstance()->SerializeObjectPasses(); },
                              [](const std::string& text) { RenderPipeline::GetInstance()->ApplyObjectPasses(text); } } };
  };

} // namespace Tako

#endif // _DEBUG

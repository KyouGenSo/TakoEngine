#pragma once

#ifdef _DEBUG

#include <array>
#include <memory>
#include <span>
#include <string>
#include <vector>

#include "EditTracker.h"
#include "MaterialLibrary.h"
#include "PreviewPanel.h"
#include "ShortcutTable.h"

namespace Tako {

  class Material;
  class Object3d;

  /// <summary>
  /// .mat.json の編集ウィンドウ。値の UI はシェーダーのリフレクションから作り、選んだマテリアルをプレビューに映す。
  /// 描画物のマテリアルは Inspector の MeshComponent > Rendering で、ポストエフェクトのマテリアルは PostEffect Editor で使う
  /// </summary>
  class TAKO_API MaterialEditor {
  public: //メンバー関数
    MaterialEditor();
    ~MaterialEditor();

    /// <summary>
    /// 初期化
    /// </summary>
    /// <param name="isOpen">ウィンドウ表示フラグ（DebugUIManager の表示状態を共有する）</param>
    /// <param name="cameraSettings">カメラ操作設定（EngineSettingsWindow の値を共有する）</param>
    void Initialize(bool* isOpen, const ViewportCameraSettings* cameraSettings) {
      isOpen_ = isOpen;
      preview_.Initialize(L"MaterialEditorPreview", cameraSettings);
    }

    /// <summary>
    /// GPU リソースを解放
    /// </summary>
    void Finalize();

    /// <summary>
    /// プレビューの生成 / 破棄と、選んだマテリアルの反映。
    /// GPU アイドル区間で呼ぶこと（描画フェーズでのモデル差し替えは危険）
    /// </summary>
    void Update();

    /// <summary>
    /// エディターウィンドウを描画
    /// </summary>
    /// <returns>プレビューがホバー中または操作中なら true（ゲーム入力の遮断に使う）</returns>
    bool Draw();

    /// <summary>
    /// プレビューをオフスクリーン RT へ描画
    /// TakoFramework::Draw() のシーン描画後・ImGui 描画前に呼ぶ
    /// </summary>
    void DrawPreviewPass();

    /// <summary>
    /// 編集して未保存なら、そのマテリアルを out に追加する（終了時の確認用）
    /// </summary>
    void CollectUnsavedChanges(std::vector<std::string>& out) const;

    /// <summary>
    /// 未保存なら確認してから name を開く
    /// </summary>
    void RequestOpen(const std::string& name);

    /// <summary>
    /// キー操作の表。Draw のキー処理と About の一覧が使う
    /// </summary>
    static std::span<const ShortcutBinding<MaterialEditor>> Shortcuts();

  private: //非公開関数
    /// <summary>
    /// name を読み込んで編集対象にする（Undo の履歴は捨てる）
    /// </summary>
    void Select(const std::string& name);

    bool Save();

    void DrawListPane();

    /// <summary>
    /// プレビューの画像と、カメラ・メッシュの行
    /// </summary>
    /// <returns>プレビューがホバー中または操作中なら true</returns>
    bool DrawPreview();

    void DrawMaterialPane(Material& material);

    /// <summary>
    /// ポストエフェクトのマテリアルの欄（描画設定は無く、値とテクスチャだけ）
    /// </summary>
    void DrawPostEffectPane();

  private: //メンバー変数
    bool*                isOpen_     = nullptr;  ///< ウィンドウ表示フラグ（DebugUIManager 所有）
    std::string          selected_;              ///< 編集中のマテリアル名
    std::array<char, 64> newName_{};             ///< 新規作成するマテリアル名
    std::string          newShader_  = "Lit";    ///< 新規作成に使うシェーダー

    //未保存と Undo
    EditTracker<>        tracker_{ "Material Editor", { [this] { return MaterialLibrary::GetInstance()->Serialize(selected_); },
                                                        [this](const std::string& text) { MaterialLibrary::GetInstance()->Apply(selected_, text); } } };
    UnsavedChangesPrompt unsavedPrompt_;  ///< 未保存のまま別のマテリアルを開くときの確認

    //プレビュー
    PreviewPanel              preview_;
    std::unique_ptr<Object3d> previewObject_;            ///< エディタ表示中のみ生存
    std::unique_ptr<Object3d> floorObject_;
    size_t                    previewPrimitive_ = 0;     ///< kPreviewPrimitives の添字（previewModel_ が空のとき使う）
    std::string               previewModel_;             ///< 読み込み済みモデルの名前（空ならプリミティブ）
    bool                      isMeshDirty_      = true;  ///< 次の Update でモデルを差し替える（描画コマンド記録済みのフレーム内での差し替えは危険）
  };

} // namespace Tako

#endif // _DEBUG

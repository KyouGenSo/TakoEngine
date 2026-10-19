#pragma once

#ifdef _DEBUG

#include <memory>
#include <string>
#include <vector>
#include "Vector2.h"
#include "Vector3.h"
#include "Vector4.h"
#include "ColorFunc.h"
#include "PrimitiveBuilder.h"
#include "PreviewPanel.h"
#include "EditTracker.h"

namespace Tako {

  class Object3d;

  /// <summary>
  /// PrimitiveBuilder のパラメータをプレビューしながら調整し、C++ コード / JSON プリセット / OBJ として出力するエディタ
  /// </summary>
  class PrimitiveEditor {
  public: //構造体
    /// <summary>
    /// 編集対象のプリミティブ種別
    /// </summary>
    enum class PrimitiveType : int {
      Cube = 0,
      Sphere,
      Plane,
      Ring,
      Cylinder,
      Torus
    };

  public: //メンバー関数
    PrimitiveEditor();
    ~PrimitiveEditor();

    /// <summary>
    /// 初期化
    /// </summary>
    /// <param name="isOpen">ウィンドウ表示フラグ（DebugUIManager の表示状態を共有する）</param>
    /// <param name="cameraSettings">カメラ操作設定（EngineSettingsWindow の値を共有する）</param>
    void Initialize(bool* isOpen, const ViewportCameraSettings* cameraSettings) {
      isOpen_ = isOpen;
      preview_.Initialize(L"PrimitiveEditorPreview", cameraSettings);
    }

    /// <summary>
    /// GPU リソースを解放
    /// </summary>
    void Finalize();

    /// <summary>
    /// 更新（プレビュー生成/破棄、dirty 時のモデル再生成、カメラ更新）
    /// GPU アイドル区間で呼ぶこと（描画フェーズでのモデル差し替えは危険）
    /// </summary>
    void Update();

    /// <summary>
    /// エディターウィンドウを描画（ビューポート + パラメータ）
    /// </summary>
    /// <returns>プレビューがホバー中または操作中なら true（ゲーム入力の遮断に使う）</returns>
    bool Draw();

    /// <summary>
    /// プレビューをオフスクリーン RT へ描画
    /// TakoFramework::Draw() のシーン描画後・ImGui 描画前に呼ぶ
    /// </summary>
    void DrawPreviewPass();

    /// <summary>
    /// 最後に保存・読込したプリセットから変更していれば out に追加する（終了時の確認用）
    /// </summary>
    void CollectUnsavedChanges(std::vector<std::string>& out) const;

  private: //非公開関数
    /// <summary>
    /// 現在のパラメータでプレビューモデルを再生成して差し替える
    /// </summary>
    void RebuildPreview();

    /// <summary>
    /// 色/ライティング/半透明をプレビューへ再適用（Model 差し替えで消えるため毎フレーム）
    /// </summary>
    void ApplyPreviewSettings();

    /// <summary>
    /// 現在のパラメータを PrimitiveBuilder 呼び出しの C++ コード文字列に変換
    /// </summary>
    /// <returns>designated initializer 形式のコード（デフォルト値と同じフィールドは省略）</returns>
    std::string GenerateCppString() const;

    /// <summary>
    /// 現在のプリミティブ設定を JSON プリセットとして保存する。成功したら読み込みの選択もこの名前にする
    /// </summary>
    /// <param name="name">プリセット名（拡張子なし）</param>
    /// <returns>成功したら true</returns>
    bool SavePreset(const std::string& name);

    /// <summary>
    /// SavePreset が書き込む内容（JSON 文字列）
    /// </summary>
    std::string SerializePreset() const;

    /// <summary>
    /// JSON プリセットを読み込んで現在の設定へ反映
    /// </summary>
    /// <param name="name">プリセット名（拡張子なし）</param>
    /// <returns>成功したら true</returns>
    bool LoadPreset(const std::string& name);

    /// <summary>
    /// SerializePreset の内容を現在の設定へ反映し、次の Update でモデルを作り直す
    /// </summary>
    /// <returns>解釈できなければ false（途中までの反映は残る）</returns>
    bool ApplyPreset(const std::string& text);

  private: //メンバー変数
    bool* isOpen_ = nullptr;  ///< ウィンドウ表示フラグ（DebugUIManager 所有）

    //編集パラメータ
    PrimitiveType                    selectedPrimitiveType_ = PrimitiveType::Cylinder;
    PrimitiveBuilder::CubeParams     cubeParams_{};
    PrimitiveBuilder::SphereParams   sphereParams_{};
    PrimitiveBuilder::PlaneParams    planeParams_{};
    PrimitiveBuilder::RingParams     ringParams_{};
    PrimitiveBuilder::CylinderParams cylinderParams_{};
    PrimitiveBuilder::TorusParams    torusParams_{};
    std::unique_ptr<Object3d>        previewObject_;                  ///< プレビュー対象（エディタ表示中のみ生存）
    std::unique_ptr<Object3d>        floorObject_;                    ///< 床参照プレーン
    bool                             paramsDirty_           = false;  ///< 次の Update でモデル再生成（描画コマンド記録済みフレーム内での差し替えは危険）
    Vector3                          previewRotate_         = {};
    Vector3                          previewScale_          = { 1.0f, 1.0f, 1.0f };
    Vector4                          previewColor_          = Color::kWhite;
    bool                             previewLighting_       = true;
    bool                             previewTransparent_    = false;
    bool                             autoRotate_            = false;
    float                            autoRotateSpeed_       = 1.0f;   ///< 自動回転速度（rad/s）
    bool                             showFloor_             = true;
    char                             presetNameBuffer_[128] = "";
    std::string                      selectedPreset_;                 ///< Load コンボの選択中プリセット名
    float                            materialShininess_     = 15.0f;  ///< Mesh::CreateMaterialData の初期値と一致
    bool                             materialHighlight_     = true;   ///< スペキュラ有効（Mesh 初期値と一致）
    std::string                      materialTexture_;                ///< 空 = white.dds デフォルト
    Vector2                          materialUvScale_       = { 1.0f, 1.0f };
    Vector2                          materialUvOffset_      = { 0.0f, 0.0f };
    float                            materialUvRotate_      = 0.0f;   ///< ラジアン
    char                             exportNameBuffer_[128] = "";     ///< OBJ 出力名（拡張子なし）

    //プリセットの未保存と Undo
    EditTracker<>        presetTracker_{ { [this] { return SerializePreset(); }, [this](const std::string& preset) { ApplyPreset(preset); } } };
    UnsavedChangesPrompt unsavedPrompt_;  ///< 未保存のまま別のプリセットを読み込むときの確認

    //専用ビューポート/カメラ（初回オープン時に生成）
    PreviewPanel preview_;
  };

} // namespace Tako

#endif // _DEBUG

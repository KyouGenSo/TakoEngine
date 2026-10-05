#pragma once

#ifdef _DEBUG

#include <memory>
#include <string>
#include "Vector2.h"
#include "Vector3.h"
#include "Vector4.h"
#include "PrimitiveBuilder.h"
#include "ViewportCameraController.h"

namespace Tako {

  class Object3d;
  class Camera;
  class PreviewViewport;

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
    void Initialize(bool* isOpen) { isOpen_ = isOpen; }

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
    /// 現在のプリミティブ設定を JSON プリセットとして保存
    /// </summary>
    /// <param name="name">プリセット名（拡張子なし）</param>
    /// <returns>成功したら true</returns>
    bool SavePreset(const std::string& name);

    /// <summary>
    /// JSON プリセットを読み込んで現在の設定へ反映
    /// </summary>
    /// <param name="name">プリセット名（拡張子なし）</param>
    /// <returns>成功したら true</returns>
    bool LoadPreset(const std::string& name);

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
    Vector4                          previewColor_          = { 1.0f, 1.0f, 1.0f, 1.0f };
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

    //専用ビューポート/カメラ
    std::unique_ptr<PreviewViewport> previewViewport_;  ///< オフスクリーンRT一式（初回オープン時に生成）
    std::unique_ptr<Camera>          previewCamera_;
    ViewportCameraController         cameraController_;
  };

} // namespace Tako

#endif // _DEBUG

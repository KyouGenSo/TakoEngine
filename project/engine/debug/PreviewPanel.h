#pragma once

#ifdef _DEBUG

#include <memory>
#include "PostProcess.h"
#include "ViewportCameraController.h"

namespace Tako {

  class Camera;
  class PreviewViewport;

  /// <summary>
  /// エディタの 3D プレビュー。オフスクリーンの描画先・カメラ・カメラ操作と、その ImGui 表示をまとめる
  /// </summary>
  class PreviewPanel {
  public: //メンバー関数
    PreviewPanel();
    ~PreviewPanel();

    /// <summary>
    /// 初期化（描画先とカメラは最初の Update で作る）
    /// </summary>
    /// <param name="debugName">描画先のリソース名の接頭辞</param>
    /// <param name="cameraSettings">感度・FOV・Reset 時の速度（EngineSettingsWindow 所有）</param>
    void Initialize(const wchar_t* debugName, const ViewportCameraSettings* cameraSettings);

    /// <summary>
    /// 初回に描画先とカメラを作り、カメラ操作をカメラへ反映する。エディタを開いている間、更新フェーズで毎フレーム呼ぶ
    /// </summary>
    void Update();

    /// <summary>
    /// 描画先とカメラを解放する
    /// </summary>
    void Finalize();

    /// <summary>
    /// 描画先を RENDER_TARGET にしてクリアする。IsReady のときだけ呼ぶ
    /// </summary>
    void BeginPass();

    /// <summary>
    /// ImGui がサンプルできるよう描画先を PIXEL_SHADER_RESOURCE に戻す
    /// </summary>
    void EndPass();

    /// <summary>
    /// Reset Camera のボタン・移動速度・操作説明の行
    /// </summary>
    void DrawCameraToolbar();

    /// <summary>
    /// 残りの領域にプレビューを描き、カメラ操作を受ける（描画先がまだ無ければ案内の文を出す）
    /// </summary>
    /// <returns>プレビューがホバー中または操作中なら true（ゲーム入力の遮断に使う）</returns>
    bool DrawImage();

    //==============================
    //Getter
    //==============================
    /// <summary>
    /// 描画先とカメラが作られているか
    /// </summary>
    bool IsReady() const;

    /// <summary>
    /// 直前の DrawImage で画像がホバーされていたか
    /// </summary>
    bool IsImageHovered() const { return isImageHovered_; }

    Camera&                   GetCamera() { return *camera_; }
    ViewportCameraController& GetController() { return controller_; }
    PostProcessStack&         GetPostProcess() { return postProcess_; }

  private: //メンバー変数
    const wchar_t*                   debugName_      = L"Preview";
    std::unique_ptr<PreviewViewport> viewport_;
    std::unique_ptr<Camera>          camera_;
    ViewportCameraController         controller_;
    PostProcessStack                 postProcess_;                  ///< プレビューに掛けるポストエフェクト（本編とは別）
    bool                             isImageHovered_ = false;
  };

} // namespace Tako

#endif // _DEBUG

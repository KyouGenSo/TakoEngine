#pragma once

#ifdef _DEBUG

#include <memory>
#include "ViewportCameraController.h"

namespace Tako {

  class Camera;
  class PreviewViewport;
  struct GridSettings;

  /// <summary>
  /// ゲームカメラとは独立した自由視点カメラでシーンを再描画するデバッグ用ビューポート
  /// </summary>
  class DebugViewport {
  public: //メンバー関数
    DebugViewport();
    ~DebugViewport();

    /// <summary>
    /// 初期化
    /// </summary>
    /// <param name="isOpen">ウィンドウ表示フラグ（DebugUIManager の表示状態を共有する）</param>
    /// <param name="grid">床グリッド設定（EngineSettingsWindow の値を共有する）</param>
    /// <param name="cameraSettings">カメラ操作設定（EngineSettingsWindow の値を共有する）</param>
    void Initialize(bool* isOpen, const GridSettings* grid, const ViewportCameraSettings* cameraSettings) {
      isOpen_                    = isOpen;
      grid_                      = grid;
      cameraController_.settings = cameraSettings;
    }

    /// <summary>
    /// GPU リソースとカメラを解放
    /// </summary>
    void Finalize();

    /// <summary>
    /// 更新（RT/カメラの生成・非表示時の解放、カメラ操作の反映）
    /// GPU アイドル区間で呼ぶこと
    /// </summary>
    void Update();

    /// <summary>
    /// ウィンドウを描画（ツールバー + ビュー画像 + カメラ入力）
    /// </summary>
    /// <returns>ビューがホバー中または操作中なら true（ゲーム入力の遮断に使う）</returns>
    bool Draw();

    /// <summary>
    /// シーンをデバッグカメラ視点でオフスクリーン RT へ描画
    /// TakoFramework::Draw() の本編描画後・LineRenderer::Reset() 前に呼ぶ（本編の線分を流用するため）
    /// </summary>
    void DrawPass();

  private: //メンバー変数
    bool*               isOpen_ = nullptr;  ///< ウィンドウ表示フラグ（DebugUIManager 所有）
    const GridSettings* grid_   = nullptr;  ///< 床グリッド設定（EngineSettingsWindow 所有）

    //ビュー
    std::unique_ptr<PreviewViewport> viewport_;              ///< オフスクリーン RT 一式（ウィンドウ表示中のみ生存）
    std::unique_ptr<Camera>          camera_;
    ViewportCameraController         cameraController_;
    bool                             showGrid_       = true;
    bool                             showGameCamera_ = true;  ///< ゲームカメラの視錐台を表示
  };

} // namespace Tako

#endif // _DEBUG

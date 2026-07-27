#pragma once

#ifdef _DEBUG

#include "Vector3.h"

namespace Tako {

  class Camera;

  /// <summary>
  /// エディタプレビュー用オービットカメラ。注視点周りの回転/パン/ズーム状態を保持し Camera へ反映する
  /// </summary>
  struct OrbitCameraController {
  public: //メンバー関数
    /// <summary>
    /// プレビュー画像ホバー中のマウス入力で状態を更新（右ドラッグ回転 / 中ドラッグパン / ホイールズーム）
    /// </summary>
    void HandleImGuiInput();

    /// <summary>
    /// 回転から前方ベクトルを求め、注視点から distance 分引いた位置へカメラを配置し VP 行列を合成する
    /// </summary>
    /// <param name="camera">反映先カメラ</param>
    void ApplyTo(Camera& camera) const;

    /// <summary>
    /// 初期視点へ戻す
    /// </summary>
    void Reset() { *this = OrbitCameraController{}; }

  public: //メンバー変数
    float   yaw      = 0.6f;                  ///< 方位角（rad）
    float   pitch    = 0.35f;                 ///< 仰角（rad、正で見下ろし）
    float   distance = 4.0f;                  ///< 注視点からの距離
    Vector3 target   = { 0.0f, 0.0f, 0.0f };  ///< 注視点
  };

} // namespace Tako

#endif // _DEBUG

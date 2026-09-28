#pragma once

#ifdef _DEBUG

#include "Vector3.h"

namespace Tako {

  class Camera;

  /// <summary>
  /// エディタプレビュー用カメラ（UE5 ビューポート準拠）。注視点・距離・回転・移動速度を保持し Camera へ反映する
  /// </summary>
  struct ViewportCameraController {
  public: //メンバー関数
    /// <summary>
    /// 直前の ImGui アイテム（プレビュー）への入力で状態を更新する。PreviewViewport::DrawImGuiImage の直後に毎フレーム呼ぶ
    /// </summary>
    /// <returns>プレビューがホバー中または操作中なら true（ゲーム入力の遮断に使う）</returns>
    bool HandleImGuiInput();

    /// <summary>
    /// 回転から前方ベクトルを求め、注視点から distance 分引いた位置へカメラを配置し VP 行列を合成する
    /// </summary>
    /// <param name="camera">反映先カメラ</param>
    void ApplyTo(Camera& camera) const;

    /// <summary>
    /// 注視点を指定位置へ移し、距離を初期値へ戻す
    /// </summary>
    /// <param name="point">新しい注視点</param>
    void Focus(const Vector3& point);

    /// <summary>
    /// 初期視点へ戻す
    /// </summary>
    void Reset() { *this = ViewportCameraController{}; }

    /// <summary>
    /// pitch/yaw 回転を適用したカメラ前方の単位ベクトル
    /// </summary>
    Vector3 Forward() const;

    /// <summary>
    /// pitch/yaw 回転を適用したカメラ右方向の単位ベクトル
    /// </summary>
    Vector3 Right() const;

    /// <summary>
    /// pitch/yaw 回転を適用したカメラ上方向の単位ベクトル
    /// </summary>
    Vector3 Up() const;

  public: //メンバー変数
    float   yaw       = 0.6f;                  ///< 方位角（rad）
    float   pitch     = 0.35f;                 ///< 仰角（rad、正で見下ろし）
    float   distance  = 4.0f;                  ///< 注視点からの距離（Alt+左ドラッグの回転半径）
    float   moveSpeed = 5.0f;                  ///< 飛行速度（unit/s）。パン・ホイールの移動量もこれに比例
    Vector3 target    = { 0.0f, 0.0f, 0.0f };  ///< 注視点
  };

} // namespace Tako

#endif // _DEBUG

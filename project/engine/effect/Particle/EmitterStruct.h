#pragma once
#include "Vector3.h"

namespace Tako {

/// <summary>
/// 球体エミッターパラメータ構造体
/// 球面上にランダムにパーティクルを射出するエミッターの設定
/// </summary>
struct SphereEmitterParams
{
  Vector3 position;   ///< エミッター中心位置
  float radius;       ///< 球の半径
  uint32_t count;     ///< 1回の射出で生成するパーティクル数
  float frequency;    ///< 射出頻度（秒）

  Vector2 scaleRangeX = { .x = 0.0f, .y = 0.0f };  ///< X スケールの範囲[min, max]
  Vector2 scaleRangeY = { .x = 0.0f, .y = 0.0f };  ///< Y スケールの範囲[min, max]
  Vector2 velRangeX = { .x = 0.0f, .y = 0.0f };    ///< X 方向速度の範囲[min, max]
  Vector2 velRangeY = { .x = 0.0f, .y = 0.0f };    ///< Y 方向速度の範囲[min, max]
  Vector2 velRangeZ = { .x = 0.0f, .y = 0.0f };    ///< Z 方向速度の範囲[min, max]
  Vector2 lifeTimeRange = { .x = 0.0f, .y = 0.0f }; ///< パーティクル寿命の範囲[min, max]（秒）

  Vector4 startColor = { .x = 1.0f, .y = 1.0f, .z = 1.0f, .w = 1.0f };  ///< 開始時の色（RGBA）
  Vector4 endColor = { .x = 1.0f, .y = 1.0f, .z = 1.0f, .w = 1.0f };    ///< 終了時の色（RGBA）

  bool isActive = true;  ///< エミッターがアクティブかどうか
  bool isNormalize;      ///< 速度ベクトルを正規化するか
};

/// <summary>
/// 箱型エミッターパラメータ構造体
/// 直方体の内部にランダムにパーティクルを射出するエミッターの設定
/// </summary>
struct BoxEmitterParams
{
  Vector3 position;   ///< エミッター中心位置
  Vector3 size;       ///< 箱のサイズ（各軸の幅）
  Vector3 rotation;   ///< 箱の回転（オイラー角）
  uint32_t count;     ///< 1回の射出で生成するパーティクル数
  float frequency;    ///< 射出頻度（秒）

  Vector2 scaleRangeX = { .x = 0.0f, .y = 0.0f };  ///< X スケールの範囲[min, max]
  Vector2 scaleRangeY = { .x = 0.0f, .y = 0.0f };  ///< Y スケールの範囲[min, max]
  Vector2 velRangeX = { .x = 0.0f, .y = 0.0f };    ///< X 方向速度の範囲[min, max]
  Vector2 velRangeY = { .x = 0.0f, .y = 0.0f };    ///< Y 方向速度の範囲[min, max]
  Vector2 velRangeZ = { .x = 0.0f, .y = 0.0f };    ///< Z 方向速度の範囲[min, max]
  Vector2 lifeTimeRange = { .x = 0.0f, .y = 0.0f }; ///< パーティクル寿命の範囲[min, max]（秒）

  Vector4 startColor = { .x = 1.0f, .y = 1.0f, .z = 1.0f, .w = 1.0f };  ///< 開始時の色（RGBA）
  Vector4 endColor = { .x = 1.0f, .y = 1.0f, .z = 1.0f, .w = 1.0f };    ///< 終了時の色（RGBA）

  bool isActive = true;  ///< エミッターがアクティブかどうか
  bool isNormalize;      ///< 速度ベクトルを正規化するか
};

/// <summary>
/// 三角形エミッターパラメータ構造体
/// 三角形の面上にランダムにパーティクルを射出するエミッターの設定
/// </summary>
struct TriangleEmitterParams
{
  Vector3 position;   ///< エミッター基準位置
  Vector3 v1;         ///< 三角形の頂点1
  Vector3 v2;         ///< 三角形の頂点2
  Vector3 v3;         ///< 三角形の頂点3
  uint32_t count;     ///< 1回の射出で生成するパーティクル数
  float frequency;    ///< 射出頻度（秒）

  Vector2 scaleRangeX = { .x = 0.0f, .y = 0.0f };  ///< X スケールの範囲[min, max]
  Vector2 scaleRangeY = { .x = 0.0f, .y = 0.0f };  ///< Y スケールの範囲[min, max]
  Vector2 velRangeX = { .x = 0.0f, .y = 0.0f };    ///< X 方向速度の範囲[min, max]
  Vector2 velRangeY = { .x = 0.0f, .y = 0.0f };    ///< Y 方向速度の範囲[min, max]
  Vector2 velRangeZ = { .x = 0.0f, .y = 0.0f };    ///< Z 方向速度の範囲[min, max]
  Vector2 lifeTimeRange = { .x = 0.0f, .y = 0.0f }; ///< パーティクル寿命の範囲[min, max]（秒）

  Vector4 startColor = { .x = 1.0f, .y = 1.0f, .z = 1.0f, .w = 1.0f };  ///< 開始時の色（RGBA）
  Vector4 endColor = { .x = 1.0f, .y = 1.0f, .z = 1.0f, .w = 1.0f };    ///< 終了時の色（RGBA）

  bool isActive = true;  ///< エミッターがアクティブかどうか
  bool isNormalize;      ///< 速度ベクトルを正規化するか
};

} // namespace Tako
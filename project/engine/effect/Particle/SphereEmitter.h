#pragma once
#include "GPUParticleEmitter.h"

namespace Tako {

/// <summary>
/// 球形パーティクルエミッター - 球体領域からパーティクルを放出
/// </summary>
class SphereEmitter : public GPUParticleEmitter
{
public:
  /// <summary>
  /// コンストラクタ
  /// </summary>
  /// <param name="particleSystem">GPUパーティクルシステムへのポインタ</param>
  /// <param name="position">位置</param>
  /// <param name="radius">球の半径</param>
  /// <param name="count">パーティクル数</param>
  /// <param name="frequency">射出頻度（秒）</param>
  SphereEmitter(
    GPUParticle* particleSystem,
    const Vector3& position,
    float radius,
    uint32_t count,
    float frequency);

  /// <summary>
  /// デストラクタ
  /// </summary>
  ~SphereEmitter() override = default;

  /// <summary>
  /// クローンメソッド
  /// </summary>
  /// <returns>複製されたエミッター</returns>
  std::shared_ptr<GPUParticleEmitter> Clone() const override;

  /// <summary>
  /// GPUデータの設定
  /// </summary>
  /// <param name="gpuData">設定するGPUデータ</param>
  void SetupGPUData(EmitterGPUData& gpuData) const override;

  /// <summary>
  /// 球体の半径を設定
  /// </summary>
  /// <param name="radius">半径</param>
  void SetRadius(float radius);

  /// <summary>
  /// 球体の半径を取得
  /// </summary>
  /// <returns>半径</returns>
  float GetRadius() const { return data_.sphere.radius; }

  /// <summary>
  /// エミッタータイプを取得
  /// </summary>
  /// <returns>エミッタータイプ</returns>
  EmitterType GetType() const override { return EmitterType::Sphere; }
};

} // namespace Tako

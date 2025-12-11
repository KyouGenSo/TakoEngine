#pragma once
#include "GPUParticleEmitter.h"

namespace Tako {

/// <summary>
/// 三角形パーティクルエミッター - 三角形領域からパーティクルを放出
/// </summary>
class TriangleEmitter : public GPUParticleEmitter
{
public:
  /// <summary>
  /// コンストラクタ
  /// </summary>
  /// <param name="particleSystem">GPUパーティクルシステムへのポインタ</param>
  /// <param name="position">位置</param>
  /// <param name="v1">頂点1</param>
  /// <param name="v2">頂点2</param>
  /// <param name="v3">頂点3</param>
  /// <param name="count">パーティクル数</param>
  /// <param name="frequency">射出頻度（秒）</param>
  TriangleEmitter(
    GPUParticle* particleSystem,
    const Vector3& position,
    const Vector3& v1, const Vector3& v2, const Vector3& v3,
    uint32_t count,
    float frequency);

  /// <summary>
  /// デストラクタ
  /// </summary>
  ~TriangleEmitter() override = default;

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
  /// 三角形の頂点を設定
  /// </summary>
  /// <param name="v1">頂点1</param>
  /// <param name="v2">頂点2</param>
  /// <param name="v3">頂点3</param>
  void SetVertices(const Vector3& v1, const Vector3& v2, const Vector3& v3);

  /// <summary>
  /// 頂点1を取得
  /// </summary>
  /// <returns>頂点1の座標</returns>
  const Vector3& GetVertex1() const { return data_.triangle.v1; }

  /// <summary>
  /// 頂点2を取得
  /// </summary>
  /// <returns>頂点2の座標</returns>
  const Vector3& GetVertex2() const { return data_.triangle.v2; }

  /// <summary>
  /// 頂点3を取得
  /// </summary>
  /// <returns>頂点3の座標</returns>
  const Vector3& GetVertex3() const { return data_.triangle.v3; }

  /// <summary>
  /// エミッタータイプを取得
  /// </summary>
  /// <returns>エミッタータイプ</returns>
  EmitterType GetType() const override { return EmitterType::Triangle; }
};

} // namespace Tako

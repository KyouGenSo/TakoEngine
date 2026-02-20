#pragma once
#include "GPUParticleEmitter.h"

namespace Tako {

  /// <summary>
  /// ボックス型パーティクルエミッター - 直方体領域からパーティクルを放出
  /// </summary>
  class BoxEmitter : public GPUParticleEmitter
  {
  public:
    /// <summary>
    /// コンストラクタ
    /// </summary>
    /// <param name="particleSystem">GPU パーティクルシステムへのポインタ</param>
    /// <param name="position">位置</param>
    /// <param name="size">ボックスのサイズ</param>
    /// <param name="rotation">回転角度</param>
    /// <param name="count">パーティクル数</param>
    /// <param name="frequency">射出頻度（秒）</param>
    BoxEmitter(GPUParticle* particleSystem,
      const Vector3& position,
      const Vector3& size,
      const Vector3& rotation,
      uint32_t count,
      float frequency);

    /// <summary>
    /// デストラクタ
    /// </summary>
    ~BoxEmitter() override = default;

    /// <summary>
    /// クローンメソッド
    /// </summary>
    /// <returns>複製されたエミッター</returns>
    std::shared_ptr<GPUParticleEmitter> Clone() const override;

    /// <summary>
    /// GPU データの設定
    /// </summary>
    /// <param name="gpuData">設定する GPU データ</param>
    void SetupGPUData(EmitterGPUData& gpuData) const override;

    /// <summary>
    /// 箱のサイズを設定
    /// </summary>
    /// <param name="size">サイズ</param>
    void SetSize(const Vector3& size);

    /// <summary>
    /// 箱の回転を設定
    /// </summary>
    /// <param name="rotation">回転角度</param>
    void SetRotation(const Vector3& rotation);

    /// <summary>
    /// 箱のサイズを取得
    /// </summary>
    /// <returns>サイズ</returns>
    const Vector3& GetSize() const { return data_.box.size; }

    /// <summary>
    /// 箱の回転を取得
    /// </summary>
    /// <returns>回転角度</returns>
    const Vector3& GetRotation() const { return data_.box.rotation; }

    /// <summary>
    /// エミッタータイプを取得
    /// </summary>
    /// <returns>エミッタータイプ</returns>
    EmitterType GetType() const override { return EmitterType::Box; }
  };

} // namespace Tako
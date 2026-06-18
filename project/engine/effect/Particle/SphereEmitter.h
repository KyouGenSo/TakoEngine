#pragma once
#include "GPUParticleEmitter.h"

namespace Tako {

  /// <summary>
  /// 球形パーティクルエミッター - 球体領域からパーティクルを放出
  /// </summary>
  class SphereEmitter : public GPUParticleEmitter
  {
  public: //メンバー関数
    /// <summary>
    /// コンストラクタ
    /// </summary>
    /// <param name="particleSystem">GPU パーティクルシステムへのポインタ</param>
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
    /// 型固有パラメータを json に書き出す
    /// </summary>
    void SerializeTypeSpecific(nlohmann::json& json) const override;

    /// <summary>
    /// JSON から SphereEmitter を構築
    /// </summary>
    /// <param name="particleSystem">GPU パーティクルシステムへのポインタ</param>
    /// <param name="json">読み込む JSON オブジェクト</param>
    /// <returns>構築されたエミッター</returns>
    static std::shared_ptr<GPUParticleEmitter> CreateFromJSON(GPUParticle* particleSystem, const nlohmann::json& json);

    //==================================
    //Setter
    //==================================
    /// <summary>
    /// 球体の半径を設定
    /// </summary>
    /// <param name="radius">半径</param>
    void SetRadius(float radius);

    //==================================
    //Getter
    //==================================
    float GetRadius() const { return data_.radius; }
    EmitterType GetType() const override { return EmitterType::Sphere; }
  };

} // namespace Tako

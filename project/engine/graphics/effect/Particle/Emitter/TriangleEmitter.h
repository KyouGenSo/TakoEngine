#pragma once
#include "GPUParticleEmitter.h"

namespace Tako {

  /// <summary>
  /// 三角形パーティクルエミッター - 三角形領域からパーティクルを放出
  /// </summary>
  class TriangleEmitter : public GPUParticleEmitter
  {
  public: //メンバー関数
    /// <summary>
    /// コンストラクタ
    /// </summary>
    /// <param name="particleSystem">GPU パーティクルシステムへのポインタ</param>
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
    /// 型固有パラメータを json に書き出す
    /// </summary>
    void SerializeTypeSpecific(nlohmann::json& json) const override;

    /// <summary>
    /// json から型固有パラメータを読み込んで適用する
    /// </summary>
    void DeserializeTypeSpecific(const nlohmann::json& json) override;

    /// <summary>
    /// JSON から TriangleEmitter を構築
    /// </summary>
    /// <param name="particleSystem">GPU パーティクルシステムへのポインタ</param>
    /// <param name="json">読み込む JSON オブジェクト</param>
    /// <returns>構築されたエミッター</returns>
    static std::shared_ptr<GPUParticleEmitter> CreateFromJSON(GPUParticle* particleSystem, const nlohmann::json& json);

    //============================================================
    //Setter
    //============================================================
    /// <summary>
    /// 三角形の頂点を設定
    /// </summary>
    /// <param name="v1">頂点1</param>
    /// <param name="v2">頂点2</param>
    /// <param name="v3">頂点3</param>
    void SetVertices(const Vector3& v1, const Vector3& v2, const Vector3& v3);

    //============================================================
    //Getter
    //============================================================
    const Vector3& GetVertex1() const { return data_.triangleV1; }
    const Vector3& GetVertex2() const { return data_.triangleV2; }
    const Vector3& GetVertex3() const { return data_.triangleV3; }
    EmitterType GetType() const override { return EmitterType::Triangle; }
  };

} // namespace Tako

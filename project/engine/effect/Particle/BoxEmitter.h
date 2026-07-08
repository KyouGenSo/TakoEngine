#pragma once
#include "GPUParticleEmitter.h"

namespace Tako {

  /// <summary>
  /// ボックス型パーティクルエミッター - 直方体領域からパーティクルを放出
  /// </summary>
  class BoxEmitter : public GPUParticleEmitter
  {
  public: //メンバー関数
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
    /// 型固有パラメータを json に書き出す
    /// </summary>
    void SerializeTypeSpecific(nlohmann::json& json) const override;

    /// <summary>
    /// json から型固有パラメータを読み込んで適用する
    /// </summary>
    void DeserializeTypeSpecific(const nlohmann::json& json) override;

    /// <summary>
    /// JSON から BoxEmitter を構築
    /// </summary>
    /// <param name="particleSystem">GPU パーティクルシステムへのポインタ</param>
    /// <param name="json">読み込む JSON オブジェクト</param>
    /// <returns>構築されたエミッター</returns>
    static std::shared_ptr<GPUParticleEmitter> CreateFromJSON(GPUParticle* particleSystem, const nlohmann::json& json);

    //========================================
    //Setter
    //========================================
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

    //========================================
    //Getter
    //========================================
    const Vector3& GetSize() const { return data_.boxSize; }
    const Vector3& GetRotation() const { return data_.boxRotation; }
    EmitterType GetType() const override { return EmitterType::Box; }
  };

} // namespace Tako
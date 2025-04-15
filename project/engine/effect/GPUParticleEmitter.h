#pragma once
#include "Vector3.h"
#include "Vector4.h"
#include "ParticleStruct.h"
#include <memory>

// 前方宣言
class GPUParticle;

// エミッターの基底クラス
class GPUParticleEmitter : public std::enable_shared_from_this<GPUParticleEmitter>
{
public:
  // コンストラクタ・デストラクタ
  GPUParticleEmitter(GPUParticle* particleSystem, uint32_t emitterId);
  virtual ~GPUParticleEmitter();

  // クローンメソッド
  virtual std::shared_ptr<GPUParticleEmitter> Clone() const = 0;

  // GPUデータの設定
  virtual void SetupGPUData(EmitterGPUData& gpuData) const;

  // エミッターの射出更新
  void UpdateEmission(float deltaTime);

  // 共通の設定メソッド
  void SetPosition(const Vector3& position);
  void SetActive(bool isActive);
  void SetParticleCount(uint32_t count);
  void SetFrequency(float frequency);

  // 色の設定メソッド
  void SetColor(const Vector4& color); // 両方の色を同じ値に設定
  void SetStartColor(const Vector4& color);
  void SetEndColor(const Vector4& color);
  void SetColors(const Vector4& startColor, const Vector4& endColor);

  // 乱数生成範囲の設定
  void SetScaleRange(const Vector2& rangeX, const Vector2& rangeY);
  void SetScaleRangeX(const Vector2& range);
  void SetScaleRangeY(const Vector2& range);
  void SetVelRange(const Vector2& rangeX, const Vector2& rangeY, const Vector2& rangeZ);
  void SetVelRangeX(const Vector2& range);
  void SetVelRangeY(const Vector2& range);
  void SetVelRangeZ(const Vector2& range);
  void SetLifeTimeRange(const Vector2& range);

  // 一時的なエミッター用のメソッド
  void SetTemporary(bool isTemporary, float lifeTime = 0.0f);
  void UpdateTemporaryLifeTime(float deltaTime);

  // ゲッター
  [[nodiscard]] const Vector3& GetPosition() const { return data_.position; }
  [[nodiscard]] bool IsActive() const { return data_.isActive; }
  [[nodiscard]] const Vector2& GetScaleRangeX() const { return data_.scaleRangeX; }
  [[nodiscard]] const Vector2& GetScaleRangeY() const { return data_.scaleRangeY; }
  [[nodiscard]] const Vector2& GetVelRangeX() const { return data_.velRangeX; }
  [[nodiscard]] const Vector2& GetVelRangeY() const { return data_.velRangeY; }
  [[nodiscard]] const Vector2& GetVelRangeZ() const { return data_.velRangeZ; }
  [[nodiscard]] const Vector2& GetLifeTimeRange() const { return data_.lifeTimeRange; }
  [[nodiscard]] const Vector4& GetStartColor() const { return data_.startColorTint; }
  [[nodiscard]] const Vector4& GetEndColor() const { return data_.endColorTint; }
  [[nodiscard]] uint32_t GetParticleCount() const { return data_.count; }
  [[nodiscard]] float GetFrequencyTime() const { return data_.frequencyTime; }
  [[nodiscard]] float GetFrequency() const { return data_.frequency; }
  [[nodiscard]] uint32_t GetEmitterId() const { return data_.emitterID; }
  [[nodiscard]] bool IsTemporary() const { return data_.isTemp; }
  [[nodiscard]] float GetEmitterLifeTime() const { return data_.emitterLifeTime; }
  [[nodiscard]] float GetEmitterCurrentTime() const { return data_.emitterCurrentTime; }
  [[nodiscard]] bool IsLifeTimeExpired() const;


  // 仮想関数
  [[nodiscard]] virtual EmitterType GetType() const = 0;

protected:
  // 共通パラメータ
  GPUParticle* particleSystem_;    // パーティクルシステムへの参照

  EmitterData data_;               // エミッターデータ
};
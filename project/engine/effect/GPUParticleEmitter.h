#pragma once
#include "Vector3.h"
#include "Vector4.h"
#include "ParticleStruct.h"

// 前方宣言
class GPUParticle;

// エミッターの基底クラス
class GPUParticleEmitter
{
public:
  // コンストラクタ・デストラクタ
  GPUParticleEmitter(GPUParticle* particleSystem, uint32_t emitterId);
  virtual ~GPUParticleEmitter();

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
  [[nodiscard]] const Vector3& GetPosition() const { return position_; }
  [[nodiscard]] bool IsActive() const { return isActive_; }
  [[nodiscard]] const Vector4& GetStartColor() const { return startColor_; }
  [[nodiscard]] const Vector4& GetEndColor() const { return endColor_; }
  [[nodiscard]] uint32_t GetParticleCount() const { return particleCount_; }
  [[nodiscard]] float GetFrequency() const { return frequency_; }
  [[nodiscard]] uint32_t GetEmitterId() const { return emitterId_; }
  [[nodiscard]] bool IsTemporary() const { return isTemp_; }
  [[nodiscard]] float GetEmitterLifeTime() const { return emitterLifeTime_; }
  [[nodiscard]] float GetEmitterCurrentTime() const { return emitterCurrentTime_; }
  [[nodiscard]] bool IsLifeTimeExpired() const;


  // 仮想関数
  [[nodiscard]] virtual EmitterType GetType() const = 0;

protected:
  // 共通パラメータ
  GPUParticle* particleSystem_;    // パーティクルシステムへの参照
  uint32_t emitterId_;             // エミッターID
  Vector3 position_;               // エミッターの位置
  Vector2 scaleRangeX_;            // Xスケール範囲
  Vector2 scaleRangeY_;            // Yスケール範囲
  Vector2 velRangeX_;              // X速度範囲
  Vector2 velRangeY_;              // Y速度範囲
  Vector2 velRangeZ_;              // Z速度範囲
  Vector2 lifeTimeRange_;          // 寿命範囲
  Vector4 startColor_;           // 開始色
  Vector4 endColor_;             // 終了色
  uint32_t particleCount_;         // 1回の射出で生成するパーティクル数
  float frequency_;                // 射出頻度（秒）
  bool isActive_;                  // アクティブ状態

  // 一時的なエミッター用の変数
  bool isTemp_ = false;            // 一時的なエミッターかどうか
  float emitterLifeTime_ = 0.0f;   // エミッターの寿命
  float emitterCurrentTime_ = 0.0f;// エミッターの経過時間
};
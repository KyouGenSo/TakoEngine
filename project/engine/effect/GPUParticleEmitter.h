#pragma once
#include <memory>
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
  void SetColor(const Vector4& color);
  void SetParticleCount(uint32_t count);
  void SetFrequency(float frequency);

  // 乱数生成範囲の設定
  void SetScaleRange(const Vector2& rangeX, const Vector2& rangeY);
  void SetScaleRangeX(const Vector2& range);
  void SetScaleRangeY(const Vector2& range);
  void SetVelRange(const Vector2& rangeX, const Vector2& rangeY, const Vector2& rangeZ);
  void SetVelRangeX(const Vector2& range);
  void SetVelRangeY(const Vector2& range);
  void SetVelRangeZ(const Vector2& range);
  void SetLifeTimeRange(const Vector2& range);

  // ゲッター
  const Vector3& GetPosition() const { return position_; }
  bool IsActive() const { return isActive_; }
  const Vector4& GetColor() const { return color_; }
  uint32_t GetParticleCount() const { return particleCount_; }
  float GetFrequency() const { return frequency_; }
  uint32_t GetEmitterId() const { return emitterId_; }

  // 仮想関数
  virtual EmitterType GetType() const = 0;

protected:
  // 共通パラメータ
  GPUParticle* particleSystem_;  // パーティクルシステムへの参照
  uint32_t emitterId_;           // エミッターID
  Vector3 position_;             // エミッターの位置
  Vector2 scaleRangeX_;          // Xスケール範囲
  Vector2 scaleRangeY_;          // Yスケール範囲
  Vector2 velRangeX_;            // X速度範囲
  Vector2 velRangeY_;            // Y速度範囲
  Vector2 velRangeZ_;            // Z速度範囲
  Vector2 lifeTimeRange_;        // 寿命範囲
  Vector4 color_;                // カラーティント
  uint32_t particleCount_;       // 1回の射出で生成するパーティクル数
  float frequency_;              // 射出頻度（秒）
  bool isActive_;                // アクティブ状態
};
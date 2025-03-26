#pragma once
#include <vector>
#include <memory>
#include <string>
#include <unordered_map>
#include "GPUParticleEmitter.h"

// 前方宣言
class GPUParticle;
class SphereEmitter;
class BoxEmitter;
class TriangleEmitter;

// エミッターグループ
struct EmitterGroup {
  std::string name;
  std::vector<std::string> emitterNames;
  bool isActive;
};

// エフェクトプリセットタイプ
enum class EffectPresetType {
  Explosion,
  Fire,
  Smoke,
  Magic,
  Rain,
  Snow
};

class EmitterManager
{
public:
  EmitterManager(GPUParticle* particleSystem);
  ~EmitterManager();

  // エミッター作成（名前付き）
  std::shared_ptr<SphereEmitter> CreateSphereEmitter(const std::string& name, const Vector3& position, float radius, uint32_t count, float frequency);
  std::shared_ptr<BoxEmitter> CreateBoxEmitter(const std::string& name, const Vector3& position, const Vector3& size, const Vector3& rotation, uint32_t count, float frequency);
  std::shared_ptr<TriangleEmitter> CreateTriangleEmitter(const std::string& name, const Vector3& position, const Vector3& v1, const Vector3& v2, const Vector3& v3, uint32_t count, float frequency);

  // エミッター管理
  std::shared_ptr<GPUParticleEmitter> GetEmitterByName(const std::string& name);
  void RemoveEmitter(const std::string& name);
  void RemoveAllEmitters();
  void ClearAllTimedEffects();

  // グループ機能
  void CreateGroup(const std::string& groupName);
  void AddToGroup(const std::string& groupName, const std::string& emitterName);
  void RemoveFromGroup(const std::string& groupName, const std::string& emitterName);
  void SetGroupActive(const std::string& groupName, bool isActive);
  void SetGroupPosition(const std::string& groupName, const Vector3& position);
  void RemoveGroup(const std::string& groupName);

  // エフェクトプリセット
  void CreateEffectPreset(EffectPresetType type, const std::string& name, const Vector3& position, float scale = 1.0f);

  // 特殊エフェクト
  void TriggerExplosion(const std::string& name, const Vector3& position, float radius = 1.0f, float duration = 0.3f);
  void CreateTrailEffect(const std::string& name, const Vector3& startPosition, const Vector3& direction, float length, float width, float duration);

  // 更新処理
  void Update();

private:
  // エフェクトグループ内部実装
  std::vector<std::shared_ptr<GPUParticleEmitter>> CreateExplosionEffect(const std::string& baseName, const Vector3& position, float scale);
  std::vector<std::shared_ptr<GPUParticleEmitter>> CreateFireEffect(const std::string& baseName, const Vector3& position, float scale);
  std::vector<std::shared_ptr<GPUParticleEmitter>> CreateSmokeEffect(const std::string& baseName, const Vector3& position, float scale);
  std::vector<std::shared_ptr<GPUParticleEmitter>> CreateMagicEffect(const std::string& baseName, const Vector3& position, float scale);

  // タイマー付きエフェクト管理
  struct TimedEffect {
    std::string name;
    std::vector<std::string> emitterNames;
    float duration;
    float currentTime;
  };

private:
  GPUParticle* particleSystem_;

  // エミッターマップ（名前→エミッター）
  std::unordered_map<std::string, std::shared_ptr<GPUParticleEmitter>> emitterMap_;

  // グループマップ（グループ名→グループ情報）
  std::unordered_map<std::string, EmitterGroup> groupMap_;

  // 一時的なエフェクト
  std::vector<TimedEffect> timedEffects_;
};

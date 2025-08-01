#pragma once
#include <vector>
#include <memory>
#include <string>
#include <unordered_map>
#include "GPUParticleEmitter.h"
#include "EmitterStruct.h"

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

class EmitterManager
{
public: // エミッター設定構造体
  struct EmitterSettings
  {
    std::string name;
    Vector3 position;
    uint32_t count;
    float frequency;
    Vector2 scaleRangeX;
    Vector2 scaleRangeY;
    Vector2 velRangeX;
    Vector2 velRangeY;
    Vector2 velRangeZ;
    Vector2 lifeTimeRange;
    Vector4 startColor;
    Vector4 endColor;
    bool isActive;
    bool isNormalize;
  };

  struct SphereEmitterSettings : EmitterSettings
  {
    float radius;
  };

  struct BoxEmitterSettings : EmitterSettings
  {
    Vector3 size;
    Vector3 rotation; // 回転を表すベクトル（例: Euler角）
  };

  struct TriangleEmitterSettings : EmitterSettings
  {
    Vector3 v1;
    Vector3 v2;
    Vector3 v3;
  };

public: // メンバー関数
  EmitterManager(GPUParticle* particleSystem);
  ~EmitterManager();

  // 基本エミッター作成
  void CreateSphereEmitter(const std::string& name, const Vector3& position, float radius,
    uint32_t count, float frequency);
  void CreateSphereEmitter(SphereEmitterSettings settings);

  void CreateBoxEmitter(const std::string& name, const Vector3& position, const Vector3& size,
    const Vector3& rotation, uint32_t count, float frequency);
  void CreateBoxEmitter(BoxEmitterSettings settings);

  void CreateTriangleEmitter(const std::string& name, const Vector3& position,
    const Vector3& v1, const Vector3& v2, const Vector3& v3,
    uint32_t count, float frequency);
  void CreateTriangleEmitter(TriangleEmitterSettings settings);

  // エミッターの更新
  void UpdateSphereEmitter(const std::string& name, const Vector3& position, float radius,
    uint32_t count = 0, float frequency = 0.0f);
  void UpdateSphereEmitter(const SphereEmitterSettings& settings);

  void UpdateBoxEmitter(const std::string& name, const Vector3& position, const Vector3& size,
    const Vector3& rotation, uint32_t count = 0, float frequency = 0.0f);
  void UpdateBoxEmitter(const BoxEmitterSettings& settings);

  void UpdateTriangleEmitter(const std::string& name, const Vector3& position,
    const Vector3& v1, const Vector3& v2, const Vector3& v3,
    uint32_t count = 0, float frequency = 0.0f);
  void UpdateTriangleEmitter(const TriangleEmitterSettings& settings);

  // 一時的なエミッター作成
  void CreateTemporaryEmitterFrom(const std::string& sourceName, const std::string& newName, float lifeTime);

  //　一時的なエミッターの更新
  void UpdateTemporaryEmitters(); // Update関数内で呼び出す

  void SetEmitterPosition(const std::string& name, const Vector3& position);
  void SetEmitterScaleRange(const std::string& name, const Vector2& scaleRangeX, const Vector2& scaleRangeY);
  void SetEmitterVelocityRange(const std::string& name, const Vector2& velRangeX, const Vector2& velRangeY, const Vector2& velRangeZ);
  void SetEmitterLifeTimeRange(const std::string& name, const Vector2& lifeTimeRange);
  void SetEmitterActive(const std::string& name, bool isActive);
  void SetEmitterNormalize(const std::string& name, bool isNormalize);
  void SetEmitterColor(const std::string& name, const Vector4& color);
  void SetEmitterStartColor(const std::string& name, const Vector4& color);
  void SetEmitterEndColor(const std::string& name, const Vector4& color);
  void SetEmitterColors(const std::string& name, const Vector4& startColor, const Vector4& endColor);

  // エミッター管理
  std::shared_ptr<GPUParticleEmitter> GetEmitterByName(const std::string& name);
  void RemoveEmitter(const std::string& name);
  void RemoveAllEmitters();

  // グループ機能
  void CreateGroup(const std::string& groupName);
  void AddToGroup(const std::string& groupName, const std::string& emitterName);
  void RemoveFromGroup(const std::string& groupName, const std::string& emitterName);
  void SetGroupActive(const std::string& groupName, bool isActive);
  void SetGroupPosition(const std::string& groupName, const Vector3& position);
  void RemoveGroup(const std::string& groupName);

  // デバッグ情報
  void DebugInfo();
  size_t GetActiveEmitterCount() const { return emitterMap_.size(); }

private: // プライベートメンバー関数



private:
  GPUParticle* particleSystem_;

  // エミッターマップ（名前→エミッター）
  std::unordered_map<std::string, std::shared_ptr<GPUParticleEmitter>> emitterMap_;

  // グループマップ（グループ名→グループ情報）
  std::unordered_map<std::string, EmitterGroup> groupMap_;

};
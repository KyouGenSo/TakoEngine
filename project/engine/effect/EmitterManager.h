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
public:
  EmitterManager(GPUParticle* particleSystem);
  ~EmitterManager();

  // 基本エミッター作成
  std::shared_ptr<SphereEmitter> CreateSphereEmitter(const std::string& name, const Vector3& position, float radius,
    uint32_t count, float frequency);

  std::shared_ptr<BoxEmitter> CreateBoxEmitter(const std::string& name, const Vector3& position, const Vector3& size,
    const Vector3& rotation, uint32_t count, float frequency);

  std::shared_ptr<TriangleEmitter> CreateTriangleEmitter(const std::string& name, const Vector3& position,
    const Vector3& v1, const Vector3& v2, const Vector3& v3,
    uint32_t count, float frequency);

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

  // 更新処理
  void Update();

  // デバッグ情報
  void DebugInfo();
  size_t GetActiveEmitterCount() const { return emitterMap_.size(); }

private:
  GPUParticle* particleSystem_;

  // エミッターマップ（名前→エミッター）
  std::unordered_map<std::string, std::shared_ptr<GPUParticleEmitter>> emitterMap_;

  // グループマップ（グループ名→グループ情報）
  std::unordered_map<std::string, EmitterGroup> groupMap_;

};
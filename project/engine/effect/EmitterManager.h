#pragma once
#include <vector>
#include <memory>
#include <string>
#include <unordered_map>
#include <array>
#include "GPUParticleEmitter.h"
#include "EmitterStruct.h"
#include <json.hpp>

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
  void CreateSphereEmitter(const std::string& name, const Vector3& position, float radius,
    uint32_t count, float frequency);

  void CreateBoxEmitter(const std::string& name, const Vector3& position, const Vector3& size,
    const Vector3& rotation, uint32_t count, float frequency);

  void CreateTriangleEmitter(const std::string& name, const Vector3& position,
    const Vector3& v1, const Vector3& v2, const Vector3& v3,
    uint32_t count, float frequency);

  // エミッターの更新
  void UpdateSphereEmitter(const std::string& name, const Vector3& position, float radius,
    uint32_t count = 0, float frequency = 0.0f);

  void UpdateBoxEmitter(const std::string& name, const Vector3& position, const Vector3& size,
    const Vector3& rotation, uint32_t count = 0, float frequency = 0.0f);

  void UpdateTriangleEmitter(const std::string& name, const Vector3& position,
    const Vector3& v1, const Vector3& v2, const Vector3& v3,
    uint32_t count = 0, float frequency = 0.0f);


  // 一時的なエミッター作成
  void CreateTemporaryEmitterFrom(const std::string& sourceName, const std::string& newName, float lifeTime);

  // 更新
  void Update();

  void SetEmitterPosition(const std::string& name, const Vector3& position);
  void SetEmitterScaleRange(const std::string& name, const Vector2& scaleRangeX, const Vector2& scaleRangeY);
  void SetEmitterVelocityRange(const std::string& name, const Vector2& velRangeX, const Vector2& velRangeY, const Vector2& velRangeZ);
  void SetEmitterLifeTimeRange(const std::string& name, const Vector2& lifeTimeRange);
  void SetEmitterActive(const std::string& name, bool isActive);
  void SetEmitterNormalize(const std::string& name, bool isNormalize);
  void SetEmitterRandomRotateZ(const std::string& name, bool isRandomRotateZ);
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

  // JSON保存・読み込み機能
  void SaveEmittersToJSON(const std::string& filename);
  void LoadEmittersFromJSON(const std::string& filename);
  void SavePreset(const std::string& presetName, const std::string& emitterName);
  void LoadPreset(const std::string& presetName, const std::string& newEmitterName);

  // エミッター情報取得（エディター用）
  std::vector<std::string> GetEmitterNames() const;
  bool HasEmitter(const std::string& name) const;

  // コピー＆ペースト機能
  bool CopyEmitterSettings(const std::string& emitterName, int slotIndex = 0);
  bool PasteEmitterSettings(const std::string& targetEmitterName, int slotIndex = 0, bool colorOnly = false, bool velocityOnly = false, bool scaleOnly = false);
  bool HasCopiedSettings(int slotIndex = 0) const;
  void ClearCopiedSettings(int slotIndex = 0);

  // グループ情報取得
  std::vector<std::string> GetGroupNames() const;
  std::vector<std::string> GetEmittersInGroup(const std::string& groupName) const;
  bool IsGroupActive(const std::string& groupName) const;
  size_t GetGroupCount() const { return groupMap_.size(); }

private: // プライベートメンバー関数

  // 一時的なエミッターの更新
  void UpdateTemporaryEmitters(); // Update関数内で呼び出す

  // JSON変換ヘルパー
  void SerializeEmitterToJSON(const std::shared_ptr<GPUParticleEmitter>& emitter, nlohmann::json& json) const;
  std::shared_ptr<GPUParticleEmitter> DeserializeEmitterFromJSON(const nlohmann::json& json);

private:
  GPUParticle* particleSystem_;

  // エミッターマップ（名前→エミッター）
  std::unordered_map<std::string, std::shared_ptr<GPUParticleEmitter>> emitterMap_;

  // グループマップ（グループ名→グループ情報）
  std::unordered_map<std::string, EmitterGroup> groupMap_;

  // コピーバッファ（5スロット）
  struct CopiedSettings {
    bool valid = false;
    EmitterData data;
    EmitterType type;
  };
  std::array<CopiedSettings, 5> copiedSettingsSlots_;

};
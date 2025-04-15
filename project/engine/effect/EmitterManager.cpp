#include "EmitterManager.h"
#include "GPUParticle.h"
#include "SphereEmitter.h"
#include "BoxEmitter.h"
#include "TriangleEmitter.h"
#include "Logger.h"
#include "FrameTimer.h"

#include <algorithm>
#include <ranges>

EmitterManager::EmitterManager(GPUParticle* particleSystem)
  : particleSystem_(particleSystem)
{
}

EmitterManager::~EmitterManager()
{
  // すべてのエミッターを削除
  RemoveAllEmitters();
}

// エミッター作成（名前付き）
void EmitterManager::CreateSphereEmitter(const std::string& name, const Vector3& position, float radius, uint32_t count, float frequency)
{
  // 名前の重複チェック
  if (emitterMap_.contains(name)) {
    Logger::Log("Warning: Emitter name '%s' already exists. Overwriting.", name.c_str());
    RemoveEmitter(name);
  }

  // エミッター作成
  std::shared_ptr<SphereEmitter> emitter = std::make_shared<SphereEmitter>(particleSystem_, position, radius, count, frequency);

  // GPUParticleにエミッターを登録
  particleSystem_->RegisterEmitter(emitter);

  // マップに追加
  emitterMap_[name] = emitter;

}

void EmitterManager::CreateBoxEmitter(const std::string& name, const Vector3& position, const Vector3& size, const Vector3& rotation, uint32_t count, float frequency)
{
  // 名前の重複チェック
  if (emitterMap_.contains(name)) {
    Logger::Log("Warning: Emitter name '%s' already exists. Overwriting.", name.c_str());
    RemoveEmitter(name);
  }

  // エミッター作成
  std::shared_ptr<BoxEmitter> emitter = std::make_shared<BoxEmitter>(particleSystem_, position, size, rotation, count, frequency);

  // GPUParticleにエミッターを登録
  particleSystem_->RegisterEmitter(emitter);

  // マップに追加
  emitterMap_[name] = emitter;

}

void EmitterManager::CreateTriangleEmitter(const std::string& name, const Vector3& position, const Vector3& v1, const Vector3& v2, const Vector3& v3, uint32_t count, float frequency)
{
  // 名前の重複チェック
  if (emitterMap_.contains(name)) {
    Logger::Log("Warning: Emitter name '%s' already exists. Overwriting.", name.c_str());
    RemoveEmitter(name);
  }

  // エミッター作成
  std::shared_ptr<TriangleEmitter> emitter = std::make_shared<TriangleEmitter>(particleSystem_, position, v1, v2, v3, count, frequency);

  // GPUParticleにエミッターを登録
  particleSystem_->RegisterEmitter(emitter);

  // マップに追加
  emitterMap_[name] = emitter;

}

void EmitterManager::UpdateSphereEmitter(const std::string& name, const Vector3& position, float radius,
  uint32_t count, float frequency)
{
  auto it = emitterMap_.find(name);

  if (it != emitterMap_.end()) {
    auto sphereEmitter = std::dynamic_pointer_cast<SphereEmitter>(it->second);

    if (sphereEmitter) {
      sphereEmitter->SetPosition(position);
      sphereEmitter->SetRadius(radius);
      if (count > 0) sphereEmitter->SetParticleCount(count);
      if (frequency > 0.0f) sphereEmitter->SetFrequency(frequency);
    } else {
      Logger::Log("UpdateSphereEmitter: Emitter '%s' is not a SphereEmitter", name.c_str());
    }

  } else {
    Logger::Log("UpdateSphereEmitter: Emitter '%s' not found", name.c_str());
  }
}

void EmitterManager::UpdateBoxEmitter(const std::string& name, const Vector3& position, const Vector3& size, const Vector3& rotation, uint32_t count, float frequency)
{
  auto it = emitterMap_.find(name);

  if (it != emitterMap_.end()) {
    auto boxEmitter = std::dynamic_pointer_cast<BoxEmitter>(it->second);

    if (boxEmitter) {
      boxEmitter->SetPosition(position);
      boxEmitter->SetSize(size);
      boxEmitter->SetRotation(rotation);
      if (count > 0) boxEmitter->SetParticleCount(count);
      if (frequency > 0.0f) boxEmitter->SetFrequency(frequency);
    } else {
      Logger::Log("UpdateBoxEmitter: Emitter '%s' is not a BoxEmitter", name.c_str());
    }

  } else {
    Logger::Log("UpdateBoxEmitter: Emitter '%s' not found", name.c_str());
  }
}

void EmitterManager::UpdateTriangleEmitter(const std::string& name, const Vector3& position, const Vector3& v1, const Vector3& v2, const Vector3& v3, uint32_t count, float frequency)
{
  auto it = emitterMap_.find(name);

  if (it != emitterMap_.end()) {
    auto triangleEmitter = std::dynamic_pointer_cast<TriangleEmitter>(it->second);

    if (triangleEmitter) {
      triangleEmitter->SetPosition(position);
      triangleEmitter->SetVertices(v1, v2, v3);
      if (count > 0) triangleEmitter->SetParticleCount(count);
      if (frequency > 0.0f) triangleEmitter->SetFrequency(frequency);
    } else {
      Logger::Log("UpdateTriangleEmitter: Emitter '%s' is not a TriangleEmitter", name.c_str());
    }

  } else {
    Logger::Log("UpdateTriangleEmitter: Emitter '%s' not found", name.c_str());
  }
}

void EmitterManager::CreateTemporaryEmitterFrom(const std::string& sourceName, const std::string& newName, float lifeTime)
{
  // ソースエミッターを取得
  auto sourceIt = emitterMap_.find(sourceName);

  // ソースエミッターの存在チェック
  if (sourceIt == emitterMap_.end()) {
    Logger::Log("CreateTemporaryEmitterFrom: Source emitter '%s' not found", sourceName.c_str());
    return;
  }

  // 名前の重複チェック
  if (emitterMap_.contains(newName)) {
    Logger::Log("Warning: Emitter name '%s' already exists. Overwriting.", newName.c_str());
    RemoveEmitter(newName);
  }

  // 一時的なエミッターを作成
  std::shared_ptr<GPUParticleEmitter> sourceEmitter = sourceIt->second;
  std::shared_ptr<GPUParticleEmitter> newEmitter = particleSystem_->CreateTemporaryEmitterFrom(sourceEmitter.get(), lifeTime);

  if (newEmitter) {
    // マップに追加
    emitterMap_[newName] = newEmitter;
    Logger::Log("CreateTemporaryEmitterFrom: Created temporary emitter '%s' from '%s' with lifetime %.2f seconds", newName.c_str(), sourceName.c_str(), lifeTime);
  } else {
    Logger::Log("CreateTemporaryEmitterFrom: Failed to create emitter from '%s'", sourceName.c_str());
  }
}

void EmitterManager::UpdateTemporaryEmitters()
{
  float deltaTime = FrameTimer::GetInstance()->GetDeltaTime();
  std::vector<std::string> emittersToRemove;

  // 一時的なエミッターの寿命を更新
  for (auto& [name, emitter] : emitterMap_) {
    if (emitter->IsTemporary()) {
      emitter->UpdateTemporaryLifeTime(deltaTime);

      // 寿命が尽きたらリストに追加
      if (emitter->IsLifeTimeExpired()) {
        emittersToRemove.push_back(name);
      }
    }
  }

  // 寿命が尽きたエミッターを削除
  for (const auto& name : emittersToRemove) {
    Logger::Log("UpdateTemporaryEmitters: Removing expired emitter '%s'", name.c_str());
    RemoveEmitter(name);
  }
}

void EmitterManager::Update()
{
  UpdateTemporaryEmitters();
}

void EmitterManager::SetEmitterPosition(const std::string& name, const Vector3& position)
{
  auto it = emitterMap_.find(name);
  if (it != emitterMap_.end()) {
    it->second->SetPosition(position);
  }
}

void EmitterManager::SetEmitterScaleRange(const std::string& name, const Vector2& scaleRangeX, const Vector2& scaleRangeY)
{
  auto it = emitterMap_.find(name);
  if (it != emitterMap_.end()) {
    it->second->SetScaleRange(scaleRangeX, scaleRangeY);
  }
}

void EmitterManager::SetEmitterVelocityRange(const std::string& name, const Vector2& velRangeX, const Vector2& velRangeY, const Vector2& velRangeZ)
{
  auto it = emitterMap_.find(name);
  if (it != emitterMap_.end()) {
    it->second->SetVelRange(velRangeX, velRangeY, velRangeZ);
  }
}

void EmitterManager::SetEmitterLifeTimeRange(const std::string& name, const Vector2& lifeTimeRange)
{
  auto it = emitterMap_.find(name);
  if (it != emitterMap_.end()) {
    it->second->SetLifeTimeRange(lifeTimeRange);
  }
}

void EmitterManager::SetEmitterActive(const std::string& name, bool isActive)
{
  auto it = emitterMap_.find(name);
  if (it != emitterMap_.end()) {
    it->second->SetActive(isActive);
  }
}

void EmitterManager::SetEmitterColor(const std::string& name, const Vector4& color)
{
  auto it = emitterMap_.find(name);
  if (it != emitterMap_.end()) {
    it->second->SetColor(color);
  }
}

void EmitterManager::SetEmitterStartColor(const std::string& name, const Vector4& color)
{
  auto it = emitterMap_.find(name);
  if (it != emitterMap_.end()) {
    it->second->SetStartColor(color);
  }
}

void EmitterManager::SetEmitterEndColor(const std::string& name, const Vector4& color)
{
  auto it = emitterMap_.find(name);
  if (it != emitterMap_.end()) {
    it->second->SetEndColor(color);
  }
}

void EmitterManager::SetEmitterColors(const std::string& name, const Vector4& startColor, const Vector4& endColor)
{
  auto it = emitterMap_.find(name);
  if (it != emitterMap_.end()) {
    it->second->SetColors(startColor, endColor);
  }
}

// エミッター管理
std::shared_ptr<GPUParticleEmitter> EmitterManager::GetEmitterByName(const std::string& name)
{
  auto it = emitterMap_.find(name);
  if (it != emitterMap_.end()) {
    return it->second;
  }

  return nullptr;
}

void EmitterManager::RemoveEmitter(const std::string& name)
{
  auto it = emitterMap_.find(name);
  if (it != emitterMap_.end()) {
    Logger::Log("RemoveEmitter: Removing emitter '%s'", name.c_str());

    // エミッターを非アクティブにして即時効果を得る
    it->second->SetActive(false);

    // エミッターをマップから削除（shared_ptrなので自動解放）
    emitterMap_.erase(it);

    // グループから安全に削除
    for (auto& [groupName, group] : groupMap_) {
      auto& emitterNames = group.emitterNames;

      // 安全に要素を削除（remove-eraseイディオム）
      if (auto newEnd = std::ranges::remove(emitterNames, name).begin(); newEnd != emitterNames.end()) {
        emitterNames.erase(newEnd, emitterNames.end());
        Logger::Log("Removed emitter '%s' from group '%s'", name.c_str(), groupName.c_str());
      }
    }

  } else {
    Logger::Log("RemoveEmitter: Emitter '%s' not found", name.c_str());
  }
}

void EmitterManager::RemoveAllEmitters()
{
  Logger::Log("RemoveAllEmitters: Removing all emitters (%zu emitters)",
    emitterMap_.size());

  // エミッターを1つずつ明示的に削除（GPUParticleシステムに通知するため）
  for (auto& val : emitterMap_ | std::views::values) {
    auto& emitter = val;
    // エミッターを非アクティブ化して即時効果を得る
    emitter->SetActive(false);
  }

  // エミッターマップをクリア
  emitterMap_.clear();

  // グループを空にする
  for (auto& val : groupMap_ | std::views::values) {
    val.emitterNames.clear();
  }
}

// グループ機能
void EmitterManager::CreateGroup(const std::string& groupName)
{
  // グループの重複チェック
  if (groupMap_.contains(groupName)) {
    Logger::Log("Warning: Group name '%s' already exists.", groupName.c_str());
    return;
  }

  // グループを作成
  EmitterGroup group;
  group.name = groupName;
  group.isActive = true;

  // グループをマップに追加
  groupMap_[groupName] = group;
}

void EmitterManager::AddToGroup(const std::string& groupName, const std::string& emitterName)
{
  // グループの存在チェック
  auto groupIt = groupMap_.find(groupName);
  if (groupIt == groupMap_.end()) {
    Logger::Log("Warning: Group '%s' not found. Creating new group.", groupName.c_str());
    CreateGroup(groupName);
    groupIt = groupMap_.find(groupName);
  }

  // エミッターの存在チェック
  if (!emitterMap_.contains(emitterName)) {
    Logger::Log("Warning: Emitter '%s' not found. Cannot add to group.", emitterName.c_str());
    return;
  }

  // 既に追加済みかチェック
  auto& emitterNames = groupIt->second.emitterNames;
  if (std::ranges::find(emitterNames, emitterName) != emitterNames.end()) {
    return; // 既に追加済み
  }

  // グループにエミッターを追加
  emitterNames.push_back(emitterName);
}

void EmitterManager::RemoveFromGroup(const std::string& groupName, const std::string& emitterName)
{
  // グループの存在チェック
  auto groupIt = groupMap_.find(groupName);
  if (groupIt == groupMap_.end()) {
    return; // グループが存在しない
  }

  // グループからエミッターを削除
  auto& emitterNames = groupIt->second.emitterNames;
  emitterNames.erase(
    std::ranges::remove(emitterNames, emitterName).begin(),
    emitterNames.end()
  );
}

void EmitterManager::SetGroupActive(const std::string& groupName, bool isActive)
{
  // グループの存在チェック
  auto groupIt = groupMap_.find(groupName);
  if (groupIt == groupMap_.end()) {
    return; // グループが存在しない
  }

  // グループのアクティブ状態を設定
  groupIt->second.isActive = isActive;

  // グループ内のすべてのエミッターのアクティブ状態を設定
  for (const auto& emitterName : groupIt->second.emitterNames) {
    auto emitterIt = emitterMap_.find(emitterName);
    if (emitterIt != emitterMap_.end()) {
      emitterIt->second->SetActive(isActive);
    }
  }
}

void EmitterManager::SetGroupPosition(const std::string& groupName, const Vector3& position)
{
  // グループの存在チェック
  auto groupIt = groupMap_.find(groupName);
  if (groupIt == groupMap_.end()) {
    return; // グループが存在しない
  }

  // グループ内の最初のエミッターの位置を取得（相対位置計算用）
  Vector3 basePosition = Vector3(0, 0, 0);
  bool hasBasePosition = false;

  if (!groupIt->second.emitterNames.empty()) {
    auto firstEmitterIt = emitterMap_.find(groupIt->second.emitterNames[0]);
    if (firstEmitterIt != emitterMap_.end()) {
      basePosition = firstEmitterIt->second->GetPosition();
      hasBasePosition = true;
    }
  }

  // グループ内のすべてのエミッターの位置を更新
  for (const auto& emitterName : groupIt->second.emitterNames) {
    auto emitterIt = emitterMap_.find(emitterName);
    if (emitterIt != emitterMap_.end()) {
      if (hasBasePosition) {
        // 最初のエミッターからの相対位置を計算
        Vector3 offset = emitterIt->second->GetPosition() - basePosition;
        emitterIt->second->SetPosition(position + offset);
      } else {
        // 単純に位置を設定
        emitterIt->second->SetPosition(position);
      }
    }
  }
}

void EmitterManager::RemoveGroup(const std::string& groupName)
{
  auto it = groupMap_.find(groupName);
  if (it != groupMap_.end()) {
    Logger::Log("RemoveGroup: Removing group '%s'", groupName.c_str());

    // グループ内のすべてのエミッターの名前をコピー（ループ中に変更されるため）
    std::vector<std::string> emitterNames = it->second.emitterNames;

    // グループに属するすべてのエミッターを削除（オプション、コメントアウト可能）
    if (!emitterNames.empty()) {
      Logger::Log("RemoveGroup: Removing %zu emitters in group '%s'",
        emitterNames.size(), groupName.c_str());

      // エミッターを一つずつ削除
      for (const auto& name : emitterNames) {
        RemoveEmitter(name);
      }
    }

    // グループをマップから削除
    groupMap_.erase(it);

  } else {
    Logger::Log("RemoveGroup: Group '%s' not found", groupName.c_str());
  }
}

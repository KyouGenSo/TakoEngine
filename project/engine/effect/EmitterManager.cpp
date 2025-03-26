#include "EmitterManager.h"
#include "GPUParticle.h"
#include "SphereEmitter.h"
#include "BoxEmitter.h"
#include "TriangleEmitter.h"
#include "Logger.h"
#include "FrameTimer.h"

#include <algorithm>
#include <iterator>
#include <minmax.h>

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
std::shared_ptr<SphereEmitter> EmitterManager::CreateSphereEmitter(const std::string& name, const Vector3& position, float radius, uint32_t count, float frequency)
{
  // 名前の重複チェック
  if (emitterMap_.find(name) != emitterMap_.end()) {
    Logger::Log("Warning: Emitter name '%s' already exists. Overwriting.", name.c_str());
    RemoveEmitter(name);
  }

  // エミッター作成
  std::shared_ptr<SphereEmitter> emitter = particleSystem_->CreateSphereEmitter(position, radius, count, frequency);

  if (!emitter) {
    Logger::Log("Error: Failed to create sphere emitter '%s'", name.c_str());
    return nullptr;
  }

  // マップに追加
  emitterMap_[name] = emitter;

  return emitter;
}

std::shared_ptr<BoxEmitter> EmitterManager::CreateBoxEmitter(const std::string& name, const Vector3& position, const Vector3& size, const Vector3& rotation, uint32_t count, float frequency)
{
  // 名前の重複チェック
  if (emitterMap_.find(name) != emitterMap_.end()) {
    Logger::Log("Warning: Emitter name '%s' already exists. Overwriting.", name.c_str());
    RemoveEmitter(name);
  }

  // エミッター作成
  std::shared_ptr<BoxEmitter> emitter = particleSystem_->CreateBoxEmitter(position, size, rotation, count, frequency);

  if (!emitter) {
    Logger::Log("Error: Failed to create box emitter '%s'", name.c_str());
    return nullptr;
  }

  // マップに追加
  emitterMap_[name] = emitter;

  return emitter;
}

std::shared_ptr<TriangleEmitter> EmitterManager::CreateTriangleEmitter(const std::string& name, const Vector3& position, const Vector3& v1, const Vector3& v2, const Vector3& v3, uint32_t count, float frequency)
{
  // 名前の重複チェック
  if (emitterMap_.find(name) != emitterMap_.end()) {
    Logger::Log("Warning: Emitter name '%s' already exists. Overwriting.", name.c_str());
    RemoveEmitter(name);
  }

  // エミッター作成
  std::shared_ptr<TriangleEmitter> emitter = particleSystem_->CreateTriangleEmitter(position, v1, v2, v3, count, frequency);

  if (!emitter) {
    Logger::Log("Error: Failed to create triangle emitter '%s'", name.c_str());
    return nullptr;
  }

  // マップに追加
  emitterMap_[name] = emitter;

  return emitter;
}

// 一時的な球体エミッター作成
void EmitterManager::MakeTimedSphereEmitter(const std::string& name, const Vector3& position, float radius,
  uint32_t count, float frequency, float duration)
{
  // 通常のエミッター作成関数を利用
  std::shared_ptr<SphereEmitter> emitter = CreateSphereEmitter(name, position, radius, count, frequency);

  if (emitter) {
    // タイマー付きエミッターとして登録
    RegisterTimedEmitter(name, name, duration);
    Logger::Log("Registered sphere emitter '%s' as timed (%.2f seconds)", name.c_str(), duration);
  }
}

// 一時的な箱型エミッター作成
void EmitterManager::MakeTimedBoxEmitter(const std::string& name, const Vector3& position, const Vector3& size,
  const Vector3& rotation, uint32_t count, float frequency, float duration)
{
  // 通常のエミッター作成関数を利用
  std::shared_ptr<BoxEmitter> emitter = CreateBoxEmitter(name, position, size, rotation, count, frequency);

  if (emitter) {
    // タイマー付きエミッターとして登録
    RegisterTimedEmitter(name, name, duration);
    Logger::Log("Registered box emitter '%s' as timed (%.2f seconds)", name.c_str(), duration);
  }
}

// 一時的な三角形エミッター作成
void EmitterManager::MakeTimedTriangleEmitter(const std::string& name, const Vector3& position,
  const Vector3& v1, const Vector3& v2, const Vector3& v3,
  uint32_t count, float frequency, float duration)
{
  // 通常のエミッター作成関数を利用
  std::shared_ptr<TriangleEmitter> emitter = CreateTriangleEmitter(name, position, v1, v2, v3, count, frequency);

  if (emitter) {
    // タイマー付きエミッターとして登録
    RegisterTimedEmitter(name, name, duration);
    Logger::Log("Registered triangle emitter '%s' as timed (%.2f seconds)", name.c_str(), duration);
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

    // タイマー付きエフェクトからも削除
    for (auto& effect : timedEffects_) {
      auto& emitterNames = effect.emitterNames;
      auto nameIt = std::find(emitterNames.begin(), emitterNames.end(), name);
      if (nameIt != emitterNames.end()) {
        emitterNames.erase(nameIt);
        Logger::Log("Removed emitter '%s' from timed effect '%s'", name.c_str(), effect.name.c_str());
      }
    }

    // グループからも削除
    for (auto& group : groupMap_) {
      auto& emitterNames = group.second.emitterNames;
      auto nameIt = std::find(emitterNames.begin(), emitterNames.end(), name);
      if (nameIt != emitterNames.end()) {
        emitterNames.erase(nameIt);
        Logger::Log("Removed emitter '%s' from group '%s'", name.c_str(), group.first.c_str());
      }
    }

    // 空になったタイマー付きエフェクトを削除
    for (auto timedIt = timedEffects_.begin(); timedIt != timedEffects_.end();) {
      if (timedIt->emitterNames.empty()) {
        Logger::Log("Removing empty timed effect '%s'", timedIt->name.c_str());
        timedIt = timedEffects_.erase(timedIt);
      } else {
        ++timedIt;
      }
    }
  } else {
    Logger::Log("RemoveEmitter: Emitter '%s' not found", name.c_str());
  }
}

void EmitterManager::RemoveAllEmitters()
{
  Logger::Log("RemoveAllEmitters: Removing all emitters (%zu emitters, %zu timed effects)",
    emitterMap_.size(), timedEffects_.size());

  // エミッターを1つずつ明示的に削除（GPUParticleシステムに通知するため）
  for (auto& pair : emitterMap_) {
    auto& emitter = pair.second;
    // エミッターを非アクティブ化して即時効果を得る
    emitter->SetActive(false);
  }

  // エミッターマップをクリア
  emitterMap_.clear();

  // グループを空にする
  for (auto& group : groupMap_) {
    group.second.emitterNames.clear();
  }

  // タイマー付きエフェクトのリストもクリア
  timedEffects_.clear();
}

// グループ機能
void EmitterManager::CreateGroup(const std::string& groupName)
{
  // グループの重複チェック
  if (groupMap_.find(groupName) != groupMap_.end()) {
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
  if (emitterMap_.find(emitterName) == emitterMap_.end()) {
    Logger::Log("Warning: Emitter '%s' not found. Cannot add to group.", emitterName.c_str());
    return;
  }

  // 既に追加済みかチェック
  auto& emitterNames = groupIt->second.emitterNames;
  if (std::find(emitterNames.begin(), emitterNames.end(), emitterName) != emitterNames.end()) {
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
    std::remove(emitterNames.begin(), emitterNames.end(), emitterName),
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

    // タイマー付きエフェクトも確認
    for (auto effIt = timedEffects_.begin(); effIt != timedEffects_.end();) {
      if (effIt->name + "_group" == groupName) {
        Logger::Log("RemoveGroup: Removing associated timed effect '%s'", effIt->name.c_str());
        effIt = timedEffects_.erase(effIt);
      } else {
        ++effIt;
      }
    }
  } else {
    Logger::Log("RemoveGroup: Group '%s' not found", groupName.c_str());
  }
}

// 更新処理
void EmitterManager::Update()
{
  float deltaTime = FrameTimer::GetInstance()->GetDeltaTime();

  // タイマー付きエフェクトの更新
  for (auto it = timedEffects_.begin(); it != timedEffects_.end();) {
    // 時間を更新
    it->currentTime += deltaTime;

    // 時間切れならエフェクトを削除
    if (it->currentTime >= it->duration) {
      Logger::Log("Timed effect '%s' expired (%.2f/%.2f seconds)",
        it->name.c_str(), it->currentTime, it->duration);

      // エミッターを削除
      for (const auto& emitterName : it->emitterNames) {
        RemoveEmitter(emitterName);
      }

      // リストから削除
      it = timedEffects_.erase(it);
    } else {
      ++it;
    }
  }
}

float EmitterManager::GetRemainingTime(const std::string& name) const
{
  for (const auto& effect : timedEffects_) {
    if (effect.name == name) {
      return (std::max)(0.0f, effect.duration - effect.currentTime);
    }
  }
  return 0.0f;  // 見つからない場合は0を返す
}

// エミッターをタイマー付きで登録する内部関数
void EmitterManager::RegisterTimedEmitter(const std::string& name, const std::string& emitterName, float duration)
{
  // 同名の既存エフェクトを探す
  for (auto it = timedEffects_.begin(); it != timedEffects_.end(); ++it) {
    if (it->name == name) {
      // 既存のエフェクトにエミッターを追加
      auto& emitterNames = it->emitterNames;
      // 重複がないか確認
      if (std::find(emitterNames.begin(), emitterNames.end(), emitterName) == emitterNames.end()) {
        emitterNames.push_back(emitterName);
        Logger::Log("Added emitter '%s' to existing timed effect '%s'", emitterName.c_str(), name.c_str());
      }
      return;
    }
  }

  // 新しいタイマー付きエフェクトを作成
  TimedEffect effect;
  effect.name = name;
  effect.duration = duration;
  effect.currentTime = 0.0f;
  effect.emitterNames.push_back(emitterName);
  timedEffects_.push_back(effect);

  Logger::Log("Created new timed effect '%s' with duration %.2f seconds", name.c_str(), duration);
}

#include "EmitterManager.h"
#include "GPUParticle.h"
#include "SphereEmitter.h"
#include "BoxEmitter.h"
#include "TriangleEmitter.h"
#include "Logger.h"
#include "FrameTimer.h"

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

  // デバッグログ
  Logger::Log("Created BoxEmitter '%s' - Pos:(%.2f,%.2f,%.2f) Size:(%.2f,%.2f,%.2f) Rot:(%.2f,%.2f,%.2f)",
    name.c_str(),
    position.x, position.y, position.z,
    size.x, size.y, size.z,
    rotation.x, rotation.y, rotation.z);

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

  // マップに追加
  emitterMap_[name] = emitter;

  return emitter;
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

    // エミッターの参照を保持
    auto emitter = it->second;

    // エミッターを非アクティブにする（即時効果）
    emitter->SetActive(false);

    // エミッターをマップから削除（shared_ptrなので自動解放）
    emitterMap_.erase(it);

    // タイマー付きエフェクトからもエミッターを削除
    for (auto& effect : timedEffects_) {
      auto& effectEmitters = effect.emitterNames;
      effectEmitters.erase(
        std::remove(effectEmitters.begin(), effectEmitters.end(), name),
        effectEmitters.end()
      );
    }

    // グループからも削除
    for (auto& group : groupMap_) {
      auto& emitterNames = group.second.emitterNames;
      size_t beforeSize = emitterNames.size();
      emitterNames.erase(
        std::remove(emitterNames.begin(), emitterNames.end(), name),
        emitterNames.end()
      );

      if (beforeSize != emitterNames.size()) {
        Logger::Log("RemoveEmitter: Removed '%s' from group '%s'",
          name.c_str(), group.first.c_str());
      }
    }

    // 空のタイマー付きエフェクトを削除
    for (auto timedIt = timedEffects_.begin(); timedIt != timedEffects_.end();) {
      if (timedIt->emitterNames.empty()) {
        Logger::Log("RemoveEmitter: Removing empty timed effect '%s'", timedIt->name.c_str());
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
  Logger::Log("EmitterManager: Removing ALL emitters (%zu emitters, %zu timed effects)",
    emitterMap_.size(), timedEffects_.size());

  // エミッターを1つずつ明示的に削除（GPUParticleシステムに通知するため）
  for (auto& pair : emitterMap_) {
    auto& emitter = pair.second;
    // エミッターを非アクティブ化して即時効果を得る
    emitter->SetActive(false);
    // GPUParticleシステムに削除を通知
    if (particleSystem_) {
      uint32_t emitterId = emitter->GetEmitterId();
      particleSystem_->RemoveEmitterById(emitterId);
    }
  }

  // エミッターマップをクリア
  emitterMap_.clear();

  // グループを空にする
  for (auto& group : groupMap_) {
    group.second.emitterNames.clear();
  }

  // タイマー付きエフェクトのリストもクリア（これが重要！）
  timedEffects_.clear();
}

void EmitterManager::ClearAllTimedEffects()
{
  Logger::Log("EmitterManager: Clearing all timed effects (%zu items)", timedEffects_.size());

  // すべてのタイマー付きエフェクトを処理
  for (auto& effect : timedEffects_) {
    // 関連するすべてのエミッターを削除
    for (const auto& emitterName : effect.emitterNames) {
      RemoveEmitter(emitterName);
    }
  }

  // リストをクリア
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

// エフェクトプリセット
void EmitterManager::CreateEffectPreset(EffectPresetType type, const std::string& name, const Vector3& position, float scale)
{
  // エフェクトタイプに応じたエミッター群を作成
  std::vector<std::shared_ptr<GPUParticleEmitter>> emitters;

  switch (type) {
  case EffectPresetType::Explosion:
    emitters = CreateExplosionEffect(name, position, scale);
    break;

  case EffectPresetType::Fire:
    emitters = CreateFireEffect(name, position, scale);
    break;

  case EffectPresetType::Smoke:
    emitters = CreateSmokeEffect(name, position, scale);
    break;

  case EffectPresetType::Magic:
    emitters = CreateMagicEffect(name, position, scale);
    break;

  default:
    Logger::Log("Warning: Unknown effect preset type: %d", static_cast<int>(type));
    return;
  }

  // グループを作成してエミッターを追加
  std::string groupName = name + "_group";
  CreateGroup(groupName);

  for (size_t i = 0; i < emitters.size(); i++) {
    std::string emitterName = name + "_" + std::to_string(i);
    emitterMap_[emitterName] = emitters[i];
    AddToGroup(groupName, emitterName);
  }
}

// 特殊エフェクト
void EmitterManager::TriggerExplosion(const std::string& name, const Vector3& position, float radius, float duration)
{
  // 同名の既存エフェクトをまず削除
  for (auto it = timedEffects_.begin(); it != timedEffects_.end();) {
    if (it->name == name) {
      // 関連エミッターを削除
      for (const auto& emitterName : it->emitterNames) {
        RemoveEmitter(emitterName);
      }
      it = timedEffects_.erase(it);
    } else {
      ++it;
    }
  }

  std::vector<std::shared_ptr<GPUParticleEmitter>> emitters = CreateExplosionEffect(name, position, radius);

  TimedEffect effect;
  effect.name = name;
  effect.duration = duration;
  effect.currentTime = 0.0f;

  for (size_t i = 0; i < emitters.size(); i++) {
    std::string emitterName = name + "_" + std::to_string(i);
    emitterMap_[emitterName] = emitters[i];
    effect.emitterNames.push_back(emitterName);
  }

  timedEffects_.push_back(effect);
}

void EmitterManager::CreateTrailEffect(const std::string& name, const Vector3& startPosition, const Vector3& direction, float length, float width, float duration)
{
  // 同名の既存エフェクトをまず削除（重要）
  for (auto it = timedEffects_.begin(); it != timedEffects_.end();) {
    if (it->name == name) {
      Logger::Log("CreateTrailEffect: Removing existing effect with same name: %s", name.c_str());
      // 関連エミッターを削除
      for (const auto& emitterName : it->emitterNames) {
        RemoveEmitter(emitterName);
      }
      it = timedEffects_.erase(it);
    } else {
      ++it;
    }
  }

  // グループがすでに存在する場合は削除
  std::string groupName = name + "_group";
  if (groupMap_.find(groupName) != groupMap_.end()) {
    Logger::Log("CreateTrailEffect: Removing existing group: %s", groupName.c_str());
    RemoveGroup(groupName);
  }

  // 方向ベクトルを正規化
  Vector3 normalizedDir = direction;
  float dirLength = std::sqrt(direction.x * direction.x + direction.y * direction.y + direction.z * direction.z);
  if (dirLength > 0.001f) {
    normalizedDir = direction * (1.0f / dirLength);
  }

  // トレイルエミッターの名前を配列で管理（コード管理を簡略化）
  std::vector<std::string> emitterNames;

  // 1. 主要なトレイル部分（中央線）
  std::string trailMainName = name + "_main";
  auto trailMain = CreateBoxEmitter(
    trailMainName,
    startPosition + normalizedDir * (length * 0.5f),
    Vector3(width, width, length),
    Vector3(0, 0, 0),
    30,
    0.05f
  );
  trailMain->SetColor(Vector4(0.8f, 0.8f, 1.0f, 0.8f));
  emitterNames.push_back(trailMainName);

  // 2. トレイルの開始位置
  std::string trailStartName = name + "_start";
  auto trailStart = CreateSphereEmitter(
    trailStartName,
    startPosition,
    width * 1.2f,
    15,
    0.1f
  );
  trailStart->SetColor(Vector4(1.0f, 0.8f, 0.5f, 0.9f));
  emitterNames.push_back(trailStartName);

  // 3. トレイルの終端
  std::string trailEndName = name + "_end";
  auto trailEnd = CreateSphereEmitter(
    trailEndName,
    startPosition + normalizedDir * length,
    width * 0.8f,
    10,
    0.15f
  );
  trailEnd->SetColor(Vector4(0.5f, 0.8f, 1.0f, 0.7f));
  emitterNames.push_back(trailEndName);

  // グループ作成
  CreateGroup(groupName);
  for (const auto& emitterName : emitterNames) {
    AddToGroup(groupName, emitterName);
  }

  // タイマー付きエフェクト
  if (duration > 0.0f) {
    TimedEffect effect;
    effect.name = name;
    effect.duration = duration;
    effect.currentTime = 0.0f;
    effect.emitterNames = emitterNames;
    timedEffects_.push_back(effect);

    Logger::Log("CreateTrailEffect: Created trail '%s' with %zu emitters, duration: %.2f seconds",
      name.c_str(), emitterNames.size(), duration);
  } else {
    Logger::Log("CreateTrailEffect: Created permanent trail '%s' with %zu emitters",
      name.c_str(), emitterNames.size());
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
      // エミッターを削除
      for (const auto& emitterName : it->emitterNames) {
        RemoveEmitter(emitterName);
      }

      // リストから削除
      it = timedEffects_.erase(it);
    } else {
      // 次のエフェクトへ
      ++it;
    }
  }
}

// 爆発エフェクト作成
std::vector<std::shared_ptr<GPUParticleEmitter>> EmitterManager::CreateExplosionEffect(const std::string& baseName, const Vector3& position, float scale)
{
  baseName;

  std::vector<std::shared_ptr<GPUParticleEmitter>> emitters;

  // 1. 中心の球体エミッター（メインの爆発）
  auto mainEmitter = particleSystem_->CreateSphereEmitter(
    position,
    0.5f * scale,
    50,
    0.05f
  );
  mainEmitter->SetColor(Vector4(1.0f, 0.7f, 0.3f, 1.0f));  // オレンジ色
  emitters.push_back(mainEmitter);

  // 2. 外側の球体エミッター（衝撃波）
  auto shockwaveEmitter = particleSystem_->CreateSphereEmitter(
    position,
    1.0f * scale,
    30,
    0.1f
  );
  shockwaveEmitter->SetColor(Vector4(0.9f, 0.9f, 0.9f, 0.8f));  // 白っぽい
  emitters.push_back(shockwaveEmitter);

  // 3. 破片エミッター
  auto debrisEmitter = particleSystem_->CreateSphereEmitter(
    position,
    0.8f * scale,
    20,
    0.2f
  );
  debrisEmitter->SetColor(Vector4(0.6f, 0.6f, 0.6f, 1.0f));  // グレー
  emitters.push_back(debrisEmitter);

  return emitters;
}

// 炎エフェクト作成
std::vector<std::shared_ptr<GPUParticleEmitter>> EmitterManager::CreateFireEffect(const std::string& baseName, const Vector3& position, float scale)
{
  baseName;

  std::vector<std::shared_ptr<GPUParticleEmitter>> emitters;

  // 1. 下部の炎（三角形）
  Vector3 v1 = { -0.5f * scale, 0.0f, -0.5f * scale };
  Vector3 v2 = { 0.5f * scale, 0.0f, -0.5f * scale };
  Vector3 v3 = { 0.0f, 0.0f, 0.5f * scale };

  auto baseFireEmitter = particleSystem_->CreateTriangleEmitter(
    position,
    v1, v2, v3,
    15,
    0.05f
  );
  baseFireEmitter->SetColor(Vector4(1.0f, 0.5f, 0.2f, 1.0f));  // オレンジ色
  emitters.push_back(baseFireEmitter);

  // 2. 中部の炎（球体）
  auto midFireEmitter = particleSystem_->CreateSphereEmitter(
    Vector3(position.x, position.y + 0.5f * scale, position.z),
    0.4f * scale,
    10,
    0.1f
  );
  midFireEmitter->SetColor(Vector4(1.0f, 0.7f, 0.3f, 0.9f));  // オレンジ色
  emitters.push_back(midFireEmitter);

  // 3. 上部の煙（球体）
  auto smokeEmitter = particleSystem_->CreateSphereEmitter(
    Vector3(position.x, position.y + 1.2f * scale, position.z),
    0.6f * scale,
    5,
    0.2f
  );
  smokeEmitter->SetColor(Vector4(0.5f, 0.5f, 0.5f, 0.7f));  // グレー
  emitters.push_back(smokeEmitter);

  return emitters;
}

// 煙エフェクト作成
std::vector<std::shared_ptr<GPUParticleEmitter>> EmitterManager::CreateSmokeEffect(const std::string& baseName, const Vector3& position, float scale)
{
  baseName;

  std::vector<std::shared_ptr<GPUParticleEmitter>> emitters;

  // 1. 下部の煙（小さい球体）
  auto baseSmoke = particleSystem_->CreateSphereEmitter(
    position,
    0.3f * scale,
    10,
    0.1f
  );
  baseSmoke->SetColor(Vector4(0.7f, 0.7f, 0.7f, 0.9f));  // グレー
  emitters.push_back(baseSmoke);

  // 2. 中部の煙（中間サイズの球体）
  auto midSmoke = particleSystem_->CreateSphereEmitter(
    Vector3(position.x, position.y + 0.5f * scale, position.z),
    0.5f * scale,
    7,
    0.15f
  );
  midSmoke->SetColor(Vector4(0.6f, 0.6f, 0.6f, 0.8f));  // グレー
  emitters.push_back(midSmoke);

  // 3. 上部の煙（大きい球体）
  auto topSmoke = particleSystem_->CreateSphereEmitter(
    Vector3(position.x, position.y + 1.0f * scale, position.z),
    0.8f * scale,
    5,
    0.2f
  );
  topSmoke->SetColor(Vector4(0.5f, 0.5f, 0.5f, 0.6f));  // グレー
  emitters.push_back(topSmoke);

  return emitters;
}

// 魔法エフェクト作成
std::vector<std::shared_ptr<GPUParticleEmitter>> EmitterManager::CreateMagicEffect(const std::string& baseName, const Vector3& position, float scale)
{
  baseName;

  std::vector<std::shared_ptr<GPUParticleEmitter>> emitters;

  // 1. 中心の輝き（小さい球体）
  auto coreEmitter = particleSystem_->CreateSphereEmitter(
    position,
    0.2f * scale,
    20,
    0.05f
  );
  coreEmitter->SetColor(Vector4(0.5f, 0.8f, 1.0f, 1.0f));  // 淡い青色
  emitters.push_back(coreEmitter);

  // 2. 周囲のオーラ（中間サイズの球体）
  auto auraEmitter = particleSystem_->CreateSphereEmitter(
    position,
    0.6f * scale,
    15,
    0.1f
  );
  auraEmitter->SetColor(Vector4(0.4f, 0.6f, 1.0f, 0.8f));  // 青色
  emitters.push_back(auraEmitter);

  // 3. 外側の粒子（大きい球体）
  auto outerEmitter = particleSystem_->CreateSphereEmitter(
    position,
    1.2f * scale,
    10,
    0.15f
  );
  outerEmitter->SetColor(Vector4(0.3f, 0.5f, 1.0f, 0.6f));  // 青色
  emitters.push_back(outerEmitter);

  // 4. 回転する粒子リング（箱型をリング状に）
  auto ringEmitter = particleSystem_->CreateBoxEmitter(
    position,
    Vector3(1.0f * scale, 0.1f * scale, 1.0f * scale),  // 薄いリング状
    Vector3(0.0f, 45.0f, 0.0f),  // 少し傾ける
    15,
    0.1f
  );
  ringEmitter->SetColor(Vector4(0.7f, 0.9f, 1.0f, 0.7f));  // 明るい青色
  emitters.push_back(ringEmitter);

  return emitters;
}
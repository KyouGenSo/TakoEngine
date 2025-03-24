#include "EmitterManager.h"
#include "GPUParticle.h"
#include "SphereEmitter.h"
#include "BoxEmitter.h"
#include "TriangleEmitter.h"
#include "Logger.h"

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
    // エミッターをマップから削除（shared_ptrなので自動解放）
    emitterMap_.erase(it);

    // グループからも削除
    for (auto& group : groupMap_) {
      auto& emitterNames = group.second.emitterNames;
      emitterNames.erase(
        std::remove(emitterNames.begin(), emitterNames.end(), name),
        emitterNames.end()
      );
    }
  }
}

void EmitterManager::RemoveAllEmitters()
{
  // すべてのエミッターを削除
  emitterMap_.clear();

  // すべてのグループを空にする
  for (auto& group : groupMap_) {
    group.second.emitterNames.clear();
  }
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
  // グループをマップから削除
  groupMap_.erase(groupName);
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
  // エミッター群を作成
  std::vector<std::shared_ptr<GPUParticleEmitter>> emitters = CreateExplosionEffect(name, position, radius);

  // 一時エフェクト情報を作成
  TimedEffect effect;
  effect.name = name;
  effect.duration = duration;
  effect.currentTime = 0.0f;

  // エミッターを登録
  for (size_t i = 0; i < emitters.size(); i++) {
    std::string emitterName = name + "_" + std::to_string(i);
    emitterMap_[emitterName] = emitters[i];
    effect.emitterNames.push_back(emitterName);
  }

  // タイマー付きエフェクトリストに追加
  timedEffects_.push_back(effect);
}

void EmitterManager::CreateTrailEffect(const std::string& name, const Vector3& startPosition, const Vector3& direction, float length, float width, float duration)
{
  // 方向ベクトルを正規化
  Vector3 normalizedDir = direction;
  float dirLength = std::sqrt(direction.x * direction.x + direction.y * direction.y + direction.z * direction.z);
  if (dirLength > 0.001f) {
    normalizedDir = direction * (1.0f / dirLength);
  }

  // トレイル用のエミッターを作成
  // 1. 主要なトレイル部分（中央線）
  auto trailMain = CreateBoxEmitter(
    name + "_main",
    startPosition + normalizedDir * (length * 0.5f),  // 中央に配置
    Vector3(width, width, length),                    // 長さ方向に伸ばす
    Vector3(0, 0, 0),                                // 回転なし
    30,                                              // パーティクル数
    0.05f                                            // 高頻度で射出
  );

  // トレイルの色を設定
  trailMain->SetColor(Vector4(0.8f, 0.8f, 1.0f, 0.8f));  // 淡い青色

  // 2. トレイルの開始位置（やや大きめ）
  auto trailStart = CreateSphereEmitter(
    name + "_start",
    startPosition,
    width * 1.2f,
    15,
    0.1f
  );
  trailStart->SetColor(Vector4(1.0f, 0.8f, 0.5f, 0.9f));  // オレンジがかった色

  // 3. トレイルの終端
  auto trailEnd = CreateSphereEmitter(
    name + "_end",
    startPosition + normalizedDir * length,
    width * 0.8f,
    10,
    0.15f
  );
  trailEnd->SetColor(Vector4(0.5f, 0.8f, 1.0f, 0.7f));  // 青色

  // グループ作成
  std::string groupName = name + "_group";
  CreateGroup(groupName);
  AddToGroup(groupName, name + "_main");
  AddToGroup(groupName, name + "_start");
  AddToGroup(groupName, name + "_end");

  // タイマー付きエフェクト
  if (duration > 0.0f) {
    TimedEffect effect;
    effect.name = name;
    effect.duration = duration;
    effect.currentTime = 0.0f;
    effect.emitterNames = { name + "_main", name + "_start", name + "_end" };
    timedEffects_.push_back(effect);
  }
}

// 更新処理
void EmitterManager::Update(float deltaTime)
{
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

      //
#include "EmitterManager.h"
#include "GPUParticle.h"
#include "SphereEmitter.h"
#include "BoxEmitter.h"
#include "TriangleEmitter.h"
#include "MeshEmitter.h"
#include "Object3d.h"
#include "ForceFieldManager.h"
#include "FrameTimer.h"
#include "TextureManager.h"
#include <algorithm>
#include <ranges>
#include <memory>
#include <filesystem>
#include <fstream>
#include <iomanip>

#ifdef _DEBUG
#include "DebugUIManager.h"
#endif

namespace Tako {

  EmitterManager::EmitterManager(GPUParticle* particleSystem)
    : particleSystem_(particleSystem)
  {
  }

  EmitterManager::~EmitterManager()
  {
    RemoveAllEmitters();
  }

  void EmitterManager::AddNamedEmitter(const std::string& name, std::shared_ptr<GPUParticleEmitter> emitter)
  {
    // 同名エミッターは警告を出して置き換える
    if (emitterMap_.contains(name)) {
#ifdef _DEBUG
      DebugUIManager::GetInstance()->AddLog(
        "Emitter name '" + name + "' already exists. Overwriting.", DebugUIManager::LogType::Warning);
#endif
      RemoveEmitter(name);
    }

    particleSystem_->RegisterEmitter(emitter);
    emitterMap_[name] = std::move(emitter);
  }

  // エミッター作成（名前付き）
  void EmitterManager::CreateSphereEmitter(const std::string& name, const Vector3& position, float radius, uint32_t count, float frequency)
  {
    AddNamedEmitter(name, std::make_shared<SphereEmitter>(particleSystem_, position, radius, count, frequency));
  }

  void EmitterManager::CreateBoxEmitter(const std::string& name, const Vector3& position, const Vector3& size, const Vector3& rotation, uint32_t count, float frequency)
  {
    AddNamedEmitter(name, std::make_shared<BoxEmitter>(particleSystem_, position, size, rotation, count, frequency));
  }

  void EmitterManager::CreateMeshEmitterFromModel(const std::string& name, const std::string& modelPath, uint32_t count, float frequency)
  {
    // 先にモデルをロードし、成功を確認してから既存 emitter を置き換える。
    Model* model = particleSystem_->AcquireModel(modelPath);
    if (model == nullptr) {
#ifdef _DEBUG
      DebugUIManager::GetInstance()->AddLog(
        "CreateMeshEmitterFromModel: failed to load model '" + modelPath + "'", DebugUIManager::LogType::Error);
#endif
      return;
    }

    auto emitter = std::make_shared<MeshEmitter>(particleSystem_, model, count, frequency);
    emitter->SetSpawnModelPath(modelPath); // JSON 永続化用 (パスからスポーン形状を復元可能に)
    AddNamedEmitter(name, std::move(emitter));
  }

  void EmitterManager::CreateMeshEmitter(const std::string& name, Object3d* obj3d, uint32_t count, float frequency)
  {
    // null の Object3d は三角形数 0 の無効エミッタとして登録されてしまうため、登録前に弾く
    if (obj3d == nullptr) {
#ifdef _DEBUG
      DebugUIManager::GetInstance()->AddLog(
        "CreateMeshEmitter: Object3d is null for emitter '" + name + "'", DebugUIManager::LogType::Error);
#endif
      return;
    }

    auto emitter = std::make_shared<MeshEmitter>(particleSystem_, obj3d->GetModel(), count, frequency);
    emitter->BindObject3d(obj3d);
    AddNamedEmitter(name, std::move(emitter));
  }

  void EmitterManager::CreateTriangleEmitter(const std::string& name, const Vector3& position, const Vector3& v1, const Vector3& v2, const Vector3& v3, uint32_t count, float frequency)
  {
    AddNamedEmitter(name, std::make_shared<TriangleEmitter>(particleSystem_, position, v1, v2, v3, count, frequency));
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
      }
      else {
#ifdef _DEBUG
        DebugUIManager::GetInstance()->AddLog(
          "UpdateSphereEmitter: Emitter '" + name + "' is not a SphereEmitter", DebugUIManager::LogType::Warning);
#endif
      }

    }
    else {
#ifdef _DEBUG
      DebugUIManager::GetInstance()->AddLog(
        "UpdateSphereEmitter: Emitter '" + name + "' not found", DebugUIManager::LogType::Warning);
#endif
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
      }
      else {
#ifdef _DEBUG
        DebugUIManager::GetInstance()->AddLog(
          "UpdateBoxEmitter: Emitter '" + name + "' is not a BoxEmitter", DebugUIManager::LogType::Warning);
#endif
      }

    }
    else {
#ifdef _DEBUG
      DebugUIManager::GetInstance()->AddLog(
        "UpdateBoxEmitter: Emitter '" + name + "' not found", DebugUIManager::LogType::Warning);
#endif
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
      }
      else {
#ifdef _DEBUG
        DebugUIManager::GetInstance()->AddLog(
          "UpdateTriangleEmitter: Emitter '" + name + "' is not a TriangleEmitter", DebugUIManager::LogType::Warning);
#endif
      }

    }
    else {
#ifdef _DEBUG
      DebugUIManager::GetInstance()->AddLog(
        "UpdateTriangleEmitter: Emitter '" + name + "' not found", DebugUIManager::LogType::Warning);
#endif
    }
  }

  void EmitterManager::CreateTemporaryEmitterFrom(const std::string& sourceName, const std::string& newName, float lifeTime)
  {
    auto sourceIt = emitterMap_.find(sourceName);
    if (sourceIt == emitterMap_.end()) {
#ifdef _DEBUG
      DebugUIManager::GetInstance()->AddLog(
        "CreateTemporaryEmitterFrom: Source emitter '" + sourceName + "' not found", DebugUIManager::LogType::Error);
#endif
      return;
    }

    if (emitterMap_.contains(newName)) {
#ifdef _DEBUG
      DebugUIManager::GetInstance()->AddLog(
        "Emitter name '" + newName + "' already exists. Overwriting.", DebugUIManager::LogType::Warning);
#endif

      RemoveEmitter(newName);
    }

    std::shared_ptr<GPUParticleEmitter> sourceEmitter = sourceIt->second;
    std::shared_ptr<GPUParticleEmitter> newEmitter = particleSystem_->CreateTemporaryEmitterFrom(sourceEmitter.get(), lifeTime);

    if (newEmitter) {
      // グループには追加しない
      emitterMap_[newName] = newEmitter;

      // frequencyTime を frequency に合わせ次フレームで即射出
      newEmitter->SetFrequencyTime(newEmitter->GetFrequency());
#ifdef _DEBUG
      DebugUIManager::GetInstance()->AddLog(
        "CreateTemporaryEmitterFrom: Created temporary emitter '" + newName + "' from '" + sourceName + "' with lifetime " + std::to_string(lifeTime) + " seconds", DebugUIManager::LogType::Info);
#endif
    }
    else {
#ifdef _DEBUG
      DebugUIManager::GetInstance()->AddLog(
        "CreateTemporaryEmitterFrom: Failed to create emitter from '" + sourceName + "'", DebugUIManager::LogType::Error);
#endif

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
#ifdef _DEBUG
      DebugUIManager::GetInstance()->AddLog(
        "UpdateTemporaryEmitters: Removing expired emitter '" + name + "'", DebugUIManager::LogType::Info);
#endif
      RemoveEmitter(name);
    }
  }

  void EmitterManager::Update()
  {
    UpdateTemporaryEmitters();
  }

  GPUParticleEmitter* EmitterManager::FindEmitter(const std::string& name)
  {
    auto it = emitterMap_.find(name);
    return it != emitterMap_.end() ? it->second.get() : nullptr;
  }

  void EmitterManager::SetEmitterPosition(const std::string& name, const Vector3& position)
  {
    if (auto* e = FindEmitter(name)) e->SetPosition(position);
  }

  void EmitterManager::SetEmitterScaleRange(const std::string& name, const Vector2& scaleRangeX, const Vector2& scaleRangeY)
  {
    if (auto* e = FindEmitter(name)) e->SetScaleRange(scaleRangeX, scaleRangeY);
  }

  void EmitterManager::SetEmitterVelocityRange(const std::string& name, const Vector2& velRangeX, const Vector2& velRangeY, const Vector2& velRangeZ)
  {
    if (auto* e = FindEmitter(name)) e->SetVelRange(velRangeX, velRangeY, velRangeZ);
  }

  void EmitterManager::SetEmitterLifeTimeRange(const std::string& name, const Vector2& lifeTimeRange)
  {
    if (auto* e = FindEmitter(name)) e->SetLifeTimeRange(lifeTimeRange);
  }

  void EmitterManager::SetEmitterActive(const std::string& name, bool isActive)
  {
    if (auto* e = FindEmitter(name)) e->SetActive(isActive);
  }

  void EmitterManager::SetEmitterCount(const std::string& name, const uint32_t count)
  {
    if (auto* e = FindEmitter(name)) e->SetParticleCount(count);
  }

  void EmitterManager::SetEmitterNormalize(const std::string& name, bool isNormalize)
  {
    if (auto* e = FindEmitter(name)) e->SetNormalize(isNormalize);
  }

  void EmitterManager::SetEmitterRandomRotateZ(const std::string& name, bool isRandomRotateZ)
  {
    if (auto* e = FindEmitter(name)) e->SetRandomRotateZ(isRandomRotateZ);
  }

  void EmitterManager::SetEmitterColor(const std::string& name, const Vector4& color)
  {
    if (auto* e = FindEmitter(name)) e->SetColor(color);
  }

  void EmitterManager::SetEmitterStartColor(const std::string& name, const Vector4& color)
  {
    if (auto* e = FindEmitter(name)) e->SetStartColor(color);
  }

  void EmitterManager::SetEmitterEndColor(const std::string& name, const Vector4& color)
  {
    if (auto* e = FindEmitter(name)) e->SetEndColor(color);
  }

  void EmitterManager::SetEmitterColors(const std::string& name, const Vector4& startColor, const Vector4& endColor)
  {
    if (auto* e = FindEmitter(name)) e->SetColors(startColor, endColor);
  }

  void EmitterManager::SetEmitterRadius(const std::string& name, float radius)
  {
    auto it = emitterMap_.find(name);

    if (it != emitterMap_.end()) {
      auto sphereEmitter = std::dynamic_pointer_cast<SphereEmitter>(it->second);

      if (sphereEmitter) {
        sphereEmitter->SetRadius(radius);
      }
      else {
#ifdef _DEBUG
        DebugUIManager::GetInstance()->AddLog(
          "UpdateSphereEmitter: Emitter '" + name + "' is not a SphereEmitter", DebugUIManager::LogType::Warning);
#endif
      }

    }
    else {
#ifdef _DEBUG
      DebugUIManager::GetInstance()->AddLog(
        "UpdateSphereEmitter: Emitter '" + name + "' not found", DebugUIManager::LogType::Warning);
#endif
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
#ifdef _DEBUG
      DebugUIManager::GetInstance()->AddLog(
        "RemoveEmitter: Removing emitter '" + name + "'",
        DebugUIManager::LogType::Info);
#endif

      // 非アクティブ化して描画へ即反映
      it->second->SetActive(false);

      std::shared_ptr<GPUParticleEmitter> emitter = it->second;
      emitterMap_.erase(it);

      if (particleSystem_) {
        particleSystem_->UnregisterEmitter(emitter);
      }

      // 所属する全グループからも名前を除去
      for (auto& [groupName, group] : groupMap_) {
        auto& names = group.emitterNames;

        auto removeIt = std::find(names.begin(), names.end(), name);
        if (removeIt != names.end()) {
          names.erase(removeIt);
#ifdef _DEBUG
          DebugUIManager::GetInstance()->AddLog(
            "Removed emitter '" + name + "' from group '" + groupName + "'",
            DebugUIManager::LogType::Info);
#endif

        }
      }
    }
    else {
#ifdef _DEBUG
      DebugUIManager::GetInstance()->AddLog(
        "RemoveEmitter: Emitter '" + name + "' not found",
        DebugUIManager::LogType::Warning);
#endif
    }
  }

  void EmitterManager::RemoveAllEmitters()
  {
#ifdef _DEBUG
    DebugUIManager::GetInstance()->AddLog(
      "RemoveAllEmitters: Removing all emitters (" + std::to_string(emitterMap_.size()) + " emitters)",
      DebugUIManager::LogType::Info);
#endif

    if (emitterMap_.empty()) return; // すでに空の場合は何もしない

    // GPUParticle へ登録解除を通知するため 1 つずつ処理する
    for (auto& val : emitterMap_ | std::views::values) {
      auto& emitter = val;
      emitter->SetActive(false);
      particleSystem_->UnregisterEmitter(emitter);
    }

    emitterMap_.clear();

    for (auto& val : groupMap_ | std::views::values) {
      val.emitterNames.clear();
    }
  }

  // グループ機能
  void EmitterManager::CreateGroup(const std::string& groupName)
  {
    if (groupMap_.contains(groupName)) {
#ifdef _DEBUG
      DebugUIManager::GetInstance()->AddLog(
        "Group name '" + groupName + "' already exists.", DebugUIManager::LogType::Warning);
#endif
      return;
    }

    EmitterGroup group;
    group.name = groupName;
    group.isActive = true;
    groupMap_[groupName] = group;
  }

  void EmitterManager::AddToGroup(const std::string& groupName, const std::string& emitterName)
  {
    auto groupIt = groupMap_.find(groupName);
    if (groupIt == groupMap_.end()) {
#ifdef _DEBUG
      DebugUIManager::GetInstance()->AddLog(
        "Group '" + groupName + "' not found. Creating new group.", DebugUIManager::LogType::Warning);
#endif

      CreateGroup(groupName);
      groupIt = groupMap_.find(groupName);
    }

    if (!emitterMap_.contains(emitterName)) {
#ifdef _DEBUG
      DebugUIManager::GetInstance()->AddLog(
        "Emitter '" + emitterName + "' not found. Cannot add to group.", DebugUIManager::LogType::Warning);
#endif

      return;
    }

    auto& emitterNames = groupIt->second.emitterNames;
    if (std::ranges::find(emitterNames, emitterName) != emitterNames.end()) {
      return; // 既に追加済み
    }

    emitterNames.push_back(emitterName);
  }

  void EmitterManager::RemoveFromGroup(const std::string& groupName, const std::string& emitterName)
  {
    auto groupIt = groupMap_.find(groupName);
    if (groupIt == groupMap_.end()) {
      return;
    }

    auto& emitterNames = groupIt->second.emitterNames;
    emitterNames.erase(
      std::ranges::remove(emitterNames, emitterName).begin(),
      emitterNames.end()
    );
  }

  void EmitterManager::SetGroupActive(const std::string& groupName, bool isActive)
  {
    auto groupIt = groupMap_.find(groupName);
    if (groupIt == groupMap_.end()) {
      return;
    }

    groupIt->second.isActive = isActive;

    // 所属エミッターにもアクティブ状態を伝播
    for (const auto& emitterName : groupIt->second.emitterNames) {
      auto emitterIt = emitterMap_.find(emitterName);
      if (emitterIt != emitterMap_.end()) {
        emitterIt->second->SetActive(isActive);
      }
    }
  }

  void EmitterManager::SetGroupPosition(const std::string& groupName, const Vector3& position)
  {
    auto groupIt = groupMap_.find(groupName);
    if (groupIt == groupMap_.end()) {
      return;
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
        }
        else {
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
#ifdef _DEBUG
      DebugUIManager::GetInstance()->AddLog(
        "RemoveGroup: Removing group '" + groupName + "'", DebugUIManager::LogType::Info);
#endif

      // グループ内のすべてのエミッターの名前をコピー（ループ中に変更されるため）
      std::vector<std::string> emitterNames = it->second.emitterNames;

      // グループに属するすべてのエミッターを削除（オプション、コメントアウト可能）
      if (!emitterNames.empty()) {
#ifdef _DEBUG
        DebugUIManager::GetInstance()->AddLog(
          "RemoveGroup: Removing " + std::to_string(emitterNames.size()) + " emitters in group '" + groupName + "'", DebugUIManager::LogType::Info);
#endif

        // エミッターを一つずつ削除
        for (const auto& name : emitterNames) {
          RemoveEmitter(name);
        }
      }

      // グループをマップから削除
      groupMap_.erase(it);

    }
    else {
#ifdef _DEBUG
      DebugUIManager::GetInstance()->AddLog(
        "RemoveGroup: Group '" + groupName + "' not found", DebugUIManager::LogType::Warning);
#endif

    }
  }

  //========================================
  // JSON 保存・読み込み機能
  //========================================

  void EmitterManager::SaveScenePreset(const std::string& filename)
  {
    using json = nlohmann::json;
    json root;

    const std::string directory = "resources/Json/ParticlePresets/";
    if (!std::filesystem::exists(directory)) {
      std::filesystem::create_directories(directory);
    }

    root["emitters"] = json::object();
    for (const auto& [name, emitter] : emitterMap_) {
      json emitterJson;
      SerializeEmitterToJSON(emitter, emitterJson);
      root["emitters"][name] = emitterJson;
    }

    // グループ情報も保存
    root["groups"] = json::object();
    for (const auto& [groupName, group] : groupMap_) {
      json groupJson;
      groupJson["name"] = group.name;
      groupJson["emitters"] = group.emitterNames;
      groupJson["isActive"] = group.isActive;
      root["groups"][groupName] = groupJson;
    }

    // フォースフィールドも統合保存（連携設定済みの場合のみ）
    if (forceFieldManager_) {
      forceFieldManager_->SerializeAllToJSON(root);
    }

    // ファイルに書き込み
    std::string filepath = directory + filename + ".json";
    std::ofstream ofs(filepath);
    if (ofs.is_open()) {
      ofs << std::setw(2) << root << std::endl;
      ofs.close();
#ifdef _DEBUG
      DebugUIManager::GetInstance()->AddLog("Saved emitters to: " + filepath, DebugUIManager::LogType::Info);
#endif

    }
    else {
#ifdef _DEBUG
      DebugUIManager::GetInstance()->AddLog(
        "Failed to save emitters to: " + filepath, DebugUIManager::LogType::Error);
#endif

    }
  }

  void EmitterManager::LoadScenePreset(const std::string& filename)
  {
    using json = nlohmann::json;

    const std::string directory = "resources/Json/ParticlePresets/";
    std::string filepath = directory + filename + ".json";

    std::ifstream ifs(filepath);
    if (!ifs.is_open()) {
#ifdef _DEBUG
      DebugUIManager::GetInstance()->AddLog(
        "Failed to load emitters from: " + filepath, DebugUIManager::LogType::Error);
#endif

      return;
    }

    json root;
    ifs >> root;
    ifs.close();

    // エミッターを読み込み (meshModelPath を持たない Mesh エミッタは警告 + スキップ)
    if (root.contains("emitters")) {
      for (auto& [name, emitterJson] : root["emitters"].items()) {
        auto emitter = DeserializeEmitterFromJSON(emitterJson);
        if (emitter) {
          particleSystem_->RegisterEmitter(emitter);
          emitterMap_[name] = emitter;
        }
      }
    }

    // グループを読み込み
    if (root.contains("groups")) {
      for (auto& [groupName, groupJson] : root["groups"].items()) {
        EmitterGroup group;
        group.name = groupJson["name"];
        group.emitterNames = groupJson["emitters"].get<std::vector<std::string>>();
        group.isActive = groupJson["isActive"];
        groupMap_[groupName] = group;
      }
    }

    // フォースフィールドも統合読込（連携設定済み + JSON にキー存在の場合のみ）
    if (forceFieldManager_) {
      forceFieldManager_->DeserializeAllFromJSON(root);
    }
#ifdef _DEBUG
    DebugUIManager::GetInstance()->AddLog("Loaded emitters from: " + filepath, DebugUIManager::LogType::Info);
#endif
  }

  void EmitterManager::SavePreset(const std::string& presetName, const std::string& emitterName)
  {
    auto it = emitterMap_.find(emitterName);
    if (it == emitterMap_.end()) {
#ifdef _DEBUG
      DebugUIManager::GetInstance()->AddLog(
        "SavePreset: Emitter '" + emitterName + "' not found", DebugUIManager::LogType::Warning);
#endif

      return;
    }

    using json = nlohmann::json;
    json preset;
    SerializeEmitterToJSON(it->second, preset);

    // プリセットディレクトリ
    const std::string directory = "resources/Json/ParticlePresets/Presets/";
    if (!std::filesystem::exists(directory)) {
      std::filesystem::create_directories(directory);
    }

    std::string filepath = directory + presetName + ".json";
    std::ofstream ofs(filepath);
    if (ofs.is_open()) {
      ofs << std::setw(2) << preset << std::endl;
      ofs.close();
#ifdef _DEBUG
      DebugUIManager::GetInstance()->AddLog("Saved preset: " + presetName, DebugUIManager::LogType::Info);
#endif

    }
  }

  void EmitterManager::LoadPreset(const std::string& presetName, const std::string& newEmitterName)
  {
    using json = nlohmann::json;

    const std::string directory = "resources/Json/ParticlePresets/Presets/";
    std::string filepath = directory + presetName + ".json";

    std::ifstream ifs(filepath);

    if (!ifs.is_open()) {
#ifdef _DEBUG
      DebugUIManager::GetInstance()->AddLog("Failed to load preset: " + presetName, DebugUIManager::LogType::Error);
#endif

      return;
    }

    json preset;
    ifs >> preset;
    ifs.close();

    auto emitter = DeserializeEmitterFromJSON(preset);
    if (emitter) {
      particleSystem_->RegisterEmitter(emitter);
      emitterMap_[newEmitterName] = emitter;
#ifdef _DEBUG
      DebugUIManager::GetInstance()->AddLog(
        "Loaded preset '" + presetName + "' as '" + newEmitterName + "'", DebugUIManager::LogType::Info);
#endif

    }
  }

  void EmitterManager::LoadPreset(const std::string& presetName)
  {
    using json = nlohmann::json;

    const std::string directory = "resources/Json/ParticlePresets/Presets/";
    std::string filepath = directory + presetName + ".json";

    std::ifstream ifs(filepath);

    if (!ifs.is_open()) {
#ifdef _DEBUG
      DebugUIManager::GetInstance()->AddLog("Failed to load preset: " + presetName, DebugUIManager::LogType::Error);
#endif

      return;
    }

    json preset;
    ifs >> preset;
    ifs.close();

    auto emitter = DeserializeEmitterFromJSON(preset);
    if (emitter) {
      particleSystem_->RegisterEmitter(emitter);
      emitterMap_[presetName] = emitter;
#ifdef _DEBUG
      DebugUIManager::GetInstance()->AddLog(
        "Loaded preset '" + presetName + "' as '" + presetName + "'", DebugUIManager::LogType::Info);
#endif

    }
  }

  void EmitterManager::LoadPreset(const std::string& presetName, const std::string& newEmitterName, Object3d* obj3d)
  {
    using json = nlohmann::json;

    const std::string directory = "resources/Json/ParticlePresets/Presets/";
    std::string filepath = directory + presetName + ".json";

    std::ifstream ifs(filepath);
    if (!ifs.is_open()) {
#ifdef _DEBUG
      DebugUIManager::GetInstance()->AddLog("Failed to load preset: " + presetName, DebugUIManager::LogType::Error);
#endif
      return;
    }

    json preset;
    ifs >> preset;
    ifs.close();

    // 引数の obj3d をスポーン形状ソース兼追従先としてバインドする
    auto emitter = DeserializeEmitterFromJSON(preset, obj3d);
    if (emitter) {
      particleSystem_->RegisterEmitter(emitter);
      emitterMap_[newEmitterName] = emitter;
#ifdef _DEBUG
      DebugUIManager::GetInstance()->AddLog(
        "Loaded preset '" + presetName + "' as '" + newEmitterName + "' with Object3d binding",
        DebugUIManager::LogType::Info);
#endif
    }
  }

  void EmitterManager::LoadPreset(const std::string& presetName, Object3d* obj3d)
  {
    LoadPreset(presetName, presetName, obj3d);
  }

  //========================================
  // エミッター情報取得
  //========================================

  std::vector<std::string> EmitterManager::GetEmitterNames() const
  {
    std::vector<std::string> names;
    for (const auto& [name, _] : emitterMap_) {
      names.push_back(name);
    }
    return names;
  }

  bool EmitterManager::HasEmitter(const std::string& name) const
  {
    return emitterMap_.contains(name);
  }

  //========================================
  // コピー＆ペースト機能
  //========================================

  bool EmitterManager::CopyEmitterSettings(const std::string& emitterName, int slotIndex)
  {
    if (slotIndex < 0 || slotIndex >= 5) return false;

    auto it = emitterMap_.find(emitterName);
    if (it == emitterMap_.end()) return false;

    auto emitter = it->second;
    CopiedSettings& slot = copiedSettingsSlots_[slotIndex];

    // エミッターの設定をコピー
    slot.type = emitter->GetType();
    slot.data = emitter->GetData();
    slot.renderModelPath = emitter->GetRenderModelPath();

    slot.valid = true;
    return true;
  }

  bool EmitterManager::PasteEmitterSettings(const std::string& targetEmitterName, int slotIndex, bool colorOnly, bool velocityOnly, bool scaleOnly)
  {
    if (slotIndex < 0 || slotIndex >= 5) return false;
    if (!copiedSettingsSlots_[slotIndex].valid) return false;

    auto it = emitterMap_.find(targetEmitterName);
    if (it == emitterMap_.end()) return false;

    auto targetEmitter = it->second;
    const CopiedSettings& slot = copiedSettingsSlots_[slotIndex];

    // 部分ペースト
    if (colorOnly) {
      targetEmitter->SetColors(slot.data.startColorTint, slot.data.endColorTint);
    }
    else if (velocityOnly) {
      targetEmitter->SetVelRange(slot.data.velRangeX, slot.data.velRangeY, slot.data.velRangeZ);
    }
    else if (scaleOnly) {
      targetEmitter->SetScaleRange(slot.data.scaleRangeX, slot.data.scaleRangeY);
    }
    else {
      // 全体ペースト（位置と型固有パラメータ以外）
      targetEmitter->SetActive((slot.data.flags & EFLAG_ACTIVE) != 0);
      targetEmitter->SetEmitting((slot.data.flags & EFLAG_EMITTING) != 0);
      targetEmitter->SetNormalize((slot.data.flags & EFLAG_NORMALIZE) != 0);
      targetEmitter->SetParticleCount(slot.data.count);
      targetEmitter->SetFrequency(slot.data.frequency);
      targetEmitter->SetScaleRange(slot.data.scaleRangeX, slot.data.scaleRangeY);
      targetEmitter->SetVelRange(slot.data.velRangeX, slot.data.velRangeY, slot.data.velRangeZ);
      targetEmitter->SetLifeTimeRange(slot.data.lifeTimeRange);
      targetEmitter->SetColors(slot.data.startColorTint, slot.data.endColorTint);
      // 描画設定 (per-emitter)
      targetEmitter->SetBlendMode(static_cast<ParticleBlendMode>(slot.data.blendMode));
      targetEmitter->SetBillboard((slot.data.flags & EFLAG_BILLBOARD) != 0);
      // 描画モデルを復元する 。
      // 空ならデフォルト板ポリにリセットする。
      if (!slot.renderModelPath.empty()) {
        targetEmitter->SetParticleModel(slot.renderModelPath);
      }
      else {
        targetEmitter->ResetParticleModel();
      }
      // テクスチャは srvIndex からファイル名を解決して再設定 (0 は既定テクスチャなので何もしない)
      if (slot.data.textureSrvIndex != 0) {
        const std::string& texName = TextureManager::GetInstance()->GetFileName(slot.data.textureSrvIndex);
        if (!texName.empty()) {
          targetEmitter->SetTexture(texName);
        }
      }
    }

    return true;
  }

  bool EmitterManager::HasCopiedSettings(int slotIndex) const
  {
    if (slotIndex < 0 || slotIndex >= 5) return false;
    return copiedSettingsSlots_[slotIndex].valid;
  }

  void EmitterManager::ClearCopiedSettings(int slotIndex)
  {
    if (slotIndex < 0 || slotIndex >= 5) return;
    copiedSettingsSlots_[slotIndex].valid = false;
  }

  //========================================
  // グループ情報取得
  //========================================

  std::vector<std::string> EmitterManager::GetGroupNames() const
  {
    std::vector<std::string> names;
    for (const auto& [name, _] : groupMap_) {
      names.push_back(name);
    }
    return names;
  }

  std::vector<std::string> EmitterManager::GetEmittersInGroup(const std::string& groupName) const
  {
    auto it = groupMap_.find(groupName);
    if (it == groupMap_.end()) {
      return {};
    }
    return it->second.emitterNames;
  }

  bool EmitterManager::IsGroupActive(const std::string& groupName) const
  {
    auto it = groupMap_.find(groupName);
    if (it == groupMap_.end()) {
      return false;
    }
    return it->second.isActive;
  }

  //========================================
  // JSON 変換ヘルパー
  //========================================

  void EmitterManager::SerializeEmitterToJSON(const std::shared_ptr<GPUParticleEmitter>& emitter, nlohmann::json& json) const
  {
    // 基本パラメータ
    json["type"] = static_cast<uint32_t>(emitter->GetType());
    json["position"] = { emitter->GetPosition().x, emitter->GetPosition().y, emitter->GetPosition().z };
    json["particleCount"] = emitter->GetParticleCount();
    json["frequency"] = emitter->GetFrequency();
    json["frequencyTime"] = emitter->GetFrequencyTime();

    // 範囲パラメータ
    json["scaleRangeX"] = { emitter->GetScaleRangeX().x, emitter->GetScaleRangeX().y };
    json["scaleRangeY"] = { emitter->GetScaleRangeY().x, emitter->GetScaleRangeY().y };
    json["velRangeX"] = { emitter->GetVelRangeX().x, emitter->GetVelRangeX().y };
    json["velRangeY"] = { emitter->GetVelRangeY().x, emitter->GetVelRangeY().y };
    json["velRangeZ"] = { emitter->GetVelRangeZ().x, emitter->GetVelRangeZ().y };
    json["lifeTimeRange"] = { emitter->GetLifeTimeRange().x, emitter->GetLifeTimeRange().y };

    // パラメータごとのランダム化フラグ
    json["randomFlags"] = emitter->GetRandomFlags();

    // アルファフェード (独立フラグ)
    json["useAlphaFade"] = emitter->IsUseAlphaFade();

    // スケール縮小消滅
    json["useScaleFade"] = emitter->IsUseScaleFade();
    json["endScaleDefault"] = { emitter->GetEndScaleDefault().x, emitter->GetEndScaleDefault().y, emitter->GetEndScaleDefault().z };

    // スポーン位置種別
    json["spawnLocation"] = static_cast<uint32_t>(emitter->GetSpawnLocation());

    // Per-Emitter Target 収束
    json["convergeToTarget"] = emitter->IsConvergeToTarget();
    json["targetPosition"] = { emitter->GetTargetPosition().x, emitter->GetTargetPosition().y, emitter->GetTargetPosition().z };
    json["convergeStiffness"] = emitter->GetConvergeStiffness();
    json["convergeDamping"] = emitter->GetConvergeDamping();

    // Per-Particle Spawn 拘束
    json["spawnLock"] = emitter->IsSpawnLock();
    json["lockStiffness"] = emitter->GetLockStiffness();
    json["lockDamping"] = emitter->GetLockDamping();

    // 色パラメータ
    json["startColor"] = { emitter->GetStartColor().x, emitter->GetStartColor().y, emitter->GetStartColor().z, emitter->GetStartColor().w };
    json["endColor"] = { emitter->GetEndColor().x, emitter->GetEndColor().y, emitter->GetEndColor().z, emitter->GetEndColor().w };

    // フラグ
    json["isActive"] = emitter->IsActive();
    json["isEmitting"] = emitter->IsEmitting();
    json["isNormalize"] = emitter->IsNormalize();
    json["isRandomRotateZ"] = emitter->IsRandomRotateZ();
    json["useForceField"] = emitter->IsUseForceField();
    json["useCurlNoise"] = emitter->IsUseCurlNoise();
    json["useDepthCollision"] = emitter->IsUseDepthCollision();
    json["isTemporary"] = emitter->IsTemporary();

    // per-emitter 物理 / Curl Noise パラメーター
    json["damping"] = emitter->GetDamping();
    json["collisionRestitution"] = emitter->GetCollisionRestitution();
    json["particleRadius"] = emitter->GetParticleRadius();
    json["noiseScale"] = emitter->GetNoiseScale();
    json["noiseStrength"] = emitter->GetNoiseStrength();

    // 描画設定 (per-emitter)
    json["blendMode"] = static_cast<uint32_t>(emitter->GetBlendMode());
    json["billboard"] = emitter->IsBillboard();
    // パーティクル描画モデルはファイルパスで保存。空=既定板ポリ。
    if (!emitter->GetRenderModelPath().empty()) {
      json["renderModelPath"] = emitter->GetRenderModelPath();
    }
    // テクスチャは SRV index でなくファイルパスで保存。
    if (emitter->GetTextureSrvIndex() != 0) {
      json["texturePath"] = TextureManager::GetInstance()->GetFileName(emitter->GetTextureSrvIndex());
    }

    // 型固有のパラメータは各エミッタクラスが書き出す
    emitter->SerializeTypeSpecific(json);
  }

  std::shared_ptr<GPUParticleEmitter> EmitterManager::DeserializeEmitterFromJSON(
    const nlohmann::json& json, Object3d* bindTarget)
  {
    EmitterType type = static_cast<EmitterType>(json["type"].get<uint32_t>());


    std::shared_ptr<GPUParticleEmitter> emitter;
    switch (type) {
    case EmitterType::Sphere:   emitter = SphereEmitter::CreateFromJSON(particleSystem_, json); break;
    case EmitterType::Box:      emitter = BoxEmitter::CreateFromJSON(particleSystem_, json); break;
    case EmitterType::Triangle: emitter = TriangleEmitter::CreateFromJSON(particleSystem_, json); break;
    case EmitterType::Mesh:     emitter = MeshEmitter::CreateFromJSON(particleSystem_, json, bindTarget); break;
    default: return nullptr;
    }
    if (!emitter) return nullptr;

    // 共通パラメータを設定
    emitter->SetScaleRange(
      Vector2{ json["scaleRangeX"][0], json["scaleRangeX"][1] },
      Vector2{ json["scaleRangeY"][0], json["scaleRangeY"][1] }
    );
    emitter->SetVelRange(
      Vector2{ json["velRangeX"][0], json["velRangeX"][1] },
      Vector2{ json["velRangeY"][0], json["velRangeY"][1] },
      Vector2{ json["velRangeZ"][0], json["velRangeZ"][1] }
    );
    emitter->SetLifeTimeRange(Vector2{ json["lifeTimeRange"][0], json["lifeTimeRange"][1] });

    Vector4 startColor = { json["startColor"][0], json["startColor"][1], json["startColor"][2], json["startColor"][3] };
    Vector4 endColor = { json["endColor"][0], json["endColor"][1], json["endColor"][2], json["endColor"][3] };
    emitter->SetColors(startColor, endColor);

    emitter->SetActive(json["isActive"]);
    emitter->SetEmitting(json["isEmitting"]);
    emitter->SetNormalize(json["isNormalize"]);
    emitter->SetRandomRotateZ(json["isRandomRotateZ"]);

    if (json.contains("useForceField")) {
      emitter->SetUseForceField(json["useForceField"]);
    }

    if (json.contains("useCurlNoise")) {
      emitter->SetUseCurlNoise(json["useCurlNoise"]);
    }

    if (json.contains("randomFlags")) {
      emitter->SetRandomFlags(json["randomFlags"]);
    }

    // アルファフェード。
    if (json.contains("useAlphaFade")) {
      emitter->SetAlphaFade(json["useAlphaFade"]);
    }

    // スケール縮小消滅。
    if (json.contains("useScaleFade") && json.contains("endScaleDefault")) {
      Vector3 endScale = { json["endScaleDefault"][0], json["endScaleDefault"][1], json["endScaleDefault"][2] };
      emitter->SetScaleFade(json["useScaleFade"], endScale);
    }

    // スポーン位置種別。
    if (json.contains("spawnLocation")) {
      emitter->SetSpawnLocation(static_cast<SpawnLocation>(json["spawnLocation"].get<uint32_t>()));
    }

    // Per-Emitter Target 収束。
    if (json.contains("targetPosition")) {
      Vector3 tp = { json["targetPosition"][0], json["targetPosition"][1], json["targetPosition"][2] };
      emitter->SetTargetPosition(tp);
    }
    if (json.contains("convergeStiffness") && json.contains("convergeDamping")) {
      emitter->SetConvergeParameters(json["convergeStiffness"], json["convergeDamping"]);
    }
    if (json.contains("convergeToTarget")) {
      emitter->SetConvergeToTarget(json["convergeToTarget"]);
    }

    // Per-Particle Spawn 拘束。
    if (json.contains("spawnLock") && json.contains("lockStiffness") && json.contains("lockDamping")) {
      emitter->SetSpawnLock(json["spawnLock"], json["lockStiffness"], json["lockDamping"]);
    }

    if (json.contains("useDepthCollision")) {
      emitter->SetUseDepthCollision(json["useDepthCollision"]);
    }

    if (json.contains("frequencyTime")) {
      emitter->SetFrequencyTime(json["frequencyTime"]);
    }

    // per-emitter 物理 / Curl Noise パラメーター
    if (json.contains("damping")) {
      emitter->SetDamping(json["damping"]);
    }
    if (json.contains("collisionRestitution")) {
      emitter->SetCollisionRestitution(json["collisionRestitution"]);
    }
    if (json.contains("particleRadius")) {
      emitter->SetParticleRadius(json["particleRadius"]);
    }
    if (json.contains("noiseScale")) {
      emitter->SetNoiseScale(json["noiseScale"]);
    }
    if (json.contains("noiseStrength")) {
      emitter->SetNoiseStrength(json["noiseStrength"]);
    }

    // 描画設定 (per-emitter)。
    if (json.contains("blendMode")) {
      emitter->SetBlendMode(static_cast<ParticleBlendMode>(json["blendMode"].get<uint32_t>()));
    }
    if (json.contains("billboard")) {
      emitter->SetBillboard(json["billboard"].get<bool>());
    }
    // 描画モデル: パス指定で復元 (全タイプ)。空=デフォルト板ポリ。
    if (json.contains("renderModelPath")) {
      std::string mp = json["renderModelPath"].get<std::string>();
      if (!mp.empty()) emitter->SetParticleModel(mp);
    }
    if (json.contains("texturePath")) {
      std::string texPath = json["texturePath"].get<std::string>();
      if (!texPath.empty()) {
        emitter->SetTexture(texPath);
      }
    }

    return emitter;
  }

} // namespace Tako

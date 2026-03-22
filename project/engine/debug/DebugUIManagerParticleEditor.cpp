#include "DebugUIManager.h"
#include "EmitterManager.h"
#include "SphereEmitter.h"
#include "BoxEmitter.h"
#include "TriangleEmitter.h"
#include "GPUParticle.h"
#include "ImGuiManager.h"

#include <cstring>

namespace Tako {

  void DebugUIManager::DrawParticleEditor() {
    ImGui::Begin("Particle Editor", &windowVisibility_["ParticleEditor"]);

    if (!emitterManager_) {
      ImGui::TextColored(ImVec4(1, 1, 0, 1), "EmitterManager not set!");
      ImGui::TextDisabled("Call SetEmitterManager() first");
      ImGui::End();
      return;
    }

    // タブバー
    if (ImGui::BeginTabBar("ParticleEditorTabs")) {
      // エミッターリストタブ
      if (ImGui::BeginTabItem("Emitters")) {
        // エミッターリスト描画
        ImGui::Text("Active Emitters: %zu", emitterManager_->GetActiveEmitterCount());
        ImGui::Separator();

        // 新規エミッター作成セクション
        if (ImGui::CollapsingHeader("Create New Emitter")) {
          ImGui::InputText("Name##CreateEmitter", newEmitterNameBuffer_, sizeof(newEmitterNameBuffer_));

          static int emitterType = 0;
          ImGui::Combo("Type##CreateEmitter", &emitterType, "Sphere\0Box\0Triangle\0");

          static Vector3 position = { 0, 0, 0 };
          ImGui::DragFloat3("Position##CreateEmitter", &position.x, 0.1f);

          if (emitterType == 0) {  // Sphere
            static float radius = 1.0f;
            ImGui::DragFloat("Radius##CreateSphere", &radius, 0.1f, 0.1f, 10.0f);

            if (ImGui::Button("Create Sphere Emitter##Create")) {
              if (strlen(newEmitterNameBuffer_) > 0) {
                emitterManager_->CreateSphereEmitter(newEmitterNameBuffer_, position, radius, 50, 0.016f);
                AddLog("Created sphere emitter: " + std::string(newEmitterNameBuffer_), LogType::Info);
                newEmitterNameBuffer_[0] = '\0';  // 入力ボックスをクリア
              }
            }
          }
          else if (emitterType == 1) {  // Box
            static Vector3 size = { 1, 1, 1 };
            static Vector3 rotation = { 0, 0, 0 };
            ImGui::DragFloat3("Size##CreateBox", &size.x, 0.1f);
            ImGui::DragFloat3("Rotation##CreateBox", &rotation.x, 0.1f);

            if (ImGui::Button("Create Box Emitter##Create")) {
              if (strlen(newEmitterNameBuffer_) > 0) {
                emitterManager_->CreateBoxEmitter(newEmitterNameBuffer_, position, size, rotation, 50, 0.016f);
                AddLog("Created box emitter: " + std::string(newEmitterNameBuffer_), LogType::Info);
                newEmitterNameBuffer_[0] = '\0';  // 入力ボックスをクリア
              }
            }
          }
          else {  // Triangle
            static Vector3 v1 = { -1, 0, 0 };
            static Vector3 v2 = { 1, 0, 0 };
            static Vector3 v3 = { 0, 1, 0 };
            ImGui::DragFloat3("Vertex 1##CreateTriangle", &v1.x, 0.1f);
            ImGui::DragFloat3("Vertex 2##CreateTriangle", &v2.x, 0.1f);
            ImGui::DragFloat3("Vertex 3##CreateTriangle", &v3.x, 0.1f);

            if (ImGui::Button("Create Triangle Emitter##Create")) {
              if (strlen(newEmitterNameBuffer_) > 0) {
                emitterManager_->CreateTriangleEmitter(newEmitterNameBuffer_, position, v1, v2, v3, 50, 0.016f);
                AddLog("Created triangle emitter: " + std::string(newEmitterNameBuffer_), LogType::Info);
                newEmitterNameBuffer_[0] = '\0';  // 入力ボックスをクリア
              }
            }
          }
        }

        ImGui::Separator();

        // エミッターリスト
        auto emitterNames = emitterManager_->GetEmitterNames();
        if (ImGui::BeginListBox("##EmitterList", ImVec2(-1, 200))) {
          for (int i = 0; i < emitterNames.size(); i++) {
            bool isSelected = (selectedEmitterIndex_ == i);
            if (ImGui::Selectable(emitterNames[i].c_str(), isSelected)) {
              selectedEmitterIndex_ = i;
            }
          }
          ImGui::EndListBox();
        }

        // 選択したエミッターの操作
        if (selectedEmitterIndex_ >= 0 && selectedEmitterIndex_ < emitterNames.size()) {
          ImGui::Separator();
          ImGui::Text("Selected: %s", emitterNames[selectedEmitterIndex_].c_str());

          if (ImGui::Button("Delete##EmitterList")) {
            emitterManager_->RemoveEmitter(emitterNames[selectedEmitterIndex_]);
            selectedEmitterIndex_ = -1;
            AddLog("Deleted emitter", LogType::Info);
          }
          ImGui::SameLine();
          if (ImGui::Button("Duplicate##EmitterList")) {
            std::string newName = emitterNames[selectedEmitterIndex_] + "_copy";
            emitterManager_->CreateTemporaryEmitterFrom(emitterNames[selectedEmitterIndex_], newName, 0.0f);
            AddLog("Duplicated emitter as: " + newName, LogType::Info);
          }
        }

        ImGui::EndTabItem();
      }

      // プロパティエディタタブ
      if (ImGui::BeginTabItem("Properties")) {
        auto emitterNames = emitterManager_->GetEmitterNames();
        if (selectedEmitterIndex_ < 0 || selectedEmitterIndex_ >= emitterNames.size()) {
          ImGui::TextDisabled("No emitter selected");
        }
        else {
          std::string selectedName = emitterNames[selectedEmitterIndex_];
          auto emitter = emitterManager_->GetEmitterByName(selectedName);

          if (emitter) {
            ImGui::Text("Editing: %s", selectedName.c_str());
            ImGui::Separator();

            // 基本プロパティ
            if (ImGui::CollapsingHeader("Basic Properties", ImGuiTreeNodeFlags_DefaultOpen)) {
              Vector3 pos = emitter->GetPosition();
              if (ImGui::DragFloat3("Position##Properties", &pos.x, 0.1f)) {
                emitter->SetPosition(pos);
              }

              bool isActive = emitter->IsActive();
              if (ImGui::Checkbox("Active", &isActive)) {
                emitter->SetActive(isActive);
              }

              bool isEmitting = emitter->IsEmitting();
              if (ImGui::Checkbox("Emitting", &isEmitting)) {
                emitter->SetEmitting(isEmitting);
              }

              bool isNormalize = emitter->IsNormalize();
              if (ImGui::Checkbox("Normalize", &isNormalize)) {
                emitter->SetNormalize(isNormalize);
              }

              bool isRandomRotateZ = emitter->IsRandomRotateZ();
              if (ImGui::Checkbox("RandomRotateZ", &isRandomRotateZ)) {
                emitter->SetRandomRotateZ(isRandomRotateZ);
              }

              bool useForceField = emitter->IsUseForceField();
              if (ImGui::Checkbox("Use ForceField", &useForceField)) {
                emitter->SetUseForceField(useForceField);
              }

              int count = emitter->GetParticleCount();
              if (ImGui::DragInt("Particle Count", &count, 1, 1, 1000)) {
                emitter->SetParticleCount(count);
              }

              float frequency = emitter->GetFrequency();
              if (ImGui::DragFloat("Frequency", &frequency, 0.001f, 0.001f, 1.0f)) {
                emitter->SetFrequency(frequency);
              }
            }

            // 範囲設定
            if (ImGui::CollapsingHeader("Range Settings")) {
              Vector2 scaleX = emitter->GetScaleRangeX();
              Vector2 scaleY = emitter->GetScaleRangeY();
              if (ImGui::DragFloat2("Scale Range X", &scaleX.x, 0.01f)) {
                emitter->SetScaleRangeX(scaleX);
              }
              if (ImGui::DragFloat2("Scale Range Y", &scaleY.x, 0.01f)) {
                emitter->SetScaleRangeY(scaleY);
              }

              Vector2 velX = emitter->GetVelRangeX();
              Vector2 velY = emitter->GetVelRangeY();
              Vector2 velZ = emitter->GetVelRangeZ();
              if (ImGui::DragFloat2("Velocity Range X", &velX.x, 0.1f)) {
                emitter->SetVelRangeX(velX);
              }
              if (ImGui::DragFloat2("Velocity Range Y", &velY.x, 0.1f)) {
                emitter->SetVelRangeY(velY);
              }
              if (ImGui::DragFloat2("Velocity Range Z", &velZ.x, 0.1f)) {
                emitter->SetVelRangeZ(velZ);
              }

              Vector2 lifeTime = emitter->GetLifeTimeRange();
              if (ImGui::DragFloat2("LifeTime Range", &lifeTime.x, 0.01f, 0.01f, 10.0f)) {
                emitter->SetLifeTimeRange(lifeTime);
              }
            }

            // 色設定
            if (ImGui::CollapsingHeader("Color Settings")) {
              Vector4 startColor = emitter->GetStartColor();
              Vector4 endColor = emitter->GetEndColor();

              if (ImGui::ColorEdit4("Start Color", &startColor.x)) {
                emitter->SetStartColor(startColor);
              }
              if (ImGui::ColorEdit4("End Color", &endColor.x)) {
                emitter->SetEndColor(endColor);
              }
            }

            // 型固有のパラメータ
            if (ImGui::CollapsingHeader("Type-Specific Settings")) {
              if (auto sphereEmitter = std::dynamic_pointer_cast<SphereEmitter>(emitter)) {
                float radius = sphereEmitter->GetRadius();
                if (ImGui::DragFloat("Radius##TypeSpecific", &radius, 0.1f, 0.1f, 100.0f)) {
                  sphereEmitter->SetRadius(radius);
                }
              }
              else if (auto boxEmitter = std::dynamic_pointer_cast<BoxEmitter>(emitter)) {
                Vector3 size = boxEmitter->GetSize();
                Vector3 rotation = boxEmitter->GetRotation();

                if (ImGui::DragFloat3("Size##TypeSpecific", &size.x, 0.1f)) {
                  boxEmitter->SetSize(size);
                }
                if (ImGui::DragFloat3("Rotation##TypeSpecific", &rotation.x, 0.1f)) {
                  boxEmitter->SetRotation(rotation);
                }
              }
              else if (auto triangleEmitter = std::dynamic_pointer_cast<TriangleEmitter>(emitter)) {
                Vector3 v1 = triangleEmitter->GetVertex1();
                Vector3 v2 = triangleEmitter->GetVertex2();
                Vector3 v3 = triangleEmitter->GetVertex3();

                bool changed = false;
                changed |= ImGui::DragFloat3("Vertex 1##TypeSpecific", &v1.x, 0.1f);
                changed |= ImGui::DragFloat3("Vertex 2##TypeSpecific", &v2.x, 0.1f);
                changed |= ImGui::DragFloat3("Vertex 3##TypeSpecific", &v3.x, 0.1f);

                if (changed) {
                  triangleEmitter->SetVertices(v1, v2, v3);
                }
              }
            }
          }
          else {
            ImGui::TextDisabled("Emitter not found");
          }
        }

        ImGui::EndTabItem();
      }

      // プリセット管理タブ
      if (ImGui::BeginTabItem("Presets")) {
        ImGui::Text("Preset Management");
        ImGui::Separator();

        // プリセット保存
        if (ImGui::CollapsingHeader("Save Preset")) {
          ImGui::InputText("Preset Name##SavePreset", presetNameBuffer_, sizeof(presetNameBuffer_));

          auto emitterNames = emitterManager_->GetEmitterNames();
          if (selectedEmitterIndex_ >= 0 && selectedEmitterIndex_ < emitterNames.size()) {
            ImGui::Text("From: %s", emitterNames[selectedEmitterIndex_].c_str());

            if (ImGui::Button("Save as Preset##SavePreset") && strlen(presetNameBuffer_) > 0) {
              emitterManager_->SavePreset(presetNameBuffer_, emitterNames[selectedEmitterIndex_]);
              AddLog("Saved preset: " + std::string(presetNameBuffer_), LogType::Info);
              presetNameBuffer_[0] = '\0';  // 入力ボックスをクリア
            }
          }
          else {
            ImGui::TextDisabled("Select an emitter first");
          }
        }

        // プリセット読み込み
        if (ImGui::CollapsingHeader("Load Preset")) {
          ImGui::InputText("Preset Name##LoadPreset", loadPresetBuffer_, sizeof(loadPresetBuffer_));
          ImGui::InputText("New Name##LoadPreset", newEmitterNameBuffer_, sizeof(newEmitterNameBuffer_));

          if (ImGui::Button("Load Preset##LoadPreset") && strlen(loadPresetBuffer_) > 0 && strlen(newEmitterNameBuffer_) > 0) {
            emitterManager_->LoadPreset(loadPresetBuffer_, newEmitterNameBuffer_);
            AddLog("Loaded preset: " + std::string(loadPresetBuffer_), LogType::Info);
            loadPresetBuffer_[0] = '\0';  // 入力ボックスをクリア
            newEmitterNameBuffer_[0] = '\0';  // 入力ボックスをクリア
          }
        }

        // 全体保存/読み込み
        if (ImGui::CollapsingHeader("Scene Presets")) {
          static char scenePresetName[128] = "scene_preset";
          ImGui::InputText("Scene Name##ScenePreset", scenePresetName, sizeof(scenePresetName));

          if (ImGui::Button("Save All Emitters##ScenePreset")) {
            emitterManager_->SaveScenePreset(scenePresetName);
            AddLog("Saved all emitters to: " + std::string(scenePresetName), LogType::Info);
          }
          ImGui::SameLine();
          if (ImGui::Button("Load All Emitters##ScenePreset")) {
            emitterManager_->LoadScenePreset(scenePresetName);
            AddLog("Loaded all emitters from: " + std::string(scenePresetName), LogType::Info);
          }
        }

        ImGui::EndTabItem();
      }

      // フォースフィールド管理タブ
      if (ImGui::BeginTabItem("ForceFields")) {
        DrawForceFieldsTab();
        ImGui::EndTabItem();
      }

      // グループ管理タブ
      if (ImGui::BeginTabItem("Groups")) {
        DrawGroupsTab();
        ImGui::EndTabItem();
      }

      ImGui::EndTabBar();
    }

    ImGui::End();
  }

  // グループ管理タブの実装
  void DebugUIManager::DrawGroupsTab() {
    ImGui::Text("Group Management");
    ImGui::Separator();

    // 新規グループ作成
    if (ImGui::CollapsingHeader("Create Group")) {
      ImGui::InputText("Group Name##NewGroup", newGroupNameBuffer_, sizeof(newGroupNameBuffer_));
      if (ImGui::Button("Create##NewGroup") && strlen(newGroupNameBuffer_) > 0) {
        emitterManager_->CreateGroup(newGroupNameBuffer_);
        AddLog("Created group: " + std::string(newGroupNameBuffer_), LogType::Info);
        newGroupNameBuffer_[0] = '\0';  // 入力ボックスをクリア
      }
    }

    // グループリスト
    auto groupNames = emitterManager_->GetGroupNames();
    ImGui::Text("Groups: %zu", groupNames.size());

    if (ImGui::BeginListBox("##GroupList", ImVec2(-1, 150))) {
      for (int i = 0; i < groupNames.size(); i++) {
        bool isSelected = (selectedGroupIndex_ == i);
        if (ImGui::Selectable(groupNames[i].c_str(), isSelected)) {
          selectedGroupIndex_ = i;
        }
      }
      ImGui::EndListBox();
    }

    // 選択したグループの操作
    if (selectedGroupIndex_ >= 0 && selectedGroupIndex_ < groupNames.size()) {
      std::string groupName = groupNames[selectedGroupIndex_];
      ImGui::Separator();
      ImGui::Text("Selected Group: %s", groupName.c_str());

      // グループアクティブ切り替え
      bool isActive = emitterManager_->IsGroupActive(groupName);
      if (ImGui::Checkbox("Group Active##Group", &isActive)) {
        emitterManager_->SetGroupActive(groupName, isActive);
      }

      // グループ位置調整
      static Vector3 groupOffset = { 0, 0, 0 };
      if (ImGui::DragFloat3("Group Position##Group", &groupOffset.x, 0.1f)) {
        emitterManager_->SetGroupPosition(groupName, groupOffset);
      }

      // グループ内のエミッター表示
      auto emittersInGroup = emitterManager_->GetEmittersInGroup(groupName);
      ImGui::Text("Emitters in group: %zu", emittersInGroup.size());
      if (ImGui::BeginListBox("##GroupEmitters", ImVec2(-1, 100))) {
        for (const auto& name : emittersInGroup) {
          ImGui::Text("%s", name.c_str());
        }
        ImGui::EndListBox();
      }

      // エミッターをグループに追加
      auto allEmitters = emitterManager_->GetEmitterNames();
      static int addEmitterIndex = 0;
      if (allEmitters.size() > 0) {
        std::vector<const char*> items;
        for (const auto& name : allEmitters) {
          items.push_back(name.c_str());
        }
        ImGui::Combo("Add Emitter##Group", &addEmitterIndex, items.data(), static_cast<int>(items.size()));
        if (ImGui::Button("Add to Group##Group")) {
          emitterManager_->AddToGroup(groupName, allEmitters[addEmitterIndex]);
          AddLog("Added " + allEmitters[addEmitterIndex] + " to group " + groupName, LogType::Info);
        }
      }

      // グループ削除
      if (ImGui::Button("Delete Group##Group")) {
        emitterManager_->RemoveGroup(groupName);
        selectedGroupIndex_ = -1;
        AddLog("Deleted group: " + groupName, LogType::Info);
      }
    }
  }

  void DebugUIManager::DrawForceFieldsTab() {
    auto* gpuParticle = GPUParticle::GetInstance();

    // --- 物理パラメータセクション ---
    if (ImGui::CollapsingHeader("Physics Parameters", ImGuiTreeNodeFlags_DefaultOpen)) {
      float damping = gpuParticle->GetDamping();
      if (ImGui::SliderFloat("Damping", &damping, 0.9f, 1.0f, "%.4f")) {
        gpuParticle->SetDamping(damping);
      }
      if (ImGui::IsItemHovered()) ImGui::SetTooltip("Speed attenuation per frame (0.99 recommended)");

      float restitution = gpuParticle->GetCollisionRestitution();
      if (ImGui::SliderFloat("Restitution", &restitution, 0.0f, 1.0f, "%.2f")) {
        gpuParticle->SetCollisionRestitution(restitution);
      }

      float particleRadius = gpuParticle->GetParticleRadius();
      if (ImGui::DragFloat("Particle Radius", &particleRadius, 0.001f, 0.001f, 1.0f, "%.3f")) {
        gpuParticle->SetParticleRadius(particleRadius);
      }
    }

    ImGui::Separator();

    // フォースフィールドタイプ名の定義
    static const char* forceTypeNames[] = {
      "Gravity", "Directional", "Vortex", "Attract", "Repel"
    };

    // --- 新規フォースフィールド追加 ---
    if (ImGui::CollapsingHeader("Add Force Field")) {
      static int newForceType = 0;
      ImGui::Combo("Type##NewFF", &newForceType, "Gravity\0Directional\0Vortex\0Attract\0Repel\0");

      static Vector3 newPosition = { 0.0f, 0.0f, 0.0f };
      static Vector3 newDirection = { 0.0f, -1.0f, 0.0f };
      static float newStrength = 1.0f;
      static float newRadius = 0.0f;
      static float newFalloff = 1.0f;

      ImGui::DragFloat3("Position##NewFF", &newPosition.x, 0.1f);

      // タイプに応じた方向ガイド
      if (newForceType == 0 || newForceType == 1) {
        ImGui::DragFloat3("Direction##NewFF", &newDirection.x, 0.1f);
      }
      else if (newForceType == 2) {
        ImGui::DragFloat3("Rotation Axis##NewFF", &newDirection.x, 0.1f);
      }

      ImGui::DragFloat("Strength##NewFF", &newStrength, 0.1f, 0.0f, 100.0f);
      ImGui::DragFloat("Radius##NewFF", &newRadius, 0.1f, 0.0f, 100.0f);
      if (ImGui::IsItemHovered()) ImGui::SetTooltip("0 = infinite range");
      ImGui::DragFloat("Falloff##NewFF", &newFalloff, 0.1f, 0.0f, 10.0f);

      if (ImGui::Button("Add##NewFF")) {
        ForceFieldData field{};
        field.type = static_cast<uint32_t>(newForceType);
        field.position = newPosition;
        field.direction = newDirection;
        field.strength = newStrength;
        field.radius = newRadius;
        field.falloff = newFalloff;
        field.pad[0] = 0.0f;
        field.pad[1] = 0.0f;

        int32_t idx = gpuParticle->AddForceField(field);
        if (idx >= 0) {
          selectedForceFieldIndex_ = idx;
          AddLog("Added force field: " + std::string(forceTypeNames[newForceType]), LogType::Info);
        }
        else {
          AddLog("Failed to add force field: max reached", LogType::Warning);
        }
      }

      // クイック追加ボタン
      ImGui::Separator();
      ImGui::Text("Quick Add:");
      if (ImGui::Button("Gravity (Y-9.8)##Quick")) {
        ForceFieldData field{};
        field.type = static_cast<uint32_t>(ForceFieldType::Gravity);
        field.direction = { .x = 0.0f, .y = -9.8f, .z = 0.0f };
        field.strength = 1.0f;
        gpuParticle->AddForceField(field);
        AddLog("Added gravity force field", LogType::Info);
      }
      ImGui::SameLine();
      if (ImGui::Button("Vortex (Y-axis)##Quick")) {
        ForceFieldData field{};
        field.type = static_cast<uint32_t>(ForceFieldType::Vortex);
        field.direction = { .x = 0.0f, .y = 1.0f, .z = 0.0f };
        field.strength = 5.0f;
        field.radius = 10.0f;
        field.falloff = 1.0f;
        gpuParticle->AddForceField(field);
        AddLog("Added vortex force field", LogType::Info);
      }
    }

    // --- フォースフィールド一覧 ---
    const auto& forceFields = gpuParticle->GetForceFields();
    ImGui::Text("Force Fields: %zu / %u", forceFields.size(), GPUParticle::kMaxForceFields);

    // リスト表示
    if (ImGui::BeginListBox("##ForceFieldList", ImVec2(-1, 120))) {
      for (int i = 0; i < static_cast<int>(forceFields.size()); i++) {
        uint32_t typeIdx = forceFields[i].type;
        const char* typeName = (typeIdx < 5) ? forceTypeNames[typeIdx] : "Unknown";

        char label[64];
        snprintf(label, sizeof(label), "[%d] %s (str: %.2f)", i, typeName, forceFields[i].strength);

        bool isSelected = (selectedForceFieldIndex_ == i);
        if (ImGui::Selectable(label, isSelected)) {
          selectedForceFieldIndex_ = i;
        }
      }
      ImGui::EndListBox();
    }

    // --- 選択中のフォースフィールド編集 ---
    if (selectedForceFieldIndex_ >= 0 && selectedForceFieldIndex_ < static_cast<int>(forceFields.size())) {
      ImGui::Separator();

      uint32_t typeIdx = forceFields[selectedForceFieldIndex_].type;
      const char* typeName = (typeIdx < 5) ? forceTypeNames[typeIdx] : "Unknown";
      ImGui::Text("Editing: [%d] %s", selectedForceFieldIndex_, typeName);

      // 編集可能なコピーを作成
      ForceFieldData editField = forceFields[selectedForceFieldIndex_];
      bool changed = false;

      // タイプ変更
      int editType = static_cast<int>(editField.type);
      if (ImGui::Combo("Type##EditFF", &editType, "Gravity\0Directional\0Vortex\0Attract\0Repel\0")) {
        editField.type = static_cast<uint32_t>(editType);
        changed = true;
      }

      changed |= ImGui::DragFloat3("Position##EditFF", &editField.position.x, 0.1f);

      // タイプに応じたラベル
      if (editField.type == static_cast<uint32_t>(ForceFieldType::Vortex)) {
        changed |= ImGui::DragFloat3("Rotation Axis##EditFF", &editField.direction.x, 0.1f);
      }
      else if (editField.type == static_cast<uint32_t>(ForceFieldType::Gravity) ||
               editField.type == static_cast<uint32_t>(ForceFieldType::Directional)) {
        changed |= ImGui::DragFloat3("Direction##EditFF", &editField.direction.x, 0.1f);
      }

      changed |= ImGui::DragFloat("Strength##EditFF", &editField.strength, 0.1f, 0.0f, 100.0f);
      changed |= ImGui::DragFloat("Radius##EditFF", &editField.radius, 0.1f, 0.0f, 100.0f);
      if (ImGui::IsItemHovered()) ImGui::SetTooltip("0 = infinite range");
      changed |= ImGui::DragFloat("Falloff##EditFF", &editField.falloff, 0.1f, 0.0f, 10.0f);

      if (changed) {
        gpuParticle->UpdateForceField(static_cast<uint32_t>(selectedForceFieldIndex_), editField);
      }

      // 削除ボタン
      ImGui::Separator();
      if (ImGui::Button("Delete##EditFF")) {
        gpuParticle->RemoveForceField(static_cast<uint32_t>(selectedForceFieldIndex_));
        AddLog("Deleted force field [" + std::to_string(selectedForceFieldIndex_) + "]", LogType::Info);
        selectedForceFieldIndex_ = -1;
      }
      ImGui::SameLine();
      if (ImGui::Button("Clear All##EditFF")) {
        gpuParticle->ClearForceFields();
        selectedForceFieldIndex_ = -1;
        AddLog("Cleared all force fields", LogType::Info);
      }
    }
  }

} // namespace Tako
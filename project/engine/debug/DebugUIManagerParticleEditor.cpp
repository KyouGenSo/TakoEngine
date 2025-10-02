#include "DebugUIManager.h"
#include "EmitterManager.h"
#include "SphereEmitter.h"
#include "BoxEmitter.h"
#include "TriangleEmitter.h"
#include "GlobalVariables.h"
#include <cstring>

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
            }
          }
        } else if (emitterType == 1) {  // Box
          static Vector3 size = { 1, 1, 1 };
          static Vector3 rotation = { 0, 0, 0 };
          ImGui::DragFloat3("Size##CreateBox", &size.x, 0.1f);
          ImGui::DragFloat3("Rotation##CreateBox", &rotation.x, 0.1f);

          if (ImGui::Button("Create Box Emitter##Create")) {
            if (strlen(newEmitterNameBuffer_) > 0) {
              emitterManager_->CreateBoxEmitter(newEmitterNameBuffer_, position, size, rotation, 50, 0.016f);
              AddLog("Created box emitter: " + std::string(newEmitterNameBuffer_), LogType::Info);
            }
          }
        } else {  // Triangle
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
      } else {
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
            } else if (auto boxEmitter = std::dynamic_pointer_cast<BoxEmitter>(emitter)) {
              Vector3 size = boxEmitter->GetSize();
              Vector3 rotation = boxEmitter->GetRotation();

              if (ImGui::DragFloat3("Size##TypeSpecific", &size.x, 0.1f)) {
                boxEmitter->SetSize(size);
              }
              if (ImGui::DragFloat3("Rotation##TypeSpecific", &rotation.x, 0.1f)) {
                boxEmitter->SetRotation(rotation);
              }
            } else if (auto triangleEmitter = std::dynamic_pointer_cast<TriangleEmitter>(emitter)) {
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
        } else {
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
          }
        } else {
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
        }
      }

      // 全体保存/読み込み
      if (ImGui::CollapsingHeader("Scene Presets")) {
        static char scenePresetName[128] = "scene_preset";
        ImGui::InputText("Scene Name##ScenePreset", scenePresetName, sizeof(scenePresetName));

        if (ImGui::Button("Save All Emitters##ScenePreset")) {
          emitterManager_->SaveEmittersToJSON(scenePresetName);
          AddLog("Saved all emitters to: " + std::string(scenePresetName), LogType::Info);
        }
        ImGui::SameLine();
        if (ImGui::Button("Load All Emitters##ScenePreset")) {
          emitterManager_->LoadEmittersFromJSON(scenePresetName);
          AddLog("Loaded all emitters from: " + std::string(scenePresetName), LogType::Info);
        }
      }

      ImGui::EndTabItem();
    }

    ImGui::EndTabBar();
  }

  ImGui::End();
}
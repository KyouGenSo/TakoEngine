#include "DebugUIManager.h"
#include "EmitterManager.h"
#include "ForceFieldManager.h"
#include "SphereEmitter.h"
#include "BoxEmitter.h"
#include "TriangleEmitter.h"
#include "MeshEmitter.h"
#include "Mesh.h"
#include "GPUParticle.h"
#include "TextureManager.h"
#include "ImGuiManager.h"
#include "Draw2D.h"
#include "OBB.h"

#include <cstring>
#include <numbers>
#include <cmath>

namespace Tako {

  void DebugUIManager::DrawParticleEditor() {
    ImGui::Begin("Particle Editor", &windowVisibility_["ParticleEditor"]);

    if (!emitterManager_) {
      ImGui::TextColored(ImVec4(1, 1, 0, 1), "EmitterManager not set!");
      ImGui::TextDisabled("Call SetEmitterManager() first");
      ImGui::End();
      return;
    }

    // アクティブパーティクル数の表示
    {
      uint32_t activeCount = GPUParticle::GetInstance()->GetActiveParticleCount();
      uint32_t maxCount = GPUParticle::GetMaxParticleCount();
      float usage = static_cast<float>(activeCount) / static_cast<float>(maxCount);

      ImVec4 color;
      if (usage < 0.5f) {
        color = ImVec4(0.2f, 1.0f, 0.2f, 1.0f);
      } else if (usage < 0.8f) {
        color = ImVec4(1.0f, 1.0f, 0.2f, 1.0f);
      } else {
        color = ImVec4(1.0f, 0.2f, 0.2f, 1.0f);
      }

      ImGui::TextColored(color, "Active Particles: %u / %u (%.1f%%)",
        activeCount, maxCount, usage * 100.0f);
      ImGui::ProgressBar(usage, ImVec2(-1, 0), "");
    }
    ImGui::Separator();

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

              bool useCurlNoise = emitter->IsUseCurlNoise();
              if (ImGui::Checkbox("Use Curl Noise", &useCurlNoise)) {
                emitter->SetUseCurlNoise(useCurlNoise);
              }

              bool useDepthCollision = emitter->IsUseDepthCollision();
              if (ImGui::Checkbox("Use Depth Collision", &useDepthCollision)) {
                emitter->SetUseDepthCollision(useDepthCollision);
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
              // パラメータごとのランダム化フラグ
              // randomFlags == 0 のときは旧来の「range != (0,0) で自動判定」が効くので、
              // チェックボックスはあくまで「明示的に Override したい」場合のための UI。
              uint32_t randomFlags = emitter->GetRandomFlags();
              const bool legacyAuto = (randomFlags == 0u);
              ImGui::TextDisabled(legacyAuto
                ? "Randomize: [Auto] (range != 0 enables randomization)"
                : "Randomize: [Manual] (per-parameter checkboxes)");
              if (legacyAuto) {
                if (ImGui::SmallButton("Switch to Manual")) {
                  // 現状の値を元に Auto 判定を Manual ビットに固定化
                  uint32_t newFlags = 0u;
                  if (emitter->GetScaleRangeX().x != 0.0f || emitter->GetScaleRangeX().y != 0.0f) newFlags |= ERAND_SCALE_X;
                  if (emitter->GetScaleRangeY().x != 0.0f || emitter->GetScaleRangeY().y != 0.0f) newFlags |= ERAND_SCALE_Y;
                  if (emitter->GetVelRangeX().x != 0.0f || emitter->GetVelRangeX().y != 0.0f) newFlags |= ERAND_VEL_X;
                  if (emitter->GetVelRangeY().x != 0.0f || emitter->GetVelRangeY().y != 0.0f) newFlags |= ERAND_VEL_Y;
                  if (emitter->GetVelRangeZ().x != 0.0f || emitter->GetVelRangeZ().y != 0.0f) newFlags |= ERAND_VEL_Z;
                  if (emitter->GetLifeTimeRange().x != 0.0f || emitter->GetLifeTimeRange().y != 0.0f) newFlags |= ERAND_LIFETIME;
                  // 全 0 だと Auto に戻ってしまうため、最低 1 ビットだけ立てて Manual 確定
                  if (newFlags == 0u) newFlags = ERAND_SCALE_X;
                  emitter->SetRandomFlags(newFlags);
                }
              }
              else {
                if (ImGui::SmallButton("Reset to Auto")) {
                  emitter->SetRandomFlags(0u);
                }
              }
              ImGui::Separator();

              auto drawRandomCheckbox = [&](const char* label, uint32_t flag) {
                bool enabled = (randomFlags & flag) != 0u;
                if (ImGui::Checkbox(label, &enabled)) {
                  if (enabled) emitter->EnableRandom(flag);
                  else        emitter->DisableRandom(flag);
                }
              };

              Vector2 scaleX = emitter->GetScaleRangeX();
              Vector2 scaleY = emitter->GetScaleRangeY();
              drawRandomCheckbox("##RandScaleX", ERAND_SCALE_X);
              ImGui::SameLine();
              if (ImGui::DragFloat2("Scale Range X", &scaleX.x, 0.01f)) {
                emitter->SetScaleRangeX(scaleX);
              }
              drawRandomCheckbox("##RandScaleY", ERAND_SCALE_Y);
              ImGui::SameLine();
              if (ImGui::DragFloat2("Scale Range Y", &scaleY.x, 0.01f)) {
                emitter->SetScaleRangeY(scaleY);
              }

              Vector2 velX = emitter->GetVelRangeX();
              Vector2 velY = emitter->GetVelRangeY();
              Vector2 velZ = emitter->GetVelRangeZ();
              drawRandomCheckbox("##RandVelX", ERAND_VEL_X);
              ImGui::SameLine();
              if (ImGui::DragFloat2("Velocity Range X", &velX.x, 0.1f)) {
                emitter->SetVelRangeX(velX);
              }
              drawRandomCheckbox("##RandVelY", ERAND_VEL_Y);
              ImGui::SameLine();
              if (ImGui::DragFloat2("Velocity Range Y", &velY.x, 0.1f)) {
                emitter->SetVelRangeY(velY);
              }
              drawRandomCheckbox("##RandVelZ", ERAND_VEL_Z);
              ImGui::SameLine();
              if (ImGui::DragFloat2("Velocity Range Z", &velZ.x, 0.1f)) {
                emitter->SetVelRangeZ(velZ);
              }

              Vector2 lifeTime = emitter->GetLifeTimeRange();
              drawRandomCheckbox("##RandLifeTime", ERAND_LIFETIME);
              ImGui::SameLine();
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

            // 描画設定 (per-emitter): ブレンドモード / ビルボード / テクスチャ / メッシュ形状描画
            if (ImGui::CollapsingHeader("Render Settings")) {
              // ブレンドモード
              static const char* kBlendLabels[] = { "Add", "Screen", "Alpha" };
              int blend = static_cast<int>(emitter->GetBlendMode());
              if (ImGui::Combo("Blend Mode##Render", &blend, kBlendLabels, IM_ARRAYSIZE(kBlendLabels))) {
                emitter->SetBlendMode(static_cast<ParticleBlendMode>(blend));
              }

              // ビルボード ON/OFF
              bool billboard = emitter->IsBillboard();
              if (ImGui::Checkbox("Billboard (camera-facing)", &billboard)) {
                emitter->SetBillboard(billboard);
              }
              ImGui::SameLine();
              ImGui::TextDisabled("(OFF: world-fixed, rotate.z applied)");

              // テクスチャ選択
              uint32_t curTex = emitter->GetTextureSrvIndex();
              std::string curName = (curTex != 0)
                ? TextureManager::GetInstance()->GetFileName(curTex)
                : std::string("(default: circle.dds)");
              ImGui::Text("Texture: %s", curName.c_str());

              // ロード済みテクスチャからの選択
              std::vector<std::string> texNames = TextureManager::GetInstance()->GetLoadedTextureFileNames();
              if (!texNames.empty()) {
                int curIdx = -1;
                for (int n = 0; n < static_cast<int>(texNames.size()); ++n) {
                  if (texNames[n] == curName) { curIdx = n; break; }
                }
                std::vector<const char*> items;
                items.reserve(texNames.size());
                for (const auto& s : texNames) items.push_back(s.c_str());
                if (ImGui::Combo("Texture##Render", &curIdx, items.data(), static_cast<int>(items.size()))) {
                  if (curIdx >= 0 && curIdx < static_cast<int>(texNames.size())) {
                    emitter->SetTexture(texNames[curIdx]);
                  }
                }
              }

              // 新規テクスチャの読込 (パス指定)
              static char texPath[256] = "";
              ImGui::InputText("Texture Path##Render", texPath, sizeof(texPath));
              ImGui::SameLine();
              if (ImGui::Button("Load & Set##Render") && texPath[0] != '\0') {
                emitter->SetTexture(texPath);
              }

              // メッシュ形状描画 (Mesh エミッターのみ)
              if (emitter->GetType() == EmitterType::Mesh) {
                bool renderAsMesh = emitter->IsRenderAsMesh();
                if (ImGui::Checkbox("Render As Mesh", &renderAsMesh)) {
                  emitter->SetRenderAsMesh(renderAsMesh);
                }
                ImGui::SameLine();
                ImGui::TextDisabled("(ON: draw each particle as this mesh)");
              }
            }

            // スポーン位置種別 (中/外/線)
            if (ImGui::CollapsingHeader("Spawn Location")) {
              static const char* kSpawnLocationLabels[] = { "Inside", "Surface", "Edge" };
              int currentLoc = static_cast<int>(emitter->GetSpawnLocation());
              if (ImGui::Combo("Location##SpawnLocation", &currentLoc, kSpawnLocationLabels, IM_ARRAYSIZE(kSpawnLocationLabels))) {
                emitter->SetSpawnLocation(static_cast<SpawnLocation>(currentLoc));
              }
              // 形状ごとの対応状況を警告表示
              EmitterType etype = emitter->GetType();
              if (etype == EmitterType::Sphere && currentLoc == static_cast<int>(SpawnLocation::Edge)) {
                ImGui::TextColored(ImVec4(1, 0.7f, 0, 1), "Sphere has no vertices: Edge falls back to Surface");
              }
              if (etype == EmitterType::Triangle && currentLoc == static_cast<int>(SpawnLocation::Inside)) {
                ImGui::TextColored(ImVec4(1, 0.7f, 0, 1), "Triangle is 2D: Inside falls back to Surface");
              }
            }

            // Per-Particle Spawn 拘束
            if (ImGui::CollapsingHeader("Spawn Lock (per-particle)")) {
              bool spawnLockOn = emitter->IsSpawnLock();
              float lockK = emitter->GetLockStiffness();
              float lockD = emitter->GetLockDamping();
              if (ImGui::Checkbox("Lock To Spawn", &spawnLockOn)) {
                emitter->SetSpawnLock(spawnLockOn, lockK, lockD);
              }
              ImGui::TextDisabled("Each particle is pulled back to its spawn position (per-particle spring).");
              ImGui::TextDisabled("Combined with Mesh emitter: particles stick to mesh surface and follow rotation/movement.");
              if (ImGui::DragFloat("Lock Stiffness (k)", &lockK, 0.1f, 0.0f, 200.0f)) {
                emitter->SetSpawnLock(spawnLockOn, lockK, lockD);
              }
              if (ImGui::DragFloat("Lock Damping (d)", &lockD, 0.05f, 0.0f, 50.0f)) {
                emitter->SetSpawnLock(spawnLockOn, lockK, lockD);
              }
            }

            // Per-Emitter Target 収束
            if (ImGui::CollapsingHeader("Target Convergence")) {
              bool convergeOn = emitter->IsConvergeToTarget();
              if (ImGui::Checkbox("Converge To Target", &convergeOn)) {
                emitter->SetConvergeToTarget(convergeOn);
              }
              ImGui::TextDisabled("Spring-damper force pulls all particles to targetPosition");
              Vector3 targetPos = emitter->GetTargetPosition();
              if (ImGui::DragFloat3("Target Position", &targetPos.x, 0.1f)) {
                emitter->SetTargetPosition(targetPos);
              }
              float stiffness = emitter->GetConvergeStiffness();
              if (ImGui::DragFloat("Stiffness (k)", &stiffness, 0.1f, 0.0f, 100.0f)) {
                emitter->SetConvergeParameters(stiffness, emitter->GetConvergeDamping());
              }
              float damping = emitter->GetConvergeDamping();
              if (ImGui::DragFloat("Damping (d)", &damping, 0.05f, 0.0f, 20.0f)) {
                emitter->SetConvergeParameters(emitter->GetConvergeStiffness(), damping);
              }
              ImGui::TextDisabled("Use BindTargetPosition(const Vector3*) in code for dynamic tracking.");
            }

            // 消滅設定: alpha フェードとスケール縮小は独立フラグ
            if (ImGui::CollapsingHeader("Death Style")) {
              // アルファフェード
              bool useAlphaFade = emitter->IsUseAlphaFade();
              if (ImGui::Checkbox("Enable Alpha Fade", &useAlphaFade)) {
                emitter->SetAlphaFade(useAlphaFade);
              }
              ImGui::SameLine();
              ImGui::TextDisabled("(alpha 1.0 -> 0.0 over lifetime)");

              // スケール縮小
              bool useScaleFade = emitter->IsUseScaleFade();
              if (ImGui::Checkbox("Enable Scale Fade", &useScaleFade)) {
                emitter->SetScaleFade(useScaleFade, emitter->GetEndScaleDefault());
              }
              ImGui::SameLine();
              ImGui::TextDisabled("(scale -> endScale over lifetime)");
              Vector3 endScale = emitter->GetEndScaleDefault();
              if (ImGui::DragFloat3("End Scale", &endScale.x, 0.01f, 0.0f, 10.0f)) {
                emitter->SetEndScaleDefault(endScale);
              }
              ImGui::TextDisabled("Independent flags. Both ON = shrink with fade. Both OFF = stays visible until death.");
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
              else if (auto meshEmitter = std::dynamic_pointer_cast<MeshEmitter>(emitter)) {
                // Mesh エミッタの情報表示 (Mesh ポインタは実行時参照なので read-only)
                const auto& edata = meshEmitter->GetData();
                ImGui::Text("Triangle Count: %u", edata.meshTriangleCount);
                ImGui::Text("AABB Min: (%.2f, %.2f, %.2f)", edata.meshAabbMin.x, edata.meshAabbMin.y, edata.meshAabbMin.z);
                ImGui::Text("AABB Max: (%.2f, %.2f, %.2f)", edata.meshAabbMax.x, edata.meshAabbMax.y, edata.meshAabbMax.z);
                ImGui::Text("Vertex SRV: %u, Index SRV: %u", edata.meshVertexSrvIndex, edata.meshIndexSrvIndex);
                Mesh* meshPtr = meshEmitter->GetMesh();
                if (meshPtr) {
                  ImGui::Text("Mesh Vertices: %u", meshPtr->GetVertexCount());
                  ImGui::Text("Mesh Indices: %u", meshPtr->GetIndexCount());
                }
                else {
                  ImGui::TextColored(ImVec4(1, 0.5f, 0.5f, 1), "Mesh pointer is null!");
                }
                ImGui::TextDisabled("Use CreateMeshEmitter() in code to assign mesh.");
                ImGui::TextDisabled("BindMeshWorld(const Matrix4x4*) for dynamic tracking.");
              }
            }

            // 物理 / Curl Noise（per-emitter）— 旧 ForceFields タブから移植
            if (ImGui::CollapsingHeader("Physics & Noise##Properties")) {
              float damping = emitter->GetDamping();
              if (ImGui::SliderFloat("Damping##P", &damping, 0.9f, 1.0f, "%.4f")) {
                emitter->SetDamping(damping);
              }
              if (ImGui::IsItemHovered()) ImGui::SetTooltip("Per-frame velocity damping (0.99 recommended)");

              float restitution = emitter->GetCollisionRestitution();
              if (ImGui::SliderFloat("Restitution##P", &restitution, 0.0f, 1.0f, "%.3f")) {
                emitter->SetCollisionRestitution(restitution);
              }
              if (ImGui::IsItemHovered()) ImGui::SetTooltip("Collision restitution (0.0=absorb, 1.0=fully elastic)");

              float pRadius = emitter->GetParticleRadius();
              if (ImGui::DragFloat("Particle Radius##P", &pRadius, 0.001f, 0.001f, 1.0f, "%.4f")) {
                emitter->SetParticleRadius(pRadius);
              }
              if (ImGui::IsItemHovered()) ImGui::SetTooltip("Particle radius used in depth collision");

              float noiseScale = emitter->GetNoiseScale();
              if (ImGui::SliderFloat("Noise Scale##P", &noiseScale, 0.01f, 10.0f, "%.3f")) {
                emitter->SetNoiseScale(noiseScale);
              }
              if (ImGui::IsItemHovered()) ImGui::SetTooltip("Curl Noise spatial scale (small=large eddies, large=fine detail)");

              float noiseStrength = emitter->GetNoiseStrength();
              if (ImGui::SliderFloat("Noise Strength##P", &noiseStrength, 0.001f, 1.0f, "%.4f")) {
                emitter->SetNoiseStrength(noiseStrength);
              }
              if (ImGui::IsItemHovered()) ImGui::SetTooltip("Curl Noise strength (0.01-0.1=subtle, 0.5+=strong turbulence)");

              ImGui::TextDisabled("Note: Changes apply to newly spawned particles only.");
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

      // 可視化設定タブ
      if (ImGui::BeginTabItem("Visualization")) {
        DrawVisualizationTab();
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
        field.pad = 0.0f;

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

    // --- プリセット保存/読込セクション ---
    ImGui::Separator();
    ImGui::Text("Force Field Presets");

    if (!forceFieldManager_) {
      ImGui::TextDisabled("Call SetForceFieldManager() first");
      return;
    }

    if (ImGui::CollapsingHeader("Save Force Field Preset")) {
      ImGui::InputText("Preset Name##SaveFFPreset", ffPresetSaveBuffer_, sizeof(ffPresetSaveBuffer_));

      const bool hasSelection = (selectedForceFieldIndex_ >= 0
        && selectedForceFieldIndex_ < static_cast<int>(forceFields.size()));

      if (hasSelection) {
        ImGui::Text("From: [Index %d]", selectedForceFieldIndex_);
      }
      else {
        ImGui::TextDisabled("Select a force field first");
      }

      if (ImGui::Button("Save##SaveFFPreset")
          && hasSelection
          && strlen(ffPresetSaveBuffer_) > 0) {
        forceFieldManager_->SavePreset(
          ffPresetSaveBuffer_, static_cast<uint32_t>(selectedForceFieldIndex_));
        AddLog("Saved force field preset: " + std::string(ffPresetSaveBuffer_), LogType::Info);
        ffPresetSaveBuffer_[0] = '\0';
      }
    }

    if (ImGui::CollapsingHeader("Load Force Field Preset")) {
      ImGui::InputText("Preset Name##LoadFFPreset", ffPresetLoadBuffer_, sizeof(ffPresetLoadBuffer_));
      if (ImGui::Button("Load##LoadFFPreset") && strlen(ffPresetLoadBuffer_) > 0) {
        forceFieldManager_->LoadPreset(ffPresetLoadBuffer_);
        AddLog("Loaded force field preset: " + std::string(ffPresetLoadBuffer_), LogType::Info);
        ffPresetLoadBuffer_[0] = '\0';
      }
    }

    if (ImGui::CollapsingHeader("Scene Presets (Force Fields included)")) {
      ImGui::TextWrapped(
        "Scene presets save/load Emitters + Groups + Force Fields together. "
        "Use the 'Presets' tab > 'Scene Presets' to save/load all.");
    }
  }

  // =====================================================
  // パーティクル可視化設定タブ
  // =====================================================
  void DebugUIManager::DrawVisualizationTab() {
    ImGui::Text("Debug Visualization Settings");
    ImGui::Separator();

    // === エミッター形状の可視化設定 ===
    if (ImGui::CollapsingHeader("Emitter Shapes", ImGuiTreeNodeFlags_DefaultOpen)) {
      ImGui::Checkbox("Show Emitter Shapes", &showEmitterShapes_);

      if (showEmitterShapes_) {
        ImGui::ColorEdit4("Sphere Color##EmVis", &emitterColorSphere_.x);
        ImGui::ColorEdit4("Box Color##EmVis", &emitterColorBox_.x);
        ImGui::ColorEdit4("Triangle Color##EmVis", &emitterColorTriangle_.x);
      }
    }

    // === フォースフィールドの可視化設定 ===
    if (ImGui::CollapsingHeader("Force Field Visualization", ImGuiTreeNodeFlags_DefaultOpen)) {
      ImGui::Checkbox("Show Radius##FFVis", &showForceFieldRadius_);
      ImGui::Checkbox("Show Direction##FFVis", &showForceFieldDirection_);

      if (showForceFieldRadius_ || showForceFieldDirection_) {
        ImGui::ColorEdit4("Radius Color##FFVis", &forceFieldRadiusColor_.x);
        ImGui::ColorEdit4("Direction Color##FFVis", &forceFieldDirectionColor_.x);
        ImGui::DragFloat("Arrow Length##FFVis", &forceFieldArrowLength_, 0.1f, 0.1f, 20.0f);
        ImGui::DragFloat("Arrow Head Size##FFVis", &forceFieldArrowHeadSize_, 0.01f, 0.05f, 1.0f);
      }
    }
  }

  // =====================================================
  // エミッター形状の描画
  // =====================================================
  void DebugUIManager::DrawEmitterShape(const std::shared_ptr<GPUParticleEmitter>& emitter) {
    auto* draw2D = Draw2D::GetInstance();
    Vector3 pos = emitter->GetPosition();

    if (auto sphereEmitter = std::dynamic_pointer_cast<SphereEmitter>(emitter)) {
      // 球エミッター: DrawSphere でワイヤーフレーム球を描画
      draw2D->DrawSphere(pos, sphereEmitter->GetRadius(), emitterColorSphere_);
    }
    else if (auto boxEmitter = std::dynamic_pointer_cast<BoxEmitter>(emitter)) {
      // 箱エミッター: OBBを構築してDrawOBBで描画
      Vector3 size = boxEmitter->GetSize();
      Vector3 rotDeg = boxEmitter->GetRotation();

      // 度数法 → ラジアン変換
      constexpr float kDeg2Rad = static_cast<float>(std::numbers::pi) / 180.0f;
      Vector3 rotRad = { rotDeg.x * kDeg2Rad, rotDeg.y * kDeg2Rad, rotDeg.z * kDeg2Rad };

      // OBB: center, halfExtents(sizeの半分), orientation(回転行列)
      Matrix4x4 orientation = Mat4x4::MakeRotateXYZ(rotRad);
      OBB obb(pos, { size.x * 0.5f, size.y * 0.5f, size.z * 0.5f }, orientation);
      draw2D->DrawOBB(obb, emitterColorBox_);
    }
    else if (auto triEmitter = std::dynamic_pointer_cast<TriangleEmitter>(emitter)) {
      // 三角形エミッター: 3辺を線で描画（頂点は相対座標なのでpositionを加算）
      Vector3 v1 = { pos.x + triEmitter->GetVertex1().x, pos.y + triEmitter->GetVertex1().y, pos.z + triEmitter->GetVertex1().z };
      Vector3 v2 = { pos.x + triEmitter->GetVertex2().x, pos.y + triEmitter->GetVertex2().y, pos.z + triEmitter->GetVertex2().z };
      Vector3 v3 = { pos.x + triEmitter->GetVertex3().x, pos.y + triEmitter->GetVertex3().y, pos.z + triEmitter->GetVertex3().z };

      draw2D->DrawLine(v1, v2, emitterColorTriangle_);
      draw2D->DrawLine(v2, v3, emitterColorTriangle_);
      draw2D->DrawLine(v3, v1, emitterColorTriangle_);
    }
  }

  // =====================================================
  // フォースフィールドの可視化
  // =====================================================
  void DebugUIManager::DrawForceFieldVisualization(const ForceFieldData& field, int index) {
    auto* draw2D = Draw2D::GetInstance();

    // 選択中のフォースフィールドは黄色でハイライト
    Vector4 radiusColor = forceFieldRadiusColor_;
    Vector4 dirColor = forceFieldDirectionColor_;
    if (index == selectedForceFieldIndex_) {
      radiusColor = { 1.0f, 1.0f, 0.0f, 0.8f };
      dirColor = { 1.0f, 1.0f, 0.0f, 1.0f };
    }

    // === 影響半径の描画 ===
    if (showForceFieldRadius_ && field.radius > 0.0f) {
      draw2D->DrawSphere(field.position, field.radius, radiusColor);
    }

    // === 方向表示 ===
    if (!showForceFieldDirection_) return;

    switch (static_cast<ForceFieldType>(field.type)) {
    case ForceFieldType::Gravity:
    case ForceFieldType::Directional: {
      // direction方向にstrength比例の矢印を描画
      float dirLen = std::sqrt(field.direction.x * field.direction.x + field.direction.y * field.direction.y + field.direction.z * field.direction.z);
      if (dirLen < 0.001f) break;

      Vector3 dir = { field.direction.x / dirLen, field.direction.y / dirLen, field.direction.z / dirLen };
      float len = (std::min)((std::max)(forceFieldArrowLength_ * field.strength, 0.5f), 10.0f);
      Vector3 end = { field.position.x + dir.x * len, field.position.y + dir.y * len, field.position.z + dir.z * len };
      draw2D->DrawArrow(field.position, end, dirColor, forceFieldArrowHeadSize_);
      break;
    }
    case ForceFieldType::Vortex: {
      // 中心に十字を描画
      constexpr float kCrossSize = 0.5f;
      draw2D->DrawLine(
        { field.position.x - kCrossSize, field.position.y, field.position.z },
        { field.position.x + kCrossSize, field.position.y, field.position.z }, dirColor);
      draw2D->DrawLine(
        { field.position.x, field.position.y - kCrossSize, field.position.z },
        { field.position.x, field.position.y + kCrossSize, field.position.z }, dirColor);
      draw2D->DrawLine(
        { field.position.x, field.position.y, field.position.z - kCrossSize },
        { field.position.x, field.position.y, field.position.z + kCrossSize }, dirColor);

      // 回転軸に垂直な平面上に3/4周の円弧を描画
      float axisLen = std::sqrt(field.direction.x * field.direction.x + field.direction.y * field.direction.y + field.direction.z * field.direction.z);
      if (axisLen < 0.001f) break;

      Vector3 axis = { field.direction.x / axisLen, field.direction.y / axisLen, field.direction.z / axisLen };
      float circleRadius = (field.radius > 0.0f) ? field.radius * 0.5f : 2.0f;
      constexpr int kSegments = 16;

      // 回転軸に垂直な2軸を算出
      Vector3 up = { 0.0f, 1.0f, 0.0f };
      float axisDotUp = axis.x * up.x + axis.y * up.y + axis.z * up.z;
      if (std::abs(axisDotUp) > 0.99f) {
        up = { 1.0f, 0.0f, 0.0f };
      }

      // right = axis x up
      Vector3 right = {
        axis.y * up.z - axis.z * up.y,
        axis.z * up.x - axis.x * up.z,
        axis.x * up.y - axis.y * up.x
      };
      float rightLen = std::sqrt(right.x * right.x + right.y * right.y + right.z * right.z);
      if (rightLen > 0.001f) {
        right = { right.x / rightLen, right.y / rightLen, right.z / rightLen };
      }

      // forward = axis x right (HLSL Vortex: cross(axis, radialDir) と回転方向を一致させる)
      Vector3 forward = {
        axis.y * right.z - axis.z * right.y,
        axis.z * right.x - axis.x * right.z,
        axis.x * right.y - axis.y * right.x
      };

      // 3/4周の円弧（回転方向が分かるように途切れさせる）
      int arcSegments = kSegments * 3 / 4;
      constexpr float kTwoPi = 2.0f * static_cast<float>(std::numbers::pi);

      for (int i = 0; i < arcSegments; i++) {
        float angle1 = (kTwoPi * i) / kSegments;
        float angle2 = (kTwoPi * (i + 1)) / kSegments;

        Vector3 p1 = {
          field.position.x + (right.x * std::cos(angle1) + forward.x * std::sin(angle1)) * circleRadius,
          field.position.y + (right.y * std::cos(angle1) + forward.y * std::sin(angle1)) * circleRadius,
          field.position.z + (right.z * std::cos(angle1) + forward.z * std::sin(angle1)) * circleRadius
        };
        Vector3 p2 = {
          field.position.x + (right.x * std::cos(angle2) + forward.x * std::sin(angle2)) * circleRadius,
          field.position.y + (right.y * std::cos(angle2) + forward.y * std::sin(angle2)) * circleRadius,
          field.position.z + (right.z * std::cos(angle2) + forward.z * std::sin(angle2)) * circleRadius
        };

        draw2D->DrawLine(p1, p2, dirColor);
      }

      // 円弧の終端に接線方向の矢印を追加（回転方向を示す）
      float endAngle = (kTwoPi * arcSegments) / kSegments;
      Vector3 arcEnd = {
        field.position.x + (right.x * std::cos(endAngle) + forward.x * std::sin(endAngle)) * circleRadius,
        field.position.y + (right.y * std::cos(endAngle) + forward.y * std::sin(endAngle)) * circleRadius,
        field.position.z + (right.z * std::cos(endAngle) + forward.z * std::sin(endAngle)) * circleRadius
      };
      // 接線方向 = 円弧の進行方向
      float tangentAngle = endAngle + static_cast<float>(std::numbers::pi) * 0.5f;
      Vector3 tangent = {
        right.x * std::cos(tangentAngle) + forward.x * std::sin(tangentAngle),
        right.y * std::cos(tangentAngle) + forward.y * std::sin(tangentAngle),
        right.z * std::cos(tangentAngle) + forward.z * std::sin(tangentAngle)
      };
      Vector3 arrowStart = {
        arcEnd.x - tangent.x * 0.3f,
        arcEnd.y - tangent.y * 0.3f,
        arcEnd.z - tangent.z * 0.3f
      };
      draw2D->DrawArrow(arrowStart, arcEnd, dirColor, forceFieldArrowHeadSize_ * 0.5f);
      break;
    }
    case ForceFieldType::Attract: {
      // 4方向（±X, ±Z）から中心へ向かう矢印
      float dist = (field.radius > 0.0f) ? field.radius : forceFieldArrowLength_ * 2.0f;
      Vector3 offsets[4] = {
        { dist, 0.0f,  0.0f },
        {-dist, 0.0f,  0.0f },
        { 0.0f, 0.0f,  dist },
        { 0.0f, 0.0f, -dist }
      };
      for (const auto& offset : offsets) {
        Vector3 start = { field.position.x + offset.x, field.position.y + offset.y, field.position.z + offset.z };
        // 中心の少し手前で止める
        Vector3 toCenter = { field.position.x - start.x, field.position.y - start.y, field.position.z - start.z };
        float tcLen = std::sqrt(toCenter.x * toCenter.x + toCenter.y * toCenter.y + toCenter.z * toCenter.z);
        if (tcLen < 0.001f) continue;
        Vector3 dir = { toCenter.x / tcLen, toCenter.y / tcLen, toCenter.z / tcLen };
        Vector3 end = { field.position.x - dir.x * 0.5f, field.position.y - dir.y * 0.5f, field.position.z - dir.z * 0.5f };
        draw2D->DrawArrow(start, end, dirColor, forceFieldArrowHeadSize_);
      }
      break;
    }
    case ForceFieldType::Repel: {
      // 中心から4方向へ向かう矢印
      float dist = (field.radius > 0.0f) ? field.radius * 0.8f : forceFieldArrowLength_ * 2.0f;
      Vector3 directions[4] = {
        { 1.0f, 0.0f,  0.0f },
        {-1.0f, 0.0f,  0.0f },
        { 0.0f, 0.0f,  1.0f },
        { 0.0f, 0.0f, -1.0f }
      };
      for (const auto& dir : directions) {
        Vector3 start = { field.position.x + dir.x * 0.5f, field.position.y + dir.y * 0.5f, field.position.z + dir.z * 0.5f };
        Vector3 end = { field.position.x + dir.x * dist, field.position.y + dir.y * dist, field.position.z + dir.z * dist };
        draw2D->DrawArrow(start, end, dirColor, forceFieldArrowHeadSize_);
      }
      break;
    }
    } // switch
  }

  // =====================================================
  // パーティクル可視化の統合描画
  // =====================================================
  void DebugUIManager::DrawParticleVisualization() {
    if (!emitterManager_) return;

    // === エミッター形状の描画 ===
    if (showEmitterShapes_) {
      auto emitterNames = emitterManager_->GetEmitterNames();
      for (const auto& name : emitterNames) {
        auto emitter = emitterManager_->GetEmitterByName(name);
        if (emitter && emitter->IsActive()) {
          DrawEmitterShape(emitter);
        }
      }
    }

    // === フォースフィールドの描画 ===
    if (showForceFieldRadius_ || showForceFieldDirection_) {
      auto* gpuParticle = GPUParticle::GetInstance();
      const auto& forceFields = gpuParticle->GetForceFields();
      for (int i = 0; i < static_cast<int>(forceFields.size()); i++) {
        DrawForceFieldVisualization(forceFields[i], i);
      }
    }
  }

} // namespace Tako
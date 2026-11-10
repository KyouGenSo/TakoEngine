#ifdef _DEBUG

#include "SceneEditor.h"
#include "Actor.h"
#include "AssetBrowser.h"
#include "BaseScene.h"
#include "CameraComponent.h"
#include "Component.h"
#include "DebugUIManager.h"
#include "ImGuiPropertyVisitor.h"
#include "LightComponent.h"
#include "Prefab.h"
#include "SceneManager.h"
#include "Script.h"
#include "ScriptComponent.h"
#include "TypeRegistry.h"

#include "imgui.h"

#include <json.hpp>

#include <filesystem>
#include <fstream>
#include <numbers>

namespace Tako {

  namespace {

    bool IsEditing()
    {
      return !SceneManager::GetInstance()->IsPlaying();
    }

    /// <summary>
    /// id の Actor を、今の値のまま name のプレハブにする（同じプレハブの他の物は、差分を保ったまま新しいプレハブに乗り換える）
    /// </summary>
    void MakePrefab(BaseScene& scene, ActorId id, const std::string& name)
    {
      Actor* actor = scene.FindActor(id);
      if (!actor || name.empty()) {
        return;
      }
      // 書き換える前のプレハブとの差分で全体を取っておき、書いた後に読み直す
      nlohmann::json before = scene.Save();
      for (nlohmann::json& entry : before["actors"]) {
        if (entry.value("id", ActorId{ 0 }) == id) {
          entry = { { "id", id }, { "prefab", name } };
        }
      }
      if (!SavePrefab(name, *actor)) {
        DebugUIManager::GetInstance()->AddLog("Failed to save prefab: " + GetPrefabPath(name), DebugUIManager::LogType::Error);
        return;
      }
      scene.Load(before);
      DebugUIManager::GetInstance()->AddLog("Saved prefab: " + GetPrefabPath(name), DebugUIManager::LogType::Info);
    }

    /// <summary>
    /// プレハブとの差分を、名前と Transform 以外は捨てる
    /// </summary>
    void RevertToPrefab(BaseScene& scene, ActorId id)
    {
      nlohmann::json before = scene.Save();
      for (nlohmann::json& entry : before["actors"]) {
        if (entry.value("id", ActorId{ 0 }) != id || !entry.contains("overrides")) {
          continue;
        }
        nlohmann::json kept = nlohmann::json::array();
        for (const nlohmann::json& operation : entry["overrides"]) {
          const std::string path = operation.value("path", std::string{});
          if (path.starts_with("/properties/name") || path.starts_with("/properties/transform")) {
            kept.push_back(operation);
          }
        }
        entry["overrides"] = std::move(kept);
      }
      scene.Load(before);
    }

    /// <summary>
    /// カメラと平行光源だけのシーンファイルを書く（Unity の新しいシーンと同じ中身）
    /// </summary>
    bool WriteNewScene(const std::string& name)
    {
      Actor camera;
      camera.SetName("Main Camera");
      camera.SetTransform({ { 1.0f, 1.0f, 1.0f }, { 0.2f, 0.0f, 0.0f }, { 0.0f, 9.0f, -34.0f } });
      camera.AddComponent<CameraComponent>();

      // +Z を前に向けるので、x を回すと斜め下を照らす
      Actor light;
      light.SetName("Directional Light");
      light.SetTransform({ { 1.0f, 1.0f, 1.0f }, { std::numbers::pi_v<float> / 3.0f, std::numbers::pi_v<float> / 6.0f, 0.0f }, { 0.0f, 10.0f, 0.0f } });
      light.AddComponent<LightComponent>();

      nlohmann::json actors = nlohmann::json::array({ camera.Save(), light.Save() });
      // ID は読むときに振る
      for (nlohmann::json& actor : actors) {
        actor.erase("id");
      }
      std::filesystem::create_directories(std::filesystem::path(BaseScene::GetScenePath(name)).parent_path());
      std::ofstream file(BaseScene::GetScenePath(name));
      if (!file) {
        return false;
      }
      file << nlohmann::json{ { "actors", std::move(actors) } }.dump(2);
      return true;
    }

  } // namespace

  bool SceneEditor::DrawHierarchy()
  {
    BaseScene* scene = GetScene();
    if (!scene) {
      return false;
    }
    SyncTracker(*scene);
    const bool editing = IsEditing();
    if (editing) {
      tracker_.BeginSection();
    }

    ImGui::SeparatorText("Actors");
    ImGui::BeginDisabled(!editing);
    if (ImGui::SmallButton("Save")) {
      Save();
    }
    ImGui::EndDisabled();
    ImGui::SetItemTooltip(editing ? "Save the scene file (Ctrl+S)" : "Stop to save. Changes made while playing are discarded on Stop");
    ImGui::SameLine();
    if (ImGui::SmallButton("+ Add")) {
      ImGui::OpenPopup("CreateActor");
    }
    if (ImGui::BeginPopup("CreateActor")) {
      DrawCreateMenu();
      ImGui::EndPopup();
    }
    ImGui::SameLine();
    ImGui::BeginDisabled(!editing);
    if (ImGui::SmallButton("New")) {
      newSceneName_[0] = '\0';
      ImGui::OpenPopup("NewScene");
    }
    ImGui::EndDisabled();
    ImGui::SetItemTooltip("Create a scene with a camera and a directional light, then open it");
    if (ImGui::BeginPopup("NewScene")) {
      DrawNewScenePopup();
      ImGui::EndPopup();
    }
    ImGui::SameLine();
    ImGui::TextDisabled("%s.scene.json%s", scene->GetSceneName().c_str(), tracker_.IsEdited() ? " *" : "");

    bool isSelected = false;
    for (const std::unique_ptr<Actor>& actor : scene->GetActors()) {
      ImGui::PushID(std::to_string(actor->GetId()).c_str());
      const std::string label = actor->GetPrefab().empty() ? actor->GetName() : actor->GetName() + "  [" + actor->GetPrefab() + "]";
      if (ImGui::Selectable(label.c_str(), selected_ == actor->GetId())) {
        selected_  = actor->GetId();
        isSelected = true;
      }
      if (ImGui::BeginDragDropSource()) {
        const ActorId id = actor->GetId();
        ImGui::SetDragDropPayload(kActorPayload, &id, sizeof(id));
        ImGui::TextUnformatted(actor->GetName().c_str());
        ImGui::EndDragDropSource();
      }
      if (ImGui::BeginPopupContextItem("ActorMenu")) {
        selected_  = actor->GetId();
        isSelected = true;
        DrawActorMenu(*actor);
        ImGui::EndPopup();
      }
      if (std::optional<std::string> prefab = AcceptAssetDrop("Prefab")) {
        RequestSpawn(*prefab);
      }
      ImGui::PopID();
    }
    if (scene->GetActors().empty()) {
      ImGui::TextDisabled("No actors (+ Add to create one)");
    }
    // 一覧の下の空きにもプレハブを落とせるようにする
    ImGui::Dummy(ImVec2(ImGui::GetContentRegionAvail().x, ImGui::GetTextLineHeight() * 2.0f));
    if (std::optional<std::string> prefab = AcceptAssetDrop("Prefab")) {
      RequestSpawn(*prefab);
    }

    if (editing) {
      tracker_.EndSection();
      ProcessShortcuts(Shortcuts(), *this);
    }
    return isSelected;
  }

  void SceneEditor::DrawInspector()
  {
    Actor*     actor = GetSelected();
    BaseScene* scene = GetScene();
    if (!actor || !scene) {
      return;
    }
    SyncTracker(*scene);
    const bool editing = IsEditing();
    if (editing) {
      tracker_.BeginSection();
    }

    ImGui::Text("Actor: %s", actor->GetName().c_str());
    ImGui::TextDisabled("%s  id %llu%s%s", actor->GetType().name, static_cast<unsigned long long>(actor->GetId()),
      actor->GetPrefab().empty() ? "" : "  prefab: ", actor->GetPrefab().c_str());
    ImGui::Separator();

    if (DrawProperties(*actor)) {
      scene->MarkChanged();
    }
    for (const std::unique_ptr<Component>& component : actor->GetComponents()) {
      DrawComponent(*actor, *component);
    }
    ImGui::Spacing();
    DrawAddComponent(*actor);

    if (editing) {
      tracker_.EndSection();
      ProcessShortcuts(Shortcuts(), *this);
    }
  }

  void SceneEditor::Update()
  {
    BaseScene* scene = GetScene();
    std::vector<std::function<void(BaseScene&)>> requests = std::move(requests_);
    requests_.clear();
    if (!scene) {
      return;
    }
    for (const auto& request : requests) {
      request(*scene);
    }
  }

  void SceneEditor::CollectUnsavedChanges(std::vector<std::string>& out) const
  {
    if (tracker_.IsEdited()) {
      out.push_back("Scene: " + trackedScene_);
    }
  }

  std::span<const ShortcutBinding<SceneEditor>> SceneEditor::Shortcuts()
  {
    static constexpr ShortcutBinding<SceneEditor> kShortcuts[] = {
      { ImGuiMod_Ctrl | ImGuiKey_S, ImGuiInputFlags_None, "Save the scene file", [](SceneEditor& editor) { editor.Save(); } },
      { ImGuiKey_Delete, ImGuiInputFlags_None, "Delete the selected actor", [](SceneEditor& editor) {
          const ActorId id = editor.selected_;
          editor.Request([id](BaseScene& scene) { scene.Destroy(scene.FindActor(id)); });
        } },
    };
    return kShortcuts;
  }

  bool SceneEditor::HasSelection() const
  {
    return GetSelected() != nullptr;
  }

  void SceneEditor::Request(std::function<void(BaseScene&)> operation)
  {
    requests_.push_back(std::move(operation));
  }

  bool SceneEditor::Save()
  {
    BaseScene* scene = GetScene();
    if (!scene || !IsEditing()) {
      return false;
    }
    if (!scene->SaveFile()) {
      DebugUIManager::GetInstance()->AddLog("Failed to save scene: " + BaseScene::GetScenePath(scene->GetSceneName()), DebugUIManager::LogType::Error);
      return false;
    }
    tracker_.MarkSaved();
    DebugUIManager::GetInstance()->AddLog("Saved scene: " + BaseScene::GetScenePath(scene->GetSceneName()), DebugUIManager::LogType::Info);
    return true;
  }

  void SceneEditor::SyncTracker(const BaseScene& scene)
  {
    if (scene.GetSceneName() != trackedScene_) {
      tracker_.Reset();
      trackedScene_ = scene.GetSceneName();
    }
  }

  void SceneEditor::DrawCreateMenu()
  {
    for (const TypeInfo* type : TypeRegistry::GetInstance()->GetCreatableTypes(Actor::StaticType())) {
      const std::string name = type == &Actor::StaticType() ? "Empty Actor" : type->name;
      if (ImGui::MenuItem(name.c_str())) {
        RequestSpawn(type->name);
      }
    }
    const std::vector<std::string> prefabs = GetPrefabNames();
    if (!prefabs.empty()) {
      ImGui::SeparatorText("Prefabs");
    }
    for (const std::string& prefab : prefabs) {
      if (ImGui::MenuItem(prefab.c_str())) {
        RequestSpawn(prefab);
      }
    }
  }

  void SceneEditor::DrawNewScenePopup()
  {
    ImGui::SetNextItemWidth(200.0f);
    const bool entered = ImGui::InputTextWithHint("##NewSceneName", "scene name", newSceneName_.data(), newSceneName_.size(), ImGuiInputTextFlags_EnterReturnsTrue);
    const std::string name = newSceneName_.data();
    const bool exists = BaseScene::HasSceneFile(name);
    if (exists) {
      ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.2f, 1.0f), "%s.scene.json already exists", name.c_str());
    }
    ImGui::BeginDisabled(name.empty() || exists);
    if ((ImGui::Button("Create") || entered) && !name.empty() && !exists) {
      if (WriteNewScene(name)) {
        DebugUIManager::GetInstance()->AddLog("Created scene: " + BaseScene::GetScenePath(name), DebugUIManager::LogType::Info);
        SceneManager::GetInstance()->ChangeScene(name);
      }
      else {
        DebugUIManager::GetInstance()->AddLog("Failed to write " + BaseScene::GetScenePath(name), DebugUIManager::LogType::Error);
      }
      ImGui::CloseCurrentPopup();
    }
    ImGui::EndDisabled();
  }

  void SceneEditor::RequestSpawn(const std::string& typeOrPrefab)
  {
    Request([typeOrPrefab](BaseScene& scene) { scene.Spawn(typeOrPrefab); });
  }

  void SceneEditor::DrawActorMenu(Actor& actor)
  {
    const ActorId id = actor.GetId();
    if (ImGui::MenuItem("Duplicate")) {
      Request([id](BaseScene& scene) {
        Actor* source = scene.FindActor(id);
        if (!source) { return; }
        nlohmann::json json = SavePrefabInstance(*source);
        json.erase("id");
        if (Actor* copy = scene.SpawnFromSave(json)) {
          copy->SetName(source->GetName() + " Copy");
        }
      });
    }
    if (ImGui::MenuItem("Delete", "Del")) {
      Request([id](BaseScene& scene) { scene.Destroy(scene.FindActor(id)); });
    }

    ImGui::SeparatorText("Prefab");
    if (!actor.GetPrefab().empty()) {
      const std::string prefab = actor.GetPrefab();
      if (ImGui::MenuItem("Apply to Prefab")) {
        Request([id, prefab](BaseScene& scene) { MakePrefab(scene, id, prefab); });
      }
      ImGui::SetItemTooltip("Write this actor's values to %s. Other instances keep their own overrides", prefab.c_str());
      if (ImGui::MenuItem("Revert to Prefab")) {
        Request([id](BaseScene& scene) { RevertToPrefab(scene, id); });
      }
      ImGui::SetItemTooltip("Discard overrides except name and transform");
    }
    ImGui::SetNextItemWidth(140.0f);
    ImGui::InputTextWithHint("##PrefabName", "prefab name", prefabName_.data(), prefabName_.size());
    ImGui::SameLine();
    ImGui::BeginDisabled(prefabName_[0] == '\0');
    if (ImGui::Button("Save as Prefab")) {
      const std::string name = prefabName_.data();
      Request([id, name](BaseScene& scene) { MakePrefab(scene, id, name); });
      prefabName_[0] = '\0';
      ImGui::CloseCurrentPopup();
    }
    ImGui::EndDisabled();
  }

  void SceneEditor::DrawComponent(Actor& actor, Component& component)
  {
    ImGui::PushID(component.GetName().c_str());
    bool isKept = true;
    const std::string header = component.GetName() + "  (" + component.GetType().name + ")";
    if (ImGui::CollapsingHeader(header.c_str(), &isKept, ImGuiTreeNodeFlags_DefaultOpen)) {
      if (DrawProperties(component)) {
        actor.GetScene()->MarkChanged();
      }
    }
    if (!isKept) {
      const ActorId     id   = actor.GetId();
      const std::string name = component.GetName();
      Request([id, name](BaseScene& scene) {
        Actor* owner = scene.FindActor(id);
        if (Component* target = owner ? owner->FindComponent(name) : nullptr) {
          owner->RemoveComponent(target);
          scene.MarkChanged();
        }
      });
    }
    ImGui::PopID();
  }

  void SceneEditor::DrawAddComponent(Actor& actor)
  {
    if (ImGui::Button("Add Component")) {
      ImGui::OpenPopup("AddComponent");
    }
    if (!ImGui::BeginPopup("AddComponent")) {
      return;
    }
    for (const TypeInfo* type : TypeRegistry::GetInstance()->GetCreatableTypes(Component::StaticType())) {
      // スクリプトは Script の中に入れて使う
      if (type->IsChildOf(Script::StaticType())) {
        continue;
      }
      const bool  isScript = type == &ScriptComponent::StaticType();
      const char* label    = isScript ? "Script" : type->name;
      if (!ImGui::MenuItem(label)) {
        continue;
      }
      const ActorId     id       = actor.GetId();
      const std::string typeName = type->name;
      const std::string name     = isScript ? "Script" : "";
      Request([id, typeName, name](BaseScene& scene) {
        Actor*                   owner  = scene.FindActor(id);
        std::unique_ptr<TObject> object = TypeRegistry::GetInstance()->Create(typeName);
        if (owner && object) {
          owner->AddComponent(std::unique_ptr<Component>(static_cast<Component*>(object.release())), name);
          scene.MarkChanged();
        }
      });
    }
    ImGui::EndPopup();
  }

  std::string SceneEditor::SerializeScene() const
  {
    BaseScene* scene = GetScene();
    return scene ? scene->Save().dump() : std::string();
  }

  void SceneEditor::RestoreScene(const std::string& text)
  {
    // Load は古い Actor を次のフレームまで残すので、描画中でもすぐに戻してよい
    if (BaseScene* scene = GetScene(); scene && !text.empty()) {
      scene->Load(nlohmann::json::parse(text));
    }
  }

  BaseScene* SceneEditor::GetScene()
  {
    return SceneManager::GetInstance()->GetCurrentScene();
  }

  Actor* SceneEditor::GetSelected() const
  {
    BaseScene* scene = GetScene();
    return scene && selected_ != 0 ? scene->FindActor(selected_) : nullptr;
  }

  uint64_t SceneEditor::GetSceneRevision() const
  {
    BaseScene* scene = GetScene();
    return scene ? scene->GetRevision() : 0;
  }

} // namespace Tako

#endif // _DEBUG

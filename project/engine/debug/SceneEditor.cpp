#ifdef _DEBUG

#include "SceneEditor.h"
#include "Actor.h"
#include "AssetBrowser.h"
#include "BaseScene.h"
#include "Camera.h"
#include "CameraComponent.h"
#include "Component.h"
#include "DebugUIManager.h"
#include "ImGuiPropertyVisitor.h"
#include "LightComponent.h"
#include "JsonPropertyVisitor.h"
#include "Mat4x4Func.h"
#include "MeshComponent.h"
#include "NewScript.h"
#include "Object3d.h"
#include "Prefab.h"
#include "SceneManager.h"
#include "Script.h"
#include "ScriptComponent.h"
#include "TypeRegistry.h"

#include "imgui.h"

#include <ImGuizmo.h>
#include <json.hpp>

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <limits>
#include <numbers>
#include <optional>

namespace Tako {

  namespace {

    /// <summary>
    /// actor の、描かれている MeshComponent ごとに fn を呼ぶ
    /// </summary>
    template<class Fn>
    void ForEachDrawnMesh(const Actor& actor, Fn&& fn)
    {
      if (!actor.IsActiveInHierarchy()) {
        return;
      }
      for (const std::unique_ptr<Component>& component : actor.GetComponents()) {
        MeshComponent* mesh = component->IsA<MeshComponent>() ? static_cast<MeshComponent*>(component.get()) : nullptr;
        if (mesh && mesh->IsVisible()) {
          fn(*mesh);
        }
      }
    }

    bool IsEditing()
    {
      return !SceneManager::GetInstance()->IsPlaying();
    }

    /// <summary>
    /// id の Actor と子孫を、今の値のまま name のプレハブにする（同じプレハブを置いた他の組は、差分を保ったまま新しい内容に乗り換える）。
    /// 置いた組の途中の Actor から同じ名前で呼ばれたら（Apply）、組の根から書く
    /// </summary>
    void MakePrefab(BaseScene& scene, ActorId id, const std::string& name)
    {
      Actor* actor = scene.FindActor(id);
      if (!actor || name.empty()) {
        return;
      }
      if (actor->GetPrefab() == name) {
        if (Actor* root = scene.FindActor(actor->GetPrefabRoot())) { actor = root; }
      }
      const ActorId        rootId = actor->GetId();
      const nlohmann::json tree   = SaveActorTree(*actor);

      // 同じプレハブから置いた Actor は今の番号を保ち（他の組の差分が同じ Actor に当たるように）、新しい Actor には続きの番号を振る
      ActorIdMap nodes;
      ActorId    nextNode = 1;
      for (const nlohmann::json& entry : tree["actors"]) {
        const ActorId entryId = entry.value("id", ActorId{ 0 });
        if (entry.value("prefab", std::string{}) == name && entry.value("root", entryId) == rootId) {
          nodes[entryId] = entry.value("node", ActorId{ 0 });
          // 中に置いたプレハブから来た、計算で決めた番号は大きいので、続きの番号の元にしない
          if ((nodes[entryId] & kImplicitPrefabNode) == 0) {
            nextNode = (std::max)(nextNode, nodes[entryId] + 1);
          }
        }
      }
      for (const nlohmann::json& entry : tree["actors"]) {
        if (const ActorId entryId = entry.value("id", ActorId{ 0 }); !nodes.contains(entryId)) {
          nodes[entryId] = nextNode++;
        }
      }

      // 書き換える前のプレハブとの差分で全体を取っておき、書いた後に読み直す
      nlohmann::json before = scene.Save();
      for (nlohmann::json& entry : before["actors"]) {
        const ActorId entryId = entry.value("id", ActorId{ 0 });
        if (auto node = nodes.find(entryId); node != nodes.end()) {
          nlohmann::json placed = { { "id", entryId }, { "prefab", name }, { "node", node->second } };
          if (entryId != rootId) {
            placed["root"] = rootId;
          } else if (const Actor* parent = actor->GetParent()) {
            placed["overrides"] = nlohmann::json::array({ { { "op", "add" }, { "path", "/parent" }, { "value", parent->GetId() } } });
          }
          entry = std::move(placed);
        } else if (entry.value("prefab", std::string{}) == name && entry.value("root", ActorId{ 0 }) == rootId) {
          // 組の外へ出した Actor は、プレハブから消える。つながりを切って、今の値のまま残す
          if (Actor* moved = scene.FindActor(entryId)) { entry = moved->Save(); }
        }
      }
      if (!SavePrefab(name, MakePrefabContent(name, tree, nodes))) {
        DebugUIManager::GetInstance()->AddLog("Failed to save prefab: " + GetPrefabPath(name), DebugUIManager::LogType::Error);
        return;
      }
      scene.Load(before);
      DebugUIManager::GetInstance()->AddLog("Saved prefab: " + GetPrefabPath(name), DebugUIManager::LogType::Info);
    }

    /// <summary>
    /// id の Actor が入っている組の、プレハブとの差分を捨てる（消した子も戻る）。根の名前・Transform・親は残す
    /// </summary>
    void RevertToPrefab(BaseScene& scene, ActorId id)
    {
      const Actor* actor = scene.FindActor(id);
      if (!actor) {
        return;
      }
      const ActorId  rootId = actor->GetPrefabRoot();
      nlohmann::json before = scene.Save();
      for (nlohmann::json& entry : before["actors"]) {
        const ActorId entryId = entry.value("id", ActorId{ 0 });
        if (entry.value("root", entryId) != rootId || !entry.contains("overrides")) {
          continue;
        }
        entry.erase("removed");
        nlohmann::json kept = nlohmann::json::array();
        for (const nlohmann::json& operation : entry["overrides"]) {
          const std::string path = operation.value("path", std::string{});
          if (entryId == rootId && (path.starts_with("/properties/name") || path.starts_with("/properties/transform") || path == "/parent")) {
            kept.push_back(operation);
          }
        }
        entry["overrides"] = std::move(kept);
      }
      scene.Load(before);
    }

    /// <summary>
    /// id の Actor が入っている組の、プレハブとのつながりを切る（値はそのまま）。
    /// ほどくのは 1 段だけで、中に置いてあった別のプレハブは、置いた物として残る
    /// </summary>
    void UnpackPrefab(BaseScene& scene, ActorId id)
    {
      const Actor* actor = scene.FindActor(id);
      if (!actor || actor->GetPrefab().empty()) {
        return;
      }
      const ActorId       rootId = actor->GetPrefabRoot();
      std::vector<Actor*> group;
      ActorIdMap          ids;  // プレハブの中の番号 → シーンの ID
      for (const std::unique_ptr<Actor>& other : scene.GetActors()) {
        if (!other->GetPrefab().empty() && other->GetPrefabRoot() == rootId) {
          group.push_back(other.get());
          ids[other->GetPrefabNode()] = other->GetId();
        }
      }
      for (Actor* member : group) {
        const PrefabLink inner     = GetNestedPrefabLink(member->GetPrefab(), member->GetPrefabNode());
        const auto       innerRoot = ids.find(inner.root);
        if (inner.prefab.empty() || (inner.root != 0 && innerRoot == ids.end())) {
          member->SetPrefabLink({}, 0, 0);
        } else {
          member->SetPrefabLink(inner.prefab, inner.node, inner.root != 0 ? innerRoot->second : 0);
        }
      }
      scene.MarkChanged();
    }

    void CollectInOrder(Actor& actor, std::vector<Actor*>& out)
    {
      out.push_back(&actor);
      for (Actor* child : actor.GetChildren()) {
        CollectInOrder(*child, out);
      }
    }

    /// <summary>
    /// Hierarchy に並ぶ順（親の次に子）の全 Actor
    /// </summary>
    std::vector<Actor*> GetHierarchyOrder(const BaseScene& scene)
    {
      std::vector<Actor*> order;
      for (const std::unique_ptr<Actor>& actor : scene.GetActors()) {
        if (!actor->GetParent()) {
          CollectInOrder(*actor, order);
        }
      }
      return order;
    }

    /// <summary>
    /// Stop の後に読み込む値。component が空なら Actor 自身の値
    /// </summary>
    /// <returns>JSON の文字列。コンポーネントが無ければ空</returns>
    std::string CaptureKeptValues(Actor& actor, const std::string& component)
    {
      if (component.empty()) {
        return nlohmann::json{ { "properties", WriteProperties(actor) } }.dump();
      }
      Component* target = actor.FindComponent(component);
      return target ? nlohmann::json{ { "type", target->GetType().name }, { "properties", WriteProperties(*target) } }.dump() : std::string();
    }

    // + Add に出す定番（Actor の名前、付けるコンポーネント、その TPROPERTY）
    struct ActorPreset {
      const char* name;
      const char* component;
      const char* properties;
    };
    constexpr ActorPreset kActorPresets[] = {
      { "Camera", "CameraComponent", "{}" },
      { "Directional Light", "LightComponent", R"({"type":"Directional"})" },
      { "Point Light", "LightComponent", R"({"type":"Point"})" },
      { "Mesh", "MeshComponent", "{}" },
      { "UI Layout", "UILayoutComponent", "{}" },
      { "Particle System", "ParticleSystemComponent", "{}" },
    };

    // クリップボードの JSON が、このエディタの物かを見分けるキー
    constexpr const char* kClipboardScene     = "takoScene";
    constexpr const char* kClipboardComponent = "takoComponent";

    // Ctrl を押している間のギズモの刻み（移動は長さ、回転は度、拡大は倍率）
    constexpr float kGizmoSnaps[][3] = { { 0.5f, 0.5f, 0.5f }, { 15.0f, 15.0f, 15.0f }, { 0.1f, 0.1f, 0.1f } };

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

  void SceneEditor::DrawHierarchy()
  {
    BaseScene* scene = GetScene();
    if (!scene) {
      return;
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

    for (const std::unique_ptr<Actor>& actor : scene->GetActors()) {
      if (!actor->GetParent()) {
        DrawActorRow(*actor);
      }
    }
    if (scene->GetActors().empty()) {
      ImGui::TextDisabled("No actors (+ Add to create one)");
    }
    // 一覧の下の空き：プレハブかモデルを落とすと生成、Actor を落とすと親を外す
    ImGui::Dummy(ImVec2(ImGui::GetContentRegionAvail().x, ImGui::GetTextLineHeight() * 2.0f));
    if (ImGui::BeginDragDropTarget()) {
      AcceptSpawnDrops();
      if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(kActorPayload)) {
        RequestParentDropped(*static_cast<const ActorId*>(payload->Data), 0);
      }
      ImGui::EndDragDropTarget();
    }
    if (ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
      pressedRow_ = 0;
    }

    if (editing) {
      tracker_.EndSection();
      ProcessShortcuts(Shortcuts(), *this);
    }
  }

  void SceneEditor::DrawInspector()
  {
    Actor*     actor = GetSelected();
    BaseScene* scene = GetScene();
    if (!actor || !scene) {
      return;
    }
    if (selection_.size() > 1) {
      ImGui::Text("%d actors selected", static_cast<int>(selection_.size()));
      ImGui::TextDisabled("Select one actor to edit its values. Delete, duplicate, copy, parenting and the gizmo apply to all of them");
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
    if (!editing) {
      bool keepsActor = IsKept(actor->GetId(), "");
      if (ImGui::Checkbox("Keep on Stop", &keepsActor)) {
        ToggleKeep(*actor, "");
      }
      ImGui::SetItemTooltip("After Stop, load this actor's own values (name, active, transform) as they are at that moment. One undo step");
      ImGui::SameLine();
      if (ImGui::SmallButton("Keep All")) {
        if (!IsKept(actor->GetId(), "")) { ToggleKeep(*actor, ""); }
        for (const std::unique_ptr<Component>& component : actor->GetComponents()) {
          if (!IsKept(actor->GetId(), component->GetName())) { ToggleKeep(*actor, component->GetName()); }
        }
      }
      ImGui::SetItemTooltip("Keep the actor's own values and every component's values. A single component: right-click its header");
    }
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

  void SceneEditor::DrawGizmoToolbar()
  {
    ImGui::RadioButton("Move (W)", &gizmoOperation_, 0);
    ImGui::SameLine();
    ImGui::RadioButton("Rotate (E)", &gizmoOperation_, 1);
    ImGui::SameLine();
    ImGui::RadioButton("Scale (R)", &gizmoOperation_, 2);
    ImGui::SameLine();
    ImGui::Checkbox("Local", &isGizmoLocal_);
    ImGui::SetItemTooltip("Off: move and rotate along the world axes");
  }

  bool SceneEditor::DrawGizmo(const Camera& camera, const ImVec2& imageMin, const ImVec2& imageMax)
  {
    Actor*     actor = GetSelected();
    BaseScene* scene = GetScene();
    if (!actor || !scene) {
      return false;
    }
    // 右ボタンを押している間の W / E は、ビューのカメラの移動に使う
    if (ImGui::IsWindowHovered() && !ImGui::IsMouseDown(ImGuiMouseButton_Right) && !ImGui::GetIO().WantTextInput) {
      if (ImGui::IsKeyPressed(ImGuiKey_W)) { gizmoOperation_ = 0; }
      if (ImGui::IsKeyPressed(ImGuiKey_E)) { gizmoOperation_ = 1; }
      if (ImGui::IsKeyPressed(ImGuiKey_R)) { gizmoOperation_ = 2; }
    }
    SyncTracker(*scene);
    const bool editing = IsEditing();
    if (editing) {
      tracker_.BeginSection();
    }

    static constexpr ImGuizmo::OPERATION kOperations[] = { ImGuizmo::TRANSLATE, ImGuizmo::ROTATE, ImGuizmo::SCALE };
    ImGuizmo::SetOrthographic(false);
    // 既定では視点に合わせて軸の向きを反転して描くが、矢印が + の向きを指さなくなるので切る
    ImGuizmo::AllowAxisFlip(false);
    ImGuizmo::SetDrawlist();
    ImGuizmo::SetRect(imageMin.x, imageMin.y, imageMax.x - imageMin.x, imageMax.y - imageMin.y);
    // 拡大は軸に沿ってしか意味が無いので、いつもローカル
    const ImGuizmo::MODE mode = isGizmoLocal_ || gizmoOperation_ == 2 ? ImGuizmo::LOCAL : ImGuizmo::WORLD;
    const Matrix4x4 before = actor->GetWorldMatrix();
    Matrix4x4       world  = before;
    const float*    snap   = ImGui::GetIO().KeyCtrl ? kGizmoSnaps[gizmoOperation_] : nullptr;
    if (ImGuizmo::Manipulate(&camera.GetViewMatrix().m[0][0], &camera.GetProjectionMatrix().m[0][0], kOperations[gizmoOperation_], mode, &world.m[0][0], nullptr, snap)) {
      MoveSelection(before, world);
      scene->MarkChanged();
    }

    if (editing) {
      tracker_.EndSection();
    }
    return ImGuizmo::IsOver() || ImGuizmo::IsUsing();
  }

  void SceneEditor::PickActor(const Camera& camera, const ImVec2& imageMin, const ImVec2& imageMax)
  {
    BaseScene* scene = GetScene();
    if (!scene) {
      return;
    }
    constexpr float kOriginRadius = 12.0f;
    const Matrix4x4 viewProjection = camera.GetViewMatrix() * camera.GetProjectionMatrix();
    const ImVec2    mouse          = ImGui::GetMousePos();
    const float     width          = imageMax.x - imageMin.x;
    const float     height         = imageMax.y - imageMin.y;

    // 描かれているメッシュのうち、マウスの下で一番手前にある物を選ぶ
    const Vector3 origin    = camera.GetTranslate();
    const Vector3 farPoint  = Mat4x4::Transform(Mat4x4::Inverse(viewProjection), { (mouse.x - imageMin.x) / width * 2.0f - 1.0f, 1.0f - (mouse.y - imageMin.y) / height * 2.0f, 1.0f });
    const Vector3 direction = (farPoint - origin).Normalize();
    ActorId       picked    = 0;
    float         nearest   = (std::numeric_limits<float>::max)();
    for (const std::unique_ptr<Actor>& actor : scene->GetActors()) {
      ForEachDrawnMesh(*actor, [&](MeshComponent& mesh) {
        const std::optional<float> hit = mesh.GetObject3d()->Raycast(origin, direction);
        if (hit && *hit < nearest) {
          picked  = actor->GetId();
          nearest = *hit;
        }
      });
    }

    // メッシュに当たらなければ、原点の近くの Actor にする（メッシュの無い物と、遠くて点にしか見えない物のため）
    // ponytail: メッシュの無い物は、後ろにメッシュがあると選べない。選べるようにするなら、目印を描いてそれを判定に足す
    if (picked == 0) {
      float pickedDepth = (std::numeric_limits<float>::max)();
      for (const std::unique_ptr<Actor>& actor : scene->GetActors()) {
        const Vector3 p     = actor->GetWorldTransform().translate;
        const auto&   m     = viewProjection.m;
        const float   depth = p.x * m[0][3] + p.y * m[1][3] + p.z * m[2][3] + m[3][3];
        if (depth <= 0.0f || depth >= pickedDepth) {
          continue;
        }
        const float x = imageMin.x + ((p.x * m[0][0] + p.y * m[1][0] + p.z * m[2][0] + m[3][0]) / depth * 0.5f + 0.5f) * width;
        const float y = imageMin.y + (0.5f - (p.x * m[0][1] + p.y * m[1][1] + p.z * m[2][1] + m[3][1]) / depth * 0.5f) * height;
        if ((x - mouse.x) * (x - mouse.x) + (y - mouse.y) * (y - mouse.y) <= kOriginRadius * kOriginRadius) {
          picked      = actor->GetId();
          pickedDepth = depth;
        }
      }
    }
    if (ImGui::GetIO().KeyCtrl) {
      if (picked != 0) { Select(*scene, picked, true, false); }
    } else if (picked != 0) {
      Select(*scene, picked, false, false);
    } else {
      ClearSelection();
    }
  }

  void SceneEditor::MoveSelection(const Matrix4x4& before, const Matrix4x4& after)
  {
    Actor* primary = GetSelected();
    if (!primary) {
      return;
    }
    const auto toLocal = [](const Actor& actor, const Matrix4x4& world) {
      const Actor* parent = actor.GetParent();
      return parent ? world * Mat4x4::Inverse(parent->GetWorldMatrix()) : world;
    };
    // 拡大は、それぞれの大きさに同じ倍率を掛ける（行列の差を掛けると、向きの違う物がゆがむ）
    Transform resized;
    Mat4x4::Decompose(toLocal(*primary, after), resized.translate, resized.rotate, resized.scale);
    const Vector3&  oldScale = primary->GetTransform().scale;
    const Vector3   ratio{ oldScale.x != 0.0f ? resized.scale.x / oldScale.x : 1.0f, oldScale.y != 0.0f ? resized.scale.y / oldScale.y : 1.0f,
                           oldScale.z != 0.0f ? resized.scale.z / oldScale.z : 1.0f };
    const Matrix4x4 delta = Mat4x4::Inverse(before) * after;

    // 先祖も選んである物は、先祖と一緒に動くので触らない
    for (Actor* target : GetTopLevelSelection()) {
      Transform& transform = target->GetTransform();
      if (target == primary) {
        transform = resized;
      } else if (gizmoOperation_ == 2) {
        transform.scale = { transform.scale.x * ratio.x, transform.scale.y * ratio.y, transform.scale.z * ratio.z };
      } else {
        Mat4x4::Decompose(toLocal(*target, target->GetWorldMatrix() * delta), transform.translate, transform.rotate, transform.scale);
      }
    }
  }

  bool SceneEditor::GetSelectionBounds(Vector3& center, float& radius) const
  {
    BaseScene* scene = GetScene();
    if (!scene || selection_.empty()) {
      return false;
    }
    constexpr float kLimit = (std::numeric_limits<float>::max)();
    AABB            bounds{ { kLimit, kLimit, kLimit }, { -kLimit, -kLimit, -kLimit } };
    const auto      add = [&bounds](const Vector3& p) {
      bounds.min = { (std::min)(bounds.min.x, p.x), (std::min)(bounds.min.y, p.y), (std::min)(bounds.min.z, p.z) };
      bounds.max = { (std::max)(bounds.max.x, p.x), (std::max)(bounds.max.y, p.y), (std::max)(bounds.max.z, p.z) };
    };
    // 原点も入れる（ギズモが出る場所。メッシュの無い Actor はこれだけになる）
    const auto visit = [&](const auto& self, const Actor& actor) -> void {
      add(actor.GetWorldPosition());
      ForEachDrawnMesh(actor, [&](MeshComponent& mesh) {
        if (const std::optional<AABB> box = mesh.GetObject3d()->GetWorldBounds()) {
          add(box->min);
          add(box->max);
        }
      });
      for (const Actor* child : actor.GetChildren()) {
        self(self, *child);
      }
    };
    for (const uint64_t id : selection_) {
      if (const Actor* actor = scene->FindActor(id)) {
        visit(visit, *actor);
      }
    }
    center = (bounds.min + bounds.max) * 0.5f;
    radius = (bounds.max - bounds.min).Length() * 0.5f;
    return true;
  }

  void SceneEditor::Update()
  {
    BaseScene* scene = GetScene();
    std::vector<std::function<void(BaseScene&)>> requests = std::move(requests_);
    requests_.clear();
    if (!scene) {
      return;
    }
    // 予約した操作は UI の区間の外で行うので、ここで前後を比べて Undo の 1 手にする（Play 中の変更は積まない）
    const bool tracks = !requests.empty() && IsEditing();
    if (tracks) {
      SyncTracker(*scene);
    }
    const std::string before = tracks ? SerializeScene() : std::string();
    for (const auto& request : requests) {
      request(*scene);
    }
    if (tracks) {
      tracker_.RecordExternalChange(before);
    }

    // 消えた Actor を選択から外す
    std::erase_if(selection_, [scene](uint64_t id) { return !scene->FindActor(id); });
    if (!IsSelected(selected_)) {
      selected_ = selection_.empty() ? 0 : selection_.back();
    }

    // Play 中は、残す印を付けた物の値を控え続ける（Stop はこの関数の前に行われるので、止まった後では読めない）
    const bool isPlaying = !IsEditing();
    if (isPlaying && scene->GetSceneName() == keptScene_) {
      for (auto& [key, values] : kept_) {
        if (Actor* actor = scene->FindActor(key.first)) {
          if (std::string captured = CaptureKeptValues(*actor, key.second); !captured.empty()) {
            values = std::move(captured);
          }
        }
      }
    } else if (!isPlaying && wasPlaying_ && !kept_.empty()) {
      ApplyKept(*scene);
    }
    wasPlaying_ = isPlaying;
  }

  void SceneEditor::ToggleKeep(Actor& actor, const std::string& component)
  {
    const std::string& sceneName = actor.GetScene()->GetSceneName();
    if (sceneName != keptScene_) {
      kept_.clear();
      keptScene_ = sceneName;
    }
    const std::pair<uint64_t, std::string> key{ actor.GetId(), component };
    if (kept_.erase(key) == 0) {
      kept_[key] = CaptureKeptValues(actor, component);
    }
  }

  void SceneEditor::ApplyKept(BaseScene& scene)
  {
    const std::map<std::pair<uint64_t, std::string>, std::string> kept = std::move(kept_);
    kept_.clear();
    DebugUIManager* console = DebugUIManager::GetInstance();
    if (scene.GetSceneName() != keptScene_) {
      console->AddLog("Kept values were discarded: they belong to the scene \"" + keptScene_ + "\"", DebugUIManager::LogType::Warning);
      return;
    }
    const std::string before  = SerializeScene();
    int               applied = 0;
    for (const auto& [key, text] : kept) {
      const std::string    label  = "actor " + std::to_string(key.first) + (key.second.empty() ? "" : " / " + key.second);
      const nlohmann::json values = nlohmann::json::parse(text, nullptr, false);
      Actor*               actor  = scene.FindActor(key.first);
      Component*           target = actor && !key.second.empty() ? actor->FindComponent(key.second) : nullptr;
      const bool           fits   = actor && values.is_object() && (key.second.empty() || (target && values.value("type", std::string{}) == target->GetType().name));
      if (!fits) {
        console->AddLog("Kept values were not applied (not in the scene before Play): " + label, DebugUIManager::LogType::Warning);
        continue;
      }
      if (target) {
        ReadProperties(*target, values.value("properties", nlohmann::json::object()));
        target->PostEditChange();
      } else {
        ReadProperties(*actor, values.value("properties", nlohmann::json::object()));
      }
      ++applied;
    }
    if (applied > 0) {
      scene.MarkChanged();
      tracker_.RecordExternalChange(before);
      console->AddLog("Kept " + std::to_string(applied) + " value set(s) from Play (Ctrl+Z to undo, Ctrl+S in Scene Hierarchy to save)", DebugUIManager::LogType::Info);
    }
  }

  bool SceneEditor::IsKept(uint64_t actor, const std::string& component) const
  {
    return kept_.contains({ actor, component });
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
      { ImGuiKey_Delete, ImGuiInputFlags_None, "Delete the selected actors", [](SceneEditor& editor) { editor.RequestDestroySelection(); } },
      { ImGuiMod_Ctrl | ImGuiKey_D, ImGuiInputFlags_None, "Duplicate the selected actors and their children", [](SceneEditor& editor) { editor.RequestDuplicateSelection(); } },
      { ImGuiMod_Ctrl | ImGuiKey_C, ImGuiInputFlags_None, "Copy the selected actors and their children", [](SceneEditor& editor) { editor.CopySelection(); } },
      { ImGuiMod_Ctrl | ImGuiKey_X, ImGuiInputFlags_None, "Cut the selected actors and their children", [](SceneEditor& editor) {
        editor.CopySelection();
        editor.RequestDestroySelection();
      } },
      { ImGuiMod_Ctrl | ImGuiKey_V, ImGuiInputFlags_None, "Paste the copied actors (also into another scene)", [](SceneEditor& editor) { editor.PasteClipboard(); } },
      { ImGuiMod_Ctrl | ImGuiKey_A, ImGuiInputFlags_None, "Select every actor", [](SceneEditor& editor) { editor.SelectAll(); } },
      { ImGuiKey_Escape, ImGuiInputFlags_None, "Deselect", [](SceneEditor& editor) { editor.ClearSelection(); } },
      { ImGuiKey_None, ImGuiInputFlags_None, "Hierarchy / Debug Viewport: add to or remove from the selection", nullptr, "Ctrl + LMB" },
      { ImGuiKey_None, ImGuiInputFlags_None, "Hierarchy: select every row between the last selected one and this one", nullptr, "Shift + LMB" },
      { ImGuiKey_None, ImGuiInputFlags_None, "Debug Viewport: select the actor under the cursor (click its mesh)", nullptr, "LMB" },
      { ImGuiKey_F, ImGuiInputFlags_None, "Debug Viewport: move the camera so that the selection fits in the view", nullptr },
      { ImGuiKey_None, ImGuiInputFlags_None, "Debug Viewport: snap the gizmo (move 0.5, rotate 15 deg, scale 0.1)", nullptr, "Ctrl + drag" },
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
    for (const ActorPreset& preset : kActorPresets) {
      if (ImGui::MenuItem(preset.name)) {
        RequestSpawnWith(preset.name, preset.component, preset.properties);
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

  void SceneEditor::RequestSpawnWith(const std::string& name, const std::string& componentType, const std::string& properties)
  {
    Request([name, componentType, properties](BaseScene& scene) {
      Actor*                   actor  = scene.Spawn("Actor");
      std::unique_ptr<TObject> object = TypeRegistry::GetInstance()->Create(componentType);
      if (!actor || !object || !object->IsA<Component>()) {
        return;
      }
      actor->SetName(name);
      Component* component = actor->AddComponent(std::unique_ptr<Component>(static_cast<Component*>(object.release())), "");
      ReadProperties(*component, nlohmann::json::parse(properties));
    });
  }

  void SceneEditor::AcceptSpawnDrops()
  {
    if (std::optional<std::string> prefab = AcceptAssetPayload("Prefab")) {
      RequestSpawn(*prefab);
    }
    if (std::optional<std::string> model = AcceptAssetPayload("Model")) {
      RequestSpawnWith(std::filesystem::path(*model).stem().string(), "MeshComponent", nlohmann::json{ { "model", *model } }.dump());
    }
  }

  void SceneEditor::RequestDuplicate(uint64_t actor)
  {
    Request([actor](BaseScene& scene) {
      Actor* source = scene.FindActor(actor);
      if (!source) { return; }
      if (Actor* copy = scene.SpawnTree(SaveActorTree(*source), true)) {
        copy->SetName(source->GetName() + " Copy");
      }
    });
  }

  void SceneEditor::RequestDuplicateSelection()
  {
    for (const Actor* actor : GetTopLevelSelection()) {
      RequestDuplicate(actor->GetId());
    }
  }

  void SceneEditor::SelectAll()
  {
    BaseScene* scene = GetScene();
    if (!scene) {
      return;
    }
    selection_.clear();
    for (const Actor* actor : GetHierarchyOrder(*scene)) {
      selection_.push_back(actor->GetId());
    }
    selected_ = selection_.empty() ? 0 : selection_.back();
  }

  void SceneEditor::RequestDestroySelection()
  {
    // 子は親と一緒に消える
    for (const Actor* actor : GetTopLevelSelection()) {
      const ActorId id = actor->GetId();
      Request([id](BaseScene& scene) { scene.Destroy(scene.FindActor(id)); });
    }
  }

  void SceneEditor::CopySelection()
  {
    const std::vector<Actor*> roots = GetTopLevelSelection();
    BaseScene*                scene = GetScene();
    if (roots.empty() || !scene) {
      return;
    }
    // 選んだ木を 1 つの配列に並べる（貼るときに、それぞれの根が元の親の下に入る）
    nlohmann::json actors = nlohmann::json::array();
    for (Actor* root : roots) {
      // 範囲 for の式に直接書くと、戻り値の一時オブジェクトがループの前に破棄される
      nlohmann::json saved = SaveActorTree(*root);
      for (nlohmann::json& entry : saved["actors"]) {
        actors.push_back(std::move(entry));
      }
    }
    const nlohmann::json tree = { { "actors", std::move(actors) }, { kClipboardScene, scene->GetSceneName() } };
    ImGui::SetClipboardText(tree.dump().c_str());
  }

  void SceneEditor::PasteClipboard()
  {
    const char* text = ImGui::GetClipboardText();
    if (!text) {
      return;
    }
    nlohmann::json tree = nlohmann::json::parse(text, nullptr, false);
    if (tree.is_discarded() || !tree.is_object() || !tree.contains(kClipboardScene)) {
      return;
    }
    Request([tree = std::move(tree)](BaseScene& scene) {
      // 同じシーンで写した物なら、組の外への親と ActorRef をそのまま使える
      scene.SpawnTree(tree, tree.value(kClipboardScene, std::string{}) == scene.GetSceneName());
    });
  }

  void SceneEditor::DrawActorRow(Actor& actor)
  {
    ImGui::PushID(std::to_string(actor.GetId()).c_str());
    std::string label = actor.GetName();
    if (actor.IsPrefabRoot()) {
      label += "  [" + actor.GetPrefab() + "]";
    } else if (!actor.GetPrefab().empty()) {
      // 組の中に置いた別のプレハブの根
      if (const PrefabLink inner = GetNestedPrefabLink(actor.GetPrefab(), actor.GetPrefabNode()); !inner.prefab.empty() && inner.root == 0) {
        label += "  [" + inner.prefab + "]";
      }
    }
    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick | ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_DefaultOpen;
    if (actor.GetChildren().empty()) {
      flags |= ImGuiTreeNodeFlags_Leaf;
    }
    if (IsSelected(actor.GetId())) {
      flags |= ImGuiTreeNodeFlags_Selected;
    }
    const bool isInactive = !actor.IsActiveInHierarchy();
    if (isInactive) {
      ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
    }
    const bool isOpen = ImGui::TreeNodeEx("##Row", flags, "%s", label.c_str());
    if (isInactive) {
      ImGui::PopStyleColor();
    }
    const ImGuiIO& io          = ImGui::GetIO();
    const bool     isPlainPick = !io.KeyCtrl && !io.KeyShift && IsSelected(actor.GetId()) && selection_.size() > 1;
    if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen()) {
      // 選んである行の素のクリックは、まとめてドラッグする始まりかもしれないので、離すまで選び直さない
      if (isPlainPick) {
        pressedRow_ = actor.GetId();
      } else {
        Select(*actor.GetScene(), actor.GetId(), io.KeyCtrl, io.KeyShift);
      }
    } else if (isPlainPick && pressedRow_ == actor.GetId() && ImGui::IsItemHovered() && ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
      const ImVec2 dragged = ImGui::GetMouseDragDelta(ImGuiMouseButton_Left);
      if (dragged.x == 0.0f && dragged.y == 0.0f) {
        Select(*actor.GetScene(), actor.GetId(), false, false);
      }
    }
    if (ImGui::BeginDragDropSource()) {
      const ActorId id = actor.GetId();
      ImGui::SetDragDropPayload(kActorPayload, &id, sizeof(id));
      ImGui::TextUnformatted(actor.GetName().c_str());
      ImGui::EndDragDropSource();
    }
    if (ImGui::BeginPopupContextItem("ActorMenu")) {
      // 選択の外の行で開いたら、その行だけを選び直す
      if (!IsSelected(actor.GetId())) {
        Select(*actor.GetScene(), actor.GetId(), false, false);
      }
      DrawActorMenu(actor);
      ImGui::EndPopup();
    }
    if (ImGui::BeginDragDropTarget()) {
      AcceptSpawnDrops();
      if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(kActorPayload)) {
        RequestParentDropped(*static_cast<const ActorId*>(payload->Data), actor.GetId());
      }
      ImGui::EndDragDropTarget();
    }
    if (isOpen) {
      for (Actor* child : actor.GetChildren()) {
        DrawActorRow(*child);
      }
      ImGui::TreePop();
    }
    ImGui::PopID();
  }

  void SceneEditor::RequestParent(uint64_t child, uint64_t parent)
  {
    Request([child, parent](BaseScene& scene) {
      Actor* childActor  = scene.FindActor(child);
      Actor* parentActor = parent != 0 ? scene.FindActor(parent) : nullptr;
      if (!childActor || (parent != 0 && !parentActor)) {
        return;
      }
      if (!childActor->SetParent(parentActor)) {
        DebugUIManager::GetInstance()->AddLog("An actor cannot be parented to itself or to one of its children", DebugUIManager::LogType::Warning);
      }
    });
  }

  void SceneEditor::RequestParentDropped(uint64_t dragged, uint64_t parent)
  {
    if (!IsSelected(dragged)) {
      RequestParent(dragged, parent);
      return;
    }
    for (const Actor* actor : GetTopLevelSelection()) {
      if (actor->GetId() != parent) {
        RequestParent(actor->GetId(), parent);
      }
    }
  }

  void SceneEditor::Select(BaseScene& scene, uint64_t id, bool adds, bool extends)
  {
    if (extends && selected_ != 0) {
      const std::vector<Actor*> order = GetHierarchyOrder(scene);
      const auto indexOf = [&order](uint64_t target) {
        return static_cast<size_t>(std::ranges::find(order, target, &Actor::GetId) - order.begin());
      };
      const size_t from  = indexOf(selected_);
      const size_t to    = indexOf(id);
      const size_t first = (std::min)(from, to);
      const size_t last  = (std::min)((std::max)(from, to), order.empty() ? 0 : order.size() - 1);
      if (!adds) {
        selection_.clear();
      }
      for (size_t i = first; i <= last && i < order.size(); ++i) {
        if (!IsSelected(order[i]->GetId())) { selection_.push_back(order[i]->GetId()); }
      }
      selected_ = id;
      return;
    }
    if (!adds) {
      selection_ = { id };
      selected_  = id;
      return;
    }
    if (auto it = std::ranges::find(selection_, id); it != selection_.end()) {
      selection_.erase(it);
      if (selected_ == id) {
        selected_ = selection_.empty() ? 0 : selection_.back();
      }
    } else {
      selection_.push_back(id);
      selected_ = id;
    }
  }

  void SceneEditor::DrawActorMenu(Actor& actor)
  {
    const ActorId id = actor.GetId();
    if (ImGui::MenuItem("Duplicate", "Ctrl+D")) {
      RequestDuplicateSelection();
    }
    ImGui::SetItemTooltip("All selected actors. Children are duplicated too");
    if (ImGui::MenuItem("Copy", "Ctrl+C")) {
      CopySelection();
    }
    if (ImGui::MenuItem("Paste", "Ctrl+V")) {
      PasteClipboard();
    }
    if (ImGui::MenuItem("Delete", "Del")) {
      RequestDestroySelection();
    }
    ImGui::SetItemTooltip("All selected actors. Children are deleted too");

    ImGui::SeparatorText("Hierarchy");
    if (ImGui::MenuItem("Unparent", nullptr, false, actor.GetParent() != nullptr)) {
      RequestParent(id, 0);
    }
    if (ImGui::MenuItem("Move Up")) {
      Request([id](BaseScene& scene) { scene.MoveActor(scene.FindActor(id), -1); });
    }
    if (ImGui::MenuItem("Move Down")) {
      Request([id](BaseScene& scene) { scene.MoveActor(scene.FindActor(id), 1); });
    }
    ImGui::SetItemTooltip("Only this actor. Actors are saved in this order. Drop a row onto another to parent it (all selected rows move together)");

    ImGui::SeparatorText("Prefab");
    if (!actor.GetPrefab().empty()) {
      const std::string prefab = actor.GetPrefab();
      if (ImGui::MenuItem("Apply to Prefab")) {
        Request([id, prefab](BaseScene& scene) { MakePrefab(scene, id, prefab); });
      }
      ImGui::SetItemTooltip("Write this actor group (the prefab root and its children) to %s. Other instances keep their own overrides", prefab.c_str());
      if (ImGui::MenuItem("Revert to Prefab")) {
        Request([id](BaseScene& scene) { RevertToPrefab(scene, id); });
      }
      ImGui::SetItemTooltip("Discard overrides of the whole group, except the root's name, transform and parent");
      if (ImGui::MenuItem("Unpack")) {
        Request([id](BaseScene& scene) { UnpackPrefab(scene, id); });
      }
      ImGui::SetItemTooltip("Cut the link to the prefab and keep the current values. Prefabs placed inside it stay linked to their own prefab");
    }
    ImGui::SetNextItemWidth(140.0f);
    ImGui::InputTextWithHint("##PrefabName", "prefab name", prefabName_.data(), prefabName_.size());
    ImGui::SameLine();
    ImGui::BeginDisabled(prefabName_[0] == '\0');
    ImGui::SetItemTooltip("Children are saved into the prefab too");
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
    // 印で文字が変わっても開閉の状態を保つよう、ID は ### の後ろで固定する
    const std::string header = component.GetName() + "  (" + component.GetType().name + ")" + (IsKept(actor.GetId(), component.GetName()) ? "  [keep]" : "") + "###Header";
    const bool        isOpen = ImGui::CollapsingHeader(header.c_str(), &isKept, ImGuiTreeNodeFlags_DefaultOpen);
    if (ImGui::BeginPopupContextItem("ComponentMenu")) {
      DrawComponentMenu(actor, component);
      ImGui::EndPopup();
    }
    if (isOpen) {
      DrawRequirement(actor, component);
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

  void SceneEditor::DrawComponentMenu(Actor& actor, Component& component)
  {
    if (!IsEditing()) {
      if (ImGui::MenuItem("Keep Values on Stop", nullptr, IsKept(actor.GetId(), component.GetName()))) {
        ToggleKeep(actor, component.GetName());
      }
      ImGui::SetItemTooltip("After Stop, load the values this component has at that moment into the same component. One undo step");
      ImGui::Separator();
    }
    if (ImGui::MenuItem("Copy Values")) {
      const nlohmann::json copied = { { kClipboardComponent, component.GetType().name }, { "properties", WriteProperties(component) } };
      ImGui::SetClipboardText(copied.dump().c_str());
    }
    ImGui::SetItemTooltip("Copy the values to the clipboard (to paste into another component of the same type)");

    const char*          text   = ImGui::GetClipboardText();
    const nlohmann::json copied = text ? nlohmann::json::parse(text, nullptr, false) : nlohmann::json();
    const bool           fits   = copied.is_object() && copied.value(kClipboardComponent, std::string{}) == component.GetType().name;
    if (ImGui::MenuItem("Paste Values", nullptr, false, fits)) {
      const ActorId     id   = actor.GetId();
      const std::string name = component.GetName();
      // Script コンポーネントはスクリプトを作り直すことがあるので、描画の外で読む
      Request([id, name, properties = copied.value("properties", nlohmann::json())](BaseScene& scene) {
        Actor* owner = scene.FindActor(id);
        if (Component* target = owner ? owner->FindComponent(name) : nullptr) {
          ReadProperties(*target, properties);
          target->PostEditChange();
          scene.MarkChanged();
        }
      });
    }
    ImGui::SetItemTooltip("Paste values copied from a component of the same type");
  }

  void SceneEditor::DrawRequirement(Actor& actor, Component& component)
  {
    const TypeInfo* type = &component.GetType();
    if (component.IsA<ScriptComponent>()) {
      if (const Script* script = static_cast<ScriptComponent&>(component).GetScript()) {
        type = &script->GetType();
      }
    }
    for (; type; type = type->base) {
      if (type->required[0] == '\0') {
        continue;
      }
      const TypeInfo* required = TypeRegistry::GetInstance()->Find(type->required);
      if (required && actor.Has(*required)) {
        continue;
      }
      ImGui::TextColored(ImVec4(1.0f, 0.75f, 0.2f, 1.0f), required ? "Needs %s on this actor" : "Needs %s (unknown type)", type->required);
      // スクリプトは Script コンポーネントの中に入れる物なので、ここからは足さない
      if (required && required->create && !required->IsChildOf(Script::StaticType())) {
        ImGui::SameLine();
        ImGui::PushID(type->required);
        if (ImGui::SmallButton("Add")) {
          RequestAddComponent(actor.GetId(), type->required, "");
        }
        ImGui::PopID();
      }
    }
  }

  void SceneEditor::RequestAddComponent(uint64_t actor, const std::string& typeName, const std::string& name)
  {
    Request([actor, typeName, name](BaseScene& scene) {
      Actor*                   owner  = scene.FindActor(actor);
      std::unique_ptr<TObject> object = TypeRegistry::GetInstance()->Create(typeName);
      if (owner && object) {
        owner->AddComponent(std::unique_ptr<Component>(static_cast<Component*>(object.release())), name);
        scene.MarkChanged();
      }
    });
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
      RequestAddComponent(actor.GetId(), type->name, isScript ? "Script" : "");
    }
    ImGui::Separator();
    if (ImGui::MenuItem("New Script...")) {
      RequestNewScriptPopup("Script");
    }
    ImGui::SetItemTooltip("Create a C++ script file. It appears in the Script field after the game is rebuilt");
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

  bool SceneEditor::CanRestoreScene()
  {
    return IsEditing();
  }

  Actor* SceneEditor::GetSelected() const
  {
    BaseScene* scene = GetScene();
    return scene && selected_ != 0 ? scene->FindActor(selected_) : nullptr;
  }

  bool SceneEditor::IsSelected(uint64_t id) const
  {
    return id != 0 && std::ranges::find(selection_, id) != selection_.end();
  }

  std::vector<Actor*> SceneEditor::GetTopLevelSelection() const
  {
    std::vector<Actor*> roots;
    BaseScene*          scene = GetScene();
    if (!scene) {
      return roots;
    }
    for (Actor* actor : GetHierarchyOrder(*scene)) {
      bool isCovered = !IsSelected(actor->GetId());
      for (const Actor* ancestor = actor->GetParent(); ancestor && !isCovered; ancestor = ancestor->GetParent()) {
        isCovered = IsSelected(ancestor->GetId());
      }
      if (!isCovered) {
        roots.push_back(actor);
      }
    }
    return roots;
  }

  uint64_t SceneEditor::GetSceneRevision() const
  {
    BaseScene* scene = GetScene();
    return scene ? scene->GetRevision() : 0;
  }

} // namespace Tako

#endif // _DEBUG

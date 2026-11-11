#include "BaseScene.h"
#include "Actor.h"
#include "Camera.h"
#include "CollisionManager.h"
#include "EmitterManager.h"
#include "ForceFieldManager.h"
#include "GPUParticle.h"
#include "Light.h"
#include "Logger.h"
#include "PostEffectManager.h"
#include "Prefab.h"
#include "Renderer.h"
#include "Script.h"
#include "ScriptComponent.h"
#include "ShadowRenderer.h"
#include "TypeRegistry.h"

#include <json.hpp>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iterator>

namespace Tako {

  namespace {

    constexpr std::string_view kScenesDirectory = "resources/Json/Scenes/";
    constexpr std::string_view kSceneExtension  = ".scene.json";

    /// <summary>
    /// シーンやコンポーネントが変える描画の状態を既定に戻す（前のシーンや Play 中の値を持ち越さない）。
    /// カメラの位置と向きは、コードのシーンもカメラのコンポーネントも毎回決めるので戻さない
    /// </summary>
    void ResetRenderState()
    {
      Renderer* renderer = Renderer::GetInstance();
      if (Camera* camera = *renderer->GetCamera()) {
        camera->SetFovY(Camera::kDefaultFovY);
        camera->SetNearClip(Camera::kDefaultNearClip);
        camera->SetFarClip(Camera::kDefaultFarClip);
      }
      renderer->GetLight()->ResetDirectionalLight();
      ShadowRenderer::GetInstance()->SetMaxShadowDistance(ShadowRenderer::kDefaultMaxShadowDistance);
      PostEffectManager::GetInstance()->ClearEffects();
    }

    /// <summary>
    /// TCLASS(Order = n)。Script コンポーネントは、中のスクリプトの値
    /// </summary>
    int32_t OrderOf(const Component& component)
    {
      if (component.IsA<ScriptComponent>()) {
        if (const Script* script = static_cast<const ScriptComponent&>(component).GetScript()) {
          return script->GetType().order;
        }
      }
      return component.GetType().order;
    }

    /// <summary>
    /// actors のコンポーネントを、呼ぶ順に並べる。Order の小さい順で、同じ値なら Actor の並び、その中ではコンポーネントの並び
    /// </summary>
    template<class Actors>
    std::vector<Component*> OrderedComponents(const Actors& actors)
    {
      std::vector<Component*> components;
      for (const auto& actor : actors) {
        for (const std::unique_ptr<Component>& component : actor->GetComponents()) {
          components.push_back(component.get());
        }
      }
      std::ranges::stable_sort(components, {}, [](const Component* component) { return OrderOf(*component); });
      return components;
    }

  } // namespace

  BaseScene::BaseScene()
    : forceFieldManager_(std::make_unique<ForceFieldManager>(GPUParticle::GetInstance()))
    , emitterManager_(std::make_unique<EmitterManager>(GPUParticle::GetInstance()))
  {
    // シーンプリセットの保存と読み込みで、フォースフィールドも一緒に扱う
    emitterManager_->SetForceFieldManager(forceFieldManager_.get());
  }

  BaseScene::~BaseScene() = default;

  void BaseScene::Start(const std::string& sceneName, const nlohmann::json* snapshot, bool playing)
  {
    sceneName_ = sceneName;
    isPlaying_ = playing;
    ResetRenderState();
    Initialize();

    if (snapshot) {
      Load(*snapshot);
    } else if (HasSceneFile(sceneName_)) {
      try {
        std::ifstream file(GetScenePath(sceneName_));
        Load(nlohmann::json::parse(file));
      } catch (const nlohmann::json::exception& e) {
        Logger::Log("BaseScene: failed to read %s: %s\n", GetScenePath(sceneName_).c_str(), e.what());
      }
    }
    // Load で入れた分を今すぐシーンに入れる（最初のフレームから描くため）
    UpdateAlways();
  }

  void BaseScene::UpdateFrame(float deltaTime)
  {
    // Tick の中でポーズが切り替わっても、このフレームの Tick と LateTick は同じ扱いにする
    const bool isPaused = isPaused_;
    // 外すのも消すのも次のフレームの頭なので、回している間にポインタは無効にならない
    // ponytail: 毎フレーム並べ直す。重くなったら、Actor の出入りとスクリプトの差し替えのときだけ作り直す
    const std::vector<Component*> components = OrderedComponents(actors_);
    const auto ticks = [isPaused](const Component* component) {
      return (!isPaused || component->TickWhilePaused()) && component->GetOwner()->IsActiveInHierarchy();
    };
    for (Component* component : components) {
      if (ticks(component)) {
        component->Tick(deltaTime);
      }
    }
    for (Component* component : components) {
      if (ticks(component)) {
        component->LateTick(deltaTime);
      }
    }
    // 当たり判定は、ポーズに入ったフレームから止める
    if (!isPaused_) {
      Update();
    }
  }

  void BaseScene::UpdateAlways()
  {
    retired_.clear();

    // 親と子を一緒に消すとき、先に消えた方を EndPlay の中で触らないよう、全部の EndPlay を済ませてから消す
    for (Actor* destroyed : pendingDestroys_) {
      if (std::ranges::any_of(actors_, [destroyed](const auto& a) { return a.get() == destroyed; })) {
        destroyed->EndPlay();
      }
    }
    for (Actor* destroyed : pendingDestroys_) {
      if (auto it = std::ranges::find_if(actors_, [destroyed](const auto& a) { return a.get() == destroyed; }); it != actors_.end()) {
        actors_.erase(it);
      } else if (auto spawn = std::ranges::find_if(pendingSpawns_, [destroyed](const auto& a) { return a.get() == destroyed; }); spawn != pendingSpawns_.end()) {
        pendingSpawns_.erase(spawn);
      }
    }
    pendingDestroys_.clear();

    for (const std::unique_ptr<Actor>& actor : actors_) {
      actor->FlushPending();
    }

    // BeginPlay の中で Spawn されても壊れないよう、取り出してから入れる
    std::vector<std::unique_ptr<Actor>> spawns = std::move(pendingSpawns_);
    pendingSpawns_.clear();
    std::vector<Actor*> entered;
    for (std::unique_ptr<Actor>& actor : spawns) {
      entered.push_back(actor.get());
      actors_.push_back(std::move(actor));
    }
    RelinkHierarchy();
    // 全部をシーンに入れて親子を張ってから呼ぶ（BeginPlay の中で、同時に入った他の Actor を ActorRef や親子で引けるように）
    if (isPlaying_) {
      BeginPlayActors(entered);
    }

    // BeginPlay の後に伝える（最初からオフの Actor は、BeginPlay で登録した物をここで止める）
    for (size_t i = 0; i < actors_.size(); ++i) {
      Actor&     actor    = *actors_[i];
      const bool isActive = actor.IsActiveInHierarchy();
      if (isActive == actor.wasActive_) {
        continue;
      }
      actor.wasActive_ = isActive;
      for (size_t j = 0; j < actor.components_.size(); ++j) {
        actor.components_[j]->OnActiveChanged(isActive);
      }
    }
  }

  void BaseScene::BeginPlayActors(const std::vector<Actor*>& actors)
  {
    // この中で足されたコンポーネントは、AddComponent が Awake と BeginPlay を呼ぶ
    const std::vector<Component*> components = OrderedComponents(actors);
    for (Actor* actor : actors) {
      actor->hasAwoken_ = true;
    }
    for (Component* component : components) {
      component->Awake();
    }
    for (Actor* actor : actors) {
      actor->hasBegunPlay_ = true;
    }
    for (Component* component : components) {
      component->BeginPlay();
    }
  }

  void BaseScene::DrawFrame()
  {
    Draw();
    for (const std::unique_ptr<Actor>& actor : actors_) {
      actor->Draw();
    }
  }

  void BaseScene::Shutdown()
  {
    for (const std::unique_ptr<Actor>& actor : actors_) {
      actor->EndPlay();
    }
    actors_.clear();
    pendingSpawns_.clear();
    pendingDestroys_.clear();
    retired_.clear();
    Finalize();
  }

  void BaseScene::BeginPlay()
  {
    isPlaying_ = true;
    std::vector<Actor*> actors;
    for (const std::unique_ptr<Actor>& actor : actors_) {
      actors.push_back(actor.get());
    }
    BeginPlayActors(actors);
  }

  void BaseScene::Update()
  {
    CollisionManager::GetInstance()->CheckAllCollisions();
    emitterManager_->Update();
  }

  Actor* BaseScene::Spawn(std::string_view typeOrPrefab)
  {
    if (const TypeInfo* type = TypeRegistry::GetInstance()->Find(typeOrPrefab); type && type->IsChildOf(Actor::StaticType())) {
      if (std::unique_ptr<TObject> object = TypeRegistry::GetInstance()->Create(typeOrPrefab)) {
        std::unique_ptr<Actor> actor(static_cast<Actor*>(object.release()));
        actor->SetName(std::string(typeOrPrefab));
        return AddPending(std::move(actor));
      }
    }

    const std::vector<ActorId> nodes = GetPrefabNodes(typeOrPrefab);
    ActorIdMap ids;
    for (ActorId node : nodes) {
      ids[node] = nextId_++;
    }
    Actor* root = nullptr;
    for (ActorId node : nodes) {
      nlohmann::json json = ResolvePrefabNode(typeOrPrefab, node, ids, nullptr);
      json["prefab"] = std::string(typeOrPrefab);
      json["node"]   = node;
      if (root) {
        json["root"] = root->GetId();
      }
      if (std::unique_ptr<Actor> actor = CreateActor(json)) {
        Actor* added = AddPending(std::move(actor));
        if (!root) { root = added; }
      }
    }
    if (!root) {
      Logger::Log("BaseScene: \"%.*s\" is neither an Actor type nor a prefab\n", static_cast<int>(typeOrPrefab.size()), typeOrPrefab.data());
    }
    return root;
  }

  Actor* BaseScene::SpawnTree(const nlohmann::json& tree, bool keepsOuterRefs)
  {
    auto found = tree.find("actors");
    if (found == tree.end() || !found->is_array()) {
      return nullptr;
    }
    nlohmann::json entries = *found;
    ActorIdMap     ids;
    for (const nlohmann::json& entry : entries) {
      if (const ActorId id = entry.value("id", ActorId{ 0 }); id != 0) {
        ids[id] = nextId_++;
      }
    }
    RemapActorIds(entries, ids, keepsOuterRefs);
    Actor* root = nullptr;
    for (const nlohmann::json& entry : entries) {
      if (std::unique_ptr<Actor> actor = CreateActor(entry)) {
        Actor* added = AddPending(std::move(actor));
        if (!root) { root = added; }
      }
    }
    RelinkHierarchy();
    return root;
  }

  void BaseScene::Destroy(Actor* actor)
  {
    if (!actor || std::ranges::find(pendingDestroys_, actor) != pendingDestroys_.end()) {
      return;
    }
    pendingDestroys_.push_back(actor);
    MarkChanged();
    for (Actor* child : actor->GetChildren()) {
      Destroy(child);
    }
  }

  void BaseScene::MoveActor(Actor* actor, int direction)
  {
    auto self = std::ranges::find_if(actors_, [actor](const auto& a) { return a.get() == actor; });
    if (self == actors_.end() || direction == 0) {
      return;
    }
    // 同じ親を持つ隣の Actor のすぐ前（後ろ）へ差し込む。入れ替えにしないのは、間にある他の Actor の並びを変えないため
    const auto isSibling = [actor](const std::unique_ptr<Actor>& other) { return other->GetParent() == actor->GetParent(); };
    if (direction < 0) {
      auto previous = std::find_if(std::make_reverse_iterator(self), actors_.rend(), isSibling);
      if (previous == actors_.rend()) { return; }
      std::rotate(std::prev(previous.base()), self, std::next(self));
    } else {
      auto next = std::find_if(std::next(self), actors_.end(), isSibling);
      if (next == actors_.end()) { return; }
      std::rotate(self, std::next(self), std::next(next));
    }
    RelinkHierarchy();
    MarkChanged();
  }

  void BaseScene::RelinkHierarchy()
  {
    for (const auto* list : { &actors_, &pendingSpawns_ }) {
      for (const std::unique_ptr<Actor>& actor : *list) {
        actor->children_.clear();
      }
    }
    for (const auto* list : { &actors_, &pendingSpawns_ }) {
      for (const std::unique_ptr<Actor>& actor : *list) {
        Actor* parent = actor->parentId_ != 0 ? FindActor(actor->parentId_) : nullptr;
        if (parent == actor.get()) {
          parent = nullptr;
        }
        actor->parent_ = parent;
        if (parent) {
          parent->children_.push_back(actor.get());
        } else {
          // 無い ID を残すと、後でその ID をもらった Actor の子になってしまう
          actor->parentId_ = 0;
        }
      }
    }
  }

  nlohmann::json BaseScene::Save()
  {
    nlohmann::json actors = nlohmann::json::array();
    for (const auto* list : { &actors_, &pendingSpawns_ }) {
      for (const std::unique_ptr<Actor>& actor : *list) {
        if (std::ranges::find(pendingDestroys_, actor.get()) == pendingDestroys_.end()) {
          actors.push_back(SaveActorEntry(*actor));
        }
      }
    }
    return { { "actors", CollapsePlacedActors(actors) } };
  }

  void BaseScene::Load(const nlohmann::json& json)
  {
    for (const std::unique_ptr<Actor>& actor : actors_) {
      actor->EndPlay();
    }
    for (auto* list : { &actors_, &pendingSpawns_ }) {
      std::ranges::move(*list, std::back_inserter(retired_));
      list->clear();
    }
    pendingDestroys_.clear();
    nextId_ = 1;

    if (auto it = json.find("actors"); it != json.end() && it->is_array()) {
      // 先に保存された ID を全部取っておき、無い物や重なった物だけ振り直す
      for (const nlohmann::json& entry : *it) {
        nextId_ = (std::max)(nextId_, entry.value("id", ActorId{ 0 }) + 1);
      }
      // プレハブから置いた物を全部の値に開く。プレハブに後から足された Actor には、新しい ID を振る
      const nlohmann::json entries = ExpandPlacedActors(*it, [this](ActorId, ActorId) { return nextId_++; });
      for (const nlohmann::json& entry : entries) {
        if (std::unique_ptr<Actor> actor = CreateActor(entry)) {
          AddPending(std::move(actor));
        }
      }
    }
    RelinkHierarchy();
    MarkChanged();
  }

  bool BaseScene::SaveFile()
  {
    std::filesystem::create_directories(kScenesDirectory);
    std::ofstream file(GetScenePath(sceneName_));
    if (!file) {
      return false;
    }
    file << Save().dump(2);
    return true;
  }

  std::string BaseScene::GetScenePath(std::string_view sceneName)
  {
    return std::string(kScenesDirectory) + std::string(sceneName) + std::string(kSceneExtension);
  }

  bool BaseScene::HasSceneFile(std::string_view sceneName)
  {
    return !sceneName.empty() && std::filesystem::is_regular_file(GetScenePath(sceneName));
  }

  std::vector<std::string> BaseScene::GetSceneFileNames()
  {
    std::vector<std::string> names;
    std::error_code error;
    for (const auto& entry : std::filesystem::directory_iterator(kScenesDirectory, error)) {
      const std::string file = entry.path().filename().string();
      if (entry.is_regular_file() && file.ends_with(kSceneExtension)) {
        names.push_back(file.substr(0, file.size() - kSceneExtension.size()));
      }
    }
    std::ranges::sort(names);
    return names;
  }

  Actor* BaseScene::FindActor(ActorId id) const
  {
    for (const auto* list : { &actors_, &pendingSpawns_ }) {
      for (const std::unique_ptr<Actor>& actor : *list) {
        if (actor->GetId() == id) { return actor.get(); }
      }
    }
    return nullptr;
  }

  Actor* BaseScene::FindActor(std::string_view name) const
  {
    for (const auto* list : { &actors_, &pendingSpawns_ }) {
      for (const std::unique_ptr<Actor>& actor : *list) {
        if (actor->GetName() == name) { return actor.get(); }
      }
    }
    return nullptr;
  }

  std::unique_ptr<Actor> BaseScene::CreateActor(const nlohmann::json& json)
  {
    const std::string        type   = json.value("type", std::string("Actor"));
    std::unique_ptr<TObject> object = TypeRegistry::GetInstance()->Create(type);
    if (!object || !object->IsA<Actor>()) {
      Logger::Log("BaseScene: actor type \"%s\" is unknown and was skipped\n", type.c_str());
      return nullptr;
    }
    std::unique_ptr<Actor> actor(static_cast<Actor*>(object.release()));
    actor->Load(json);
    actor->SetPrefabLink(json.value("prefab", std::string{}), json.value("node", ActorId{ 0 }), json.value("root", ActorId{ 0 }));
    return actor;
  }

  Actor* BaseScene::AddPending(std::unique_ptr<Actor> actor)
  {
    actor->scene_ = this;
    // ID が無いか、他の Actor と重なれば振り直す（複製・貼り付け・プレハブからの生成）
    if (actor->id_ == 0 || FindActor(actor->id_)) {
      actor->id_ = nextId_++;
    } else {
      nextId_ = (std::max)(nextId_, actor->id_ + 1);
    }
    Actor* raw = actor.get();
    pendingSpawns_.push_back(std::move(actor));
    MarkChanged();
    return raw;
  }

} // namespace Tako

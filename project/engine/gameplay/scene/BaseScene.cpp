#include "BaseScene.h"
#include "Actor.h"
#include "Camera.h"
#include "DX12Basic.h"
#include "EmitterManager.h"
#include "ForceFieldManager.h"
#include "FrameProfiler.h"
#include "GPUParticle.h"
#include "Light.h"
#include "Logger.h"
#include "PhysicsWorld.h"
#include "PostEffectManager.h"
#include "Prefab.h"
#include "Renderer.h"
#include "SceneFile.h"
#include "Script.h"
#include "ScriptComponent.h"
#include "ShadowRenderer.h"
#include "TypeRegistry.h"

#include <json.hpp>

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <iterator>
#include <random>
#include <unordered_set>

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

    /// <summary>
    /// component と、Script コンポーネントなら中のスクリプトにも行う（タイマーと受け取る関数は、それぞれが自分で持つ）
    /// </summary>
    template<class Function>
    void ForSelfAndScript(Component& component, Function&& function)
    {
      function(component);
      if (component.IsA<ScriptComponent>()) {
        if (Script* script = static_cast<ScriptComponent&>(component).GetScript()) {
          function(*script);
        }
      }
    }

    /// <summary>
    /// 計測の行に出す型。Script コンポーネントは、中のスクリプトの型
    /// </summary>
    const TypeInfo& ProfiledType(const Component& component)
    {
      if (component.IsA<ScriptComponent>()) {
        if (const Script* script = static_cast<const ScriptComponent&>(component).GetScript()) {
          return script->GetType();
        }
      }
      return component.GetType();
    }

  } // namespace

  BaseScene::BaseScene()
    : forceFieldManager_(std::make_unique<ForceFieldManager>(GPUParticle::GetInstance()))
    , emitterManager_(std::make_unique<EmitterManager>(GPUParticle::GetInstance()))
  {
  }

  BaseScene::~BaseScene() = default;

  void BaseScene::Start(const std::string& sceneName, const nlohmann::json* snapshot, bool playing)
  {
    sceneName_ = sceneName;
    isPlaying_ = playing;
    ResetRenderState();

    if (snapshot) {
      Load(*snapshot);
    } else if (HasSceneFile(sceneName_)) {
      try {
        Load(SceneFile::Read(sceneName_));
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

    using Clock = std::chrono::steady_clock;
    FrameProfiler& profiler    = Renderer::GetInstance()->GetDX12Basic()->GetProfiler();
    const bool     isProfiling = profiler.IsEnabled();

    // 物理は Tick の前に、固定の刻みで進める（Unity の FixedUpdate と同じ順）。ポーズ中は進めず、時間も溜めない
    if (!isPaused) {
      PhysicsWorld*   physics        = PhysicsWorld::GetInstance();
      const uint32_t  physicsScope   = profiler.BeginCpuScope("Physics");
      Clock::duration fixedTickTime{};
      uint32_t        fixedTickCount = 0;
      physics->BeginFrame();
      physicsAccumulator_ += deltaTime;
      while (physicsAccumulator_ >= PhysicsWorld::kFixedDeltaTime) {
        physicsAccumulator_ -= PhysicsWorld::kFixedDeltaTime;
        const Clock::time_point fixedTickStart = isProfiling ? Clock::now() : Clock::time_point{};
        for (Component* component : components) {
          if (ticks(component)) {
            component->FixedTick(PhysicsWorld::kFixedDeltaTime);
            ++fixedTickCount;
          }
        }
        if (isProfiling) {
          fixedTickTime += Clock::now() - fixedTickStart;
        }
        physics->Step();
      }
      physics->EndFrame(physicsAccumulator_ / PhysicsWorld::kFixedDeltaTime);
      // FixedTick・ステップ・通知は交互に走るので区間では囲めず、合計を足す
      const PhysicsWorld::FrameStats& stats = physics->GetFrameStats();
      profiler.AddCpuSample("FixedTick", std::chrono::duration<float, std::milli>(fixedTickTime).count(), fixedTickCount);
      profiler.AddCpuSample("Step", stats.stepMs, stats.stepCount);
      profiler.AddCpuSample("Callbacks", stats.callbackMs, stats.callbackCount);
      profiler.EndScope(physicsScope, stats.stepCount);
    }

    // 型ごとの時間（計測中だけ）。型が入り混じって並ぶので区間では囲めず、1 個ずつ測って型ごとに足す
    struct TypeTime {
      const TypeInfo* type;
      Clock::duration time{};
      uint32_t        count = 0;
    };
    const uint32_t    scope       = profiler.BeginCpuScope("Components");
    Clock::time_point lapStart    = isProfiling ? Clock::now() : Clock::time_point{};
    // スクリプトの DLL を差し替えると TypeInfo のポインタが変わるので、フレームをまたいで持たない
    std::vector<TypeTime> typeTimes;
    // 前の計測の終わりを次の始まりに使い、時刻を読むのを 1 個につき 1 回にする（飛ばした物とループの分は、次の型の時間に入る）
    const auto addLap = [&typeTimes, &lapStart](const Component& component, uint32_t count) {
      const Clock::time_point now   = Clock::now();
      const TypeInfo*         type  = &ProfiledType(component);
      auto                    found = std::ranges::find(typeTimes, type, &TypeTime::type);
      if (found == typeTimes.end()) {
        found = typeTimes.insert(found, TypeTime{ .type = type });
      }
      found->time  += now - lapStart;
      found->count += count;
      lapStart      = now;
    };

    for (Component* component : components) {
      if (ticks(component)) {
        ForSelfAndScript(*component, [deltaTime](Component& target) { target.AdvanceTimers(deltaTime); });
        component->Tick(deltaTime);
        if (isProfiling) {
          addLap(*component, 1);
        }
      }
    }
    for (Component* component : components) {
      if (ticks(component)) {
        component->LateTick(deltaTime);
        if (isProfiling) {
          addLap(*component, 0);
        }
      }
    }

    uint32_t tickedCount = 0;
    for (const TypeTime& typeTime : typeTimes) {
      profiler.AddCpuSample(typeTime.type->name, std::chrono::duration<float, std::milli>(typeTime.time).count(), typeTime.count);
      tickedCount += typeTime.count;
    }
    profiler.EndScope(scope, tickedCount);

    // 一時エミッターの寿命は、ポーズに入ったフレームから止める
    if (!isPaused_) {
      emitterManager_->Update();
    }
  }

  void BaseScene::UpdateAlways()
  {
    retired_.clear();
    issuedIds_.clear();

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
    for (const std::unique_ptr<Actor>& actor : actors_) {
      actor->Draw();
    }
    if (const Camera* camera = *Renderer::GetInstance()->GetCamera()) {
      PhysicsWorld::GetInstance()->DrawDebug(camera->GetTranslate());
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
      ids[node] = NewActorId();
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
        ids[id] = NewActorId();
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

  void BaseScene::SendEvent(std::string_view name, const std::any& payload)
  {
    // ponytail: 全コンポーネントを回す。送る回数や Actor の数で重くなったら、名前ごとの受け取りの表にする
    // 受け取った側がコンポーネントを足しても壊れないよう、添字で回す
    for (size_t i = 0; i < actors_.size(); ++i) {
      const std::vector<std::unique_ptr<Component>>& components = actors_[i]->GetComponents();
      for (size_t j = 0; j < components.size(); ++j) {
        ForSelfAndScript(*components[j], [&](Component& target) { target.HandleEvent(name, payload); });
      }
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

    if (auto it = json.find("actors"); it != json.end() && it->is_array()) {
      std::unordered_set<ActorId> used;
      for (const nlohmann::json& entry : *it) {
        used.insert(entry.value("id", ActorId{ 0 }));
      }
      // プレハブから置いた物を全部の値に開く。プレハブに後から足された Actor の ID は、乱数にせず計算で決める
      // （同じシーンを開いた人ごとに違う ID を保存すると、マージで同じ Actor が 2 件になる）
      const nlohmann::json entries = ExpandPlacedActors(*it, [&used](ActorId root, ActorId node) {
        ActorId id = MakePlacedActorId(root, node);
        while (id == 0 || !used.insert(id).second) {
          id = (id + 1) & 0xffffffffull;
        }
        return id;
      });
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
    return SceneFile::Write(sceneName_, Save());
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

  std::vector<Actor*> BaseScene::FindActorsWithTag(std::string_view tag) const
  {
    std::vector<Actor*> found;
    for (const auto* list : { &actors_, &pendingSpawns_ }) {
      for (const std::unique_ptr<Actor>& actor : *list) {
        if (actor->HasTag(tag)) { found.push_back(actor.get()); }
      }
    }
    return found;
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
      actor->id_ = NewActorId();
    }
    Actor* raw = actor.get();
    pendingSpawns_.push_back(std::move(actor));
    MarkChanged();
    return raw;
  }

  ActorId BaseScene::NewActorId()
  {
    // 連番にしない。別々のブランチで足した Actor の ID（シーンのファイル名になる）が、マージで重ならないように
    static std::mt19937 engine{ std::random_device{}() };
    ActorId             id = 0;
    do {
      id = engine();
    } while (id == 0 || FindActor(id) || std::ranges::find(issuedIds_, id) != issuedIds_.end());
    issuedIds_.push_back(id);
    return id;
  }

} // namespace Tako

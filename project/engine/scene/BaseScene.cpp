#include "BaseScene.h"
#include "Actor.h"
#include "Camera.h"
#include "CollisionManager.h"
#include "EmitterManager.h"
#include "GPUParticle.h"
#include "Light.h"
#include "Logger.h"
#include "PostEffectManager.h"
#include "Prefab.h"
#include "Renderer.h"
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

  } // namespace

  BaseScene::BaseScene()
    : emitterManager_(std::make_unique<EmitterManager>(GPUParticle::GetInstance()))
  {
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
    for (size_t i = 0; i < actors_.size(); ++i) {
      actors_[i]->Tick(deltaTime);
    }
    Update();
  }

  void BaseScene::UpdateAlways()
  {
    retired_.clear();

    for (Actor* destroyed : pendingDestroys_) {
      if (auto it = std::ranges::find_if(actors_, [destroyed](const auto& a) { return a.get() == destroyed; }); it != actors_.end()) {
        (*it)->EndPlay();
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
    for (std::unique_ptr<Actor>& actor : spawns) {
      Actor* raw = actor.get();
      actors_.push_back(std::move(actor));
      if (isPlaying_) {
        raw->BeginPlay();
      }
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
    for (size_t i = 0; i < actors_.size(); ++i) {
      actors_[i]->BeginPlay();
    }
  }

  void BaseScene::Update()
  {
    CollisionManager::GetInstance()->CheckAllCollisions();
    emitterManager_->Update();
  }

  Actor* BaseScene::Spawn(std::string_view typeOrPrefab)
  {
    std::unique_ptr<Actor> actor = CreateActor(typeOrPrefab);
    if (!actor) {
      Logger::Log("BaseScene: \"%.*s\" is neither an Actor type nor a prefab\n", static_cast<int>(typeOrPrefab.size()), typeOrPrefab.data());
      return nullptr;
    }
    return AddPending(std::move(actor));
  }

  Actor* BaseScene::SpawnFromSave(const nlohmann::json& json)
  {
    std::unique_ptr<Actor> actor = CreateActor(json);
    return actor ? AddPending(std::move(actor)) : nullptr;
  }

  void BaseScene::Destroy(Actor* actor)
  {
    if (actor && std::ranges::find(pendingDestroys_, actor) == pendingDestroys_.end()) {
      pendingDestroys_.push_back(actor);
      MarkChanged();
    }
  }

  nlohmann::json BaseScene::Save()
  {
    nlohmann::json actors = nlohmann::json::array();
    for (const auto* list : { &actors_, &pendingSpawns_ }) {
      for (const std::unique_ptr<Actor>& actor : *list) {
        if (std::ranges::find(pendingDestroys_, actor.get()) == pendingDestroys_.end()) {
          actors.push_back(SavePrefabInstance(*actor));
        }
      }
    }
    return { { "actors", std::move(actors) } };
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
      for (const nlohmann::json& entry : *it) {
        if (std::unique_ptr<Actor> actor = CreateActor(entry)) {
          AddPending(std::move(actor));
        }
      }
    }
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
    const nlohmann::json resolved = ResolvePrefabInstance(json);
    const std::string    type     = resolved.value("type", std::string("Actor"));
    std::unique_ptr<TObject> object = TypeRegistry::GetInstance()->Create(type);
    if (!object || !object->IsA<Actor>()) {
      Logger::Log("BaseScene: actor type \"%s\" is unknown and was skipped\n", type.c_str());
      return nullptr;
    }
    std::unique_ptr<Actor> actor(static_cast<Actor*>(object.release()));
    actor->Load(resolved);
    actor->SetPrefab(json.value("prefab", std::string{}));
    return actor;
  }

  std::unique_ptr<Actor> BaseScene::CreateActor(std::string_view typeOrPrefab)
  {
    if (const TypeInfo* type = TypeRegistry::GetInstance()->Find(typeOrPrefab); type && type->IsChildOf(Actor::StaticType())) {
      std::unique_ptr<TObject> object = TypeRegistry::GetInstance()->Create(typeOrPrefab);
      if (object) {
        std::unique_ptr<Actor> actor(static_cast<Actor*>(object.release()));
        actor->SetName(std::string(typeOrPrefab));
        return actor;
      }
    }
    if (HasPrefab(typeOrPrefab)) {
      return CreateActor(nlohmann::json{ { "prefab", std::string(typeOrPrefab) } });
    }
    return nullptr;
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

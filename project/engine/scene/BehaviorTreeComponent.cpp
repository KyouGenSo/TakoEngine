#include "BehaviorTreeComponent.h"
#include "Actor.h"
#include "BTActorNodes.h"
#include "BehaviorTree.h"
#include "Logger.h"

#include <algorithm>
#include <filesystem>

namespace Tako {

  namespace {

    constexpr const char* kTreesDirectory = "resources/Json/BT/";

    std::vector<BehaviorTreeComponent*>& Playing()
    {
      static std::vector<BehaviorTreeComponent*> playing;
      return playing;
    }

  } // namespace

  BehaviorTreeComponent::BehaviorTreeComponent()
    : runtime_(std::make_unique<BehaviorTree>())
  {
  }

  BehaviorTreeComponent::~BehaviorTreeComponent()
  {
    std::erase(Playing(), this);
  }

  void BehaviorTreeComponent::Awake()
  {
    runtime_->GetBlackboard()->SetPtr<Actor>(kBTOwnerKey, GetOwner());
    if (tree_.empty()) {
      return;
    }
    Playing().push_back(this);
    const std::string path = std::string(kTreesDirectory) + tree_ + ".json";
    if (!std::filesystem::is_regular_file(path) || !runtime_->LoadFromJSON(path)) {
      Logger::Log("BehaviorTreeComponent: \"%s\" could not be read\n", path.c_str());
    }
  }

  void BehaviorTreeComponent::Tick(float deltaTime)
  {
    if (autoTick_) {
      runtime_->Tick(deltaTime);
    }
  }

  void BehaviorTreeComponent::EndPlay()
  {
    std::erase(Playing(), this);
    runtime_->SetRootNode(nullptr);
  }

  const std::vector<BehaviorTreeComponent*>& BehaviorTreeComponent::GetPlaying()
  {
    return Playing();
  }

} // namespace Tako

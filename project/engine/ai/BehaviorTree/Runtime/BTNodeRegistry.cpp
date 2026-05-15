#include "BTNodeRegistry.h"
#include "BTSequence.h"
#include "BTSelector.h"
#include "BTRandomSelector.h"
#include "BTParallel.h"

namespace Tako {

  std::unique_ptr<BTNodeRegistry> BTNodeRegistry::instance_ = nullptr;

  BTNodeRegistry* BTNodeRegistry::GetInstance() {
    if (!instance_) {
      instance_ = std::unique_ptr<BTNodeRegistry>(new BTNodeRegistry());
    }
    return instance_.get();
  }

  void BTNodeRegistry::Initialize() {
    // 標準コンポジットを事前登録 (色はエディタ慣習に合わせた仮値)
    RegisterNode<BTSelector>("BTSelector", NodeMeta{
        "Selector",
        NodeCategory::Composite,
        NodeColor(0.7f, 0.5f, 0.3f, 1.0f),  // 茶系
        true
      });
    RegisterNode<BTSequence>("BTSequence", NodeMeta{
        "Sequence",
        NodeCategory::Composite,
        NodeColor(0.3f, 0.5f, 0.7f, 1.0f),  // 青系
        true
      });
    RegisterNode<BTRandomSelector>("BTRandomSelector", NodeMeta{
        "RandomSelector",
        NodeCategory::Composite,
        NodeColor(0.6f, 0.4f, 0.7f, 1.0f),  // 紫系
        true
      });
    RegisterNode<BTParallel>("BTParallel", NodeMeta{
        "Parallel",
        NodeCategory::Composite,
        NodeColor(0.4f, 0.7f, 0.4f, 1.0f),  // 緑系
        true
      });
  }

  void BTNodeRegistry::Finalize() {
    factories_.clear();
    metas_.clear();
  }

  void BTNodeRegistry::RegisterFactory(const std::string& typeName,
    std::function<BTNodePtr()> factory,
    const NodeMeta& meta) {
    factories_[typeName] = std::move(factory);
    metas_[typeName] = meta;
  }

  BTNodePtr BTNodeRegistry::Create(const std::string& typeName) const {
    auto it = factories_.find(typeName);
    if (it == factories_.end()) {
      return nullptr;
    }
    return it->second();
  }

  std::vector<std::string> BTNodeRegistry::GetAllTypes() const {
    std::vector<std::string> result;
    result.reserve(metas_.size());
    for (const auto& [name, meta] : metas_) {
      (void)meta;
      result.push_back(name);
    }
    return result;
  }

  std::vector<std::string> BTNodeRegistry::GetTypesByCategory(NodeCategory category) const {
    std::vector<std::string> result;
    for (const auto& [name, meta] : metas_) {
      if (meta.category == category) {
        result.push_back(name);
      }
    }
    return result;
  }

  const NodeMeta* BTNodeRegistry::GetMeta(const std::string& typeName) const {
    auto it = metas_.find(typeName);
    if (it == metas_.end()) {
      return nullptr;
    }
    return &it->second;
  }

  bool BTNodeRegistry::IsRegistered(const std::string& typeName) const {
    return factories_.find(typeName) != factories_.end();
  }

} // namespace Tako

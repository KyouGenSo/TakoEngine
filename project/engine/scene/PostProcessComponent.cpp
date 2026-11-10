#include "PostProcessComponent.h"
#include "PostEffectManager.h"

namespace Tako {

  void PostProcessComponent::Draw()
  {
    PostEffectManager* manager = PostEffectManager::GetInstance();
    if (profile_.empty() || profile_ == manager->GetLoadedProfile()) {
      return;
    }
    manager->LoadProfile(profile_);
  }

} // namespace Tako

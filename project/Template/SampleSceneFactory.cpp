#include "SampleSceneFactory.h"
#include "SampleScene.h"

#ifdef _DEBUG
#include "DebugUIManager.h"
#endif

using namespace Tako;

std::unique_ptr<Tako::BaseScene> SampleSceneFactory::CreateScene(const std::string& sceneName)
{
  if (sceneName == "sample") {
    return std::make_unique<SampleScene>();
  }

#ifdef _DEBUG
  DebugUIManager::GetInstance()->AddLog("Unknown scene name: " + sceneName, DebugUIManager::LogType::Error);
#endif

  return nullptr;
}
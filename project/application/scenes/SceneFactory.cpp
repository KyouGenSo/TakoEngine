#include "SceneFactory.h"
#include "TitleScene.h"
#include "GameScene.h"
#include "GameOverScene.h"
#include "GameClearScene.h"

BaseScene* SceneFactory::CreateScene(const std::string& sceneName)
{
	BaseScene* newScene = nullptr;

	if (sceneName == "title") {
		newScene = new TitleScene();
	} 
	else if (sceneName == "game") {
		newScene = new GameScene();
	}else if (sceneName == "clear") {
		newScene = new GameClearScene();
	} else if (sceneName == "over") {
		newScene = new GameOverScene();
	}

	return newScene;
}

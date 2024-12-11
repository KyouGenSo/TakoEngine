#pragma once
#include "AbstractSceneFactory.h"

class SceneFactory : public AbstractSceneFactory
{
public: // メンバ関数

	/// <summary>
	/// シーンの生成
	/// </summary>
	BaseScene* CreateScene(const std::string& sceneName) override;

};

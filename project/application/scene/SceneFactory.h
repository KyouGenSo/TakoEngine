#pragma once
#include "AbstractSceneFactory.h"

/// <summary>
/// シーンファクトリークラス
/// 各種シーンの生成を行う
/// </summary>
class SceneFactory : public AbstractSceneFactory
{
public: // メンバ関数

	/// <summary>
	/// シーンの生成
	/// </summary>
	/// <param name="sceneName">生成するシーン名</param>
	/// <returns>生成されたシーンのポインタ</returns>
	BaseScene* CreateScene(const std::string& sceneName) override;

};

#pragma once
#include "BaseScene.h"
#include <string>

class AbstractSceneFactory
{
public: // メンバ関数

	/// <summary>
	/// 仮想デストラクタ
	/// </summary>
	virtual ~AbstractSceneFactory() = default;

	///<summary>
	/// シーンの生成
	///	</summary>
	virtual BaseScene* CreateScene(const std::string& sceneName) = 0;
};
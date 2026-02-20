#pragma once
#include "BaseScene.h"
#include <string>
#include <memory>

namespace Tako {

  /// <summary>
  /// シーン生成のための抽象ファクトリークラス
  /// </summary>
  class AbstractSceneFactory
  {
  public: // メンバ関数

    /// <summary>
    /// 仮想デストラクタ
    /// </summary>
    virtual ~AbstractSceneFactory() = default;

    /// <summary>
    /// シーンの生成
    /// </summary>
    virtual std::unique_ptr<BaseScene> CreateScene(const std::string& sceneName) = 0;
  };

} // namespace Tako
#pragma once
#include "BaseScene.h"
#include <string>
#include <vector>
#include <memory>

namespace Tako {

  /// <summary>
  /// シーン生成のための抽象ファクトリークラス
  /// </summary>
  class AbstractSceneFactory
  {
  public: //メンバー関数

    /// <summary>
    /// 仮想デストラクタ
    /// </summary>
    virtual ~AbstractSceneFactory() = default;

    /// <summary>
    /// シーンの生成
    /// </summary>
    virtual std::unique_ptr<BaseScene> CreateScene(const std::string& sceneName) = 0;

    /// <summary>
    /// CreateScene が受け付けるシーン名の一覧（デバッグ UI のシーン切替に使う）
    /// </summary>
    virtual std::vector<std::string> GetSceneNames() const { return {}; }
  };

} // namespace Tako

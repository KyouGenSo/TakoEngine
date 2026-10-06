#pragma once
#include "AbstractSceneFactory.h"
#include <memory>

/// <summary>
/// シーンファクトリークラス
/// シーン名から対応するシーンインスタンスを生成
/// </summary>
class SampleSceneFactory : public Tako::AbstractSceneFactory
{
public: // メンバ関数

  /// <summary>
  /// シーンの生成
  /// </summary>
  /// <param name="sceneName">生成するシーン名</param>
  /// <returns>生成されたシーンインスタンス（生成失敗時は nullptr）</returns>
  std::unique_ptr<Tako::BaseScene> CreateScene(const std::string& sceneName) override;

  /// <summary>
  /// 登録済みのシーン名（先頭が起動シーンの既定値。Engine Settings のシーン選択にも使う）
  /// </summary>
  std::vector<std::string> GetSceneNames() const override;

};


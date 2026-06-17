#pragma once
#include "AbstractSceneFactory.h"
#include "TransitionManager.h"
#include <memory>

namespace Tako {

  class ITransitionEffect;

  /// <summary>
  /// シーン管理を行うシングルトンクラス。シーンの切り替え、更新、描画を統括
  /// </summary>
  class SceneManager
  {
  private: // シングルトン設定

    static std::unique_ptr<SceneManager> instance_; ///< インスタンス

    SceneManager() = default;
    ~SceneManager() = default;

    friend struct std::default_delete<SceneManager>;

  public:
    SceneManager(const SceneManager&) = delete;
    SceneManager& operator=(const SceneManager&) = delete;

  private:

  public: // メンバ関数

    /// <summary>
    /// インスタンスの取得
    /// </summary>
    static SceneManager* GetInstance();

    /// <summary>
    /// 更新
    /// </summary>
    void Update();

    /// <summary>
    /// 描画
    /// </summary>
    void Draw();

    /// <summary>
    /// エフェクトなしで描画
    /// </summary>
    void DrawWithoutEffect();

    /// <summary>
    /// imgui の描画
    /// </summary>
    void DrawImGui();

    /// <summary>
    /// 終了処理
    /// </summary>
    void Finalize();

    /// <summary>
    /// 次のシーン予約（デフォルトのフェード演出）
    /// </summary>
    /// <param name="sceneName">次のシーン名</param>
    void ChangeScene(const std::string& sceneName);

    /// <summary>
    /// 次のシーン予約（時間指定）
    /// </summary>
    /// <param name="sceneName">次のシーン名</param>
    /// <param name="transitionTime">遷移時間</param>
    void ChangeScene(const std::string& sceneName, float transitionTime);

    /// <summary>
    /// 次のシーン予約（エフェクトタイプ指定）
    /// </summary>
    /// <param name="sceneName">次のシーン名</param>
    /// <param name="effectType">エフェクトタイプ</param>
    /// <param name="transitionTime">遷移時間</param>
    void ChangeScene(const std::string& sceneName,
      TransitionManager::EffectType effectType,
      float transitionTime);

    /// <summary>
    /// 次のシーン予約（エフェクト名指定）
    /// </summary>
    /// <param name="sceneName">次のシーン名</param>
    /// <param name="effectName">エフェクト名</param>
    /// <param name="transitionTime">遷移時間</param>
    void ChangeScene(const std::string& sceneName,
      const std::string& effectName,
      float transitionTime);

    /// <summary>
    /// 次のシーン予約（エフェクトインスタンス直接指定）
    /// </summary>
    /// <param name="sceneName">次のシーン名</param>
    /// <param name="effect">エフェクトインスタンス</param>
    /// <param name="transitionTime">遷移時間</param>
    void ChangeScene(const std::string& sceneName,
      std::unique_ptr<ITransitionEffect> effect,
      float transitionTime);

    /// <summary>
    /// シーンファクトリーの設定
    /// </summary>
    /// <param name="sceneFactory">シーンファクトリーのポインタ</param>
    void SetSceneFactory(AbstractSceneFactory* sceneFactory) { m_sceneFactory_ = sceneFactory; }

  private: // メンバ変数

    std::unique_ptr<BaseScene> scene_; ///< 現在のシーン

    std::unique_ptr<BaseScene> nextScene_; ///< 次のシーン

    AbstractSceneFactory* m_sceneFactory_ = nullptr; ///< シーンファクトリー

    float transitionTime_ = 0.5f; ///< シーン遷移アニメーション時間
  };

} // namespace Tako

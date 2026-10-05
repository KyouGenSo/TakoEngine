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

    struct Token {};  ///< 外部からの直接生成を防ぐ生成キー
    ~SceneManager() = default;

    friend struct std::default_delete<SceneManager>;

  public:
    explicit SceneManager(Token) {}
    SceneManager(const SceneManager&) = delete;
    SceneManager& operator=(const SceneManager&) = delete;

  public: //メンバー関数

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
    /// 次のシーン予約（エフェクトインスタンス直接指定）
    /// </summary>
    /// <param name="sceneName">次のシーン名</param>
    /// <param name="effect">エフェクトインスタンス</param>
    /// <param name="transitionTime">遷移時間</param>
    void ChangeScene(const std::string& sceneName,
      std::unique_ptr<ITransitionEffect> effect,
      float transitionTime);

    //======================================================
    //Setter
    //======================================================
    void SetSceneFactory(AbstractSceneFactory* sceneFactory) { sceneFactory_ = sceneFactory; }

    //======================================================
    //Getter
    //======================================================
    /// <summary>
    /// インスタンスの取得
    /// </summary>
    static SceneManager* GetInstance();

    /// <summary>
    /// シーンファクトリーが生成できるシーン名の一覧（ファクトリー未設定なら空）
    /// </summary>
    std::vector<std::string> GetSceneNames() const {
      return sceneFactory_ ? sceneFactory_->GetSceneNames() : std::vector<std::string>{};
    }

    const std::string& GetCurrentSceneName() const { return currentSceneName_; }

  private: //非公開関数
    /// <summary>
    /// 次シーンを生成して予約する。予約済み・生成失敗なら false（遷移演出は開始しないこと）
    /// </summary>
    bool ReserveNextScene(const std::string& sceneName, float transitionTime);

  private: //メンバー変数

    //シーン
    std::unique_ptr<BaseScene> scene_;             ///< 現在のシーン
    std::unique_ptr<BaseScene> nextScene_;         ///< 次のシーン
    std::string                currentSceneName_;  ///< scene_ の生成に使ったファクトリーのキー
    std::string                nextSceneName_;

    AbstractSceneFactory* sceneFactory_ = nullptr;  ///< シーンファクトリー

    float transitionTime_ = 0.5f;  ///< シーン遷移アニメーション時間
  };

} // namespace Tako

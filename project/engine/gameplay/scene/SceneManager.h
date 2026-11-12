#pragma once
#include "BaseScene.h"
#include "TransitionManager.h"

#include <json_fwd.hpp>

#include <memory>

namespace Tako {

  class ITransitionEffect;

  /// <summary>
  /// シーン管理を行うシングルトンクラス。シーンの切り替え、更新、描画を統括
  /// </summary>
  class TAKO_API SceneManager
  {
  private: //構造体
    enum class PlayRequest { None, Play, Stop };

  private: // シングルトン設定

    static std::unique_ptr<SceneManager> instance_; ///< インスタンス

    struct Token {};  ///< 外部からの直接生成を防ぐ生成キー
    ~SceneManager();

    friend struct std::default_delete<SceneManager>;

  public:
    explicit SceneManager(Token);
    SceneManager(const SceneManager&) = delete;
    SceneManager& operator=(const SceneManager&) = delete;

  public: //メンバー関数

    /// <summary>
    /// 毎フレームの最初に呼ぶ（一時停止中も）。Play / Stop の要求と、シーンの予約された生成・削除をここで行う
    /// </summary>
    void UpdateAlways();

    /// <summary>
    /// 更新。シーンの切り替えと、Play 中のシーンの更新
    /// </summary>
    void Update();

    /// <summary>
    /// 描画
    /// </summary>
    void Draw();

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

    /// <summary>
    /// 起動シーンへ遷移なしで切り替える（上書き → 起動シーン → 一覧の先頭の順に、シーンファイルのある名前を使う）
    /// </summary>
    void ChangeToStartupScene();

    /// <summary>
    /// Edit から Play に入る（次のフレームの頭）。今のシーンの中身を Stop で戻すために取っておく
    /// </summary>
    void RequestPlay();

    /// <summary>
    /// Play をやめて Edit に入る（次のフレームの頭）。Play を始めたシーンを作り直し、Play 前の中身に戻す
    /// </summary>
    void RequestStop();

    /// <summary>
    /// ゲームの DLL を差し替える前に呼ぶ。Play 中なら Stop と同じく Play 前の中身に戻す。作り直すための中身を控えて、シーンを消す
    /// </summary>
    void UnloadSceneForModuleReload();

    /// <summary>
    /// UnloadSceneForModuleReload で消したシーンを、Edit で作り直す
    /// </summary>
    void RestoreSceneAfterModuleReload();

    //======================================================
    //Setter
    //======================================================
    void SetStartupScene(const std::string& sceneName) { startupScene_ = sceneName; }

    /// <summary>
    /// 起動シーンを開発時だけ差し替える（空で無効）
    /// </summary>
    void SetStartupSceneOverride(const std::string& sceneName) { startupSceneOverride_ = sceneName; }

    /// <summary>
    /// 起動シーンを Play ではなく Edit で始める。ChangeToStartupScene の前に呼ぶ
    /// </summary>
    void StartInEdit() { isPlaying_ = false; }

    //======================================================
    //Getter
    //======================================================
    /// <summary>
    /// インスタンスの取得
    /// </summary>
    static SceneManager* GetInstance();

    /// <summary>
    /// resources/Json/Scenes にあるシーンファイルの名前の一覧
    /// </summary>
    std::vector<std::string> GetSceneNames() const;

    BaseScene* GetCurrentScene() const { return scene_.get(); }

    bool IsPlaying() const { return isPlaying_; }

    /// <summary>
    /// 遷移中でなければ true（Stop は遷移中に作り直せない）
    /// </summary>
    bool CanStop() const { return isPlaying_ && !nextScene_; }

    /// <summary>
    /// 遷移中でなければ true（次のシーンを持っている間は、ゲームの DLL を差し替えない）
    /// </summary>
    bool CanReloadModule() const { return !nextScene_; }

    /// <summary>
    /// シーンを替えても残る値（スコア、選んだステージなど）。キーと値はゲームが決める。エディタの Stop で空に戻る
    /// </summary>
    nlohmann::json& GetSessionData();

    const std::string& GetCurrentSceneName() const { return currentSceneName_; }
    const std::string& GetStartupScene() const { return startupScene_; }
    const std::string& GetStartupSceneOverride() const { return startupSceneOverride_; }

  private: //非公開関数
    /// <summary>
    /// 次シーンを生成して予約する。予約済み・生成失敗なら false（遷移演出は開始しないこと）
    /// </summary>
    bool ReserveNextScene(const std::string& sceneName, float transitionTime);

    /// <summary>
    /// シーンファイルがあれば、空のシーンを作る（中身は Start が読む）。無ければ nullptr
    /// </summary>
    std::unique_ptr<BaseScene> CreateScene(const std::string& sceneName) const;

    /// <summary>
    /// 遷移なしでシーンを作り直す（snapshot があればファイルの代わりに読む）
    /// </summary>
    void ReloadSceneImmediate(const std::string& sceneName, const nlohmann::json* snapshot);

  private: //メンバー変数

    //シーン
    std::unique_ptr<BaseScene> scene_;             ///< 現在のシーン
    std::unique_ptr<BaseScene> nextScene_;         ///< 次のシーン
    std::string                currentSceneName_;  ///< scene_ のシーンファイルの名前
    std::string                nextSceneName_;

    float transitionTime_ = 0.5f;  ///< シーン遷移アニメーション時間

    //起動シーン
    std::string startupScene_;          ///< ProjectSettings で共有
    std::string startupSceneOverride_;  ///< 個人設定。非空なら startupScene_ より優先

    //Play / Edit
    bool                            isPlaying_   = true;               ///< 起動直後は Play（Release は常に Play）
    PlayRequest                     playRequest_ = PlayRequest::None;
    std::unique_ptr<nlohmann::json> playSnapshot_;                     ///< Edit から Play に入ったときの中身。Stop で戻す
    std::string                     playSceneName_;                    ///< Play を始めたシーン
    std::unique_ptr<nlohmann::json> sessionData_;                      ///< GetSessionData の中身。最初に使うときに作る

    //ゲームの DLL の差し替え
    std::unique_ptr<nlohmann::json> moduleReloadSnapshot_;  ///< 作り直す中身。空ならシーンファイルから読む
    std::string                     moduleReloadScene_;     ///< 作り直すシーン。空なら、消したシーンは無い
  };

} // namespace Tako

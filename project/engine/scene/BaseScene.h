#pragma once
#include <json_fwd.hpp>

#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace Tako {

  class Actor;
  class EmitterManager;
  class ForceFieldManager;

  using ActorId = uint64_t;

  /// <summary>
  /// 全てのシーンの基底クラス。Actor の入れ物で、シーンファイル（resources/Json/Scenes/名前.scene.json）から中身を読む。
  /// 手書きのシーンは Initialize などのフックを override する
  /// </summary>
  class BaseScene
  {
  public: //メンバー関数
    BaseScene();

    virtual ~BaseScene();

    //=====================================
    //SceneManager が呼ぶ。順番の決まりはここで守る
    //=====================================
    /// <summary>
    /// Initialize → シーンファイル（snapshot があればそれ）を読む → playing なら BeginPlay
    /// </summary>
    /// <param name="sceneName">シーンファイルの名前</param>
    /// <param name="snapshot">Stop で戻す内容（nullptr ならファイル）</param>
    /// <param name="playing">Play 中なら true</param>
    void Start(const std::string& sceneName, const nlohmann::json* snapshot, bool playing);

    /// <summary>
    /// Play 中だけ呼ぶ。全 Actor の Tick → 全 Actor の LateTick → Update。
    /// ポーズ中は TickWhilePaused のコンポーネントだけ動かし、Update（当たり判定など）は呼ばない
    /// </summary>
    void UpdateFrame(float deltaTime);

    /// <summary>
    /// 毎フレームの最初に呼ぶ（Edit 中・一時停止中も）。予約された生成・削除をここで行う（前のフレームの GPU 処理が終わった後なので消してよい）
    /// </summary>
    void UpdateAlways();

    /// <summary>
    /// Draw → 全 Actor を描画に登録
    /// </summary>
    void DrawFrame();

    /// <summary>
    /// 全 Actor の EndPlay と破棄 → Finalize。Actor はここで先に消す（派生シーンのメンバーより後に消えて、消えた物を触らないため）
    /// </summary>
    void Shutdown();

    /// <summary>
    /// Edit から Play に入るとき、全 Actor の BeginPlay を呼ぶ
    /// </summary>
    void BeginPlay();

    /// <summary>
    /// Stop で作り直した後、手書きの物の行列を作るためにフックの Update を 1 回だけ呼ぶ
    /// </summary>
    void UpdateHooksOnce() { Update(); }

    //=====================================
    //Actor
    //=====================================
    /// <summary>
    /// 型名かプレハブ名から Actor を作る。シーンに入るのは次のフレームの頭
    /// </summary>
    /// <returns>作れなければ nullptr</returns>
    Actor* Spawn(std::string_view typeOrPrefab);

    /// <summary>
    /// Save の 1 件から Actor を作る（複製・貼り付け用。ID が重なれば振り直す）。シーンに入るのは次のフレームの頭
    /// </summary>
    Actor* SpawnFromSave(const nlohmann::json& json);

    /// <summary>
    /// 次のフレームの頭で消す。子も一緒に消える
    /// </summary>
    void Destroy(Actor* actor);

    /// <summary>
    /// 同じ親の中で、並びを 1 つ前（direction < 0）か後ろへ動かす。Tick と保存はこの並び順
    /// </summary>
    void MoveActor(Actor* actor, int direction);

    /// <summary>
    /// 全 Actor の親と子のポインタを、保存してある親の ID から張り直す（Actor の出入りと親の変更の後に呼ぶ）
    /// </summary>
    void RelinkHierarchy();

    /// <summary>
    /// 全 Actor を JSON にする（シーンファイルの中身）
    /// </summary>
    nlohmann::json Save();

    /// <summary>
    /// 全 Actor を json の内容で作り直す。いつ呼んでもよい（古い Actor は次のフレームの頭まで残し、描画中の物を消さない）
    /// </summary>
    void Load(const nlohmann::json& json);

    /// <summary>
    /// シーンファイルに書く
    /// </summary>
    bool SaveFile();

    //=====================================
    //Getter
    //=====================================
    static std::string GetScenePath(std::string_view sceneName);

    static bool HasSceneFile(std::string_view sceneName);

    /// <summary>
    /// resources/Json/Scenes にあるシーンファイルの名前
    /// </summary>
    static std::vector<std::string> GetSceneFileNames();

    const std::string& GetSceneName() const { return sceneName_; }

    bool IsPlaying() const { return isPlaying_; }

    bool IsPaused() const { return isPaused_; }

    const std::vector<std::unique_ptr<Actor>>& GetActors() const { return actors_; }

    /// <summary>
    /// このシーンのエミッター（パーティクル）。シーンと一緒に消える
    /// </summary>
    EmitterManager& GetEmitterManager() const { return *emitterManager_; }

    /// <summary>
    /// このシーンのフォースフィールド（パーティクルに掛かる力）。シーンと一緒に消える
    /// </summary>
    ForceFieldManager& GetForceFieldManager() const { return *forceFieldManager_; }

    Actor* FindActor(ActorId id) const;

    Actor* FindActor(std::string_view name) const;

    /// <summary>
    /// Actor の追加・削除・値の変更のたびに増える番号（Undo の比較を省くのに使う）
    /// </summary>
    uint64_t GetRevision() const { return revision_; }

    void MarkChanged() { ++revision_; }

    //=====================================
    //Setter
    //=====================================
    /// <summary>
    /// ゲームの進行を止める（UE の SetGamePaused）。描画は続く
    /// </summary>
    void SetPaused(bool paused) { isPaused_ = paused; }

  protected: //メンバー関数
    friend class SceneManager;

    /// <summary>
    /// 初期化。シーンファイルを読む前に呼ばれる
    /// </summary>
    virtual void Initialize() {}

    /// <summary>
    /// 終了処理。Actor を消した後に呼ばれる
    /// </summary>
    virtual void Finalize() {}

    /// <summary>
    /// 更新。Actor の Tick の後に呼ばれる。既定は CollisionManager の当たり判定と、一時エミッターの寿命（手書きのシーンは自分で呼ぶ）
    /// </summary>
    virtual void Update();

    /// <summary>
    /// 描画物を登録する。描画キューが kQueueOverlay 以上の物（既定のスプライト）はポストエフェクトの後に描かれる
    /// </summary>
    virtual void Draw() {}

    /// <summary>
    /// ImGui の描画
    /// </summary>
    virtual void DrawImGui() {}

  private: //非公開関数
    /// <summary>
    /// JSON 1 件から Actor を作る。ID が無いか重なれば振り直す
    /// </summary>
    std::unique_ptr<Actor> CreateActor(const nlohmann::json& json);

    /// <summary>
    /// 型名かプレハブ名から、まだシーンに入れていない Actor を作る
    /// </summary>
    std::unique_ptr<Actor> CreateActor(std::string_view typeOrPrefab);

    /// <summary>
    /// ID を決めて、次のフレームの頭にシーンへ入る列に足す
    /// </summary>
    Actor* AddPending(std::unique_ptr<Actor> actor);

  private: //メンバー変数
    // Actor のコンポーネントがエミッターを消すので、Actor より先に宣言して後に消す。
    // EmitterManager は ForceFieldManager を指すので、ForceFieldManager をさらに先に宣言する
    std::unique_ptr<ForceFieldManager>  forceFieldManager_;
    std::unique_ptr<EmitterManager>     emitterManager_;
    std::string                         sceneName_;
    std::vector<std::unique_ptr<Actor>> actors_;
    std::vector<std::unique_ptr<Actor>> pendingSpawns_;
    std::vector<Actor*>                 pendingDestroys_;
    std::vector<std::unique_ptr<Actor>> retired_;                    ///< Load で外した Actor。次の UpdateAlways で消す
    ActorId                             nextId_            = 1;
    uint64_t                            revision_          = 0;
    bool                                isPlaying_         = false;
    bool                                isPaused_          = false;
  };

} // namespace Tako

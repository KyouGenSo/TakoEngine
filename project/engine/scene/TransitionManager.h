#pragma once
#include <memory>
#include <string>
#include <unordered_map>
#include <functional>
#include "transition/ITransitionEffect.h"

namespace Tako {

  /// <summary>
  /// トランジション効果の管理クラス
  /// 各種トランジション効果の生成・管理を行う
  /// </summary>
  class TransitionManager
  {
  private: // シングルトン設定
    static std::unique_ptr<TransitionManager> instance_; ///< インスタンス

    TransitionManager() = default;
    ~TransitionManager() = default;

    friend struct std::default_delete<TransitionManager>;

  public:
    TransitionManager(const TransitionManager&) = delete;
    TransitionManager& operator=(const TransitionManager&) = delete;

  private:

  public: // 列挙型
    /// <summary>
    /// 組み込みエフェクトタイプ
    /// </summary>
    enum class EffectType
    {
      Fade,    // フェード演出
      Circle,  // 円形演出
      Custom   // カスタム演出（登録された名前で取得）
    };

  public: // メンバ関数

    /// <summary>
    /// インスタンスの取得
    /// </summary>
    static TransitionManager* GetInstance();

    /// <summary>
    /// 初期化
    /// </summary>
    void Initialize();

    /// <summary>
    /// 終了処理
    /// </summary>
    void Finalize();

    /// <summary>
    /// 更新
    /// </summary>
    void Update();

    /// <summary>
    /// 描画
    /// </summary>
    void Draw();

    /// <summary>
    /// エフェクトタイプからエフェクトを生成
    /// </summary>
    /// <param name="type">エフェクトタイプ</param>
    /// <returns>生成されたエフェクト</returns>
    std::unique_ptr<ITransitionEffect> CreateEffect(EffectType type) const;

    /// <summary>
    /// 名前からエフェクトを生成
    /// </summary>
    /// <param name="effectName">エフェクト名</param>
    /// <returns>生成されたエフェクト</returns>
    std::unique_ptr<ITransitionEffect> CreateEffect(const std::string& effectName) const;

    /// <summary>
    /// カスタムエフェクトの登録
    /// </summary>
    /// <param name="name">エフェクト名</param>
    /// <param name="factory">エフェクト生成関数</param>
    void RegisterCustomEffect(const std::string& name,
      std::function<std::unique_ptr<ITransitionEffect>()> factory);

    /// <summary>
    /// 現在のエフェクトを設定
    /// </summary>
    /// <param name="effect">設定するエフェクト</param>
    void SetCurrentEffect(std::unique_ptr<ITransitionEffect> effect);

    /// <summary>
    /// 現在のエフェクトを設定（タイプ指定）
    /// </summary>
    /// <param name="type">エフェクトタイプ</param>
    void SetCurrentEffect(EffectType type);

    /// <summary>
    /// 現在のエフェクトを設定（名前指定）
    /// </summary>
    /// <param name="effectName">エフェクト名</param>
    void SetCurrentEffect(const std::string& effectName);

    /// <summary>
    /// 現在のエフェクトを取得
    /// </summary>
    /// <returns>現在のエフェクト</returns>
    ITransitionEffect* GetCurrentEffect() const;

    /// <summary>
    /// シーン遷移アニメーション開始
    /// </summary>
    /// <param name="state">遷移状態</param>
    /// <param name="duration">遷移時間</param>
    void Start(ITransitionEffect::TransitionState state, float duration);

    /// <summary>
    /// シーン遷移アニメーション開始（エフェクト指定）
    /// </summary>
    /// <param name="state">遷移状態</param>
    /// <param name="type">エフェクトタイプ</param>
    /// <param name="duration">遷移時間</param>
    void Start(ITransitionEffect::TransitionState state, EffectType type, float duration);

    /// <summary>
    /// シーン遷移アニメーション開始（エフェクト名指定）
    /// </summary>
    /// <param name="state">遷移状態</param>
    /// <param name="effectName">エフェクト名</param>
    /// <param name="duration">遷移時間</param>
    void Start(ITransitionEffect::TransitionState state, const std::string& effectName, float duration);

    /// <summary>
    /// シーン遷移アニメーション中止
    /// </summary>
    void Stop();

    /// <summary>
    /// シーン遷移アニメーションが終了しているか
    /// </summary>
    /// <returns>終了している場合 true</returns>
    bool IsFinished() const;

  private: // メンバ変数

    std::unique_ptr<ITransitionEffect> currentEffect_; ///< 現在使用中のエフェクト

    std::unordered_map<std::string, std::function<std::unique_ptr<ITransitionEffect>()>> effectFactories_; ///< カスタムエフェクトのファクトリ登録用

    EffectType defaultEffectType_ = EffectType::Fade; ///< デフォルトエフェクトタイプ
  };

} // namespace Tako
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

    struct Token {};  ///< 外部からの直接生成を防ぐ生成キー
    ~TransitionManager() = default;

    friend struct std::default_delete<TransitionManager>;

  public:
    explicit TransitionManager(Token) {}
    TransitionManager(const TransitionManager&) = delete;
    TransitionManager& operator=(const TransitionManager&) = delete;

  public: //構造体
    /// <summary>
    /// 組み込みエフェクトタイプ（独自演出は SetCurrentEffect / ChangeScene に ITransitionEffect を直接渡す）
    /// </summary>
    enum class EffectType
    {
      Fade,         // 白フェード
      BlackFade,    // 黒フェード
      ScaleExpand,  // 画面中央から広がる
      ScaleShrink   // 画面中央へ縮む
    };

  public: //メンバー関数

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
    /// シーン遷移アニメーション中止
    /// </summary>
    void Stop();

    //============================================================
    //Setter
    //============================================================
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

    //============================================================
    //Getter
    //============================================================
    /// <summary>
    /// インスタンスの取得
    /// </summary>
    static TransitionManager* GetInstance();

    ITransitionEffect* GetCurrentEffect() const;

    /// <summary>
    /// シーン遷移アニメーションが終了しているか
    /// </summary>
    /// <returns>終了している場合 true</returns>
    bool IsFinished() const;

  private: //メンバー変数

    std::unique_ptr<ITransitionEffect> currentEffect_; ///< 現在使用中のエフェクト

    EffectType defaultEffectType_ = EffectType::Fade; ///< デフォルトエフェクトタイプ
  };

} // namespace Tako
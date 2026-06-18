#pragma once
#include "ITransitionEffect.h"
#include <memory>
#include <string>
#include "Vector2.h"
#include "Vector4.h"

namespace Tako {

  class Sprite;

  /// <summary>
  /// フェードトランジション
  /// 画面全体を指定色でフェードイン/アウトする演出
  /// </summary>
  class FadeTransition : public ITransitionEffect
  {
  public: //メンバー関数
    /// <summary>
    /// デフォルトコンストラクタ（白フェード）
    /// </summary>
    FadeTransition();

    /// <summary>
    /// 色指定コンストラクタ
    /// </summary>
    /// <param name="color">フェード色（RGBA）</param>
    explicit FadeTransition(const Vector4& color);

    /// <summary>
    /// テクスチャ指定コンストラクタ
    /// </summary>
    /// <param name="textureName">使用するテクスチャ名</param>
    explicit FadeTransition(const std::string& textureName);

    /// <summary>
    /// デストラクタ
    /// </summary>
    ~FadeTransition() override = default;

    /// <summary>
    /// 初期化
    /// </summary>
    void Initialize() override;

    /// <summary>
    /// 更新
    /// </summary>
    void Update() override;

    /// <summary>
    /// 描画
    /// </summary>
    void Draw() override;

    /// <summary>
    /// トランジション開始
    /// </summary>
    void Start(TransitionState state, float duration) override;

    /// <summary>
    /// トランジション中止
    /// </summary>
    void Stop() override;

    //=====================================
    //Setter
    //=====================================
    /// <summary>
    /// フェード色の設定
    /// </summary>
    /// <param name="color">新しいフェード色</param>
    void SetColor(const Vector4& color);

    //=====================================
    //Getter
    //=====================================
    /// <summary>
    /// トランジションが終了したか
    /// </summary>
    bool IsFinished() const override;

    TransitionState GetState() const override { return state_; }
    float GetAlpha() const { return alpha_; }

  private: //メンバー変数
    TransitionState state_;  ///< 状態

    //タイミング
    float duration_;
    float transitionTime_;
    float transitionSpeed_;

    //表示
    float       alpha_;
    Vector4     fadeColor_;
    std::string textureName_;

    std::unique_ptr<Sprite> fadeSprite_;  ///< スプライト

    bool isInitialized_;  ///< 初期化フラグ
  };

} // namespace Tako
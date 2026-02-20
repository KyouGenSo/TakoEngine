#pragma once
#include "ITransitionEffect.h"
#include <memory>
#include <string>
#include "Vector2.h"
#include "Vector4.h"

namespace Tako {

  class Sprite;

  /// <summary>
  /// 円形トランジション
  /// 円形のマスクが拡大/縮小しながら画面を覆う演出
  /// </summary>
  class ScaleTransition : public ITransitionEffect
  {
  public:
    /// <summary>
    /// デフォルトコンストラクタ
    /// </summary>
    ScaleTransition();

    /// <summary>
    /// パラメータ指定コンストラクタ
    /// </summary>
    /// <param name="center">円の中心位置（スクリーン座標）</param>
    /// <param name="expandOut">true: 内から外へ拡大, false: 外から内へ縮小</param>
    ScaleTransition(const Vector2& center, bool expandOut = true);

    /// <summary>
    /// 色指定付きコンストラクタ
    /// </summary>
    /// <param name="center">円の中心位置</param>
    /// <param name="color">背景色</param>
    /// <param name="expandOut">拡大方向</param>
    ScaleTransition(const Vector2& center, const Vector4& color, bool expandOut = true);

    /// <summary>
    /// デストラクタ
    /// </summary>
    ~ScaleTransition() override = default;

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

    /// <summary>
    /// トランジションが終了したか
    /// </summary>
    bool IsFinished() const override;

    /// <summary>
    /// 現在の状態を取得
    /// </summary>
    TransitionState GetState() const override { return state_; }

    /// <summary>
    /// 円の中心位置を設定
    /// </summary>
    /// <param name="center">新しい中心位置</param>
    void SetCenter(const Vector2& center);

    /// <summary>
    /// 背景色の設定
    /// </summary>
    /// <param name="color">新しい背景色</param>
    void SetColor(const Vector4& color);

  private:
    // 状態
    TransitionState state_;

    // タイミング
    float duration_;
    float transitionTime_;
    float transitionSpeed_;

    // パラメータ
    Vector2 center_;      // 円の中心位置
    bool expandOut_;      // 拡大方向（true: 内→外, false: 外→内）
    Vector4 color_; // 背景色

    // 現在の値
    float currentRadius_;  // 現在の半径（スケール値として使用）
    float alpha_;         // 現在のアルファ値

    // スプライト
    std::unique_ptr<Sprite> circleSprite_;    // 円形マスク用スプライト

    // 初期化フラグ
    bool isInitialized_;

    // 定数
    static constexpr float MIN_SCALE = 0.01f;  // 最小スケール
    static constexpr float MAX_SCALE = 3.0f;   // 最大スケール（画面全体を覆うため大きめに）
  };

} // namespace Tako
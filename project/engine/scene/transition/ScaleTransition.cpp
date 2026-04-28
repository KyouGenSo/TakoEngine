#include "ScaleTransition.h"
#include "Sprite.h"
#include "SpriteBasic.h"
#include "TextureManager.h"
#include "WinApp.h"
#include <algorithm>
#include <cmath>

namespace Tako {

  ScaleTransition::ScaleTransition()
    : center_(static_cast<float>(WinApp::clientWidth) * 0.5f, static_cast<float>(WinApp::clientHeight) * 0.5f)
    , expandOut_(true)
    , color_(1.0f, 1.0f, 1.0f, 1.0f)
    , state_(NONE)
    , duration_(0.0f)
    , transitionTime_(0.0f)
    , transitionSpeed_(1.0f / 60.0f)
    , currentRadius_(1.0f)
    , alpha_(1.0f)
    , isInitialized_(false)
  {
  }

  ScaleTransition::ScaleTransition(const Vector2& center, bool expandOut)
    : center_(center)
    , expandOut_(expandOut)
    , color_(1.0f, 1.0f, 1.0f, 1.0f)
    , state_(NONE)
    , duration_(0.0f)
    , transitionTime_(0.0f)
    , transitionSpeed_(1.0f / 60.0f)
    , currentRadius_(1.0f)
    , alpha_(1.0f)
    , isInitialized_(false)
  {
  }

  ScaleTransition::ScaleTransition(const Vector2& center, const Vector4& color, bool expandOut)
    : center_(center)
    , expandOut_(expandOut)
    , color_(color)
    , state_(NONE)
    , duration_(0.0f)
    , transitionTime_(0.0f)
    , transitionSpeed_(1.0f / 60.0f)
    , currentRadius_(1.0f)
    , alpha_(1.0f)
    , isInitialized_(false)
  {
  }

  void ScaleTransition::Initialize()
  {
    // テクスチャ読み込み（エンジン提供のデフォルトテクスチャ）
    TextureManager::GetInstance()->LoadEngineDefault("white.png");

    // スプライトの初期化
    circleSprite_ = std::make_unique<Sprite>();
    circleSprite_->Initialize("EngineResources/Texture/white.png");

    // サイズを画面サイズに基づいて設定（最大時に画面全体を覆うサイズ）
    float maxDimension = std::max<float>(static_cast<float>(WinApp::clientWidth),
      static_cast<float>(WinApp::clientHeight));
    circleSprite_->SetSize(Vector2(maxDimension, maxDimension));
    circleSprite_->SetAnchorPoint(Vector2(0.5f, 0.5f));  // 中心を基準点に
    circleSprite_->SetPos(center_);
    circleSprite_->SetColor(color_);
    circleSprite_->SetAlpha(alpha_);

    isInitialized_ = true;
  }

  void ScaleTransition::Update()
  {
    if (!isInitialized_) {
      return;
    }

    float progress = 0.0f;

    switch (state_) {
    case NONE:
      break;

    case FADE_OUT:
      // フェードアウト（演出開始）
      transitionTime_ += transitionSpeed_;
      transitionTime_ = std::min<float>(transitionTime_, duration_);
      progress = transitionTime_ / duration_;

      if (expandOut_) {
        // 内から外へ拡大（円が小さい→大きい）
        currentRadius_ = MIN_SCALE + (MAX_SCALE - MIN_SCALE) * progress;
        alpha_ = progress;  // 背景も徐々に表示
      }
      else {
        // 外から内へ縮小（円が大きい→小さい）
        currentRadius_ = MAX_SCALE - (MAX_SCALE - MIN_SCALE) * progress;
        alpha_ = progress;
      }
      break;

    case FADE_IN:
      // フェードイン（演出終了）
      transitionTime_ -= transitionSpeed_;
      if (transitionTime_ <= 0.0f) {
        transitionTime_ = 0.0f;
        state_ = NONE;
      }
      progress = transitionTime_ / duration_;

      if (expandOut_) {
        // 内から外へ拡大の逆再生（円が大きい→小さい）
        currentRadius_ = MIN_SCALE + (MAX_SCALE - MIN_SCALE) * progress;
        alpha_ = progress;
      }
      else {
        // 外から内へ縮小の逆再生（円が小さい→大きい）
        currentRadius_ = MAX_SCALE - (MAX_SCALE - MIN_SCALE) * progress;
        alpha_ = progress;
      }
      break;
    }

    // ウィンドウサイズに基づいて実際のサイズを計算
    float maxDimension = std::max<float>(static_cast<float>(WinApp::clientWidth),
      static_cast<float>(WinApp::clientHeight));

    // スケール値をピクセル値に変換して円のサイズを更新
    float actualSize = maxDimension * currentRadius_;
    circleSprite_->SetSize(Vector2(actualSize, actualSize));
    circleSprite_->SetPos(center_);

    circleSprite_->Update();
  }

  void ScaleTransition::Draw()
  {
    if (!isInitialized_ || state_ == NONE) {
      return;
    }

    SpriteBasic::GetInstance()->SetCommonRenderSetting();

    // 円を描画
    circleSprite_->Draw();
  }

  void ScaleTransition::Start(TransitionState state, float duration)
  {
    if (!isInitialized_) {
      Initialize();
    }

    state_ = state;
    duration_ = duration;

    if (state_ == FADE_IN) {
      // フェードイン開始（演出の逆再生開始）
      transitionTime_ = duration_;

      if (expandOut_) {
        currentRadius_ = MAX_SCALE;
        alpha_ = 1.0f;
      }
      else {
        currentRadius_ = MIN_SCALE;
        alpha_ = 1.0f;
      }
    }
    else if (state_ == FADE_OUT) {
      // フェードアウト開始（演出開始）
      transitionTime_ = 0.0f;

      if (expandOut_) {
        currentRadius_ = MIN_SCALE;
        alpha_ = 0.0f;
      }
      else {
        currentRadius_ = MAX_SCALE;
        alpha_ = 0.0f;
      }
    }
  }

  void ScaleTransition::Stop()
  {
    state_ = NONE;
  }

  bool ScaleTransition::IsFinished() const
  {
    switch (state_) {
    case FADE_IN:
      return transitionTime_ <= 0.0f;

    case FADE_OUT:
      return transitionTime_ >= duration_;

    case NONE:
    default:
      return true;
    }
  }

  void ScaleTransition::SetCenter(const Vector2& center)
  {
    center_ = center;
    if (circleSprite_) {
      circleSprite_->SetPos(center_);
    }
  }

  void ScaleTransition::SetColor(const Vector4& color)
  {
    color_ = color;
  }

} // namespace Tako
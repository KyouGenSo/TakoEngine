#include "FadeTransition.h"
#include "Sprite.h"
#include "SpriteBasic.h"
#include "TextureManager.h"
#include "WinApp.h"
#include <algorithm>

namespace Tako {

  FadeTransition::FadeTransition()
    : fadeColor_(1.0f, 1.0f, 1.0f, 1.0f)
    , textureName_("EngineResources/Texture/white.dds")
    , state_(NONE)
    , duration_(0.0f)
    , transitionTime_(0.0f)
    , transitionSpeed_(1.0f / 60.0f)
    , alpha_(0.0f)
    , isInitialized_(false)
  {
  }

  FadeTransition::FadeTransition(const Vector4& color)
    : fadeColor_(color)
    , textureName_("EngineResources/Texture/white.dds")
    , state_(NONE)
    , duration_(0.0f)
    , transitionTime_(0.0f)
    , transitionSpeed_(1.0f / 60.0f)
    , alpha_(0.0f)
    , isInitialized_(false)
  {
  }

  FadeTransition::FadeTransition(const std::string& textureName)
    : fadeColor_(1.0f, 1.0f, 1.0f, 1.0f)
    , textureName_(textureName)
    , state_(NONE)
    , duration_(0.0f)
    , transitionTime_(0.0f)
    , transitionSpeed_(1.0f / 60.0f)
    , alpha_(0.0f)
    , isInitialized_(false)
  {
  }

  void FadeTransition::Initialize()
  {
    // テクスチャの読み込み（既に読み込まれていれば何もしない）
    TextureManager::GetInstance()->LoadTexture(textureName_);

    fadeSprite_ = std::make_unique<Sprite>();
    fadeSprite_->Initialize(textureName_);
    fadeSprite_->SetSize(Vector2(static_cast<float>(WinApp::clientWidth),
      static_cast<float>(WinApp::clientHeight)));
    fadeSprite_->SetPos(Vector2(0.0f, 0.0f));
    fadeSprite_->SetColor(fadeColor_);

    isInitialized_ = true;
  }

  void FadeTransition::Update()
  {
    if (!isInitialized_) {
      return;
    }

    switch (state_) {
    case NONE:
      break;

    case FADE_OUT:
      // フェードアウト（0→1）
      transitionTime_ += transitionSpeed_;
      transitionTime_ = std::min<float>(transitionTime_, duration_);
      alpha_ = std::clamp(transitionTime_ / duration_, 0.0f, 1.0f);
      break;

    case FADE_IN:
      // フェードイン（1→0）
      transitionTime_ -= transitionSpeed_;
      if (transitionTime_ <= 0.0f) {
        transitionTime_ = 0.0f;
        state_ = NONE;
      }
      alpha_ = std::clamp(transitionTime_ / duration_, 0.0f, 1.0f);
      break;
    }

    fadeSprite_->SetAlpha(alpha_);
    fadeSprite_->SetSize(Vector2(static_cast<float>(WinApp::clientWidth),
      static_cast<float>(WinApp::clientHeight)));
    fadeSprite_->Update();
  }

  void FadeTransition::Draw()
  {
    if (!isInitialized_ || state_ == NONE) {
      return;
    }

    SpriteBasic::GetInstance()->SetCommonRenderSetting();
    fadeSprite_->Draw();
  }

  void FadeTransition::Start(TransitionState state, float duration)
  {
    if (!isInitialized_) {
      Initialize();
    }

    state_ = state;
    duration_ = duration;

    if (state_ == FADE_IN) {
      // フェードイン開始（完全に表示された状態から始める）
      alpha_ = 1.0f;
      transitionTime_ = duration_;
    }
    else if (state_ == FADE_OUT) {
      // フェードアウト開始（完全に透明な状態から始める）
      alpha_ = 0.0f;
      transitionTime_ = 0.0f;
    }
  }

  void FadeTransition::Stop()
  {
    state_ = NONE;
  }

  bool FadeTransition::IsFinished() const
  {
    switch (state_) {
    case FADE_IN:
      // フェードインは透明になったら終了
      return transitionTime_ <= 0.0f;

    case FADE_OUT:
      // フェードアウトは不透明になったら終了
      return transitionTime_ >= duration_;

    case NONE:
    default:
      return true;
    }
  }

  void FadeTransition::SetColor(const Vector4& color)
  {
    fadeColor_ = color;
    if (fadeSprite_) {
      fadeSprite_->SetColor(fadeColor_);
    }
  }

} // namespace Tako
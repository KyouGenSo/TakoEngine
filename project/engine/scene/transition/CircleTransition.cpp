#include "CircleTransition.h"
#include "Sprite.h"
#include "SpriteBasic.h"
#include "TextureManager.h"
#include "WinApp.h"
#include <algorithm>
#include <cmath>

CircleTransition::CircleTransition()
	: center_(static_cast<float>(WinApp::clientWidth) * 0.5f, static_cast<float>(WinApp::clientHeight) * 0.5f)
	, expandOut_(true)
	, backgroundColor_(0.0f, 0.0f, 0.0f, 1.0f)  // デフォルトは黒背景
	, state_(NONE)
	, duration_(0.0f)
	, transitionTime_(0.0f)
	, transitionSpeed_(1.0f / 60.0f)
	, currentRadius_(1.0f)
	, alpha_(0.0f)
	, isInitialized_(false)
{
}

CircleTransition::CircleTransition(const Vector2& center, bool expandOut)
	: center_(center)
	, expandOut_(expandOut)
	, backgroundColor_(0.0f, 0.0f, 0.0f, 1.0f)
	, state_(NONE)
	, duration_(0.0f)
	, transitionTime_(0.0f)
	, transitionSpeed_(1.0f / 60.0f)
	, currentRadius_(1.0f)
	, alpha_(0.0f)
	, isInitialized_(false)
{
}

CircleTransition::CircleTransition(const Vector2& center, const Vector4& color, bool expandOut)
	: center_(center)
	, expandOut_(expandOut)
	, backgroundColor_(color)
	, state_(NONE)
	, duration_(0.0f)
	, transitionTime_(0.0f)
	, transitionSpeed_(1.0f / 60.0f)
	, currentRadius_(1.0f)
	, alpha_(0.0f)
	, isInitialized_(false)
{
}

void CircleTransition::Initialize()
{
	// 円形マスク用のテクスチャを読み込み（circle.pngが必要）
	TextureManager::GetInstance()->LoadTexture("circle.png");

	// 背景用のテクスチャ
	TextureManager::GetInstance()->LoadTexture("white.png");

	// 円形スプライトの初期化
	circleSprite_ = std::make_unique<Sprite>();
	circleSprite_->Initialize("circle.png");

	// 円のサイズを画面サイズに基づいて設定（最大時に画面全体を覆うサイズ）
	float maxDimension = std::max<float>(static_cast<float>(WinApp::clientWidth),
	                              static_cast<float>(WinApp::clientHeight));
	circleSprite_->SetSize(Vector2(maxDimension, maxDimension));
	circleSprite_->SetAnchorPoint(Vector2(0.5f, 0.5f));  // 中心を基準点に
	circleSprite_->SetPos(center_);

	// 背景スプライトの初期化
	backgroundSprite_ = std::make_unique<Sprite>();
	backgroundSprite_->Initialize("white.png");
	backgroundSprite_->SetSize(Vector2(static_cast<float>(WinApp::clientWidth),
	                                   static_cast<float>(WinApp::clientHeight)));
	backgroundSprite_->SetPos(Vector2(0.0f, 0.0f));
	backgroundSprite_->SetColor(backgroundColor_);

	isInitialized_ = true;
}

void CircleTransition::Update()
{
	if (!isInitialized_)
	{
		return;
	}

	float progress = 0.0f;

	switch (state_)
	{
	case NONE:
		break;

	case FADE_OUT:
		// フェードアウト（演出開始）
		transitionTime_ += transitionSpeed_;
		transitionTime_ = std::min<float>(transitionTime_, duration_);
		progress = transitionTime_ / duration_;

		if (expandOut_)
		{
			// 内から外へ拡大（円が小さい→大きい）
			currentRadius_ = MIN_SCALE + (MAX_SCALE - MIN_SCALE) * progress;
			alpha_ = progress;  // 背景も徐々に表示
		}
		else
		{
			// 外から内へ縮小（円が大きい→小さい）
			currentRadius_ = MAX_SCALE - (MAX_SCALE - MIN_SCALE) * progress;
			alpha_ = progress;
		}
		break;

	case FADE_IN:
		// フェードイン（演出終了）
		transitionTime_ -= transitionSpeed_;
		if (transitionTime_ <= 0.0f)
		{
			transitionTime_ = 0.0f;
			state_ = NONE;
		}
		progress = transitionTime_ / duration_;

		if (expandOut_)
		{
			// 内から外へ拡大の逆再生（円が大きい→小さい）
			currentRadius_ = MIN_SCALE + (MAX_SCALE - MIN_SCALE) * progress;
			alpha_ = progress;
		}
		else
		{
			// 外から内へ縮小の逆再生（円が小さい→大きい）
			currentRadius_ = MAX_SCALE - (MAX_SCALE - MIN_SCALE) * progress;
			alpha_ = progress;
		}
		break;
	}

	// スプライトの更新
	circleSprite_->SetSize(Vector2(currentRadius_, currentRadius_));
	circleSprite_->SetPos(center_);

	// expandOutの場合、円が小さいときは背景を表示、大きいときは円を不透明に
	// !expandOutの場合、円が大きいときは背景を隠し、小さいときは背景を表示
	if (expandOut_)
	{
		circleSprite_->SetColor(backgroundColor_);
		circleSprite_->SetAlpha(1.0f);
		backgroundSprite_->SetAlpha(0.0f);  // expandOutでは背景は使わない
	}
	else
	{
		circleSprite_->SetColor(Vector4(0, 0, 0, 1));  // 縮小時は黒い円でマスク
		circleSprite_->SetAlpha(1.0f);
		backgroundSprite_->SetAlpha(alpha_);
	}

	// ウィンドウサイズに追従
	float maxDimension = std::max<float>(static_cast<float>(WinApp::clientWidth),
	                              static_cast<float>(WinApp::clientHeight));
	circleSprite_->SetSize(Vector2(maxDimension, maxDimension));
	backgroundSprite_->SetSize(Vector2(static_cast<float>(WinApp::clientWidth),
	                                   static_cast<float>(WinApp::clientHeight)));

	circleSprite_->Update();
	backgroundSprite_->Update();
}

void CircleTransition::Draw()
{
	if (!isInitialized_ || state_ == NONE)
	{
		return;
	}

	SpriteBasic::GetInstance()->SetCommonRenderSetting();

	// 演出タイプによって描画順を変える
	if (!expandOut_)
	{
		// 縮小タイプの場合、先に背景を描画
		backgroundSprite_->Draw();
	}

	// 円を描画
	circleSprite_->Draw();
}

void CircleTransition::Start(TransitionState state, float duration)
{
	if (!isInitialized_)
	{
		Initialize();
	}

	state_ = state;
	duration_ = duration;

	if (state_ == FADE_IN)
	{
		// フェードイン開始（演出の逆再生開始）
		transitionTime_ = duration_;

		if (expandOut_)
		{
			currentRadius_ = MAX_SCALE;
			alpha_ = 1.0f;
		}
		else
		{
			currentRadius_ = MIN_SCALE;
			alpha_ = 1.0f;
		}
	}
	else if (state_ == FADE_OUT)
	{
		// フェードアウト開始（演出開始）
		transitionTime_ = 0.0f;

		if (expandOut_)
		{
			currentRadius_ = MIN_SCALE;
			alpha_ = 0.0f;
		}
		else
		{
			currentRadius_ = MAX_SCALE;
			alpha_ = 0.0f;
		}
	}
}

void CircleTransition::Stop()
{
	state_ = NONE;
}

bool CircleTransition::IsFinished() const
{
	switch (state_)
	{
	case FADE_IN:
		return transitionTime_ <= 0.0f;

	case FADE_OUT:
		return transitionTime_ >= duration_;

	case NONE:
	default:
		return true;
	}
}

void CircleTransition::SetCenter(const Vector2& center)
{
	center_ = center;
	if (circleSprite_)
	{
		circleSprite_->SetPos(center_);
	}
}

void CircleTransition::SetBackgroundColor(const Vector4& color)
{
	backgroundColor_ = color;
	if (backgroundSprite_)
	{
		backgroundSprite_->SetColor(backgroundColor_);
	}
	if (circleSprite_ && expandOut_)
	{
		circleSprite_->SetColor(backgroundColor_);
	}
}
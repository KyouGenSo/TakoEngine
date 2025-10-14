#include "Transition.h"
#include "Draw2D.h"
#include "WinApp.h"
#include <algorithm>
#include "Sprite.h"
#include "SpriteBasic.h"
#include "TextureManager.h"
#include "TransitionManager.h"
#include "transition/ITransitionEffect.h"

Transition* Transition::instance_ = nullptr;

Transition* Transition::GetInstance()
{
	if (instance_ == nullptr)
	{
		instance_ = new Transition();
	}
	return instance_;
}

void Transition::Initialize()
{
	// TransitionManagerの初期化に移行
	TransitionManager::GetInstance()->Initialize();

	// 後方互換性のための初期値設定
	type_ = FADE;
	state_ = NONE;
	duration_ = 0.0f;
	transitionTime_ = 0.0f;
	transitionSpeed_ = 1.0f / 60.0f;
	alpha_ = 0.0f;

	// 旧システムのスプライト（使用されない予定だが念のため初期化）
	TextureManager::GetInstance()->LoadTexture("white.png");
	blackBoxsp_ = std::make_unique<Sprite>();
	blackBoxsp_->Initialize("white.png");
	blackBoxsp_->SetSize(Vector2(static_cast<float>(WinApp::clientWidth), static_cast<float>(WinApp::clientHeight)));
	blackBoxsp_->SetPos(Vector2(0.0f, 0.0f));
}

void Transition::Finalize()
{
	TransitionManager::GetInstance()->Finalize();

	if (instance_ != nullptr)
	{
		delete instance_;
		instance_ = nullptr;
	}
}

void Transition::Update()
{
	// TransitionManagerに処理を委譲
	TransitionManager::GetInstance()->Update();

	// 後方互換性のための状態同期
	auto* currentEffect = TransitionManager::GetInstance()->GetCurrentEffect();
	if (currentEffect)
	{
		// TransitionManagerの状態をTransitionの状態に反映
		auto effectState = currentEffect->GetState();
		switch (effectState)
		{
		case ITransitionEffect::NONE:
			state_ = NONE;
			break;
		case ITransitionEffect::FADE_IN:
			state_ = FADE_IN;
			break;
		case ITransitionEffect::FADE_OUT:
			state_ = FADE_OUT;
			break;
		}
	}
}

void Transition::Start(TransitionState state, TransitionType type, float duration)
{
	// 内部状態を更新
	state_ = state;
	type_ = type;
	duration_ = duration;

	// TransitionManagerに処理を委譲
	// TransitionStateをITransitionEffect::TransitionStateに変換
	ITransitionEffect::TransitionState effectState;
	switch (state)
	{
	case FADE_IN:
		effectState = ITransitionEffect::FADE_IN;
		break;
	case FADE_OUT:
		effectState = ITransitionEffect::FADE_OUT;
		break;
	default:
		effectState = ITransitionEffect::NONE;
		break;
	}

	// タイプに応じてエフェクトを設定
	if (type == FADE)
	{
		TransitionManager::GetInstance()->Start(effectState, TransitionManager::EffectType::Fade, duration);
	}
	else if (type == SLIDE)
	{
		// SLIDEは未実装なので、デフォルトでFadeを使用
		TransitionManager::GetInstance()->Start(effectState, TransitionManager::EffectType::Fade, duration);
	}
}

void Transition::Stop()
{
	state_ = NONE;
	// TransitionManagerにも停止を伝える
	TransitionManager::GetInstance()->Stop();
}

bool Transition::IsFinished()
{
	// TransitionManagerに処理を委譲
	return TransitionManager::GetInstance()->IsFinished();
}

void Transition::Draw()
{
	// TransitionManagerに処理を委譲
	TransitionManager::GetInstance()->Draw();
}

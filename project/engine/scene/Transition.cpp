#include "Transition.h"
#include "Draw2D.h"
#include "WinApp.h"
#include <algorithm>

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
	type_ = FADE;

	state_ = NONE;

	duration_ = 0.0f;

	transitionTime_ = 0.0f;

	transitionSpeed_ = 1.0f / 60.0f;

	alpha_ = 0.0f;
}

void Transition::Finalize()
{
	if (instance_ != nullptr)
	{
		delete instance_;
		instance_ = nullptr;
	}
}

void Transition::Update()
{
	if (type_ == FADE) {
		switch (state_)
		{
		case NONE:
			break;

		case FADE_OUT:
			transitionTime_ += transitionSpeed_;

			if (transitionTime_ >= duration_)
			{
				transitionTime_ = duration_;
			}

			alpha_ = std::clamp(transitionTime_ / duration_, 0.0f, 1.0f);
			break;

		case FADE_IN:
			transitionTime_ -= transitionSpeed_;

			if (transitionTime_ <= 0.0f)
			{
				transitionTime_ = 0.0f;
				state_ = NONE;
			}

			alpha_ = std::clamp(transitionTime_ / duration_, 0.0f, 1.0f);
			break;
		}
	}
}

void Transition::Start(TransitionState state, TransitionType type, float duration)
{
	state_ = state;

	type_ = type;

	duration_ = duration;

	if (type_ == FADE)
	{
		if (state_ == FADE_IN)
		{
			alpha_ = 1.0f;
			transitionTime_ = duration_;
		} else if (state_ == FADE_OUT)
		{
			alpha_ = 0.0f;
			transitionTime_ = 0.0f;
		}
	}
}

void Transition::Stop()
{
	state_ = NONE;
}

bool Transition::IsFinished()
{
	switch (state_)
	{
	case FADE_IN:
	case FADE_OUT:
		if (transitionTime_ >= duration_)
		{
			return true;
		} else {
			return false;
		}
	}

	return true;
}

void Transition::Draw()
{
	if (state_ == NONE) {
		return;
	}
	Draw2D::GetInstance()->DrawBox(Vector2(0.0f, 0.0f), Vector2(WinApp::kClientWidth, WinApp::kClientHeight), Vector4(0.0f, 0.0f, 0.0f, alpha_));
}

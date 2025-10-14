#include "TransitionManager.h"
#include "transition/FadeTransition.h"
#include "transition/CircleTransition.h"
#include <cassert>

TransitionManager* TransitionManager::instance_ = nullptr;

TransitionManager* TransitionManager::GetInstance()
{
	if (instance_ == nullptr)
	{
		instance_ = new TransitionManager();
	}
	return instance_;
}

void TransitionManager::Initialize()
{
	// デフォルトエフェクトの設定（Fade）
	currentEffect_ = CreateEffect(defaultEffectType_);
	if (currentEffect_)
	{
		currentEffect_->Initialize();
	}

	// 組み込みエフェクトのファクトリを登録
	RegisterCustomEffect("Fade", []() {
		return std::make_unique<FadeTransition>();
	});

	RegisterCustomEffect("Circle", []() {
		return std::make_unique<CircleTransition>();
	});

	// よく使うバリエーションも登録しておく
	RegisterCustomEffect("BlackFade", []() {
		return std::make_unique<FadeTransition>(Vector4(0, 0, 0, 1));
	});

	RegisterCustomEffect("WhiteFade", []() {
		return std::make_unique<FadeTransition>(Vector4(1, 1, 1, 1));
	});

	RegisterCustomEffect("CircleCenter", []() {
		return std::make_unique<CircleTransition>();
	});

	RegisterCustomEffect("CircleExpand", []() {
		return std::make_unique<CircleTransition>(Vector2(960, 540), true);
	});

	RegisterCustomEffect("CircleShrink", []() {
		return std::make_unique<CircleTransition>(Vector2(960, 540), false);
	});
}

void TransitionManager::Finalize()
{
	currentEffect_.reset();

	if (instance_ != nullptr)
	{
		delete instance_;
		instance_ = nullptr;
	}
}

void TransitionManager::Update()
{
	if (currentEffect_)
	{
		currentEffect_->Update();
	}
}

void TransitionManager::Draw()
{
	if (currentEffect_)
	{
		currentEffect_->Draw();
	}
}

std::unique_ptr<ITransitionEffect> TransitionManager::CreateEffect(EffectType type) const
{
	switch (type)
	{
	case EffectType::Fade:
		return std::make_unique<FadeTransition>();

	case EffectType::Circle:
		return std::make_unique<CircleTransition>();

	case EffectType::Custom:
	default:
		// カスタムの場合はnullptrを返す（名前指定で取得してもらう）
		return nullptr;
	}
}

std::unique_ptr<ITransitionEffect> TransitionManager::CreateEffect(const std::string& effectName) const
{
	auto it = effectFactories_.find(effectName);
	if (it != effectFactories_.end())
	{
		return it->second();
	}

	// 見つからない場合はデフォルトのFadeを返す
	return std::make_unique<FadeTransition>();
}

void TransitionManager::RegisterCustomEffect(const std::string& name,
                                            std::function<std::unique_ptr<ITransitionEffect>()> factory)
{
	effectFactories_[name] = factory;
}

void TransitionManager::SetCurrentEffect(std::unique_ptr<ITransitionEffect> effect)
{
	if (effect)
	{
		currentEffect_ = std::move(effect);
		currentEffect_->Initialize();
	}
}

void TransitionManager::SetCurrentEffect(EffectType type)
{
	auto effect = CreateEffect(type);
	if (effect)
	{
		SetCurrentEffect(std::move(effect));
	}
}

void TransitionManager::SetCurrentEffect(const std::string& effectName)
{
	auto effect = CreateEffect(effectName);
	if (effect)
	{
		SetCurrentEffect(std::move(effect));
	}
}

ITransitionEffect* TransitionManager::GetCurrentEffect() const
{
	return currentEffect_.get();
}

void TransitionManager::Start(ITransitionEffect::TransitionState state, float duration)
{
	if (currentEffect_)
	{
		currentEffect_->Start(state, duration);
	}
}

void TransitionManager::Start(ITransitionEffect::TransitionState state, EffectType type, float duration)
{
	// 新しいエフェクトを設定してから開始
	SetCurrentEffect(type);
	if (currentEffect_)
	{
		currentEffect_->Start(state, duration);
	}
}

void TransitionManager::Start(ITransitionEffect::TransitionState state, const std::string& effectName, float duration)
{
	// 新しいエフェクトを設定してから開始
	SetCurrentEffect(effectName);
	if (currentEffect_)
	{
		currentEffect_->Start(state, duration);
	}
}

void TransitionManager::Stop()
{
	if (currentEffect_)
	{
		currentEffect_->Stop();
	}
}

bool TransitionManager::IsFinished() const
{
	if (currentEffect_)
	{
		return currentEffect_->IsFinished();
	}
	return true;
}
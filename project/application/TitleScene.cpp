#include "TitleScene.h"
#include "SceneManager.h"
#include "TextureManager.h"
#include "Object3dBasic.h"
#include "SpriteBasic.h"
#include"ModelManager.h"
#include "ParticleManager.h"
#include "Input.h"
#include "Draw2D.h"
#include "Camera.h"

#ifdef _DEBUG
#include"ImGui.h"
#include "DebugCamera.h"
#endif

void TitleScene::Initialize()
{
#ifdef _DEBUG
	DebugCamera::GetInstance()->Initialize();
#endif

	/// ================================== ///
	///              初期化処理              ///
	/// ================================== ///

	// エミッターの設定
	emitterParam_.name_ = "circle";
	emitterParam_.transform_.translate = Vector3(-1.0f, 0.0f, 0.0f);
	emitterParam_.transform_.scale = Vector3(0.1f, 0.1f, 0.1f);
	emitterParam_.velocity_ = Vector3(0.0f, 1.0f, 0.0f);
	emitterParam_.range_.min = Vector3(-0.5f, -0.5f, -0.5f);
	emitterParam_.range_.max = Vector3(0.5f, 0.5f, 0.5f);
	emitterParam_.color_ = Vector4(1.0f, 1.0f, 1.0f, 1.0f);
	emitterParam_.count_ = 1;
	emitterParam_.lifeTime = 1.0f;
	emitterParam_.frequency_ = 0.1f;
	emitterParam_.isRandomColor_ = false;
	emitterParam_.isVisualize_ = true;

	particleEmitter_ = std::make_unique<ParticleEmitter>(emitterParam_.name_, emitterParam_.transform_, emitterParam_.velocity_, emitterParam_.range_, emitterParam_.lifeTime, emitterParam_.count_, emitterParam2_.color_, emitterParam_.frequency_, emitterParam_.isRandomColor_);

	// エミッターの設定2
	emitterParam2_.name_ = "white";
	emitterParam2_.transform_.translate = Vector3(1.0f, 0.0f, 0.0f);
	emitterParam2_.transform_.scale = Vector3(0.1f, 0.1f, 0.1f);
	emitterParam2_.velocity_ = Vector3(0.0f, 1.0f, 0.0f);
	emitterParam2_.range_.min = Vector3(-0.5f, -0.5f, -0.5f);
	emitterParam2_.range_.max = Vector3(0.5f, 0.5f, 0.5f);
	emitterParam2_.color_ = Vector4(1.0f, 1.0f, 1.0f, 1.0f);
	emitterParam2_.count_ = 1;
	emitterParam2_.lifeTime = 1.0f;
	emitterParam2_.frequency_ = 0.1f;
	emitterParam2_.isRandomColor_ = false;
	emitterParam2_.isVisualize_ = true;

	particleEmitter2_ = std::make_unique<ParticleEmitter>(emitterParam2_.name_, emitterParam2_.transform_, emitterParam2_.velocity_, emitterParam2_.range_, emitterParam2_.lifeTime, emitterParam2_.count_, emitterParam2_.color_, emitterParam2_.frequency_, emitterParam2_.isRandomColor_);
}

void TitleScene::Finalize()
{
}

void TitleScene::Update()
{
#ifdef _DEBUG
	if (Input::GetInstance()->TriggerKey(DIK_F1)) {
		Object3dBasic::GetInstance()->SetDebug(!Object3dBasic::GetInstance()->GetDebug());
		Draw2D::GetInstance()->SetDebug(!Draw2D::GetInstance()->GetDebug());
		isDebug_ = !isDebug_;
	}

	if (isDebug_) {
		DebugCamera::GetInstance()->Update();
	}
#endif
	/// ================================== ///
	///              更新処理               ///
	/// ================================== ///

	particleEmitter_->SetIsVisualize(emitterParam_.isVisualize_);
	particleEmitter_->SetTranslate(emitterParam_.transform_.translate);
	particleEmitter_->SetScale(emitterParam_.transform_.scale);
	particleEmitter_->SetFrequency(emitterParam_.frequency_);
	particleEmitter_->SetCount(emitterParam_.count_);
	particleEmitter_->SetVelocity(emitterParam_.velocity_);
	particleEmitter_->SetRange(emitterParam_.range_);
	particleEmitter_->SetColor(emitterParam_.color_);
	particleEmitter_->SetLifeTime(emitterParam_.lifeTime);
	particleEmitter_->SetIsRandomColor(emitterParam_.isRandomColor_);

	particleEmitter_->Update();

	particleEmitter2_->SetIsVisualize(emitterParam2_.isVisualize_);
	particleEmitter2_->SetTranslate(emitterParam2_.transform_.translate);
	particleEmitter2_->SetScale(emitterParam2_.transform_.scale);
	particleEmitter2_->SetFrequency(emitterParam2_.frequency_);
	particleEmitter2_->SetCount(emitterParam2_.count_);
	particleEmitter2_->SetVelocity(emitterParam2_.velocity_);
	particleEmitter2_->SetRange(emitterParam2_.range_);
	particleEmitter2_->SetColor(emitterParam2_.color_);
	particleEmitter2_->SetLifeTime(emitterParam2_.lifeTime);
	particleEmitter2_->SetIsRandomColor(emitterParam2_.isRandomColor_);

	particleEmitter2_->Update();


	if (Input::GetInstance()->TriggerKey(DIK_RETURN))
	{
		SceneManager::GetInstance()->ChangeScene("game");
	}
}

void TitleScene::Draw()
{
	/// ================================== ///
	///              描画処理               ///
	/// ================================== ///
	//------------------背景Spriteの描画------------------//
	// スプライト共通描画設定
	SpriteBasic::GetInstance()->SetCommonRenderSetting();



	//--------------------------------------------------//


	//-------------------Modelの描画-------------------//
	// 3Dモデル共通描画設定
	Object3dBasic::GetInstance()->SetCommonRenderSetting();


	//------------------------------------------------//


	//------------------前景Spriteの描画------------------//
	// スプライト共通描画設定
	SpriteBasic::GetInstance()->SetCommonRenderSetting();



	//--------------------------------------------------//

	particleEmitter_->Draw();
	particleEmitter2_->Draw();
}

void TitleScene::DrawImGui()
{
#ifdef _DEBUG

	/// ================================== ///
	///             ImGuiの描画              ///
	/// ================================== ///

	ImGui::Begin("EmitterParam1");
	ImGui::DragFloat3("translate", &emitterParam_.transform_.translate.x, 0.1f);
	ImGui::DragFloat3("scale", &emitterParam_.transform_.scale.x, 0.1f);
	ImGui::DragFloat3("velocity", &emitterParam_.velocity_.x, 0.1f);
	ImGui::DragFloat3("range min", &emitterParam_.range_.min.x, 0.1f);
	ImGui::DragFloat3("range max", &emitterParam_.range_.max.x, 0.1f);
	ImGui::DragFloat("lifeTime", &emitterParam_.lifeTime, 0.1f);
	ImGui::DragInt("count", &emitterParam_.count_, 1, 1, 1024);
	ImGui::DragFloat("frequency", &emitterParam_.frequency_, 0.1f);
	ImGui::Checkbox("isRandomColor", &emitterParam_.isRandomColor_);
	ImGui::Checkbox("isVisualize", &emitterParam_.isVisualize_);
	// color picker
	ImGui::ColorEdit4("color", &emitterParam_.color_.x);
	ImGui::End();

	ImGui::Begin("EmitterParam2");
	ImGui::DragFloat3("translate", &emitterParam2_.transform_.translate.x, 0.1f);
	ImGui::DragFloat3("scale", &emitterParam2_.transform_.scale.x, 0.1f);
	ImGui::DragFloat3("velocity", &emitterParam2_.velocity_.x, 0.1f);
	ImGui::DragFloat3("range min", &emitterParam2_.range_.min.x, 0.1f);
	ImGui::DragFloat3("range max", &emitterParam2_.range_.max.x, 0.1f);
	ImGui::DragFloat("lifeTime", &emitterParam2_.lifeTime, 0.1f);
	ImGui::DragInt("count", &emitterParam2_.count_, 1, 1, 1024);
	ImGui::DragFloat("frequency", &emitterParam2_.frequency_, 0.1f);
	ImGui::Checkbox("isRandomColor", &emitterParam2_.isRandomColor_);
	ImGui::Checkbox("isVisualize", &emitterParam2_.isVisualize_);
	// color picker
	ImGui::ColorEdit4("color", &emitterParam2_.color_.x);
	ImGui::End();

#endif // _DEBUG
}

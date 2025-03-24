#include "GameScene.h"
#include "ModelManager.h"
#include "Object3dBasic.h"
#include "TextureManager.h"
#include "SpriteBasic.h"
#include "Input.h"
#include "DebugCamera.h"
#include "Draw2D.h"
#include "GlobalVariables.h"
#include "ParticleManager.h"
#include "FrameTimer.h"
#include "GPUParticle.h"
#include "SceneManager.h"
#include "SphereEmitter.h"
#include "BoxEmitter.h"

#include <numbers>

#ifdef _DEBUG
#include"ImGui.h"
#include "DebugCamera.h"
#endif

void GameScene::Initialize()
{
#ifdef _DEBUG
	DebugCamera::GetInstance()->Initialize();
  Object3dBasic::GetInstance()->SetDebug(false);
  Draw2D::GetInstance()->SetDebug(false);
  ParticleManager::GetInstance()->SetIsDebug(false);
  GPUParticle::GetInstance()->SetIsDebug(false);
#endif
	/// ================================== ///
	///              初期化処理              ///
	/// ================================== ///

	object3d_ = new Object3d();
	object3d_->Initialize();
	object3d_->SetModel("terrain.obj");
	// y軸90度回転
  Vector3 rotate = { 0.0f, DirectX::XMConvertToRadians(90.0f), 0.0f };
	object3d_->SetRotate(rotate);

  modelPos_ = { 0.0f, 0.0f, 0.0f };

	object3d2_ = new Object3d();
	object3d2_->Initialize();
	object3d2_->SetModel("sneakWalk.gltf");
  modelRotate2_ = { 0.0f, DirectX::XMConvertToRadians(180.0f), 0.0f };
	object3d2_->SetRotate(rotate);

  modelPos2_ = { 0.0f, 6.7f, -24.0f };

  GPUParticle* particleSystem = GPUParticle::GetInstance();

  emitterManager_ = std::make_unique<EmitterManager>(particleSystem);

  // 球体エミッターの作成
  sphereEmitter_ = emitterManager_->CreateSphereEmitter("player", { 0.0f, 100.0f, 0.0f }, 10.0f, 10, 1.0f);
  sphereEmitter_->SetColor({ 1.0f, 1.0f, 1.0f, 1.0f });
  sphereEmitter_->SetActive(true);

  // 箱エミッターの作成
  boxEmitter_ = emitterManager_->CreateBoxEmitter("box", { 0.0f, 0.0f, 0.0f }, { 1.0f, 1.0f, 1.0f }, { 0.0f, 0.0f, 0.0f }, 10, 0.1f);
  boxEmitter_->SetColor({ 1.0f, 1.0f, 1.0f, 1.0f });
  boxEmitter_->SetActive(true);

}

void GameScene::Finalize()
{
	delete object3d_;
	delete object3d2_;
}

void GameScene::Update()
{
#ifdef _DEBUG
	if (Input::GetInstance()->TriggerKey(DIK_F1))
	{
		Object3dBasic::GetInstance()->SetDebug(!Object3dBasic::GetInstance()->GetDebug());
		Draw2D::GetInstance()->SetDebug(!Draw2D::GetInstance()->GetDebug());
    GPUParticle::GetInstance()->SetIsDebug(!GPUParticle::GetInstance()->GetIsDebug());
		isDebug_ = !isDebug_;
	}

	if (isDebug_)
	{
		DebugCamera::GetInstance()->Update();
	}
#endif
	/// ================================== ///
	///              更新処理               ///
	/// ================================== ///

	object3d_->SetScale(modelScale_);
	object3d_->SetTranslate(modelPos_);
	object3d_->SetRotate(modelRotate_);
	object3d_->SetShininess(shininess_);
	object3d_->SetEnableLighting(isLighting_);
	object3d_->SetEnableHighlight(isHighlight_);
  object3d_->SetMaterialColor(materialColor1_);

	object3d2_->SetScale(modelScale2_);
	object3d2_->SetTranslate(modelPos2_);
	object3d2_->SetRotate(modelRotate2_);
	object3d2_->SetShininess(shininess_);
	object3d2_->SetEnableLighting(isLighting_);
	object3d2_->SetEnableHighlight(isHighlight_);
  object3d2_->SetMaterialColor(materialColor2_);

	object3d_->Update();
	object3d2_->Update();

  emitterManager_->Update();

	// ライトの設定
	Object3dBasic::GetInstance()->SetDirectionalLight(lightDirection_, lightColor_, 1, lightIntensity_);


	// シーン遷移
	if (Input::GetInstance()->TriggerKey(DIK_RETURN))
	{
		SceneManager::GetInstance()->ChangeScene("title");
	}
}

void GameScene::Draw()
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

	// モデル描画
	object3d_->Draw();
  object3d2_->Draw();

  

	//-------------------Modelの描画-------------------//


	//------------------前景Spriteの描画------------------//
	// スプライト共通描画設定
	SpriteBasic::GetInstance()->SetCommonRenderSetting();



	//--------------------------------------------------//
}

void GameScene::DrawImGui()
{
#ifdef _DEBUG
	ImGui::Begin("object3d");
	ImGui::DragFloat3("Scale", &modelScale_.x, 0.01f, 0.1f, 50.0f);
	ImGui::DragFloat3("Position", &modelPos_.x, 0.01f, -50.0f, 50.0f);
	ImGui::DragFloat3("Rotate", &modelRotate_.x, 0.01f, DirectX::XMConvertToRadians(-180.0f), DirectX::XMConvertToRadians(180.0f));
  ImGui::ColorEdit4("MaterialColor1", &materialColor1_.x);
	ImGui::End();

	ImGui::Begin("object3d2");
	ImGui::DragFloat3("Scale", &modelScale2_.x, 0.01f, 0.1f, 50.0f);
	ImGui::DragFloat3("Position", &modelPos2_.x, 0.01f, -50.0f, 50.0f);
	ImGui::DragFloat3("Rotate", &modelRotate2_.x, 0.01f, DirectX::XMConvertToRadians(-180.0f), DirectX::XMConvertToRadians(180.0f));
  ImGui::ColorEdit4("MaterialColor2", &materialColor2_.x);
	ImGui::End();

	// Lightの設定
	ImGui::Begin("Directional Light");
	ImGui::Separator();
	ImGui::DragFloat3("Direction", &lightDirection_.x, 0.01f, -1.0f, 1.0f);
	ImGui::DragFloat("Intensity", &lightIntensity_, 0.01f, 0.0f, 10.0f);
	ImGui::SliderFloat("Shininess", &shininess_, 1.0f, 1000.0f);
	ImGui::ColorEdit4("Color", &lightColor_.x);
	ImGui::Checkbox("Lighting", &isLighting_);
	ImGui::Checkbox("Highlight", &isHighlight_);
	ImGui::End();

  ImGui::Begin("GameTime");
  ImGui::Text("GameTime : %f", FrameTimer::GetInstance()->GetGameTime());
  ImGui::End();

#endif // DEBUG
}

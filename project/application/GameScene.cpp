#include "GameScene.h"
#include "SceneManager.h"
#include"Audio.h"
#include"ModelManager.h"
#include"Object3dBasic.h"
#include"TextureManager.h"
#include"SpriteBasic.h"
#include"Input.h"
#include "DebugCamera.h"

#ifdef _DEBUG
#include"ImGui.h"
#include "DebugCamera.h"
#endif

void GameScene::Initialize()
{
#ifdef _DEBUG
	DebugCamera::GetInstance()->Initialize();
	DebugCamera::GetInstance()->Set3D();
#endif
	/// ================================== ///
	///              初期化処理              ///
	/// ================================== ///

	ModelManager::GetInstance()->LoadModel("terrain.obj");

	object3d_ = new Object3d();
	object3d_->Initialize();
	object3d_->SetModel("terrain.obj");
	// y軸90度回転
	Vector3 rotate = Vector3(0.0f, DirectX::XMConvertToRadians(90.0f), 0.0f);
	object3d_->SetRotate(rotate);

	modelPos_ = Vector3(0.0f, -7.37f, 22.28f);
}

void GameScene::Finalize()
{
	delete object3d_;
}

void GameScene::Update()
{
#ifdef _DEBUG
	if (Input::GetInstance()->TriggerKey(DIK_F1))
	{
		Object3dBasic::GetInstance()->SetDebug(!Object3dBasic::GetInstance()->GetDebug());
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


	object3d_->Update();


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
	object3d_->SetScale(modelScale_);
	object3d_->SetTranslate(modelPos_);
	// Lightの設定
	ImGui::Text("Directional Light");
	ImGui::Separator();
	ImGui::DragFloat3("Direction", &lightDirection_.x, 0.01f, -1.0f, 1.0f);
	ImGui::DragFloat("Intensity", &lightIntensity_, 0.01f, 0.0f, 10.0f);
	ImGui::SliderFloat("Shininess", &shininess_, 1.0f, 1000.0f);
	ImGui::ColorEdit4("Color", &lightColor_.x);
	ImGui::Checkbox("Lighting", &isLighting_);
	ImGui::Checkbox("Highlight", &isHighlight_);
	object3d_->SetShininess(shininess_);
	object3d_->SetEnableLighting(isLighting_);
	object3d_->SetEnableHighlight(isHighlight_);
	object3d_->SetLightDirection(lightDirection_);
	object3d_->SetLightColor(lightColor_);
	object3d_->SetLightIntensity(lightIntensity_);
	ImGui::End();

	ImGui::Begin("Point Light");
	ImGui::DragFloat3("Position", &pointLightPos_.x, 0.01f, -50.0f, 50.0f);
	ImGui::DragFloat("Intensity", &pointLightIntensity_, 0.01f, 0.0f, 10.0f);
	ImGui::DragFloat("Radius", &pointLightRadius_, 0.01f, 0.0f, 100.0f);
	ImGui::DragFloat("Decay", &pointLightDecay_, 0.01f, 0.0f, 10.0f);
	ImGui::ColorEdit4("Color", &pointLightColor_.x);
	ImGui::Checkbox("Enable", &isPointLightEnable_);
	object3d_->SetPointLightPosition(pointLightPos_);
	object3d_->SetPointLightColor(pointLightColor_);
	object3d_->SetPointLightIntensity(pointLightIntensity_);
	object3d_->SetPointLightRadius(pointLightRadius_);
	object3d_->SetPointLightDecay(pointLightDecay_);
	object3d_->SetPointLightEnable(isPointLightEnable_);
	ImGui::End();

#endif // DEBUG
}

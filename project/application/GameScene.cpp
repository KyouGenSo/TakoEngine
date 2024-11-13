#include "GameScene.h"
#include "SceneManager.h"
#include"Audio.h"
#include"ModelManager.h"
#include"Object3dBasic.h"
#include"TextureManager.h"
#include"SpriteBasic.h"
#include"Input.h"
#include "DebugCamera.h"
#include <numbers>

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

	object3d2_ = new Object3d();
	object3d2_->Initialize();
	object3d2_->SetModel("terrain.obj");

	// y軸90度回転
	Vector3 rotate2 = Vector3(0.0f, DirectX::XMConvertToRadians(90.0f), 0.0f);
	object3d2_->SetRotate(rotate2);

	spotLight_.color = Vector4(1.0f, 1.0f, 1.0f, 1.0f);
	spotLight_.position = Vector3(0.0f, -6.0f, 22.0f);
	spotLight_.intensity = 1.0f;
	spotLight_.direction = Vector3(-1.0f, -1.0f, 0.0f);
	spotLight_.distance = 7.0f;
	spotLight_.decay = 1.0f;
	spotLight_.cosAngle = std::cos(std::numbers::pi_v<float> / 3.0f);
	spotLight_.enable = true;
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
	object3d2_->Update();


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
	object3d_->SetScale(modelScale_);
	object3d_->SetTranslate(modelPos_);
	object3d_->SetRotate(modelRotate_);
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
	object3d2_->SetShininess(shininess_);
	object3d2_->SetEnableLighting(isLighting_);
	object3d2_->SetEnableHighlight(isHighlight_);
	object3d2_->SetLightDirection(lightDirection_);
	object3d2_->SetLightColor(lightColor_);
	object3d2_->SetLightIntensity(lightIntensity_);
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
	object3d2_->SetPointLightPosition(pointLightPos_);
	object3d2_->SetPointLightColor(pointLightColor_);
	object3d2_->SetPointLightIntensity(pointLightIntensity_);
	object3d2_->SetPointLightRadius(pointLightRadius_);
	object3d2_->SetPointLightDecay(pointLightDecay_);
	object3d2_->SetPointLightEnable(isPointLightEnable_);
	ImGui::End();

	ImGui::Begin("Spot Light");
	ImGui::DragFloat3("Position", &spotLight_.position.x, 0.01f, -50.0f, 50.0f);
	ImGui::DragFloat3("Direction", &spotLight_.direction.x, 0.01f, -1.0f, 1.0f);
	ImGui::DragFloat("Intensity", &spotLight_.intensity, 0.01f, 0.0f, 10.0f);
	ImGui::DragFloat("Distance", &spotLight_.distance, 0.01f, 0.0f, 100.0f);
	ImGui::DragFloat("Decay", &spotLight_.decay, 0.01f, 0.0f, 10.0f);
	ImGui::DragFloat("CosAngle", &spotLight_.cosAngle, 0.01f, 0.0f, 1.0f);
	ImGui::ColorEdit4("Color", &spotLight_.color.x);
	ImGui::Checkbox("Enable", &spotLight_.enable);
	object3d_->SetSpotLightPosition(spotLight_.position);
	object3d_->SetSpotLightDirection(spotLight_.direction);
	object3d_->SetSpotLightIntensity(spotLight_.intensity);
	object3d_->SetSpotLightDistance(spotLight_.distance);
	object3d_->SetSpotLightDecay(spotLight_.decay);
	object3d_->SetSpotLightCosAngle(spotLight_.cosAngle);
	object3d_->SetSpotLightColor(spotLight_.color);
	object3d_->SetSpotLightEnable(spotLight_.enable);
	object3d2_->SetSpotLightPosition(spotLight_.position);
	object3d2_->SetSpotLightDirection(spotLight_.direction);
	object3d2_->SetSpotLightIntensity(spotLight_.intensity);
	object3d2_->SetSpotLightDistance(spotLight_.distance);
	object3d2_->SetSpotLightDecay(spotLight_.decay);
	object3d2_->SetSpotLightCosAngle(spotLight_.cosAngle);
	object3d2_->SetSpotLightColor(spotLight_.color);
	object3d2_->SetSpotLightEnable(spotLight_.enable);
	ImGui::End();
#endif // DEBUG
}

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
#endif
	/// ================================== ///
	///              初期化処理              ///
	/// ================================== ///

	ModelManager::GetInstance()->LoadModel("terrain.obj");
	ModelManager::GetInstance()->LoadModel("AnimatedCube.gltf", true); 
	ModelManager::GetInstance()->LoadModel("uvChecker.gltf");
	ModelManager::GetInstance()->LoadModel("walk.gltf", true, true);
	ModelManager::GetInstance()->LoadModel("simpleSkin.gltf", true, true);

	object3d_ = new Object3d();
	object3d_->Initialize();
	object3d_->SetModel("terrain.obj");
	// y軸90度回転
	Vector3 rotate = Vector3(0.0f, DirectX::XMConvertToRadians(90.0f), 0.0f);
	object3d_->SetRotate(rotate);

	modelPos_ = Vector3(0.0f, -7.37f, 22.28f);

	object3d2_ = new Object3d();
	object3d2_->Initialize();
	object3d2_->SetModel("walk.gltf");

	//modelPos2_ = Vector3(0.0f, 1.87f, -6.39f);
	//modelRotate2_ = Vector3(1.65f, 0.0f, 0.0f);

	spotLight_.color = Vector4(1.0f, 1.0f, 1.0f, 1.0f);
	spotLight_.position = Vector3(0.0f, -6.0f, 22.0f);
	spotLight_.intensity = 1.0f;
	spotLight_.direction = Vector3(-1.0f, -1.0f, 0.0f);
	spotLight_.distance = 7.0f;
	spotLight_.decay = 1.0f;
	spotLight_.cosAngle = std::cos(std::numbers::pi_v<float> / 3.0f);
	spotLight_.enable = true;

	pointLight_.position = Vector3(0.0f, -5.37f, 22.0f); // ライトの位置
	pointLight_.color = { 1.0f, 1.0f, 1.0f, 1.0f };     // ライトの色
	pointLight_.intensity = 1.0f;                       // 輝度
	pointLight_.radius = 10.0f;                         // 半径
	pointLight_.decay = 1.0f;                           // 減衰
	pointLight_.enable = false;                         // 点光源の有効無効

	pointLight2_.position = Vector3(0.0f, -5.37f, 22.0f); // ライトの位置
	pointLight2_.color = { 1.0f, 1.0f, 1.0f, 1.0f };     // ライトの色
	pointLight2_.intensity = 1.0f;                       // 輝度
	pointLight2_.radius = 10.0f;                         // 半径
	pointLight2_.decay = 1.0f;                           // 減衰
	pointLight2_.enable = false;                         // 点光源の有効無効
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

	object3d_->SetScale(modelScale_);
	object3d_->SetTranslate(modelPos_);
	object3d_->SetRotate(modelRotate_);
	object3d_->SetShininess(shininess_);
	object3d_->SetEnableLighting(isLighting_);
	object3d_->SetEnableHighlight(isHighlight_);

	object3d2_->SetScale(modelScale2_);
	object3d2_->SetTranslate(modelPos2_);
	object3d2_->SetRotate(modelRotate2_);
	object3d2_->SetShininess(shininess_);
	object3d2_->SetEnableLighting(isLighting_);
	object3d2_->SetEnableHighlight(isHighlight_);

	object3d_->Update();
	object3d2_->Update();

	// ライトの設定
	Object3dBasic::GetInstance()->SetDirectionalLight(lightDirection_, lightColor_, 1, lightIntensity_);
	Object3dBasic::GetInstance()->SetSpotLight(spotLight_.position, spotLight_.direction, spotLight_.color, spotLight_.intensity, spotLight_.distance, spotLight_.decay, spotLight_.cosAngle, spotLight_.enable);
	Object3dBasic::GetInstance()->SetPointLight(pointLight_.position, pointLight_.color, pointLight_.intensity, pointLight_.radius, pointLight_.decay, pointLight_.enable, 0);
	Object3dBasic::GetInstance()->SetPointLight(pointLight2_.position, pointLight2_.color, pointLight2_.intensity, pointLight2_.radius, pointLight2_.decay, pointLight2_.enable, 1);

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
	ImGui::End();

	ImGui::Begin("object3d2");
	ImGui::DragFloat3("Scale", &modelScale2_.x, 0.01f, 0.1f, 50.0f);
	ImGui::DragFloat3("Position", &modelPos2_.x, 0.01f, -50.0f, 50.0f);
	ImGui::DragFloat3("Rotate", &modelRotate2_.x, 0.01f, DirectX::XMConvertToRadians(-180.0f), DirectX::XMConvertToRadians(180.0f));
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

	ImGui::Begin("Point Light");
	ImGui::Text("Point Light1");
	ImGui::DragFloat3("Position", &pointLight_.position.x, 0.01f, -50.0f, 50.0f);
	ImGui::DragFloat("Intensity", &pointLight_.intensity, 0.01f, 0.0f, 10.0f);
	ImGui::DragFloat("Radius", &pointLight_.radius, 0.01f, 0.0f, 100.0f);
	ImGui::DragFloat("Decay", &pointLight_.decay, 0.01f, 0.0f, 10.0f);
	ImGui::ColorEdit4("Color", &pointLight_.color.x);
	ImGui::Checkbox("Enable", &pointLight_.enable);
	ImGui::End();

	ImGui::Begin("Point Light2");
	ImGui::Text("Point Light2");
	ImGui::DragFloat3("Position", &pointLight2_.position.x, 0.01f, -50.0f, 50.0f);
	ImGui::DragFloat("Intensity", &pointLight2_.intensity, 0.01f, 0.0f, 10.0f);
	ImGui::DragFloat("Radius", &pointLight2_.radius, 0.01f, 0.0f, 100.0f);
	ImGui::DragFloat("Decay", &pointLight2_.decay, 0.01f, 0.0f, 10.0f);
	ImGui::ColorEdit4("Color", &pointLight2_.color.x);
	ImGui::Checkbox("Enable", &pointLight2_.enable);
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
	ImGui::End();
#endif // DEBUG
}

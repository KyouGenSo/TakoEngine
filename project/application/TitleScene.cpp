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
#include <numbers>


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
	ModelManager::GetInstance()->LoadModel("plane.obj", false);
	ModelManager::GetInstance()->LoadModel("uvChecker.gltf", false);
	ModelManager::GetInstance()->LoadModel("terrain.obj", false);
	ModelManager::GetInstance()->LoadModel("ball.obj", false);
	ModelManager::GetInstance()->LoadModel("Animation_Node/Animation_Node_00.gltf", true);
	ModelManager::GetInstance()->LoadModel("Animation_Node/Animation_Node_01.gltf", true);
	ModelManager::GetInstance()->LoadModel("Animation_Node/Animation_Node_02.gltf", true);

	camera_ = new Camera();
	camera_->SetTranslate(cameraPos_);
	camera_->SetRotate(cameraRot_);

	Object3dBasic::GetInstance()->SetCamera(camera_);
	Draw2D::GetInstance()->SetCamera(camera_);

	ball_ = new Object3d();
	ball_->Initialize();
	ball_->SetModel("ball.obj");

	plane_ = new Object3d();
	plane_->Initialize();
	plane_->SetModel("plane.obj");
	planePos_ = Vector3(0.0f, 2.f, -11.8f);
	planeRotate_ = Vector3(0.0f, DirectX::XMConvertToRadians(180.0f), 0.0f);

	plane2_ = new Object3d();
	plane2_->Initialize();
	plane2_->SetModel("uvChecker.gltf");
	plane2Pos_ = Vector3(-5.3f, 2.f, -11.8f);
	plane2Rotate_ = Vector3(0.0f, DirectX::XMConvertToRadians(180.0f), 0.0f);

	terrain_ = new Object3d();
	terrain_->Initialize();
	terrain_->SetModel("terrain.obj");

	anim_Node_00_ = new Object3d();
	anim_Node_00_->Initialize();
	anim_Node_00_->SetModel("Animation_Node/Animation_Node_00.gltf");
	gltfAsset_Transform_.translate = Vector3(0.0f, 5.2f, -14.0f);
	gltfAsset_Transform_.scale = Vector3(1.0f, 1.0f, 1.0f);
	gltfAsset_Transform_.rotate = Vector3(0.0f, DirectX::XMConvertToRadians(180.0f), 0.0f);

	// 平行光源
	directionalLightData_.color = Vector4(1.0f, 1.0f, 1.0f, 1.0f); // ライトの色
	directionalLightData_.direction = Vector3(0.0f, -1.0f, 0.0f); // ライトの方向
	directionalLightData_.intensity = 1.0f;						 // 輝度

	// スポットライト1
	spotLight_.color = Vector4(1.0f, 1.0f, 1.0f, 1.0f); // ライトの色
	spotLight_.position = Vector3(0.0f, 2.0f, 0.0f);    // ライトの位置
	spotLight_.intensity = 2.5f;						// 輝度
	spotLight_.direction = Vector3(-1.0f, -1.0f, 0.0f); // ライトの方向
	spotLight_.distance = 7.0f;							// 距離
	spotLight_.decay = 1.0f;							// 減衰
	spotLight_.cosAngle = std::cos(std::numbers::pi_v<float> / 3.0f); // 角度
	spotLight_.enable = false;

	// スポットライト2
	spotLight2_.color = Vector4(1.0f, 1.0f, 1.0f, 1.0f); // ライトの色
	spotLight2_.position = Vector3(0.0f, 2.0f, 0.0f);	 // ライトの位置
	spotLight2_.intensity = 2.5f;						 // 輝度
	spotLight2_.direction = Vector3(-1.0f, -1.0f, 0.0f); // ライトの方向
	spotLight2_.distance = 7.0f;						 // 距離
	spotLight2_.decay = 1.0f;							 // 減衰
	spotLight2_.cosAngle = std::cos(std::numbers::pi_v<float> / 3.0f);
	spotLight2_.enable = false;

	// 点光源1
	pointLight_.position = Vector3(0.0f, 2.f, 0.0f); // ライトの位置
	pointLight_.color = { 1.0f, 1.0f, 1.0f, 1.0f };     // ライトの色
	pointLight_.intensity = 2.5f;                       // 輝度
	pointLight_.radius = 10.0f;                         // 半径
	pointLight_.decay = 1.0f;                           // 減衰
	pointLight_.enable = false;                         // 点光源の有効無効

	// 点光源2
	pointLight2_.position = Vector3(0.0f, 2.f, 0.0f); // ライトの位置
	pointLight2_.color = { 1.0f, 1.0f, 1.0f, 1.0f };     // ライトの色
	pointLight2_.intensity = 2.5f;                       // 輝度
	pointLight2_.radius = 10.0f;                         // 半径
	pointLight2_.decay = 1.0f;                           // 減衰
	pointLight2_.enable = false;                         // 点光源の有効無効

}

void TitleScene::Finalize()
{
	/// ================================== ///
	///              終了処理               ///
 	/// ================================== ///
	delete ball_;
	delete plane_;
 	delete plane2_;
	delete anim_Node_00_;
	delete terrain_;
	delete camera_;
}

void TitleScene::Update()
{
#ifdef _DEBUG
	if (Input::GetInstance()->TriggerKey(DIK_F1)) {
		Object3dBasic::GetInstance()->SetDebug(!Object3dBasic::GetInstance()->GetDebug());
		Draw2D::GetInstance()->SetDebug(!Draw2D::GetInstance()->GetDebug());
		ParticleManager::GetInstance()->SetIsDebug(!ParticleManager::GetInstance()->GetIsDebug());
		isDebug_ = !isDebug_;
	}

	if (isDebug_) {
		DebugCamera::GetInstance()->Update();
	}
#endif
	/// ================================== ///
	///              更新処理               ///
	/// ================================== ///

	camera_->SetTranslate(cameraPos_);
	camera_->SetRotate(cameraRot_);
	camera_->Update();
	
	ball_->SetTranslate(ballPos_);
	ball_->SetScale(ballScale_);
	ball_->SetRotate(ballRotate_);
	ball_->SetEnableHighlight(ballHighlight_);
	ball_->SetEnableLighting(enableLighting_);
	ball_->Update();

	plane_->SetTranslate(planePos_);
	plane_->SetScale(planeScale_);
	plane_->SetRotate(planeRotate_);
	plane_->SetEnableHighlight(planeHighlight_);
	plane_->SetEnableLighting(enableLighting_);
	plane_->Update();

	
	plane2_->SetTranslate(plane2Pos_);
	plane2_->SetScale(plane2Scale_);
	plane2_->SetRotate(plane2Rotate_);
	plane2_->SetEnableHighlight(plane2Highlight_);
	plane2_->SetEnableLighting(enableLighting_);
	plane2_->Update();

	terrain_->SetTranslate(terrainPos_);
	terrain_->SetScale(terrainScale_);
	terrain_->SetRotate(terrainRotate_);
	terrain_->SetEnableHighlight(terrainHighlight_);
	terrain_->SetEnableLighting(enableLighting_);
	terrain_->Update();

	anim_Node_00_->SetTranslate(gltfAsset_Transform_.translate);
	anim_Node_00_->SetScale(gltfAsset_Transform_.scale);
	anim_Node_00_->SetRotate(gltfAsset_Transform_.rotate);
	anim_Node_00_->SetEnableHighlight(anim_Node_00_Highlight_);
	anim_Node_00_->SetEnableLighting(enableLighting_);
	anim_Node_00_->Update();


	Object3dBasic::GetInstance()->SetDirectionalLight(directionalLightData_.direction, directionalLightData_.color, 1, directionalLightData_.intensity);
	Object3dBasic::GetInstance()->SetSpotLight(spotLight_.position, spotLight_.direction, spotLight_.color, spotLight_.intensity, spotLight_.distance, spotLight_.decay, spotLight_.cosAngle, spotLight_.enable, 0);
	Object3dBasic::GetInstance()->SetSpotLight(spotLight2_.position, spotLight2_.direction, spotLight2_.color, spotLight2_.intensity, spotLight2_.distance, spotLight2_.decay, spotLight2_.cosAngle, spotLight2_.enable, 1);
	Object3dBasic::GetInstance()->SetPointLight(pointLight_.position, pointLight_.color, pointLight_.intensity, pointLight_.radius, pointLight_.decay, pointLight_.enable, 0);
	Object3dBasic::GetInstance()->SetPointLight(pointLight2_.position, pointLight2_.color, pointLight2_.intensity, pointLight2_.radius, pointLight2_.decay, pointLight2_.enable, 1);

}

void TitleScene::Draw()
{
	/// ================================== ///
	///              描画処理               ///
	/// ================================== ///
	
	Draw2D::GetInstance()->DrawGrid(100.0f, 20.0f, Vector4(1.0f, 1.0f, 1.0f, 1.0f));
	
	Draw2D::GetInstance()->Draw();

	//------------------背景Spriteの描画------------------//
	// スプライト共通描画設定
	SpriteBasic::GetInstance()->SetCommonRenderSetting();



	//--------------------------------------------------//


	//-------------------Modelの描画-------------------//
	// 3Dモデル共通描画設定
	Object3dBasic::GetInstance()->SetCommonRenderSetting();

	// モデルの描画
	ball_->Draw();
	plane_->Draw();
	plane2_->Draw();
	terrain_->Draw();
	anim_Node_00_->Draw();

	//------------------------------------------------//


	//------------------前景Spriteの描画------------------//
	// スプライト共通描画設定
	SpriteBasic::GetInstance()->SetCommonRenderSetting();



	//--------------------------------------------------//
}

void TitleScene::DrawImGui()
{
#ifdef _DEBUG

	/// ================================== ///
	///             ImGuiの描画              ///
	/// ================================== ///

	ImGui::Begin("Model");
	if (ImGui::BeginTabBar("Model")) {

		if (ImGui::BeginTabItem("ball")) {
			ImGui::DragFloat3("Pos", &ballPos_.x, 0.1f);
			ImGui::DragFloat3("Scale", &ballScale_.x, 0.1f);
			ImGui::DragFloat3("Rotate", &ballRotate_.x, 0.1f);
			ImGui::Checkbox("BlinnPhongShading", &ballHighlight_);
			ImGui::EndTabItem();
		}

		if (ImGui::BeginTabItem("plane_obj")) {
			ImGui::DragFloat3("Pos", &planePos_.x, 0.1f);
			ImGui::DragFloat3("Scale", &planeScale_.x, 0.1f);
			ImGui::DragFloat3("Rotate", &planeRotate_.x, 0.1f);
			ImGui::Checkbox("BlinnPhongShading", &planeHighlight_);
			ImGui::EndTabItem();
		}

		if (ImGui::BeginTabItem("plane_gltf")) {
			ImGui::DragFloat3("Pos", &plane2Pos_.x, 0.1f);
			ImGui::DragFloat3("Scale", &plane2Scale_.x, 0.1f);
			ImGui::DragFloat3("Rotate", &plane2Rotate_.x, 0.1f);
			ImGui::Checkbox("BlinnPhongShading", &plane2Highlight_);
			ImGui::EndTabItem();
		}

		if (ImGui::BeginTabItem("terrain")) {
			ImGui::DragFloat3("Pos", &terrainPos_.x, 0.1f);
			ImGui::DragFloat3("Scale", &terrainScale_.x, 0.1f);
			ImGui::DragFloat3("Rotate", &terrainRotate_.x, 0.1f);
			ImGui::Checkbox("BlinnPhongShading", &terrainHighlight_);
			ImGui::EndTabItem();
		}
		ImGui::EndTabBar();
	}
	ImGui::End();


	ImGui::Begin("gltf-Asset-Generator");

	ImGui::DragFloat3("Pos", &gltfAsset_Transform_.translate.x, 0.1f);
	ImGui::DragFloat3("Scale", &gltfAsset_Transform_.scale.x, 0.1f);
	ImGui::DragFloat3("Rotate", &gltfAsset_Transform_.rotate.x, 0.1f);
	ImGui::Checkbox("BlinnPhongShading", &anim_Node_00_Highlight_);

	ImGui::Separator();

	const char* ModelType_items[] = { "Animation_Node_00", "Animation_Node_01", "Animation_Node_02"};
	static int ModelType_selected = 0;
	ImGui::Combo("AssetType", &ModelType_selected, ModelType_items, IM_ARRAYSIZE(ModelType_items));

	if (ImGui::Button("Apply")) {
		if (ModelType_selected == 0) anim_Node_00_->SetModel("Animation_Node/Animation_Node_00.gltf");
		if (ModelType_selected == 1) anim_Node_00_->SetModel("Animation_Node/Animation_Node_01.gltf");
		if (ModelType_selected == 2) anim_Node_00_->SetModel("Animation_Node/Animation_Node_02.gltf");
	}


	ImGui::End();


	ImGui::Begin("Light");
	// enable lighting
	ImGui::Checkbox("EnableLighting", &enableLighting_);

	ImGui::Separator();
	if (ImGui::BeginTabBar("DirectionalLight")) {

		if (ImGui::BeginTabItem("DirectionalLight")) {
			ImGui::DragFloat3("Direction", &directionalLightData_.direction.x, 0.1f);
			ImGui::ColorEdit4("Color", &directionalLightData_.color.x);
			ImGui::DragFloat("Intensity", &directionalLightData_.intensity, 0.1f);
			ImGui::EndTabItem();
		}

		if (ImGui::BeginTabItem("PointLight1")) {
			ImGui::DragFloat3("Position", &pointLight_.position.x, 0.1f);
			ImGui::ColorEdit4("Color", &pointLight_.color.x);
			ImGui::DragFloat("Intensity", &pointLight_.intensity, 0.1f);
			ImGui::DragFloat("Radius", &pointLight_.radius, 0.1f);
			ImGui::DragFloat("Decay", &pointLight_.decay, 0.1f);
			ImGui::Checkbox("Enable", &pointLight_.enable);
			ImGui::EndTabItem();
		}


		if (ImGui::BeginTabItem("PointLight2")) {
			ImGui::DragFloat3("Position", &pointLight2_.position.x, 0.1f);
			ImGui::ColorEdit4("Color", &pointLight2_.color.x);
			ImGui::DragFloat("Intensity", &pointLight2_.intensity, 0.1f);
			ImGui::DragFloat("Radius", &pointLight2_.radius, 0.1f);
			ImGui::DragFloat("Decay", &pointLight2_.decay, 0.1f);
			ImGui::Checkbox("Enable", &pointLight2_.enable);
			ImGui::EndTabItem();
		}

		if (ImGui::BeginTabItem("SpotLight1")) {
			ImGui::DragFloat3("Position", &spotLight_.position.x, 0.1f);
			ImGui::DragFloat3("Direction", &spotLight_.direction.x, 0.1f);
			ImGui::ColorEdit4("Color", &spotLight_.color.x);
			ImGui::DragFloat("Intensity", &spotLight_.intensity, 0.1f);
			ImGui::DragFloat("Distance", &spotLight_.distance, 0.1f);
			ImGui::DragFloat("Decay", &spotLight_.decay, 0.1f);
			ImGui::DragFloat("CosAngle", &spotLight_.cosAngle, 0.1f);
			ImGui::Checkbox("Enable", &spotLight_.enable);
			ImGui::EndTabItem();
		}

		if (ImGui::BeginTabItem("SpotLight2")) {
			ImGui::DragFloat3("Position", &spotLight2_.position.x, 0.1f);
			ImGui::DragFloat3("Direction", &spotLight2_.direction.x, 0.1f);
			ImGui::ColorEdit4("Color", &spotLight2_.color.x);
			ImGui::DragFloat("Intensity", &spotLight2_.intensity, 0.1f);
			ImGui::DragFloat("Distance", &spotLight2_.distance, 0.1f);
			ImGui::DragFloat("Decay", &spotLight2_.decay, 0.1f);
			ImGui::DragFloat("CosAngle", &spotLight2_.cosAngle, 0.1f);
			ImGui::Checkbox("Enable", &spotLight2_.enable);
			ImGui::EndTabItem();
		}
		ImGui::EndTabBar();
	}


	ImGui::End();

#endif // _DEBUG
}

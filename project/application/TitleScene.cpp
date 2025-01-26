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
#include "DirectXMath.h"

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

	camera_ = new Camera();
	camera_->SetTranslate(cameraPos_);
	camera_->SetRotate(cameraRot_);

	Object3dBasic::GetInstance()->SetCamera(camera_);
	Draw2D::GetInstance()->SetCamera(camera_);

	plane_ = new Object3d();
	plane_->Initialize();
	plane_->SetModel("plane.obj");
	planePos_ = Vector3(1.8f, 2.6f, -11.8f);
	planeRotate_ = Vector3(0.0f, DirectX::XMConvertToRadians(180.0f), 0.0f);

	plane2_ = new Object3d();
	plane2_->Initialize();
	plane2_->SetModel("uvChecker.gltf");
	plane2Pos_ = Vector3(-5.3f, 2.6f, -11.8f);
	plane2Rotate_ = Vector3(0.0f, DirectX::XMConvertToRadians(180.0f), 0.0f);

	terrain_ = new Object3d();
	terrain_->Initialize();
	terrain_->SetModel("terrain.obj");

}

void TitleScene::Finalize()
{
	/// ================================== ///
	///              終了処理               ///
 	/// ================================== ///
 	delete plane_;
 	delete plane2_;
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
	
	plane_->SetTranslate(planePos_);
	plane_->SetScale(planeScale_);
	plane_->SetRotate(planeRotate_);
	plane_->Update();

	
	plane2_->SetTranslate(plane2Pos_);
	plane2_->SetScale(plane2Scale_);
	plane2_->SetRotate(plane2Rotate_);
	plane2_->Update();

	terrain_->SetTranslate(terrainPos_);
	terrain_->SetScale(terrainScale_);
	terrain_->SetRotate(terrainRotate_);
	terrain_->Update();
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
	plane_->Draw();
	plane2_->Draw();
	terrain_->Draw();

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


	ImGui::Begin("plane");
	ImGui::DragFloat3("planePos", &planePos_.x, 0.1f);
	ImGui::DragFloat3("planeScale", &planeScale_.x, 0.1f);
	ImGui::DragFloat3("planeRotate", &planeRotate_.x, 0.1f);
	ImGui::End();

	ImGui::Begin("plane2");
	ImGui::DragFloat3("plane2Pos", &plane2Pos_.x, 0.1f);
	ImGui::DragFloat3("plane2Scale", &plane2Scale_.x, 0.1f);
	ImGui::DragFloat3("plane2Rotate", &plane2Rotate_.x, 0.1f);
	ImGui::End();

	ImGui::Begin("terrain");
	ImGui::DragFloat3("terrainPos", &terrainPos_.x, 0.1f);
	ImGui::DragFloat3("terrainScale", &terrainScale_.x, 0.1f);
	ImGui::DragFloat3("terrainRotate", &terrainRotate_.x, 0.1f);
	ImGui::End();

	ImGui::Begin("camera");
	ImGui::DragFloat3("cameraPos", &cameraPos_.x, 0.1f);
	ImGui::DragFloat3("cameraRotate", &cameraRot_.x, 0.1f);
	ImGui::End();


#endif // _DEBUG
}

#include "TitleScene.h"
#include "SceneManager.h"
#include "TextureManager.h"
#include "Object3dBasic.h"
#include "SpriteBasic.h"
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

	center = { 0.0f, 0.0f, 0.0f };

	// スプライトの初期化
	TextureManager::GetInstance()->LoadTexture("uvChecker.png");

	sprite_ = new Sprite();
	sprite_->Initialize("uvChecker.png");
	sprite_->SetPos(Vector2(0.0f, 0.0f));


}

void TitleScene::Finalize()
{
	delete sprite_;
}

void TitleScene::Update()
{
#ifdef _DEBUG
	if (Input::GetInstance()->TriggerKey(DIK_F1)) {
		Object3dBasic::GetInstance()->SetDebug(!Object3dBasic::GetInstance()->GetDebug());
		isDebug_ = !isDebug_;
	}

	if (isDebug_) {
		DebugCamera::GetInstance()->Update();
	}
#endif
	/// ================================== ///
	///              更新処理               ///
	/// ================================== ///

	sprite_->Update();


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

	sprite_->Draw();

	//--------------------------------------------------//

	Draw2D::GetInstance()->DrawBox(Vector2(0.0f, 0.0f), Vector2(100.0f, 100.0f), Vector4(1.0f, 1.0f, 1.0f, 1.0f));

	Matrix4x4 viewProjectionMatrix = Object3dBasic::GetInstance()->GetCamera()->GetViewProjectionMatrix();

	Draw2D::GetInstance()->DrawSphere(center, 5.0f, Vector4(1.0f, 1.0f, 1.0f, 1.0f), viewProjectionMatrix);
}

void TitleScene::DrawImGui()
{
#ifdef _DEBUG

	ImGui::Begin("Sphere");
	ImGui::DragFloat3("Center", &center.x, 0.01f, -50.0f, 50.0f);
	ImGui::End();

	ImGui::Begin("Camera");
	ImGui::DragFloat3("Position", &cameraPosition.x, 0.01f, -50.0f, 50.0f);
	ImGui::DragFloat3("Rotation", &cameraRotation.x, 0.01f, -50.0f, 50.0f);
	Object3dBasic::GetInstance()->GetCamera()->SetTranslate(cameraPosition);
	Object3dBasic::GetInstance()->GetCamera()->SetRotate(cameraRotation);
	ImGui::End();

#endif // _DEBUG
}

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

	aixs_ = Vector3(1.0f, 1.0f, 1.0f);
	angle_ = 0.44f;
	rotMat_ = Mat4x4::MakeRotateAxisAngle(aixs_, angle_);

}

void TitleScene::Finalize()
{

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

}

void TitleScene::DrawImGui()
{
#ifdef _DEBUG

	// display rotMat_
	ImGui::Begin("Display");

	ImGui::Text("rotMat_");
	ImGui::Text("rotMat_[0][0]: %.3f , rotMat_[0][1]: %.3f , rotMat_[0][2]: %.3f , rotMat_[0][3]: %.3f", rotMat_.m[0][0], rotMat_.m[0][1], rotMat_.m[0][2], rotMat_.m[0][3]);
	ImGui::Text("rotMat_[1][0]: %.3f , rotMat_[1][1]: %.3f , rotMat_[1][2]: %.3f , rotMat_[1][3]: %.3f", rotMat_.m[1][0], rotMat_.m[1][1], rotMat_.m[1][2], rotMat_.m[1][3]);
	ImGui::Text("rotMat_[2][0]: %.3f , rotMat_[2][1]: %.3f , rotMat_[2][2]: %.3f , rotMat_[2][3]: %.3f", rotMat_.m[2][0], rotMat_.m[2][1], rotMat_.m[2][2], rotMat_.m[2][3]);
	ImGui::Text("rotMat_[3][0]: %.3f , rotMat_[3][1]: %.3f , rotMat_[3][2]: %.3f , rotMat_[3][3]: %.3f", rotMat_.m[3][0], rotMat_.m[3][1], rotMat_.m[3][2], rotMat_.m[3][3]);

	ImGui::End();


#endif // _DEBUG
}

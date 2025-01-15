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

	from0 = Vector3(1.0f, 0.7f, 0.5f);
	to0 = Vector3(-from0.x, -from0.y, -from0.z);

	from1 = Vector3(-0.6f, 0.9f, 0.2f);
	to1 = Vector3(0.4f, 0.7f, -0.5f);

	rotMat0 = Mat4x4::DirectionToDirection(Vector3(1.0f, 0.0f, 0.0f), Vector3(-1.0f, 0.0f, 0.0f));
	rotMat1 = Mat4x4::DirectionToDirection(from0, to0);
	rotMat2 = Mat4x4::DirectionToDirection(from1, to1);

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

	ImGui::Begin("rotMat0");
	ImGui::Text("rotMat0");
	ImGui::Text("%.3f %.3f %.3f %.3f", rotMat0.m[0][0], rotMat0.m[0][1], rotMat0.m[0][2], rotMat0.m[0][3]);
	ImGui::Text("%.3f %.3f %.3f %.3f", rotMat0.m[1][0], rotMat0.m[1][1], rotMat0.m[1][2], rotMat0.m[1][3]);
	ImGui::Text("%.3f %.3f %.3f %.3f", rotMat0.m[2][0], rotMat0.m[2][1], rotMat0.m[2][2], rotMat0.m[2][3]);
	ImGui::Text("%.3f %.3f %.3f %.3f", rotMat0.m[3][0], rotMat0.m[3][1], rotMat0.m[3][2], rotMat0.m[3][3]);
	ImGui::End();

	ImGui::Begin("rotMat1");
	ImGui::Text("rotMat1");
	ImGui::Text("%.3f %.3f %.3f %.3f", rotMat1.m[0][0], rotMat1.m[0][1], rotMat1.m[0][2], rotMat1.m[0][3]);
	ImGui::Text("%.3f %.3f %.3f %.3f", rotMat1.m[1][0], rotMat1.m[1][1], rotMat1.m[1][2], rotMat1.m[1][3]);
	ImGui::Text("%.3f %.3f %.3f %.3f", rotMat1.m[2][0], rotMat1.m[2][1], rotMat1.m[2][2], rotMat1.m[2][3]);
	ImGui::Text("%.3f %.3f %.3f %.3f", rotMat1.m[3][0], rotMat1.m[3][1], rotMat1.m[3][2], rotMat1.m[3][3]);
	ImGui::End();

	ImGui::Begin("rotMat2");
	ImGui::Text("rotMat2");
	ImGui::Text("%.3f %.3f %.3f %.3f", rotMat2.m[0][0], rotMat2.m[0][1], rotMat2.m[0][2], rotMat2.m[0][3]);
	ImGui::Text("%.3f %.3f %.3f %.3f", rotMat2.m[1][0], rotMat2.m[1][1], rotMat2.m[1][2], rotMat2.m[1][3]);
	ImGui::Text("%.3f %.3f %.3f %.3f", rotMat2.m[2][0], rotMat2.m[2][1], rotMat2.m[2][2], rotMat2.m[2][3]);
	ImGui::Text("%.3f %.3f %.3f %.3f", rotMat2.m[3][0], rotMat2.m[3][1], rotMat2.m[3][2], rotMat2.m[3][3]);
	ImGui::End();

#endif // _DEBUG
}

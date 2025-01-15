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

	rotation = Quat::MakeRotateAxisAngle(Vector3(1.0f, 0.4f, -0.2f), 0.45f);
	pointY = Vector3(2.1f, -0.9f, 1.3f);
	rotMat = Mat4x4::MakeRotateXYZ(rotation);
	rotateByQuat = Quat::RotateVec3(pointY, rotation);
	rotateByMat = Mat4x4::TransForm(rotMat, pointY);

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

	/// ================================== ///
	///              ImGui描画               ///
		/// ================================== ///

	ImGui::Begin("TitleScene");

	// rotation quat
	ImGui::Text("rotation quat");
	ImGui::Text("%.2f, %.2f, %.2f, %.2f", rotation.x, rotation.y, rotation.z, rotation.w);
	ImGui::Separator();

	// rotate matrix
	ImGui::Text("rotate matrix");
	ImGui::Text("%.3f, %.3f, %.3f, %.3f", rotMat.m[0][0], rotMat.m[0][1], rotMat.m[0][2], rotMat.m[0][3]);
	ImGui::Text("%.3f, %.3f, %.3f, %.3f", rotMat.m[1][0], rotMat.m[1][1], rotMat.m[1][2], rotMat.m[1][3]);
	ImGui::Text("%.3f, %.3f, %.3f, %.3f", rotMat.m[2][0], rotMat.m[2][1], rotMat.m[2][2], rotMat.m[2][3]);
	ImGui::Text("%.3f, %.3f, %.3f, %.3f", rotMat.m[3][0], rotMat.m[3][1], rotMat.m[3][2], rotMat.m[3][3]);
	ImGui::Separator();

	// rotate point by quat
	ImGui::Text("rotate by quat");
	ImGui::Text("%.2f, %.2f, %.2f", rotateByQuat.x, rotateByQuat.y, rotateByQuat.z);
	ImGui::Separator();

	// rotate point by matrix
	ImGui::Text("rotate by matrix");
	ImGui::Text("%.2f, %.2f, %.2f", rotateByMat.x, rotateByMat.y, rotateByMat.z);

	ImGui::End();


#endif // _DEBUG
}

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

	rotation0 = Quat::MakeRotateAxisAngle(Vector3(0.71f, 0.71f, 0.0f), 0.3f);
	rotation1 = Quat::MakeRotateAxisAngle(Vector3(0.71f, 0.0f, 0.71f), 3.141592f);

	interpolate0 = Quat::Slerp(rotation0, rotation1, 0.0f);
	interpolate1 = Quat::Slerp(rotation0, rotation1, 0.3f);
	interpolate2 = Quat::Slerp(rotation0, rotation1, 0.5f);
	interpolate3 = Quat::Slerp(rotation0, rotation1, 0.7f);
	interpolate4 = Quat::Slerp(rotation0, rotation1, 1.0f);
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
	///              ImGui描画              ///
	/// ================================== ///

	ImGui::Begin("TitleScene");

	// interpolate0
	ImGui::Text("interpolate0, Slerp(rotation0, rotation1, 0.0f)");
	ImGui::Text("x:%.2f y:%.2f z:%.2f w:%.2f", interpolate0.x, interpolate0.y, interpolate0.z, interpolate0.w);

	// interpolate1
	ImGui::Text("interpolate1, Slerp(rotation0, rotation1, 0.3f)");
	ImGui::Text("x:%.2f y:%.2f z:%.2f w:%.2f", interpolate1.x, interpolate1.y, interpolate1.z, interpolate1.w);

	// interpolate2
	ImGui::Text("interpolate2, Slerp(rotation0, rotation1, 0.5f)");
	ImGui::Text("x:%.2f y:%.2f z:%.2f w:%.2f", interpolate2.x, interpolate2.y, interpolate2.z, interpolate2.w);

	// interpolate3
	ImGui::Text("interpolate3, Slerp(rotation0, rotation1, 0.7f)");
	ImGui::Text("x:%.2f y:%.2f z:%.2f w:%.2f", interpolate3.x, interpolate3.y, interpolate3.z, interpolate3.w);

	// interpolate4
	ImGui::Text("interpolate4, Slerp(rotation0, rotation1, 1.0f)");
	ImGui::Text("x:%.2f y:%.2f z:%.2f w:%.2f", interpolate4.x, interpolate4.y, interpolate4.z, interpolate4.w);


	ImGui::End();


#endif // _DEBUG
}

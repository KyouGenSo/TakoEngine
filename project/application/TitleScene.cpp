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

	q1 = { 2.0f, 3.0f, 4.0f, 1.0f };
	q2 = { 1.0f, 3.0f, 5.0f, 2.0f };
	identity = Quat::Identity();
	conj = Quat::Conjugate(q1);
	inv = Quat::Inverse(q1);
	normal = Quat::Normalize(q1);
	mul1 = Quat::Multiply(q1, q2);
	mul2 = Quat::Multiply(q2, q1);
	norm = Quat::Norm(q1);
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

	ImGui::Begin("Quaternion");

	ImGui::Text("q1 = { %.2f, %.2f, %.2f, %.2f }", q1.x, q1.y, q1.z, q1.w);
	ImGui::Text("q2 = { %.2f, %.2f, %.2f, %.2f }", q2.x, q2.y, q2.z, q2.w);
	ImGui::Text("identity = { %.2f, %.2f, %.2f, %.2f }", identity.x, identity.y, identity.z, identity.w);
	ImGui::Text("conj = { %.2f, %.2f, %.2f, %.2f }", conj.x, conj.y, conj.z, conj.w);
	ImGui::Text("inv = { %.2f, %.2f, %.2f, %.2f }", inv.x, inv.y, inv.z, inv.w);
	ImGui::Text("normal = { %.2f, %.2f, %.2f, %.2f }", normal.x, normal.y, normal.z, normal.w);
	ImGui::Text("mul1 = { %.2f, %.2f, %.2f, %.2f }", mul1.x, mul1.y, mul1.z, mul1.w);
	ImGui::Text("mul2 = { %.2f, %.2f, %.2f, %.2f }", mul2.x, mul2.y, mul2.z, mul2.w);
	ImGui::Text("norm = %.2f", norm);

	ImGui::End();

	//--------------------------------------------------//

#endif // _DEBUG
}

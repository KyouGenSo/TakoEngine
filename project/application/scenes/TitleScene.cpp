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
#include "Xinput.h"

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

	// 背景Spriteの初期化
	TextureManager::GetInstance()->LoadTexture("title.png");
	bgSprite_ = std::make_unique<Sprite>();
	bgSprite_->Initialize("title.png");
	bgSprite_->SetPos({ 0.0f, 0.0f });
}

void TitleScene::Finalize()
{
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


	bgSprite_->Update();

	// シーン遷移
	if (Input::GetInstance()->TriggerKey(DIK_SPACE))
	{
		SceneManager::GetInstance()->ChangeScene("game");
	}

	XINPUT_STATE joyState_;
	if (Input::GetInstance()->GetJoystickState(0, joyState_)) {
		if (joyState_.Gamepad.wButtons & XINPUT_GAMEPAD_A) {
			SceneManager::GetInstance()->ChangeScene("game");
		}
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


	bgSprite_->Draw();

	//--------------------------------------------------//

}

void TitleScene::DrawImGui()
{
#ifdef _DEBUG


#endif // _DEBUG
}

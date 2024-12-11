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
	ModelManager::GetInstance()->LoadModel("ground_black.gltf");
	ModelManager::GetInstance()->LoadModel("skydome.obj");

	// 天球
	skydome_ = std::make_unique<Skydome>();
	skydomeModel_ = std::make_unique<Object3d>();
	skydomeModel_->Initialize();
	skydomeModel_->SetModel("skydome.obj");
	skydome_->Initialize(skydomeModel_.get());

	skydomeModel_->SetEnableLighting(false);

	// 地面
	ground_ = std::make_unique<Ground>();
	groundModel_ = std::make_unique<Object3d>();
	groundModel_->Initialize();
	groundModel_->SetModel("ground_black.gltf");
	ground_->Initialize(groundModel_.get());

}

void GameScene::Finalize()
{

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

	skydome_->Update();
	ground_->Update();


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

	// 天球
	skydome_->Draw();

	// 地面
	ground_->Draw();

	//-------------------Modelの描画-------------------//


	//------------------前景Spriteの描画------------------//
	// スプライト共通描画設定
	SpriteBasic::GetInstance()->SetCommonRenderSetting();



	//--------------------------------------------------//
}

void GameScene::DrawImGui()
{
#ifdef _DEBUG

#endif // DEBUG
}

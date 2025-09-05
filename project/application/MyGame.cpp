#include "MyGame.h"
#include"Audio.h"
#include"Input.h"
#include "SceneFactory.h"
#include "SceneManager.h"
#include "TextureManager.h"
#include "Draw2D.h"
#include "Object3dBasic.h"
#include "PostEffectManager.h"
#include "FrameTimer.h"
#include "GlobalVariables.h"
#include "ModelManager.h"
#include "GPUParticle.h"
#include "SpriteBasic.h"
#include "Transition.h"
#include "ShadowRenderer.h"

void MyGame::Initialize()
{
  winApp_->SetWindowSize(1600, 900);
	TakoFramework::Initialize();

  winApp_->SetWindowTitle(L"TakoEngine Sample Game");
  

#pragma region 汎用機能初期化-------------------------------------------------------------------------------------------------------------------
	// 入力クラスの初期化
	Input::GetInstance()->Initialize(winApp_);

	// オーディオの初期化
	Audio::GetInstance()->Initialize("resources/Sound/");

#pragma endregion

	// シーンの初期化
	sceneFactory_ = new SceneFactory();
	SceneManager::GetInstance()->SetSceneFactory(sceneFactory_);
	SceneManager::GetInstance()->ChangeScene("game", 0.0f);

  TextureManager::GetInstance()->LoadTexture("white.png");
  TextureManager::GetInstance()->LoadTexture("circle.png");
  TextureManager::GetInstance()->LoadTexture("uvChecker.png");
  TextureManager::GetInstance()->LoadTexture("my_skybox.dds");

  spriteBasicOnresizeId = winApp_->RegisterOnResizeFunc(std::bind(&SpriteBasic::OnResize, SpriteBasic::GetInstance(), std::placeholders::_1));

  // GPUパーティクルの初期化
  GPUParticle::GetInstance()->Initialize(dx12_, defaultCamera_);
}

void MyGame::Finalize()
{
  winApp_->UnregisterOnResizeFunc(spriteBasicOnresizeId);

	TakoFramework::Finalize();

	// Audioの解放
	Audio::GetInstance()->Finalize();

	// 入力クラスの解放
	Input::GetInstance()->Finalize();

  // GPUパーティクルの解放
  GPUParticle::GetInstance()->Finalize();
}

void MyGame::Update()
{
	// カメラの更新
	defaultCamera_->Update();

	// 入力情報の更新
	Input::GetInstance()->Update();

  // F11キーでフルスクリーン切り替え
  if (Input::GetInstance()->TriggerKey(DIK_F11))
  {
    ToggleFullScreen();
  }

  // GPUパーティクルの更新
  GPUParticle::GetInstance()->Update();

	TakoFramework::Update();

	//　サウンドの更新
	Audio::GetInstance()->Update();

  // ゲームパッドの状態をリスレッシュ
	Input::GetInstance()->RefreshGamePadState();
}

void MyGame::Draw()
{
	/// ============================================= ///
	/// ------------------シーン描画-------------------///
	/// ============================================= ///

  //ポストエフェクト適用対象のレンダーテクスチャを描画先に設定
	dx12_->SetEffectRenderTexture();

	// テクスチャ用のsrvヒープの設定
	SrvManager::GetInstance()->BeginDraw();

	SceneManager::GetInstance()->Draw();

  GPUParticle::GetInstance()->Draw();

  Draw2D::GetInstance()->Draw();

	/// ===================================================== ///
	/// ------------------ポストエフェクト描画-------------------///
	/// ===================================================== ///

    // ポストエフェクトの描画
  PostEffectManager::GetInstance()->Draw();

  /// ===================================================== ///
  /// ------------ポストエフェクト非適用対象の描画---------------///
  /// ===================================================== ///
  // ポストエフェクト非適用対象のレンダーテクスチャを描画先に設定
  dx12_->SetNonEffectRenderTexture();

  // シーンの描画
  SceneManager::GetInstance()->DrawWithoutEffect();

  Transition::GetInstance()->Draw();

  Draw2D::GetInstance()->Reset();

  /// ============================================= ///
  /// ---------最終結果をスワップチェーンに描画---------///
  /// ============================================= ///
  // GameViewportWindowVisibleがfalseの時のみスワップチェーンに描画
  PostEffectManager::GetInstance()->DrawFinalResult(!GameViewportWindowVisible);


	/// ========================================= ///
	///-------------------ImGui-------------------///
	/// ========================================= ///
#ifdef _DEBUG

	imguiManager_->Begin();

  TakoFramework::Draw();

	SceneManager::GetInstance()->DrawImGui();

	Draw2D::GetInstance()->ImGui();

  // GlobalVariablesの更新
  GlobalVariables::GetInstance()->Update();

	imguiManager_->End();

	//imguiの描画
	imguiManager_->Draw();
#endif

	// 描画後の処理
	dx12_->EndDraw();
}

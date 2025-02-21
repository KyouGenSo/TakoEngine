#include "MyGame.h"
#include"Audio.h"
#include"Input.h"
#include "SceneFactory.h"
#include "SceneManager.h"
#include "TextureManager.h"
#include "ParticleManager.h"
#include "Draw2D.h"
#include "Object3dBasic.h"
#include "PostEffect.h"
#include "FrameTimer.h"
#include "GlobalVariables.h"
#include "Vector4.h"

void MyGame::Initialize()
{

	TakoFramework::Initialize();

#pragma region 汎用機能初期化-------------------------------------------------------------------------------------------------------------------
	// 入力クラスの初期化
	Input::GetInstance()->Initialize(winApp_);

	// オーディオの初期化
	Audio::GetInstance()->Initialize("resources/Sound/");

#pragma endregion

	// シーンの初期化
	sceneFactory_ = new SceneFactory();
	SceneManager::GetInstance()->SetSceneFactory(sceneFactory_);
	SceneManager::GetInstance()->ChangeScene("title", 0.0f);

  // PostEffectParamの設定
  postEffectParam.vignettePower = 0.f;
  postEffectParam.vignetteRange = 20.0f;
  postEffectParam.bloomThreshold = 1.0f;
  postEffectParam.bloomIntensity = 1.0f;
  postEffectParam.bloomSigma = 2.0f;
  postEffectParam.fogColor = {1.0f, 1.0f, 1.0f, 1.0f};
  postEffectParam.fogDensity = 0.01f;

}

void MyGame::Finalize()
{
	TakoFramework::Finalize();

	// Audioの解放
	Audio::GetInstance()->Finalize();

	// 入力クラスの解放
	Input::GetInstance()->Finalize();
}

void MyGame::Update()
{
	// カメラの更新
	defaultCamera_->Update();

	// 入力情報の更新
	Input::GetInstance()->Update();

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

	// 描画前の処理(レンダーテクスチャを描画対象に設定)
	dx12_->SetRenderTexture();

	// テクスチャ用のsrvヒープの設定
	SrvManager::GetInstance()->BeginDraw();

	// シーンの描画
	SceneManager::GetInstance()->Draw();

	ParticleManager::GetInstance()->Draw();

	Draw2D::GetInstance()->Draw();

	Draw2D::GetInstance()->Reset();


	/// ===================================================== ///
	/// ------------------ポストエフェクト描画-------------------///
	/// ===================================================== ///
	// SwapChainを描画対象に設定
	dx12_->SetSwapChain();

	// PostEffectの描画
	switch (postEffectType)
	{
	case::MyGame::NoEffect:
		PostEffect::GetInstance()->Draw("NoEffect");
		break;
	case::MyGame::VignetteRed:
		PostEffect::GetInstance()->Draw("VignetteRed");
		break;
	case::MyGame::VignetteRedBloom:
		PostEffect::GetInstance()->Draw("VignetteRedBloom");
		break;
	case::MyGame::GrayScale:
		PostEffect::GetInstance()->Draw("GrayScale");
		break;
	case::MyGame::VigRedGrayScale:
		PostEffect::GetInstance()->Draw("VigRedGrayScale");
		break;
	case::MyGame::Bloom:
		PostEffect::GetInstance()->Draw("Bloom");
		break;
	case::MyGame::BloomFog:
		PostEffect::GetInstance()->Draw("BloomFog");
		break;
	}


	/// ========================================= ///
	///-------------------ImGui-------------------///
	/// ========================================= ///
#ifdef _DEBUG

	imguiManager_->Begin();

	SceneManager::GetInstance()->DrawImGui();

	Draw2D::GetInstance()->ImGui();

  // GlobalVariablesの更新
  GlobalVariables::GetInstance()->Update();

  ImGui::Begin("Option");
  // buttonでFPSの表示を切り替え
  if (ImGui::Button("Display FPS"))
  {
    FPSWindowVisible = !FPSWindowVisible;
  }
  ImGui::SameLine();
  if (ImGui::Button("PostEffect Option"))
  {
    PostEffectWindowVisible = !PostEffectWindowVisible;
  }

  ImGui::End();

  // fpsの表示
  if (FPSWindowVisible)
  {
    ImGui::Begin("FPS", &FPSWindowVisible);
    ImGui::ProgressBar(FrameTimer::GetInstance()->GetFPS() / 60.0f, ImVec2(0.0f, 0.0f), "");
    ImGui::SameLine();
    ImGui::Text("FPS : %.0f", FrameTimer::GetInstance()->GetFPS());
    ImGui::End();
  }


	// PostEffectのパラメータ調整
  if (PostEffectWindowVisible) {
    ImGui::Begin("PostEffect", &PostEffectWindowVisible);
    if (ImGui::BeginTabBar("PostEffectTab"))
    {

      if (ImGui::BeginTabItem("PostEffectType"))
      {
        ImGui::RadioButton("NoEffect", (int*)&postEffectType, NoEffect);
        ImGui::RadioButton("VignetteRed", (int*)&postEffectType, VignetteRed);
        ImGui::RadioButton("VignetteRedBloom", (int*)&postEffectType, VignetteRedBloom);
        ImGui::RadioButton("GrayScale", (int*)&postEffectType, GrayScale);
        ImGui::RadioButton("VigRedGrayScale", (int*)&postEffectType, VigRedGrayScale);
        ImGui::RadioButton("Bloom", (int*)&postEffectType, Bloom);
        ImGui::RadioButton("BloomFog", (int*)&postEffectType, BloomFog);

        ImGui::EndTabItem();
      }

      //ImGui::Separator();
      if (ImGui::BeginTabItem("PostEffect"))
      {
        if (postEffectType == VignetteRed || postEffectType == VignetteRedBloom || postEffectType == VigRedGrayScale)
        {
          ImGui::DragFloat("VignettePower", &postEffectParam.vignettePower, 0.01f, 0.0f, 10.0f);
          PostEffect::GetInstance()->SetVignettePower(postEffectParam.vignettePower);
          ImGui::DragFloat("VignetteRange", &postEffectParam.vignetteRange, 0.01f, 0.0f, 100.0f);
          PostEffect::GetInstance()->SetVignetteRange(postEffectParam.vignetteRange);
        }

        if (postEffectType == VignetteRedBloom)
        {
          ImGui::DragFloat("BloomThreshold", &postEffectParam.bloomThreshold, 0.01f, 0.0f, 1.0f);
          PostEffect::GetInstance()->SetBloomThreshold(postEffectParam.bloomThreshold);
        }

        if (postEffectType == Bloom || postEffectType == BloomFog)
        {
          ImGui::DragFloat("BloomIntensity", &postEffectParam.bloomIntensity, 0.01f, 0.0f, 10.0f);
          PostEffect::GetInstance()->SetBloomIntensity(postEffectParam.bloomIntensity);
          ImGui::DragFloat("BloomThreshold", &postEffectParam.bloomThreshold, 0.01f, 0.0f, 1.0f);
          PostEffect::GetInstance()->SetBloomThreshold(postEffectParam.bloomThreshold);
          ImGui::DragFloat("BloomSigma", &postEffectParam.bloomSigma, 0.01f, 0.0f, 10.0f);
          PostEffect::GetInstance()->SetBloomSigma(postEffectParam.bloomSigma);
        }

        if (postEffectType == BloomFog)
        {
          ImGui::ColorEdit4("FogColor", &postEffectParam.fogColor.x);
          PostEffect::GetInstance()->SetFogColor(postEffectParam.fogColor);
          ImGui::DragFloat("FogDensity", &postEffectParam.fogDensity, 0.01f, 0.0f, 1.0f);
          PostEffect::GetInstance()->SetFogDensity(postEffectParam.fogDensity);
        }

        ImGui::EndTabItem();
      }

      ImGui::EndTabBar();
    }


    ImGui::End();
  }

	imguiManager_->End();

	//imguiの描画
	imguiManager_->Draw();
#endif


	// 描画後の処理
	dx12_->EndDraw();
}

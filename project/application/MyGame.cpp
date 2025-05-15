#include "MyGame.h"
#include"Audio.h"
#include"Input.h"
#include "SceneFactory.h"
#include "SceneManager.h"
#include "TextureManager.h"
#include "Draw2D.h"
#include "Object3dBasic.h"
#include "PostEffect.h"
#include "FrameTimer.h"
#include "GlobalVariables.h"
#include "ModelManager.h"
#include "GPUParticle.h"

void MyGame::Initialize()
{

  winApp_->SetWindowSize(1280, 720);

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
	SceneManager::GetInstance()->ChangeScene("game", 0.0f);

  TextureManager::GetInstance()->LoadTexture("white.png");
  TextureManager::GetInstance()->LoadTexture("circle.png");
  TextureManager::GetInstance()->LoadTexture("uvChecker.png");

  // GPUパーティクルの初期化
  GPUParticle::GetInstance()->Initialize(dx12_, defaultCamera_);

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

#ifdef _DEBUG
  switch (postEffectType)
  {
  case NoEffect:
    PostEffect::GetInstance()->SetEffectType("NoEffect");
    break;
  case VignetteRed:
    PostEffect::GetInstance()->SetEffectType("VignetteRed");
    break;
  case VignetteRedBloom:
    PostEffect::GetInstance()->SetEffectType("VignetteRedBloom");
    break;
  case GrayScale:
    PostEffect::GetInstance()->SetEffectType("GrayScale");
    break;
  case VigRedGrayScale:
    PostEffect::GetInstance()->SetEffectType("VigRedGrayScale");
    break;
  case Bloom:
    PostEffect::GetInstance()->SetEffectType("Bloom");
    break;
  case BloomFog:
    PostEffect::GetInstance()->SetEffectType("BloomFog");
    break;
  case RadialBlur:
    PostEffect::GetInstance()->SetEffectType("RadialBlur");
    break;
  }
#endif // _DEBUG
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

	/// ===================================================== ///
	/// ------------------ポストエフェクト描画-------------------///
	/// ===================================================== ///

    // ポストエフェクトの描画
  PostEffect::GetInstance()->Draw();

  /// ===================================================== ///
  /// ------------ポストエフェクト非適用対象の描画---------------///
  /// ===================================================== ///
  // ポストエフェクト非適用対象のレンダーテクスチャを描画先に設定
  dx12_->SetNonEffectRenderTexture();

  // シーンの描画
  SceneManager::GetInstance()->DrawWithoutEffect();

  GPUParticle::GetInstance()->Draw();

  Draw2D::GetInstance()->Draw();

  Draw2D::GetInstance()->Reset();

  /// ============================================= ///
  /// ---------最終結果をスワップチェーンに描画---------///
  /// ============================================= ///
  PostEffect::GetInstance()->DrawFinalResult();


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
        ImGui::RadioButton("RadialBlur", (int*)&postEffectType, RadialBlur);

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

        if (postEffectType == RadialBlur)
        {
          ImGui::DragFloat2("RadialBlurCenter", &postEffectParam.radialBlurCenter.x, 0.01f, 0.0f, 1.0f);
          PostEffect::GetInstance()->SetRadialBlurCenter(postEffectParam.radialBlurCenter);
          ImGui::DragFloat("RadialBlurWidth", &postEffectParam.radialBlurWidth, 0.01f, 0.0f, 1.0f);
          PostEffect::GetInstance()->SetRadialBlurWidth(postEffectParam.radialBlurWidth);
          ImGui::DragInt("RadialBlurSampleCount", &postEffectParam.radialBlurSampleCount, 1.0f, 1, 100);
          PostEffect::GetInstance()->SetRadialBlurSampleCount(postEffectParam.radialBlurSampleCount);
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

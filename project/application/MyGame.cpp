#include "MyGame.h"
#include"Audio.h"
#include"Input.h"
#include "SceneFactory.h"
#include "SceneManager.h"
#include "Draw2D.h"
#include "Object3dBasic.h"
#include "PostEffect.h"

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
	SceneManager::GetInstance()->ChangeScene("game", 0.0f);
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

	TakoFramework::Update();

	// 入力情報の更新
	Input::GetInstance()->Update();

	//　サウンドの更新
	Audio::GetInstance()->Update();

	// カメラの更新
	defaultCamera_->Update();

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
	}


	/// ========================================= ///
	///-------------------ImGui-------------------///
	/// ========================================= ///
#ifdef _DEBUG

	imguiManager_->Begin();

	SceneManager::GetInstance()->DrawImGui();

	Draw2D::GetInstance()->ImGui();

	// PostEffectのパラメータ調整
	ImGui::Begin("PostEffect");
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

			ImGui::EndTabItem();
		}

		//ImGui::Separator();
		if (ImGui::BeginTabItem("PostEffect"))
		{
			if (postEffectType == VignetteRed || postEffectType == VignetteRedBloom || postEffectType == VigRedGrayScale)
			{
				ImGui::DragFloat("VignettePower", &vignettePower, 0.01f, 0.0f, 10.0f);
				PostEffect::GetInstance()->SetVignettePower(vignettePower);
				ImGui::DragFloat("VignetteRange", &vignetteRange, 0.01f, 0.0f, 100.0f);
				PostEffect::GetInstance()->SetVignetteRange(vignetteRange);
			}

			if (postEffectType == VignetteRedBloom)
			{
				ImGui::DragFloat("BloomThreshold", &bloomThreshold, 0.01f, 0.0f, 1.0f);
				PostEffect::GetInstance()->SetBloomThreshold(bloomThreshold);
			}

			if (postEffectType == Bloom)
			{
				ImGui::DragFloat("BloomIntensity", &bloomIntensity, 0.01f, 0.0f, 10.0f);
				PostEffect::GetInstance()->SetBloomIntensity(bloomIntensity);
				ImGui::DragFloat("BloomThreshold", &bloomThreshold, 0.01f, 0.0f, 1.0f);
				PostEffect::GetInstance()->SetBloomThreshold(bloomThreshold);
				ImGui::DragFloat("BloomSigma", &bloomSigma, 0.01f, 0.0f, 10.0f);
				PostEffect::GetInstance()->SetBloomSigma(bloomSigma);
			}

			ImGui::EndTabItem();
		}

		ImGui::EndTabBar();
	}


	ImGui::End();

	imguiManager_->End();

	//imguiの描画
	imguiManager_->Draw();
#endif


	// 描画後の処理
	dx12_->EndDraw();
}

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
	SceneManager::GetInstance()->ChangeScene("title");
}

void MyGame::Finalize()
{
	// Audioの解放
	Audio::GetInstance()->Finalize();

	// 入力クラスの解放
	Input::GetInstance()->Finalize();

	TakoFramework::Finalize();
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
	// 描画前の処理(レンダーテクスチャを描画対象に設定)
	dx12_->SetRenderTexture();

	/// ============================================= ///
	/// ------------------シーン描画-------------------///
	/// ============================================= ///

	// テクスチャ用のsrvヒープの設定
	SrvManager::GetInstance()->BeginDraw();

	// シーンの描画
	SceneManager::GetInstance()->Draw();

	// SwapChainを描画対象に設定
	dx12_->SetSwapChain();

	// PostEffectの描画
	switch (postEffectType)
	{
	case::MyGame::VignetteRed:
		PostEffect::GetInstance()->Draw("VignetteRed");
		break;
	case::MyGame::GrayScale:
		PostEffect::GetInstance()->Draw("GrayScale");
		break;
	case::MyGame::VigRedGrayScale:
		PostEffect::GetInstance()->Draw("VigRedGrayScale");
		break;
	}

	/// ============================================= ///
	/// ------------------シーン描画-------------------///
	/// ============================================= ///



	/// ========================================= ///
	///-------------------ImGui-------------------///
	/// ========================================= ///
#ifdef _DEBUG

	imguiManager_->Begin();

	SceneManager::GetInstance()->DrawImGui();

	Draw2D::GetInstance()->ImGui();

	ImGui::Begin("PostEffect");

	ImGui::BeginTabBar("PostEffectType");
	if (ImGui::BeginTabItem("PostEffectType"))
	{
		if (ImGui::Selectable("VignetteRed", postEffectType == VignetteRed))
		{
			postEffectType = VignetteRed;
		}
		if (ImGui::Selectable("GrayScale", postEffectType == GrayScale))
		{
			postEffectType = GrayScale;
		}
		if (ImGui::Selectable("VigRedGrayScale", postEffectType == VigRedGrayScale))
		{
			postEffectType = VigRedGrayScale;
		}
		ImGui::EndTabItem();
	}
	ImGui::EndTabBar();

	ImGui::Separator();

	if (postEffectType == VigRedGrayScale || postEffectType == VignetteRed)
	{
		ImGui::Text("VignetteParam");
		ImGui::DragFloat("Intensity", &vignetteIntensity, 0.01f, 0.0f, 5.0f);
		ImGui::DragFloat("Power", &vignettePower, 0.01f, 0.0f, 5.0f);
	}

	ImGui::End();

	PostEffect::GetInstance()->SetVignetteParam(vignetteIntensity, vignettePower);

	imguiManager_->End();

	//imguiの描画
	imguiManager_->Draw();
#endif
	/// ========================================= ///
	///-------------------ImGui-------------------///
	/// ========================================= ///

	Draw2D::GetInstance()->Reset();

	// 描画後の処理
	dx12_->EndDraw();
}

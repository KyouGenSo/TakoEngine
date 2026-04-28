#include "SampleGame.h"

#include "Input.h"
#include "SceneManager.h"
#include "SampleSceneFactory.h"

using namespace Tako;

void SampleGame::Initialize()
{
  winApp_->SetWindowSize(1920, 1080);

  winApp_->SetWindowTitle(L"LE4A_12_キョウ_ゲンソ_Slash");

  TakoFramework::Initialize();

  // シーンの初期化
  sceneFactory_ = std::make_unique<SampleSceneFactory>();
  SceneManager::GetInstance()->SetSceneFactory(sceneFactory_.get());
  SceneManager::GetInstance()->ChangeScene("sample", 0.0f);
}

void SampleGame::Finalize()
{
  TakoFramework::Finalize();
}

void SampleGame::Update()
{
  // F11キーでフルスクリーン切り替え
  if (Input::GetInstance()->TriggerKey(DIK_F11)) {
    ToggleFullScreen();
  }
  TakoFramework::Update();
}

void SampleGame::Draw()
{
  TakoFramework::Draw();
}
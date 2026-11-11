#include "SampleGame.h"

#include "Input.h"
#include "SceneManager.h"

using namespace Tako;

void SampleGame::Initialize()
{
  // ウィンドウのタイトル・サイズと起動シーンは resources/Json/ProjectSettings.json（Engine Settings で編集・保存）
  TakoFramework::Initialize();

  // シーンは resources/Json/Scenes のシーンファイルから作る（エディタで Actor を置いて保存する）
  SceneManager::GetInstance()->ChangeToStartupScene();
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
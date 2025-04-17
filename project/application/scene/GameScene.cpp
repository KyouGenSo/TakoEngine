#include "GameScene.h"
#include "ModelManager.h"
#include "Object3dBasic.h"
#include "SpriteBasic.h"
#include "Input.h"
#include "DebugCamera.h"
#include "Draw2D.h"
#include "GlobalVariables.h"
#include "FrameTimer.h"
#include "GPUParticle.h"
#include "SceneManager.h"
#include "EmitterManager.h"
#include "Object3d.h"

#include <numbers>

#ifdef _DEBUG
#include"ImGui.h"
#include "DebugCamera.h"
#endif

void GameScene::Initialize()
{
#ifdef _DEBUG
  DebugCamera::GetInstance()->Initialize();
  Object3dBasic::GetInstance()->SetDebug(false);
  Draw2D::GetInstance()->SetDebug(false);
  GPUParticle::GetInstance()->SetIsDebug(false);
#endif
  /// ================================== ///
  ///              初期化処理              ///
  /// ================================== ///


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
    Draw2D::GetInstance()->SetDebug(!Draw2D::GetInstance()->GetDebug());
    GPUParticle::GetInstance()->SetIsDebug(!GPUParticle::GetInstance()->GetIsDebug());
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




  // シーン遷移
  if (Input::GetInstance()->TriggerKey(DIK_RETURN))
  {
    //SceneManager::GetInstance()->ChangeScene("title");
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

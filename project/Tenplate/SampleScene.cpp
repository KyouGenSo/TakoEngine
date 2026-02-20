#include "SampleScene.h"

#include "Draw2D.h"
#include "GPUParticle.h"
#include "Input.h"
#include "Object3dBasic.h"
#include "SceneManager.h"
#include "SpriteBasic.h"

#ifdef _DEBUG
#include"ImGui.h"
#include "DebugCamera.h"
#endif

using namespace Tako;

void SampleScene::Initialize()
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


void SampleScene::Finalize()
{

}

void SampleScene::Update()
{
  /// ================================== ///
  ///              更新処理               ///
  /// ================================== ///



  if (Input::GetInstance()->TriggerKey(DIK_RETURN))
  {
    SceneManager::GetInstance()->ChangeScene("");
  }
}

void SampleScene::Draw()
{
  /// ================================== ///
  ///              描画処理               ///
  /// ================================== ///
  //------------------背景Spriteの描画------------------//
  // スプライト共通描画設定
  SpriteBasic::GetInstance()->SetCommonRenderSetting();




  //-------------------Modelの描画-------------------//
  // 3Dモデル共通描画設定
  Object3dBasic::GetInstance()->SetCommonRenderSetting();




  //------------------前景Spriteの描画------------------//
  // スプライト共通描画設定
  SpriteBasic::GetInstance()->SetCommonRenderSetting();



}

void SampleScene::DrawWithoutEffect()
{
  /// ================================== ///
  ///              描画処理               ///
  /// ================================== ///

  //------------------背景Spriteの描画------------------//
  // スプライト共通描画設定
  SpriteBasic::GetInstance()->SetCommonRenderSetting();




  //-------------------Modelの描画-------------------//
  // 3Dモデル共通描画設定
  Object3dBasic::GetInstance()->SetCommonRenderSetting();





  //------------------前景Spriteの描画------------------//
  // スプライト共通描画設定
  SpriteBasic::GetInstance()->SetCommonRenderSetting();




}

void SampleScene::DrawImGui()
{
#ifdef _DEBUG

  /// ================================== ///
  ///             ImGuiの描画              ///
  /// ================================== ///


#endif // _DEBUG
}
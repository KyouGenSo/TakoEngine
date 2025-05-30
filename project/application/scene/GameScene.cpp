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



  GPUParticle* particleSystem = GPUParticle::GetInstance();

  emitterManager_ = std::make_unique<EmitterManager>(particleSystem);

  // 構造体をすべてのフィールドを明示的に初期化
  spEmitterSett_ = {
    .position = {.x = 0.0f, .y = 7.38f, .z = -27.0f },
    .radius = 0.001f,
    .count = 60,
    .frequency = 0.9f,
    .scaleRangeX = {.x = 0.05f, .y = 0.05f},
    .scaleRangeY = {.x = 1.0f, .y = 1.0f},
    .velRangeX = {.x = 0.001, .y = 0.001},
    .velRangeY = {.x = 0.001, .y = 0.001},
    .velRangeZ = {.x = 0.001, .y = 0.001},
    .lifeTimeRange = {.x = 0.0f, .y = 0.0f}
  };


  isRandomRotateZ = true;
  isActive = true;

  // 球体エミッターの作成
  emitterManager_->CreateSphereEmitter("HitEffect", spEmitterSett_.position, spEmitterSett_.radius, spEmitterSett_.count, spEmitterSett_.frequency);
  emitterManager_->SetEmitterColors("HitEffect", { .x = 121.0f/255.f, .y = 101.0f/255.f, .z = 193.0f / 255.f, .w = 1.0f }, { .x = 141.0f / 255.f, .y = 216.0f / 255.f, .z = 255.0f / 255.f, .w = 1.0f });
  emitterManager_->SetEmitterScaleRange("HitEffect", spEmitterSett_.scaleRangeX, spEmitterSett_.scaleRangeY);
  emitterManager_->SetEmitterVelocityRange("HitEffect", spEmitterSett_.velRangeX, spEmitterSett_.velRangeY, spEmitterSett_.velRangeZ);
  emitterManager_->SetEmitterLifeTimeRange("HitEffect", spEmitterSett_.lifeTimeRange);
  emitterManager_->SetEmitterRandomRotateZ("HitEffect", isRandomRotateZ);
  emitterManager_->SetEmitterActive("HitEffect", isActive);

}

void GameScene::Finalize()
{
  delete object3d_;
  delete object3d2_;

  emitterManager_->RemoveAllEmitters();
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






  emitterManager_->Update();

  emitterManager_->UpdateSphereEmitter("HitEffect", spEmitterSett_.position, spEmitterSett_.radius, spEmitterSett_.count, spEmitterSett_.frequency);
  emitterManager_->SetEmitterScaleRange("HitEffect", spEmitterSett_.scaleRangeX, spEmitterSett_.scaleRangeY);
  emitterManager_->SetEmitterVelocityRange("HitEffect", spEmitterSett_.velRangeX, spEmitterSett_.velRangeY, spEmitterSett_.velRangeZ);
  emitterManager_->SetEmitterLifeTimeRange("HitEffect", spEmitterSett_.lifeTimeRange);
  emitterManager_->SetEmitterRandomRotateZ("HitEffect", isRandomRotateZ);
  emitterManager_->SetEmitterActive("HitEffect", isActive);

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

  


  //-------------------Modelの描画-------------------//
  // 3Dモデル共通描画設定
  Object3dBasic::GetInstance()->SetCommonRenderSetting();





  //------------------前景Spriteの描画------------------//
  // スプライト共通描画設定
  SpriteBasic::GetInstance()->SetCommonRenderSetting();

}

void GameScene::DrawWithoutEffect()
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

  

  //------------------------------------------------//


  //------------------前景Spriteの描画------------------//
  // スプライト共通描画設定
  SpriteBasic::GetInstance()->SetCommonRenderSetting();



  //--------------------------------------------------//
}

void GameScene::DrawImGui()
{
#ifdef _DEBUG




  // ImGui TabでEmitterの設定
  ImGui::Begin("Emitter Setting");

  if (ImGui::BeginTabBar("EmitterTab"))
  {
    if (ImGui::BeginTabItem("SphereEmitter"))
    {
      ImGui::DragFloat3("Position", &spEmitterSett_.position.x, 0.01f, -50.0f, 50.0f);
      ImGui::DragFloat("Radius", &spEmitterSett_.radius, 0.01f, 0.1f, 50.0f);
      int* count = reinterpret_cast<int*>(&spEmitterSett_.count);
      ImGui::DragInt("Count", count, 1, 1, 100);
      ImGui::DragFloat("Frequency", &spEmitterSett_.frequency, 0.01f, 0.1f, 10.0f);

      ImGui::DragFloat2("ScaleRangeX", &spEmitterSett_.scaleRangeX.x, 0.01f, -10.0f, 10.0f);
      ImGui::DragFloat2("ScaleRangeY", &spEmitterSett_.scaleRangeY.x, 0.01f, -10.0f, 10.0f);
      ImGui::DragFloat2("VelRangeX", &spEmitterSett_.velRangeX.x, 0.01f, -10.0f, 10.0f);
      ImGui::DragFloat2("VelRangeY", &spEmitterSett_.velRangeY.x, 0.01f, -10.0f, 10.0f);
      ImGui::DragFloat2("VelRangeZ", &spEmitterSett_.velRangeZ.x, 0.01f, -10.0f, 10.0f);
      ImGui::DragFloat2("LifeTimeRange", &spEmitterSett_.lifeTimeRange.x, 0.01f, 0.1f, 10.0f);
      ImGui::Checkbox("RandomRotateZ", &isRandomRotateZ);
      ImGui::Checkbox("Active", &isActive);

      ImGui::EndTabItem();
    }
    ImGui::EndTabBar();
  }

  ImGui::Separator();

  // Button to Create Temp Emitter
  if (ImGui::Button("Create Sphere Temp Emitter"))
  {
    emitterManager_->CreateTemporaryEmitterFrom("HitEffect", "spTemp", 1.0f);
  }
  ImGui::End();



  ImGui::End();

#endif // DEBUG
}

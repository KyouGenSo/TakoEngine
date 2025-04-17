#include "GameScene.h"
#include "ModelManager.h"
#include "Object3dBasic.h"
#include "SpriteBasic.h"
#include "Input.h"
#include "DebugCamera.h"
#include "Draw2D.h"
#include "FrameTimer.h"
#include "GPUParticle.h"
#include "SceneManager.h"
#include "EmitterManager.h"

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
    .position = {.x = 0.0f, .y = 0.0f, .z = 20.0f },
    .radius = 4.0f,
    .count = 100,
    .frequency = 1.0f,
    .scaleRangeX = {.x = 0.0f, .y = 0.0f},
    .scaleRangeY = {.x = 0.0f, .y = 0.0f},
    .velRangeX = {.x = -0.0f, .y = 0.0f},
    .velRangeY = {.x = -0.0f, .y = 0.0f},
    .velRangeZ = {.x = -0.0f, .y = 0.0f},
    .lifeTimeRange = {.x = 0.5f, .y = 0.5f},
    .startColor = {.x = 0.5f, .y = 0.0f, .z = 0.0f, .w = 1.0f },
    .endColor = {.x = 0.15f, .y = 0.15f, .z = 0.0f, .w = 1.0f },
    .isActive = true
  };

  boxEmitterSett_ = {
    .position = {.x = 13.69f, .y = 0.0f, .z = 20.0f },
    .size = {.x = 10.f, .y = 10.f, .z = 10.f }, // y値をしっかり初期化
    .rotation = {.x = 0.0f, .y = 0.0f, .z = 0.0f },
    .count = 100,
    .frequency = 0.1f,
    .scaleRangeX = {.x = 0.0f, .y = 0.0f},
    .scaleRangeY = {.x = 0.0f, .y = 0.0f},
    .velRangeX = {.x = -0.0f, .y = 0.0f},
    .velRangeY = {.x = -0.0f, .y = 0.0f},
    .velRangeZ = {.x = -0.0f, .y = 0.0f},
    .lifeTimeRange = {.x = 0.5f, .y = 0.5f},
    .startColor = {.x = 0.0f, .y = 0.0f, .z = 1.0f, .w = 1.0f },
    .endColor = {.x = 1.0f, .y = 1.0f, .z = 1.0f, .w = 1.0f },
    .isActive = true
  };

  triEmitterSett_ = {
    .position = {.x = -17.68f, .y = -3.6f, .z = 20.0f },
    .v1 = {.x = 0.0f, .y = 10.0f, .z = 0.0f },
    .v2 = {.x = 10.0f, .y = 0.0f, .z = 0.0f },
    .v3 = {.x = 0.0f, .y = 0.0f, .z = 10.0f },
    .count = 100,
    .frequency = 0.5f,
    .scaleRangeX = {.x = 0.0f, .y = 0.0f},
    .scaleRangeY = {.x = 0.0f, .y = 0.0f},
    .velRangeX = {.x = -0.0f, .y = 0.0f},
    .velRangeY = {.x = -0.0f, .y = 0.0f},
    .velRangeZ = {.x = -0.0f, .y = 0.0f},
    .lifeTimeRange = {.x = 0.5f, .y = 0.5f},
    .startColor = {.x = 0.0f, .y = 1.0f, .z = 0.0f, .w = 1.0f },
    .endColor = {.x = 1.0f, .y = 1.0f, .z = 1.0f, .w = 1.0f },
    .isActive = true
  };

  // 球体エミッターの作成
  emitterManager_->CreateSphereEmitter("player", spEmitterSett_.position, spEmitterSett_.radius, spEmitterSett_.count, spEmitterSett_.frequency);
  emitterManager_->SetEmitterColors("player", spEmitterSett_.startColor, spEmitterSett_.endColor);
  emitterManager_->SetEmitterScaleRange("player", spEmitterSett_.scaleRangeX, spEmitterSett_.scaleRangeY);
  emitterManager_->SetEmitterVelocityRange("player", spEmitterSett_.velRangeX, spEmitterSett_.velRangeY, spEmitterSett_.velRangeZ);
  emitterManager_->SetEmitterLifeTimeRange("player", spEmitterSett_.lifeTimeRange);

  // 箱エミッターの作成 - すべてのプロパティを明示的に設定
  emitterManager_->CreateBoxEmitter("box", boxEmitterSett_.position, boxEmitterSett_.size, boxEmitterSett_.rotation, boxEmitterSett_.count, boxEmitterSett_.frequency);
  emitterManager_->SetEmitterColor("box", boxEmitterSett_.startColor);
  emitterManager_->SetEmitterScaleRange("box", boxEmitterSett_.scaleRangeX, boxEmitterSett_.scaleRangeY);
  emitterManager_->SetEmitterVelocityRange("box", boxEmitterSett_.velRangeX, boxEmitterSett_.velRangeY, boxEmitterSett_.velRangeZ);
  emitterManager_->SetEmitterLifeTimeRange("box", boxEmitterSett_.lifeTimeRange);

  // 三角形エミッターの作成 - すべてのプロパティを明示的に設定
  emitterManager_->CreateTriangleEmitter("triangle", triEmitterSett_.position, triEmitterSett_.v1, triEmitterSett_.v2, triEmitterSett_.v3, triEmitterSett_.count, triEmitterSett_.frequency);
  emitterManager_->SetEmitterColor("triangle", triEmitterSett_.startColor);
  emitterManager_->SetEmitterScaleRange("triangle", triEmitterSett_.scaleRangeX, triEmitterSett_.scaleRangeY);
  emitterManager_->SetEmitterVelocityRange("triangle", triEmitterSett_.velRangeX, triEmitterSett_.velRangeY, triEmitterSett_.velRangeZ);
  emitterManager_->SetEmitterLifeTimeRange("triangle", triEmitterSett_.lifeTimeRange);
}

void GameScene::Finalize()
{
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

  emitterManager_->UpdateSphereEmitter("player", spEmitterSett_.position, spEmitterSett_.radius, spEmitterSett_.count, spEmitterSett_.frequency);
  emitterManager_->UpdateBoxEmitter("box", boxEmitterSett_.position, boxEmitterSett_.size, boxEmitterSett_.rotation, boxEmitterSett_.count, boxEmitterSett_.frequency);
  emitterManager_->UpdateTriangleEmitter("triangle", triEmitterSett_.position, triEmitterSett_.v1, triEmitterSett_.v2, triEmitterSett_.v3, triEmitterSett_.count, triEmitterSett_.frequency);

  emitterManager_->SetEmitterScaleRange("player", spEmitterSett_.scaleRangeX, spEmitterSett_.scaleRangeY);
  emitterManager_->SetEmitterScaleRange("box", boxEmitterSett_.scaleRangeX, boxEmitterSett_.scaleRangeY);
  emitterManager_->SetEmitterScaleRange("triangle", triEmitterSett_.scaleRangeX, triEmitterSett_.scaleRangeY);

  emitterManager_->SetEmitterVelocityRange("player", spEmitterSett_.velRangeX, spEmitterSett_.velRangeY, spEmitterSett_.velRangeZ);
  emitterManager_->SetEmitterVelocityRange("box", boxEmitterSett_.velRangeX, boxEmitterSett_.velRangeY, boxEmitterSett_.velRangeZ);
  emitterManager_->SetEmitterVelocityRange("triangle", triEmitterSett_.velRangeX, triEmitterSett_.velRangeY, triEmitterSett_.velRangeZ);

  emitterManager_->SetEmitterLifeTimeRange("player", spEmitterSett_.lifeTimeRange);
  emitterManager_->SetEmitterLifeTimeRange("box", boxEmitterSett_.lifeTimeRange);
  emitterManager_->SetEmitterLifeTimeRange("triangle", triEmitterSett_.lifeTimeRange);

  // 色の更新
  emitterManager_->SetEmitterColors("player", spEmitterSett_.startColor, spEmitterSett_.endColor);
  emitterManager_->SetEmitterColor("box", boxEmitterSett_.startColor);
  emitterManager_->SetEmitterColor("triangle", triEmitterSett_.startColor);

  // 有効状態の更新
  emitterManager_->SetEmitterActive("player", spEmitterSett_.isActive);
  emitterManager_->SetEmitterActive("box", boxEmitterSett_.isActive);
  emitterManager_->SetEmitterActive("triangle", triEmitterSett_.isActive);

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

      ImGui::DragFloat2("ScaleRangeX", &spEmitterSett_.scaleRangeX.x, 0.01f, -100.0f, 100.0f);
      ImGui::DragFloat2("ScaleRangeY", &spEmitterSett_.scaleRangeY.x, 0.01f, -100.0f, 100.0f);
      ImGui::DragFloat2("VelRangeX", &spEmitterSett_.velRangeX.x, 0.01f, -100.0f, 100.0f);
      ImGui::DragFloat2("VelRangeY", &spEmitterSett_.velRangeY.x, 0.01f, -100.0f, 100.0f);
      ImGui::DragFloat2("VelRangeZ", &spEmitterSett_.velRangeZ.x, 0.01f, -100.0f, 100.0f);
      ImGui::DragFloat2("LifeTimeRange", &spEmitterSett_.lifeTimeRange.x, 0.01f, 0.1f, 50.0f);
      ImGui::ColorEdit4("StartColor", &spEmitterSett_.startColor.x);
      ImGui::ColorEdit4("EndColor", &spEmitterSett_.endColor.x);
      ImGui::Checkbox("IsActive", &spEmitterSett_.isActive);

      ImGui::EndTabItem();
    }
    if (ImGui::BeginTabItem("BoxEmitter"))
    {
      ImGui::DragFloat3("Position", &boxEmitterSett_.position.x, 0.01f, -50.0f, 50.0f);
      ImGui::DragFloat3("Size", &boxEmitterSett_.size.x, 0.01f, 0.1f, 50.0f);
      ImGui::DragFloat3("Rotation", &boxEmitterSett_.rotation.x, 0.01f, DirectX::XMConvertToRadians(-180.0f), DirectX::XMConvertToRadians(180.0f));
      int* count = reinterpret_cast<int*>(&boxEmitterSett_.count);
      ImGui::DragInt("Count", count, 1, 1, 100);
      ImGui::DragFloat("Frequency", &boxEmitterSett_.frequency, 0.01f, 0.1f, 10.0f);
      ImGui::DragFloat2("ScaleRangeX", &boxEmitterSett_.scaleRangeX.x, 0.01f, -100.0f, 100.0f);
      ImGui::DragFloat2("ScaleRangeY", &boxEmitterSett_.scaleRangeY.x, 0.01f, -100.0f, 100.0f);
      ImGui::DragFloat2("VelRangeX", &boxEmitterSett_.velRangeX.x, 0.01f, -100.0f, 100.0f);
      ImGui::DragFloat2("VelRangeY", &boxEmitterSett_.velRangeY.x, 0.01f, -100.0f, 100.0f);
      ImGui::DragFloat2("VelRangeZ", &boxEmitterSett_.velRangeZ.x, 0.01f, -100.0f, 100.0f);
      ImGui::DragFloat2("LifeTimeRange", &boxEmitterSett_.lifeTimeRange.x, 0.01f, 0.1f, 50.0f);
      ImGui::ColorEdit4("Color", &boxEmitterSett_.startColor.x);
      ImGui::Checkbox("IsActive", &boxEmitterSett_.isActive);

      ImGui::EndTabItem();
    }
    if (ImGui::BeginTabItem("TriangleEmitter"))
    {
      ImGui::DragFloat3("Position", &triEmitterSett_.position.x, 0.01f, -50.0f, 50.0f);
      ImGui::DragFloat3("V1", &triEmitterSett_.v1.x, 0.01f, -50.0f, 50.0f);
      ImGui::DragFloat3("V2", &triEmitterSett_.v2.x, 0.01f, -50.0f, 50.0f);
      ImGui::DragFloat3("V3", &triEmitterSett_.v3.x, 0.01f, -50.0f, 50.0f);
      int* count = reinterpret_cast<int*>(&triEmitterSett_.count);
      ImGui::DragInt("Count", count, 1, 1, 100);
      ImGui::DragFloat("Frequency", &triEmitterSett_.frequency, 0.01f, 0.1f, 10.0f);
      ImGui::DragFloat2("ScaleRangeX", &triEmitterSett_.scaleRangeX.x, 0.01f, -100.0f, 100.0f);
      ImGui::DragFloat2("ScaleRangeY", &triEmitterSett_.scaleRangeY.x, 0.01f, -100.0f, 100.0f);
      ImGui::DragFloat2("VelRangeX", &triEmitterSett_.velRangeX.x, 0.01f, -100.0f, 100.0f);
      ImGui::DragFloat2("VelRangeY", &triEmitterSett_.velRangeY.x, 0.01f, -100.0f, 100.0f);
      ImGui::DragFloat2("VelRangeZ", &triEmitterSett_.velRangeZ.x, 0.01f, -100.0f, 100.0f);
      ImGui::DragFloat2("LifeTimeRange", &triEmitterSett_.lifeTimeRange.x, 0.01f, 0.1f, 50.0f);
      ImGui::ColorEdit4("Color", &triEmitterSett_.startColor.x);
      ImGui::Checkbox("IsActive", &triEmitterSett_.isActive);

      ImGui::EndTabItem();
    }
    ImGui::EndTabBar();
  }

  ImGui::End();


  ImGui::Begin("Temporary Emitter");

  // Button to Create Temp Emitter
  if (ImGui::Button("Create Sphere Temp Emitter"))
  {
    emitterManager_->CreateTemporaryEmitterFrom("player", "spTemp", 1.0f);
  }

  if (ImGui::Button("Create Box Temp Emitter"))
  {
    emitterManager_->CreateTemporaryEmitterFrom("box", "boxTemp", 1.0f);
  }

  if (ImGui::Button("Create Triangle Temp Emitter"))
  {
    emitterManager_->CreateTemporaryEmitterFrom("triangle", "triTemp", 1.0f);
  }

  // Button to Create Temp Emitter
  if (ImGui::Button("Create all 3 Temp Emitter"))
  {
    emitterManager_->CreateTemporaryEmitterFrom("player", "spTemp", 1.0f);
    emitterManager_->CreateTemporaryEmitterFrom("box", "boxTemp", 1.0f);
    emitterManager_->CreateTemporaryEmitterFrom("triangle", "triTemp", 1.0f);
  }

  ImGui::End();


#endif // DEBUG
}

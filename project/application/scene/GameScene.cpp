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

  object3d_ = new Object3d();
  object3d_->Initialize();
  object3d_->SetModel("terrain.obj");

  // y軸90度回転
  Vector3 rotate = { .x = 0.0f, .y = DirectX::XMConvertToRadians(90.0f), .z = 0.0f };
  object3d_->SetRotate(rotate);

  modelPos_ = { .x = 0.0f, .y = 0.0f, .z = 0.0f };

  object3d2_ = new Object3d();
  object3d2_->Initialize();
  object3d2_->SetModel("sneakWalk.gltf");
  modelRotate2_ = { .x = 0.0f, .y = DirectX::XMConvertToRadians(180.0f), .z = 0.0f };
  object3d2_->SetRotate(rotate);

  modelPos2_ = { .x = 0.0f, .y = 6.7f, .z = -24.0f };

  GPUParticle* particleSystem = GPUParticle::GetInstance();

  emitterManager_ = std::make_unique<EmitterManager>(particleSystem);

  // 構造体をすべてのフィールドを明示的に初期化
  spEmitterSett_ = {
    .position = {.x = 0.0f, .y = 0.0f, .z = 0.0f },
    .radius = 10.0f,
    .count = 10,
    .frequency = 1.0f,
    .scaleRangeX = {.x = 0.0f, .y = 0.0f},
    .scaleRangeY = {.x = 0.0f, .y = 0.0f},
    .velRangeX = {.x = -0.0f, .y = 0.0f},
    .velRangeY = {.x = -0.0f, .y = 0.0f},
    .velRangeZ = {.x = -0.0f, .y = 0.0f},
    .lifeTimeRange = {.x = 0.0f, .y = 0.0f}
  };

  boxEmitterSett_ = {
    .position = {.x = 0.0f, .y = 5.0f, .z = 0.0f },
    .size = {.x = 0.3f, .y = 0.3f, .z = 0.3f }, // y値をしっかり初期化
    .rotation = {.x = 0.0f, .y = 0.0f, .z = 0.0f },
    .count = 10,
    .frequency = 0.5f,
    .scaleRangeX = {.x = 0.0f, .y = 0.0f},
    .scaleRangeY = {.x = 0.0f, .y = 0.0f},
    .velRangeX = {.x = -0.0f, .y = 0.0f},
    .velRangeY = {.x = -0.0f, .y = 0.0f},
    .velRangeZ = {.x = -0.0f, .y = 0.0f},
    .lifeTimeRange = {.x = 1.0f, .y = 3.0f}

  };

  triEmitterSett_ = {
    .position = {.x = 0.0f, .y = 0.0f, .z = 0.0f },
    .v1 = {.x = 0.0f, .y = 0.0f, .z = 1.0f },
    .v2 = {.x = 1.0f, .y = 0.0f, .z = 0.0f },
    .v3 = {.x = 0.0f, .y = 1.0f, .z = 0.0f },
    .count = 10,
    .frequency = 0.1f,
    .scaleRangeX = {.x = 0.0f, .y = 0.0f},
    .scaleRangeY = {.x = 0.0f, .y = 0.0f},
    .velRangeX = {.x = -0.0f, .y = 0.0f},
    .velRangeY = {.x = -0.0f, .y = 0.0f},
    .velRangeZ = {.x = -0.0f, .y = 0.0f},
    .lifeTimeRange = {.x = 1.0f, .y = 3.0f}
  };

  // 球体エミッターの作成
  emitterManager_->CreateSphereEmitter("player", spEmitterSett_.position, spEmitterSett_.radius, spEmitterSett_.count, spEmitterSett_.frequency);
  emitterManager_->SetEmitterColors("player", { .x = 1.0f, .y = 0.0f, .z = 0.0f, .w = 1.0f }, { .x = 0.0f, .y = 1.0f, .z = 0.0f, .w = 1.0f });
  emitterManager_->SetEmitterScaleRange("player", spEmitterSett_.scaleRangeX, spEmitterSett_.scaleRangeY);
  emitterManager_->SetEmitterVelocityRange("player", spEmitterSett_.velRangeX, spEmitterSett_.velRangeY, spEmitterSett_.velRangeZ);
  emitterManager_->SetEmitterLifeTimeRange("player", spEmitterSett_.lifeTimeRange);

  // 箱エミッターの作成 - すべてのプロパティを明示的に設定
  emitterManager_->CreateBoxEmitter("box", boxEmitterSett_.position, boxEmitterSett_.size, boxEmitterSett_.rotation, boxEmitterSett_.count, boxEmitterSett_.frequency);
  emitterManager_->SetEmitterColor("box", { .x = 0.0f, .y = 1.0f, .z = 0.0f, .w = 1.0f });
  emitterManager_->SetEmitterScaleRange("box", boxEmitterSett_.scaleRangeX, boxEmitterSett_.scaleRangeY);
  emitterManager_->SetEmitterVelocityRange("box", boxEmitterSett_.velRangeX, boxEmitterSett_.velRangeY, boxEmitterSett_.velRangeZ);
  emitterManager_->SetEmitterLifeTimeRange("box", boxEmitterSett_.lifeTimeRange);

  // 三角形エミッターの作成 - すべてのプロパティを明示的に設定
  emitterManager_->CreateTriangleEmitter("triangle", triEmitterSett_.position, triEmitterSett_.v1, triEmitterSett_.v2, triEmitterSett_.v3, triEmitterSett_.count, triEmitterSett_.frequency);
  emitterManager_->SetEmitterColor("triangle", { .x = 0.0f, .y = 0.0f, .z = 1.0f, .w = 1.0f });
  emitterManager_->SetEmitterScaleRange("triangle", triEmitterSett_.scaleRangeX, triEmitterSett_.scaleRangeY);
  emitterManager_->SetEmitterVelocityRange("triangle", triEmitterSett_.velRangeX, triEmitterSett_.velRangeY, triEmitterSett_.velRangeZ);
  emitterManager_->SetEmitterLifeTimeRange("triangle", triEmitterSett_.lifeTimeRange);

  emitterManager_->CreateGroup("group1");
  emitterManager_->AddToGroup("group1", "player");
  emitterManager_->AddToGroup("group1", "box");
  emitterManager_->AddToGroup("group1", "triangle");

  emitterManager_->SetGroupActive("group1", false);

  groupPosition_ = { .x = 0.0f, .y = 10.0f, .z = 0.0f };
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

  object3d_->SetScale(modelScale_);
  object3d_->SetTranslate(modelPos_);
  object3d_->SetRotate(modelRotate_);
  object3d_->SetShininess(shininess_);
  object3d_->SetEnableLighting(isLighting_);
  object3d_->SetEnableHighlight(isHighlight_);

  object3d2_->SetScale(modelScale2_);
  object3d2_->SetTranslate(modelPos2_);
  object3d2_->SetRotate(modelRotate2_);
  object3d2_->SetShininess(shininess_);
  object3d2_->SetEnableLighting(isLighting_);
  object3d2_->SetEnableHighlight(isHighlight_);

  object3d_->Update();
  object3d2_->Update();

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

  emitterManager_->SetGroupPosition("group1", groupPosition_);

  // ライトの設定
  Object3dBasic::GetInstance()->SetDirectionalLight(lightDirection_, lightColor_, 1, lightIntensity_);


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



  //--------------------------------------------------//


  //-------------------Modelの描画-------------------//
  // 3Dモデル共通描画設定
  Object3dBasic::GetInstance()->SetCommonRenderSetting();

  // モデル描画
  object3d_->Draw();
  object3d2_->Draw();



  //-------------------Modelの描画-------------------//


  //------------------前景Spriteの描画------------------//
  // スプライト共通描画設定
  SpriteBasic::GetInstance()->SetCommonRenderSetting();



  //--------------------------------------------------//
}

void GameScene::DrawImGui()
{
#ifdef _DEBUG
  ImGui::Begin("object3d");
  ImGui::DragFloat3("Scale", &modelScale_.x, 0.01f, 0.1f, 50.0f);
  ImGui::DragFloat3("Position", &modelPos_.x, 0.01f, -50.0f, 50.0f);
  ImGui::DragFloat3("Rotate", &modelRotate_.x, 0.01f, DirectX::XMConvertToRadians(-180.0f), DirectX::XMConvertToRadians(180.0f));
  ImGui::End();

  ImGui::Begin("object3d2");
  ImGui::DragFloat3("Scale", &modelScale2_.x, 0.01f, 0.1f, 50.0f);
  ImGui::DragFloat3("Position", &modelPos2_.x, 0.01f, -50.0f, 50.0f);
  ImGui::DragFloat3("Rotate", &modelRotate2_.x, 0.01f, DirectX::XMConvertToRadians(-180.0f), DirectX::XMConvertToRadians(180.0f));
  ImGui::End();

  // Lightの設定
  ImGui::Begin("Directional Light");
  ImGui::Separator();
  ImGui::DragFloat3("Direction", &lightDirection_.x, 0.01f, -1.0f, 1.0f);
  ImGui::DragFloat("Intensity", &lightIntensity_, 0.01f, 0.0f, 10.0f);
  ImGui::SliderFloat("Shininess", &shininess_, 1.0f, 1000.0f);
  ImGui::ColorEdit4("Color", &lightColor_.x);
  ImGui::Checkbox("Lighting", &isLighting_);
  ImGui::Checkbox("Highlight", &isHighlight_);
  ImGui::End();

  ImGui::Begin("GameTime");
  ImGui::Text("GameTime : %f", FrameTimer::GetInstance()->GetGameTime());
  ImGui::End();

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



      ImGui::EndTabItem();
    }
    ImGui::EndTabBar();
  }

  ImGui::Separator();

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
    auto emitter = emitterManager_->GetEmitterByName("triTemp");
    emitter->SetPosition({ .x = 0.0f, .y = 0.0f, .z = 0.0f });
  }

  ImGui::End();

  ImGui::Begin("Particle");

  ImGui::DragFloat3("Group Position", &groupPosition_.x, 0.01f, -50.0f, 50.0f);

  // Button to remove all TimedEmitters
  if (ImGui::Button("Remove AllEmitters"))
  {
    // Remove all TimedEmitters
    //emitterManager_->ClearAllTimedEffects();
    emitterManager_->RemoveAllEmitters();
  }

  // Button to Set Group Active
  if (ImGui::Button("Set Group Active"))
  {
    isActive_ = !isActive_;
  }

  ImGui::End();

#endif // DEBUG
}

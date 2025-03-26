#include "GameScene.h"
#include "ModelManager.h"
#include "Object3dBasic.h"
#include "TextureManager.h"
#include "SpriteBasic.h"
#include "Input.h"
#include "DebugCamera.h"
#include "Draw2D.h"
#include "GlobalVariables.h"
#include "ParticleManager.h"
#include "FrameTimer.h"
#include "GPUParticle.h"
#include "SceneManager.h"
#include "SphereEmitter.h"
#include "BoxEmitter.h"
#include "TriangleEmitter.h"

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
  ParticleManager::GetInstance()->SetIsDebug(false);
  GPUParticle::GetInstance()->SetIsDebug(false);
#endif
  /// ================================== ///
  ///              初期化処理              ///
  /// ================================== ///

  object3d_ = new Object3d();
  object3d_->Initialize();
  object3d_->SetModel("terrain.obj");
  // y軸90度回転
  Vector3 rotate = { 0.0f, DirectX::XMConvertToRadians(90.0f), 0.0f };
  object3d_->SetRotate(rotate);

  modelPos_ = { 0.0f, 0.0f, 0.0f };

  object3d2_ = new Object3d();
  object3d2_->Initialize();
  object3d2_->SetModel("sneakWalk.gltf");
  modelRotate2_ = { 0.0f, DirectX::XMConvertToRadians(180.0f), 0.0f };
  object3d2_->SetRotate(rotate);

  modelPos2_ = { 0.0f, 6.7f, -24.0f };

  GPUParticle* particleSystem = GPUParticle::GetInstance();

  emitterManager_ = std::make_unique<EmitterManager>(particleSystem);

  spEmitterSett_ = { { 0.0f, 0.0f, 0.0f }, 10.0f,  10, 1.0f };
  boxEmitterSett_ = { { 0.0f, 5.0f, 0.0f }, { 0.3f, 0.0f, 0.1f }, { 0.0f, 0.0f, 0.0f }, 10, 0.5f };
  triEmitterSett_ = { { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 1.0f }, { 1.0f, 0.0f, 0.0f }, { 0.0f, 1.0f, 0.0f }, 10, 0.1f };

  //// 球体エミッターの作成
  //sphereEmitter_ = emitterManager_->CreateSphereEmitter("player", spEmitterSett_.position, spEmitterSett_.radius, spEmitterSett_.count, spEmitterSett_.frequency);
  //sphereEmitter_->SetColor({ 0.0f, 0.0f, 1.0f, 1.0f });

  ////箱エミッターの作成
  //boxEmitter_ = emitterManager_->CreateBoxEmitter("box", boxEmitterSett_.position, boxEmitterSett_.size, boxEmitterSett_.rotation, boxEmitterSett_.count, boxEmitterSett_.frequency);
  //boxEmitter_->SetColor({ 1.0f, 0.0f, 0.0f, 1.0f });

  ////三角形エミッターの作成
  //triangleEmitter_ = emitterManager_->CreateTriangleEmitter("triangle", triEmitterSett_.position, triEmitterSett_.v1, triEmitterSett_.v2, triEmitterSett_.v3, triEmitterSett_.count, triEmitterSett_.frequency);
  //triangleEmitter_->SetColor({ 0.0f, 1.0f, 1.0f, 1.0f });

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
  object3d_->SetMaterialColor(materialColor1_);

  object3d2_->SetScale(modelScale2_);
  object3d2_->SetTranslate(modelPos2_);
  object3d2_->SetRotate(modelRotate2_);
  object3d2_->SetShininess(shininess_);
  object3d2_->SetEnableLighting(isLighting_);
  object3d2_->SetEnableHighlight(isHighlight_);
  object3d2_->SetMaterialColor(materialColor2_);

  object3d_->Update();
  object3d2_->Update();


  //sphereEmitter_->SetPosition(spEmitterSett_.position);
  //sphereEmitter_->SetRadius(spEmitterSett_.radius);
  //sphereEmitter_->SetParticleCount(spEmitterSett_.count);
  //sphereEmitter_->SetFrequency(spEmitterSett_.frequency);

  //boxEmitter_->SetPosition(boxEmitterSett_.position);
  //boxEmitter_->SetSize(boxEmitterSett_.size);
  //boxEmitter_->SetRotation(boxEmitterSett_.rotation);
  //boxEmitter_->SetParticleCount(boxEmitterSett_.count);
  //boxEmitter_->SetFrequency(boxEmitterSett_.frequency);

  //triangleEmitter_->SetPosition(triEmitterSett_.position);
  //triangleEmitter_->SetVertices(triEmitterSett_.v1, triEmitterSett_.v2, triEmitterSett_.v3);
  //triangleEmitter_->SetParticleCount(triEmitterSett_.count);
  //triangleEmitter_->SetFrequency(triEmitterSett_.frequency);


  emitterManager_->Update();

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
  ImGui::ColorEdit4("MaterialColor1", &materialColor1_.x);
  ImGui::End();

  ImGui::Begin("object3d2");
  ImGui::DragFloat3("Scale", &modelScale2_.x, 0.01f, 0.1f, 50.0f);
  ImGui::DragFloat3("Position", &modelPos2_.x, 0.01f, -50.0f, 50.0f);
  ImGui::DragFloat3("Rotate", &modelRotate2_.x, 0.01f, DirectX::XMConvertToRadians(-180.0f), DirectX::XMConvertToRadians(180.0f));
  ImGui::ColorEdit4("MaterialColor2", &materialColor2_.x);
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

  //// ImGui TabでEmitterの設定
  //ImGui::Begin("Emitter Setting");

  //if (ImGui::BeginTabBar("EmitterTab"))
  //{
  //  if (ImGui::BeginTabItem("SphereEmitter"))
  //  {
  //    ImGui::DragFloat3("Position", &spEmitterSett_.position.x, 0.01f, -50.0f, 50.0f);
  //    ImGui::DragFloat("Radius", &spEmitterSett_.radius, 0.01f, 0.1f, 50.0f);
  //    int* count = reinterpret_cast<int*>(&spEmitterSett_.count);
  //    ImGui::DragInt("Count", count, 1, 1, 100);
  //    ImGui::DragFloat("Frequency", &spEmitterSett_.frequency, 0.01f, 0.1f, 10.0f);


  //    ImGui::EndTabItem();
  //  }
  //  if (ImGui::BeginTabItem("BoxEmitter"))
  //  {
  //    ImGui::DragFloat3("Position", &boxEmitterSett_.position.x, 0.01f, -50.0f, 50.0f);
  //    ImGui::DragFloat3("Size", &boxEmitterSett_.size.x, 0.01f, 0.1f, 50.0f);
  //    ImGui::DragFloat3("Rotation", &boxEmitterSett_.rotation.x, 0.01f, DirectX::XMConvertToRadians(-180.0f), DirectX::XMConvertToRadians(180.0f));
  //    int* count = reinterpret_cast<int*>(&boxEmitterSett_.count);
  //    ImGui::DragInt("Count", count, 1, 1, 100);
  //    ImGui::DragFloat("Frequency", &boxEmitterSett_.frequency, 0.01f, 0.1f, 10.0f);


  //    ImGui::EndTabItem();
  //  }
  //  if (ImGui::BeginTabItem("TriangleEmitter"))
  //  {
  //    ImGui::DragFloat3("Position", &triEmitterSett_.position.x, 0.01f, -50.0f, 50.0f);
  //    ImGui::DragFloat3("V1", &triEmitterSett_.v1.x, 0.01f, -50.0f, 50.0f);
  //    ImGui::DragFloat3("V2", &triEmitterSett_.v2.x, 0.01f, -50.0f, 50.0f);
  //    ImGui::DragFloat3("V3", &triEmitterSett_.v3.x, 0.01f, -50.0f, 50.0f);
  //    int* count = reinterpret_cast<int*>(&triEmitterSett_.count);
  //    ImGui::DragInt("Count", count, 1, 1, 100);
  //    ImGui::DragFloat("Frequency", &triEmitterSett_.frequency, 0.01f, 0.1f, 10.0f);



  //    ImGui::EndTabItem();
  //  }
  //  ImGui::EndTabBar();
  //}

  //ImGui::End();

  ImGui::Begin("Particle");

  // Button to create a new Timed SphereEmitter
  if (ImGui::Button("Create Sphere Emitter"))
  {
    // Create a new SphereEmitter
    emitterManager_->MakeTimedSphereEmitter("timedSphere", spEmitterSett_.position, spEmitterSett_.radius, spEmitterSett_.count, spEmitterSett_.frequency, 1.0f);
  }

  // Button to remove all TimedEmitters
  if (ImGui::Button("Remove All Timed Emitters"))
  {
    // Remove all TimedEmitters
    //emitterManager_->ClearAllTimedEffects();
    emitterManager_->RemoveAllEmitters();
  }

  ImGui::End();

#endif // DEBUG
}

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
#include "Model.h"

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

  // SkyBoxの初期化
  skyBox_ = std::make_unique<SkyBox>();
  skyBox_->Initialize("my_skybox.dds");

  characterModel_ = new Object3d();
  characterModel_->Initialize();
  characterModel_->SetModel("sneakwalk.gltf", true, true);
  characterTransform2_.translate = { .x = 0.0f, .y = 6.f, .z = -24.0f };
  characterTransform2_.scale = { .x = 1.0f, .y = 1.0f, .z = 1.0f };
  characterTransform2_.rotate = { .x = 0.0f, .y = DirectX::XMConvertToRadians(180.0f), .z = 0.0f };
  characterModel_->SetTransform(characterTransform2_);
  characterModel_->SetEnvironmentTexture(skyBox_->GetTextureIndex());

  GPUParticle* particleSystem = GPUParticle::GetInstance();

  emitterManager_ = std::make_unique<EmitterManager>(particleSystem);

  sphereEmitterSett_.name = "sphere";
  sphereEmitterSett_.position = Vector3(-8.0f, 0.0f, 0.0f);
  sphereEmitterSett_.radius = 5.f;
  sphereEmitterSett_.count = 100;
  sphereEmitterSett_.frequency = 0.01f;
  sphereEmitterSett_.scaleRangeX = Vector2(0.5f, 0.5f);
  sphereEmitterSett_.scaleRangeY = Vector2(0.5f, 0.5f);
  sphereEmitterSett_.velRangeX = Vector2(0.0f, 0.0f);
  sphereEmitterSett_.velRangeY = Vector2(0.0f, 0.0f);
  sphereEmitterSett_.velRangeZ = Vector2(0.0f, 0.0f);
  sphereEmitterSett_.lifeTimeRange = Vector2(0.5f, 1.0f);
  sphereEmitterSett_.startColor = Vector4(1.0f, 1.0f, 1.0f, 1.0f);
  sphereEmitterSett_.endColor = Vector4(1.0f, 0.0f, 0.0f, 0.0f);
  sphereEmitterSett_.isActive = true;
  sphereEmitterSett_.isNormalize = false;
  emitterManager_->CreateSphereEmitter(sphereEmitterSett_);

  boxEmitterSett_.name = "box";
  boxEmitterSett_.position = Vector3(0.0f, 0.0f, 38.6f);
  boxEmitterSett_.size = Vector3(10.0f, 10.0f, 10.0f);
  boxEmitterSett_.count = 100;
  boxEmitterSett_.frequency = 0.01f;
  boxEmitterSett_.scaleRangeX = Vector2(0.5f, 0.5f);
  boxEmitterSett_.scaleRangeY = Vector2(0.5f, 0.5f);
  boxEmitterSett_.velRangeX = Vector2(0.0f, 0.0f);
  boxEmitterSett_.velRangeY = Vector2(0.0f, 0.0f);
  boxEmitterSett_.velRangeZ = Vector2(0.0f, 0.0f);
  boxEmitterSett_.lifeTimeRange = Vector2(0.5f, 1.0f);
  boxEmitterSett_.startColor = Vector4(1.0f, 1.0f, 1.0f, 1.0f);
  boxEmitterSett_.endColor = Vector4(0.0f, 0.0f, 1.0f, 0.0f);
  boxEmitterSett_.isActive = true;
  boxEmitterSett_.isNormalize = false;
  emitterManager_->CreateBoxEmitter(boxEmitterSett_);

  triangleEmitterSett_.name = "triangle";
  triangleEmitterSett_.position = Vector3(5.4f, 0.0f, 0.0f);
  triangleEmitterSett_.v1 = Vector3(10.0f, 0.0f, 0.0f);
  triangleEmitterSett_.v2 = Vector3(0.0f, 10.0f, 0.0f);
  triangleEmitterSett_.v3 = Vector3(0.0f, 0.0f, 10.0f);
  triangleEmitterSett_.count = 150;
  triangleEmitterSett_.frequency = 0.15f;
  triangleEmitterSett_.scaleRangeX = Vector2(0.5f, 0.5f);
  triangleEmitterSett_.scaleRangeY = Vector2(0.5f, 0.5f);
  triangleEmitterSett_.velRangeX = Vector2(0.0f, 0.0f);
  triangleEmitterSett_.velRangeY = Vector2(0.0f, 0.0f);
  triangleEmitterSett_.velRangeZ = Vector2(0.0f, 0.0f);
  triangleEmitterSett_.lifeTimeRange = Vector2(0.5f, 1.0f);
  triangleEmitterSett_.startColor = Vector4(1.0f, 1.0f, 1.0f, 1.0f);
  triangleEmitterSett_.endColor = Vector4(0.0f, 1.0f, 0.0f, 0.0f);
  triangleEmitterSett_.isActive = true;
  triangleEmitterSett_.isNormalize = false;
  emitterManager_->CreateTriangleEmitter(triangleEmitterSett_);


}

void GameScene::Finalize()
{
  delete characterModel_;

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

  skyBox_->Update();

  characterModel_->SetTransform(characterTransform2_);
  characterModel_->SetShininess(shininess_);
  characterModel_->SetEnableLighting(isLighting_);
  characterModel_->SetEnableHighlight(isHighlight_);
  characterModel_->SetEnableEnvMap(enableEnvMap);
  characterModel_->SetEnvMapCoefficient(envMapCoefficient_);

  characterModel_->Update();

  emitterManager_->UpdateSphereEmitter(sphereEmitterSett_);
  emitterManager_->UpdateBoxEmitter(boxEmitterSett_);
  emitterManager_->UpdateTriangleEmitter(triangleEmitterSett_);
  emitterManager_->UpdateTemporaryEmitters();


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

  skyBox_->Draw();

  //------------------背景Spriteの描画------------------//
  // スプライト共通描画設定
  SpriteBasic::GetInstance()->SetCommonRenderSetting();



  //--------------------------------------------------//


  //-------------------Modelの描画-------------------//
  // 3Dモデル共通描画設定
  Object3dBasic::GetInstance()->SetCommonRenderSetting();
  // モデル描画
  characterModel_->Draw();



  //-------------------Modelの描画-------------------//


  //------------------前景Spriteの描画------------------//
  // スプライト共通描画設定
  SpriteBasic::GetInstance()->SetCommonRenderSetting();



  //--------------------------------------------------//
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

  ImGui::Begin("character setting");
  ImGui::DragFloat3("Scale", &characterTransform2_.scale.x, 0.01f, 0.1f, 50.0f);
  ImGui::DragFloat3("Position", &characterTransform2_.translate.x, 0.01f, -50.0f, 50.0f);
  ImGui::DragFloat3("Rotate", &characterTransform2_.rotate.x, 0.01f, DirectX::XMConvertToRadians(-180.0f), DirectX::XMConvertToRadians(180.0f));
  ImGui::Separator();
  ImGui::Checkbox("Lighting", &isLighting_);
  ImGui::Checkbox("Highlight", &isHighlight_);
  ImGui::Checkbox("EnvMap", &enableEnvMap);
  if (enableEnvMap)
  {
    ImGui::DragFloat("EnvMap Coefficient", &envMapCoefficient_, 0.01f, 0.0f, 1.0f);
  }
  ImGui::End();

  //characterModel_->DrawImGui();

  // Emitter Managerウィンドウ（統合版）
  ImGui::Begin("Emitter Manager");
  
  if (ImGui::BeginTabBar("EmitterTabs"))
  {
    // Sphere Emitterタブ
    if (ImGui::BeginTabItem("Sphere"))
    {
      // Basic Settings
      ImGui::Text("Basic Settings");
      ImGui::Columns(2, "SphereBasicColumns", false);
      ImGui::SetColumnWidth(0, 150.0f);
      
      ImGui::Text("Position"); ImGui::NextColumn();
      ImGui::DragFloat3("##SpherePos", &sphereEmitterSett_.position.x, 0.1f); ImGui::NextColumn();
      
      ImGui::Text("Radius"); ImGui::NextColumn();
      ImGui::DragFloat("##SphereRadius", &sphereEmitterSett_.radius, 0.01f, 0.1f, 10.0f); ImGui::NextColumn();
      
      ImGui::Text("Count"); ImGui::NextColumn();
      ImGui::DragInt("##SphereCount", (int*)&sphereEmitterSett_.count, 1, 1, 1000); ImGui::NextColumn();
      
      ImGui::Text("Frequency"); ImGui::NextColumn();
      ImGui::DragFloat("##SphereFreq", &sphereEmitterSett_.frequency, 0.01f, 0.01f, 10.0f); ImGui::NextColumn();
      
      ImGui::Text("Active"); ImGui::NextColumn();
      ImGui::Checkbox("##SphereActive", &sphereEmitterSett_.isActive); 
      ImGui::SameLine();
      ImGui::Text("Normalize"); 
      ImGui::SameLine();
      ImGui::Checkbox("##SphereNorm", &sphereEmitterSett_.isNormalize); ImGui::NextColumn();
      
      ImGui::Columns(1);
      
      // Scale & Velocity
      if (ImGui::CollapsingHeader("Scale & Velocity##Sphere"))
      {
        ImGui::Columns(2, "SphereScaleVelColumns", false);
        ImGui::SetColumnWidth(0, 150.0f);
        
        ImGui::Text("Scale Range X"); ImGui::NextColumn();
        ImGui::DragFloat2("##SphereScaleX", &sphereEmitterSett_.scaleRangeX.x, 0.01f, 0.01f, 10.0f); ImGui::NextColumn();
        
        ImGui::Text("Scale Range Y"); ImGui::NextColumn();
        ImGui::DragFloat2("##SphereScaleY", &sphereEmitterSett_.scaleRangeY.x, 0.01f, 0.01f, 10.0f); ImGui::NextColumn();
        
        ImGui::Text("Velocity Range X"); ImGui::NextColumn();
        ImGui::DragFloat2("##SphereVelX", &sphereEmitterSett_.velRangeX.x, 0.01f, -10.0f, 10.0f); ImGui::NextColumn();
        
        ImGui::Text("Velocity Range Y"); ImGui::NextColumn();
        ImGui::DragFloat2("##SphereVelY", &sphereEmitterSett_.velRangeY.x, 0.01f, -10.0f, 10.0f); ImGui::NextColumn();
        
        ImGui::Text("Velocity Range Z"); ImGui::NextColumn();
        ImGui::DragFloat2("##SphereVelZ", &sphereEmitterSett_.velRangeZ.x, 0.01f, -10.0f, 10.0f); ImGui::NextColumn();
        
        ImGui::Text("Lifetime Range"); ImGui::NextColumn();
        ImGui::DragFloat2("##SphereLifetime", &sphereEmitterSett_.lifeTimeRange.x, 0.01f, 0.1f, 10.0f); ImGui::NextColumn();
        
        ImGui::Columns(1);
      }
      
      // Colors
      if (ImGui::CollapsingHeader("Colors##Sphere"))
      {
        ImGui::ColorEdit4("Start Color##Sphere", &sphereEmitterSett_.startColor.x);
        ImGui::ColorEdit4("End Color##Sphere", &sphereEmitterSett_.endColor.x);
      }
      
      ImGui::EndTabItem();
    }
    
    // Box Emitterタブ
    if (ImGui::BeginTabItem("Box"))
    {
      // Basic Settings
      ImGui::Text("Basic Settings");
      ImGui::Columns(2, "BoxBasicColumns", false);
      ImGui::SetColumnWidth(0, 150.0f);
      
      ImGui::Text("Position"); ImGui::NextColumn();
      ImGui::DragFloat3("##BoxPos", &boxEmitterSett_.position.x, 0.1f); ImGui::NextColumn();
      
      ImGui::Text("Size"); ImGui::NextColumn();
      ImGui::DragFloat3("##BoxSize", &boxEmitterSett_.size.x, 0.01f, 0.1f, 10.0f); ImGui::NextColumn();
      
      ImGui::Text("Count"); ImGui::NextColumn();
      ImGui::DragInt("##BoxCount", (int*)&boxEmitterSett_.count, 1, 1, 1000); ImGui::NextColumn();
      
      ImGui::Text("Frequency"); ImGui::NextColumn();
      ImGui::DragFloat("##BoxFreq", &boxEmitterSett_.frequency, 0.01f, 0.01f, 10.0f); ImGui::NextColumn();
      
      ImGui::Text("Active"); ImGui::NextColumn();
      ImGui::Checkbox("##BoxActive", &boxEmitterSett_.isActive); 
      ImGui::SameLine();
      ImGui::Text("Normalize"); 
      ImGui::SameLine();
      ImGui::Checkbox("##BoxNorm", &boxEmitterSett_.isNormalize); ImGui::NextColumn();
      
      ImGui::Columns(1);
      
      // Scale & Velocity
      if (ImGui::CollapsingHeader("Scale & Velocity##Box"))
      {
        ImGui::Columns(2, "BoxScaleVelColumns", false);
        ImGui::SetColumnWidth(0, 150.0f);
        
        ImGui::Text("Scale Range X"); ImGui::NextColumn();
        ImGui::DragFloat2("##BoxScaleX", &boxEmitterSett_.scaleRangeX.x, 0.01f, 0.01f, 10.0f); ImGui::NextColumn();
        
        ImGui::Text("Scale Range Y"); ImGui::NextColumn();
        ImGui::DragFloat2("##BoxScaleY", &boxEmitterSett_.scaleRangeY.x, 0.01f, 0.01f, 10.0f); ImGui::NextColumn();
        
        ImGui::Text("Velocity Range X"); ImGui::NextColumn();
        ImGui::DragFloat2("##BoxVelX", &boxEmitterSett_.velRangeX.x, 0.01f, -10.0f, 10.0f); ImGui::NextColumn();
        
        ImGui::Text("Velocity Range Y"); ImGui::NextColumn();
        ImGui::DragFloat2("##BoxVelY", &boxEmitterSett_.velRangeY.x, 0.01f, -10.0f, 10.0f); ImGui::NextColumn();
        
        ImGui::Text("Velocity Range Z"); ImGui::NextColumn();
        ImGui::DragFloat2("##BoxVelZ", &boxEmitterSett_.velRangeZ.x, 0.01f, -10.0f, 10.0f); ImGui::NextColumn();
        
        ImGui::Text("Lifetime Range"); ImGui::NextColumn();
        ImGui::DragFloat2("##BoxLifetime", &boxEmitterSett_.lifeTimeRange.x, 0.01f, 0.1f, 10.0f); ImGui::NextColumn();
        
        ImGui::Columns(1);
      }
      
      // Colors
      if (ImGui::CollapsingHeader("Colors##Box"))
      {
        ImGui::ColorEdit4("Start Color##Box", &boxEmitterSett_.startColor.x);
        ImGui::ColorEdit4("End Color##Box", &boxEmitterSett_.endColor.x);
      }
      
      ImGui::EndTabItem();
    }
    
    // Triangle Emitterタブ
    if (ImGui::BeginTabItem("Triangle"))
    {
      // Basic Settings
      ImGui::Text("Basic Settings");
      ImGui::Columns(2, "TriangleBasicColumns", false);
      ImGui::SetColumnWidth(0, 150.0f);
      
      ImGui::Text("Position"); ImGui::NextColumn();
      ImGui::DragFloat3("##TrianglePos", &triangleEmitterSett_.position.x, 0.1f); ImGui::NextColumn();
      
      ImGui::Text("Vertex 1"); ImGui::NextColumn();
      ImGui::DragFloat3("##TriangleV1", &triangleEmitterSett_.v1.x, 0.1f); ImGui::NextColumn();
      
      ImGui::Text("Vertex 2"); ImGui::NextColumn();
      ImGui::DragFloat3("##TriangleV2", &triangleEmitterSett_.v2.x, 0.1f); ImGui::NextColumn();
      
      ImGui::Text("Vertex 3"); ImGui::NextColumn();
      ImGui::DragFloat3("##TriangleV3", &triangleEmitterSett_.v3.x, 0.1f); ImGui::NextColumn();
      
      ImGui::Text("Count"); ImGui::NextColumn();
      ImGui::DragInt("##TriangleCount", (int*)&triangleEmitterSett_.count, 1, 1, 1000); ImGui::NextColumn();
      
      ImGui::Text("Frequency"); ImGui::NextColumn();
      ImGui::DragFloat("##TriangleFreq", &triangleEmitterSett_.frequency, 0.01f, 0.01f, 10.0f); ImGui::NextColumn();
      
      ImGui::Text("Active"); ImGui::NextColumn();
      ImGui::Checkbox("##TriangleActive", &triangleEmitterSett_.isActive); 
      ImGui::SameLine();
      ImGui::Text("Normalize"); 
      ImGui::SameLine();
      ImGui::Checkbox("##TriangleNorm", &triangleEmitterSett_.isNormalize); ImGui::NextColumn();
      
      ImGui::Columns(1);
      
      // Scale & Velocity
      if (ImGui::CollapsingHeader("Scale & Velocity##Triangle"))
      {
        ImGui::Columns(2, "TriangleScaleVelColumns", false);
        ImGui::SetColumnWidth(0, 150.0f);
        
        ImGui::Text("Scale Range X"); ImGui::NextColumn();
        ImGui::DragFloat2("##TriangleScaleX", &triangleEmitterSett_.scaleRangeX.x, 0.01f, 0.01f, 10.0f); ImGui::NextColumn();
        
        ImGui::Text("Scale Range Y"); ImGui::NextColumn();
        ImGui::DragFloat2("##TriangleScaleY", &triangleEmitterSett_.scaleRangeY.x, 0.01f, 0.01f, 10.0f); ImGui::NextColumn();
        
        ImGui::Text("Velocity Range X"); ImGui::NextColumn();
        ImGui::DragFloat2("##TriangleVelX", &triangleEmitterSett_.velRangeX.x, 0.01f, -10.0f, 10.0f); ImGui::NextColumn();
        
        ImGui::Text("Velocity Range Y"); ImGui::NextColumn();
        ImGui::DragFloat2("##TriangleVelY", &triangleEmitterSett_.velRangeY.x, 0.01f, -10.0f, 10.0f); ImGui::NextColumn();
        
        ImGui::Text("Velocity Range Z"); ImGui::NextColumn();
        ImGui::DragFloat2("##TriangleVelZ", &triangleEmitterSett_.velRangeZ.x, 0.01f, -10.0f, 10.0f); ImGui::NextColumn();
        
        ImGui::Text("Lifetime Range"); ImGui::NextColumn();
        ImGui::DragFloat2("##TriangleLifetime", &triangleEmitterSett_.lifeTimeRange.x, 0.01f, 0.1f, 10.0f); ImGui::NextColumn();
        
        ImGui::Columns(1);
      }
      
      // Colors
      if (ImGui::CollapsingHeader("Colors##Triangle"))
      {
        ImGui::ColorEdit4("Start Color##Triangle", &triangleEmitterSett_.startColor.x);
        ImGui::ColorEdit4("End Color##Triangle", &triangleEmitterSett_.endColor.x);
      }
      
      ImGui::EndTabItem();
    }
    
    ImGui::EndTabBar();
  }
  
  ImGui::End();

#endif // DEBUG
}

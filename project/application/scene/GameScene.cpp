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

  // SkyBoxの初期化
  skyBox_ = std::make_unique<SkyBox>();
  skyBox_->Initialize("my_skybox.dds");

  object3d_ = new Object3d();
  object3d_->Initialize();
  object3d_->SetModel("terrain.obj");
  // y軸90度回転
  object3dTransform_.translate = { .x = 0.0f, .y = 0.0f, .z = 0.0f };
  object3dTransform_.scale = { .x = 1.0f, .y = 1.0f, .z = 1.0f };
  object3dTransform_.rotate = { .x = 0.0f, .y = DirectX::XMConvertToRadians(90.0f), .z = 0.0f };
  object3d_->SetTransform(object3dTransform_);

  characterTransform_.translate = { .x = 0.0f, .y = 0.0f, .z = 0.0f };
  characterTransform_.scale = { .x = 1.0f, .y = 1.0f, .z = 1.0f };
  characterTransform_.rotate = { .x = 0.0f, .y = DirectX::XMConvertToRadians(180.0f), .z = 0.0f };

  characterModel_ = new Object3d();
  characterModel_->Initialize();
  characterModel_->SetModel("BrainStem.gltf", true, true);
  characterModel_->SetEnvironmentTexture(skyBox_->GetTextureIndex());

  followCamera_ = std::make_unique<FollowCamera>();
  followCamera_->Initialize((*Object3dBasic::GetInstance()->GetCamera()));
  followCamera_->SetTarget(&characterTransform_);
  followCamera_->SetOffset(Vector3(0.0f, 1.6f, -7.5f));

  GPUParticle* particleSystem = GPUParticle::GetInstance();

  emitterManager_ = std::make_unique<EmitterManager>(particleSystem);

  // ボーントラッカーを初期化
  boneTracker_ = std::make_unique<BoneTracker>();
  boneTracker_->Initialize(characterModel_->GetModel(), emitterManager_.get());

  emitterManager_->CreateSphereEmitter("fire_left", Vector3(0.0f, 0.0f, 0.0f), 0.01f, 10, 0.02f);
  emitterManager_->SetEmitterScaleRange("fire_left", Vector2(0.1f, 0.1f), Vector2(0.1f, 0.1f));
  emitterManager_->SetEmitterVelocityRange("fire_left", Vector2(-0.01f, 0.01f), Vector2(0.1f, 0.5f), Vector2(-0.01f, 0.01f));
  emitterManager_->SetEmitterLifeTimeRange("fire_left", Vector2(0.5f, 1.0f));
  emitterManager_->SetEmitterColors("fire_left",
    Vector4(1.0f, 0.4f, 0.1f, 1.0f),  // オレンジ色
    Vector4(1.0f, 0.1f, 0.0f, 0.0f)   // 赤から透明へ
  );

  boneTracker_->LinkBoneToEmitter("fire_left_link", "nodes[15]", "fire_left");

}

void GameScene::Finalize()
{
  delete object3d_;
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

  CharacterMove();

  skyBox_->Update();

  object3d_->SetTransform(object3dTransform_);
  object3d_->SetShininess(shininess_);
  object3d_->SetEnableLighting(isLighting_);
  object3d_->SetEnableHighlight(isHighlight_);
  object3d_->SetMaterialColor(modelColor_);

  characterModel_->SetTransform(characterTransform_);
  characterModel_->SetShininess(shininess_);
  characterModel_->SetEnableLighting(isLighting_);
  characterModel_->SetEnableHighlight(isHighlight_);
  characterModel_->SetEnableEnvMap(enableEnvMap);
  characterModel_->SetEnvMapCoefficient(envMapCoefficient_);

  object3d_->Update();
  characterModel_->Update();

  // ボーントラッカーの更新（エミッター位置をボーンに追従）
  boneTracker_->Update(characterTransform_);

  emitterManager_->Update();

  followCamera_->Update();

  // ライトの設定
  Object3dBasic::GetInstance()->SetDirectionalLight(lightDirection_, lightColor_, 1, lightIntensity_);

  // シーン遷移
  //if (Input::GetInstance()->TriggerKey(DIK_RETURN))
  //{
  //  SceneManager::GetInstance()->ChangeScene("title");
  //}
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
  object3d_->Draw();



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
  ImGui::Begin("object3d");
  ImGui::DragFloat3("Scale", &object3dTransform_.scale.x, 0.01f, 0.1f, 50.0f);
  ImGui::DragFloat3("Position", &object3dTransform_.translate.x, 0.01f, -50.0f, 50.0f);
  ImGui::DragFloat3("Rotate", &object3dTransform_.rotate.x, 0.01f, DirectX::XMConvertToRadians(-180.0f), DirectX::XMConvertToRadians(180.0f));
  ImGui::ColorEdit4("Model Color", &modelColor_.x);

  ImGui::End();

  ImGui::Begin("object3d2");
  ImGui::DragFloat3("Scale", &characterTransform_.scale.x, 0.01f, 0.1f, 50.0f);
  ImGui::DragFloat3("Position", &characterTransform_.translate.x, 0.01f, -50.0f, 50.0f);
  ImGui::DragFloat3("Rotate", &characterTransform_.rotate.x, 0.01f, DirectX::XMConvertToRadians(-180.0f), DirectX::XMConvertToRadians(180.0f));
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
  ImGui::Checkbox("EnvMap", &enableEnvMap);
  if (enableEnvMap)
  {
    ImGui::DragFloat("EnvMap Coefficient", &envMapCoefficient_, 0.01f, 0.0f, 1.0f);
  }
  ImGui::End();

  followCamera_->ImGuiDraw();


#endif // DEBUG
}

void GameScene::CharacterMove()
{
  // gamepadでキャラクターを移動
  Vector3 velocity = { 0.0f, 0.0f, 0.0f };
  Camera* camera = followCamera_->GetCamera();
  bool isMoving = false;
  if (Input::GetInstance()->IsConnect() && !Input::GetInstance()->LStickInDeadZone())
  {
    velocity.x = Input::GetInstance()->GetLeftStick().x * 0.1f;
    velocity.z = Input::GetInstance()->GetLeftStick().y * 0.1f;

    isMoving = true;
  }

  if (isMoving)
  {
    velocity = velocity.Normalize() * 0.1f;
    Matrix4x4 rotateMatrix = Mat4x4::MakeRotateXYZ(camera->GetRotate());
    velocity = Mat4x4::TransFormNormal(rotateMatrix, velocity);
    characterTransform_.translate += velocity;
  }


}

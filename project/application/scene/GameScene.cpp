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

  object3d_ = new Object3d();
  object3d_->Initialize();
  object3d_->SetModel("terrain.obj");

  // y軸90度回転
  Vector3 rotate = { .x = 0.0f, .y = DirectX::XMConvertToRadians(90.0f), .z = 0.0f };
  object3d_->SetRotate(rotate);

  modelPos_ = { .x = 0.0f, .y = 0.0f, .z = 0.0f };

  characterModel_ = new Object3d();
  characterModel_->Initialize();
  characterModel_->SetModel("sneakwalk.gltf", true, true);
  characterTransform_.rotate = { .x = 0.0f, .y = DirectX::XMConvertToRadians(180.0f), .z = 0.0f };
  characterModel_->SetRotate(rotate);
  characterModel_->SetEnvironmentTexture(skyBox_->GetTextureIndex());

  characterModel2_ = new Object3d();
  characterModel2_->Initialize();
  characterModel2_->SetModel("BrainStem.gltf", true, true);
  characterModel2_->SetRotate(rotate);
  characterModel2_->SetEnvironmentTexture(skyBox_->GetTextureIndex());

  characterTransform_.translate = { .x = 0.0f, .y = 6.7f, .z = -24.0f };
  characterTransform_.scale = { .x = 1.0f, .y = 1.0f, .z = 1.0f };

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

  boneTracker_->LinkBoneToEmitter("fire_left_link", "nodes[16]", "fire_left");
  
  // 武器モデルの初期化
  weaponModel_ = new Object3d();
  weaponModel_->Initialize();
  weaponModel_->SetModel("axis.obj"); // 仮のモデル、実際には剣などのモデルを使用
  weaponModel_->SetScale(Vector3(0.3f, 0.3f, 0.3f)); // サイズ調整
  
  // キャラクターにアタッチ
  weaponModel_->AttachToJoint(characterModel_, "mixamorig:LeftHand", Vector3(0.1f, 0.0f, 0.0f)); // 仮のJoint名

}

void GameScene::Finalize()
{
  delete object3d_;
  delete characterModel_;
  delete characterModel2_;
  delete weaponModel_;

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

  object3d_->SetScale(modelScale_);
  object3d_->SetTranslate(modelPos_);
  object3d_->SetRotate(modelRotate_);
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

  characterModel2_->SetShininess(shininess_);
  characterModel2_->SetEnableLighting(isLighting_);
  characterModel2_->SetEnableHighlight(isHighlight_);
  characterModel2_->SetEnableEnvMap(enableEnvMap);
  characterModel2_->SetEnvMapCoefficient(envMapCoefficient_);

  weaponModel_->SetAttachmentRotate(weaponRotate_);

  object3d_->Update();
  characterModel_->Update();
  characterModel2_->Update();
  weaponModel_->Update();

  // ボーントラッカーの更新（エミッター位置をボーンに追従）
  boneTracker_->Update(characterTransform_);

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
  characterModel2_->Draw();
  object3d_->Draw();
  weaponModel_->Draw(); // 武器の描画



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
  SrvAllocateCount_ = SrvManager::GetInstance()->GetAllocatedCount();
  ImGui::Text("SRV Allocate Count : %d", SrvAllocateCount_);
  ImGui::DragFloat3("Scale", &modelScale_.x, 0.01f, 0.1f, 50.0f);
  ImGui::DragFloat3("Position", &modelPos_.x, 0.01f, -50.0f, 50.0f);
  ImGui::DragFloat3("Rotate", &modelRotate_.x, 0.01f, DirectX::XMConvertToRadians(-180.0f), DirectX::XMConvertToRadians(180.0f));
  ImGui::ColorEdit4("Model Color", &modelColor_.x);

  ImGui::End();

  ImGui::Begin("object3d2");
  ImGui::DragFloat3("Scale", &characterTransform_.scale.x, 0.01f, 0.1f, 50.0f);
  ImGui::DragFloat3("Position", &characterTransform_.translate.x, 0.01f, -50.0f, 50.0f);
  ImGui::DragFloat3("Rotate", &characterTransform_.rotate.x, 0.01f, DirectX::XMConvertToRadians(-180.0f), DirectX::XMConvertToRadians(180.0f));
  ImGui::End();
  
  // 武器アタッチメントのデバッグUI
  ImGui::Begin("Weapon Attachment");
  
  // アタッチメント状態の表示
  bool isAttached = weaponModel_->IsAttached();
  ImGui::Text("Attachment Status: %s", isAttached ? "Attached" : "Detached");
  
  // Joint選択用のドロップダウン（実際のJoint名はモデルによって異なる）
  static int selectedJoint = 0;
  const char* jointNames[] = { "mixamorig:LeftHand", "mixamorig:RightHand"}; // 仮のJoint名リスト
  ImGui::Combo("Target Joint", &selectedJoint, jointNames, IM_ARRAYSIZE(jointNames));
  
  // オフセット調整
  static Vector3 attachmentOffset = Vector3(0.1f, 0.0f, 0.0f);
  ImGui::DragFloat3("Attachment Translate", &attachmentOffset.x, 0.01f, -1.0f, 1.0f);

  ImGui::DragFloat3("Weapon Rotate", &weaponRotate_.x, 0.01f, DirectX::XMConvertToRadians(-180.0f), DirectX::XMConvertToRadians(180.0f));
  
  // アタッチ/デタッチボタン
  if (isAttached)
  {
    if (ImGui::Button("Detach Weapon"))
    {
      weaponModel_->DetachFromJoint();
    }
  }
  else
  {
    if (ImGui::Button("Attach Weapon"))
    {
      weaponModel_->AttachToJoint(characterModel_, jointNames[selectedJoint], attachmentOffset);
    }
  }
  
  // 武器のローカル変換（デタッチ時のみ有効）
  if (!isAttached)
  {
    ImGui::Separator();
    ImGui::Text("Weapon Transform (when detached)");
    static Vector3 weaponPos = Vector3(0.0f, 0.0f, 0.0f);
    static Vector3 weaponRot = Vector3(0.0f, 0.0f, 0.0f);
    static Vector3 weaponScale = Vector3(0.3f, 0.3f, 0.3f);
    
    ImGui::DragFloat3("Weapon Position", &weaponPos.x, 0.01f, -50.0f, 50.0f);
    ImGui::DragFloat3("Weapon Rotation", &weaponRot.x, 0.01f, DirectX::XMConvertToRadians(-180.0f), DirectX::XMConvertToRadians(180.0f));
    ImGui::DragFloat3("Weapon Scale", &weaponScale.x, 0.01f, 0.1f, 5.0f);
    
    weaponModel_->SetTranslate(weaponPos);
    weaponModel_->SetRotate(weaponRot);
    weaponModel_->SetScale(weaponScale);
  }
  else
  {
    // アタッチ中はオフセットを更新
    weaponModel_->SetAttachmentTranslate(attachmentOffset);
  }
  
  ImGui::End();

  // Draw skeleton debug UI for the character model
  if (characterModel_ && characterModel_->GetModel()) {
    characterModel_->GetModel()->DrawSkeletonDebugUI();
  }

  if (characterModel2_ && characterModel2_->GetModel()) {
    characterModel2_->GetModel()->DrawSkeletonDebugUI();
  }

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


#endif // DEBUG
}

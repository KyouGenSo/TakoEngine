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

  terrain_ = new Object3d();
  terrain_->Initialize();
  terrain_->SetModel("terrain.obj");
  terrainTransform_.translate = { .x = 0.0f, .y = 0.0f, .z = 0.0f };
  terrainTransform_.scale = { .x = 1.0f, .y = 1.0f, .z = 1.0f };
  terrainTransform_.rotate = { .x = 0.0f, .y = 0.0f, .z = 0.0f };
  terrain_->SetTransform(terrainTransform_);

  characterModel_ = new Object3d();
  characterModel_->Initialize();
  characterModel_->SetModel("BrainStem2.gltf");
  characterTransform_.translate = { .x = -1.0f, .y = 6.7f, .z = -24.0f };
  characterTransform_.scale = { .x = 1.0f, .y = 1.0f, .z = 1.0f };
  characterTransform_.rotate = { .x = 0.0f, .y = DirectX::XMConvertToRadians(180.0f), .z = 0.0f };
  characterModel_->SetTransform(characterTransform_);
  characterModel_->SetEnvironmentTexture(skyBox_->GetTextureIndex());

  characterModel2_ = new Object3d();
  characterModel2_->Initialize();
  characterModel2_->SetModel("AnimatedCube.gltf");
  characterTransform2_.translate = { .x = 1.0f, .y = 6.7f, .z = -24.0f };
  characterTransform2_.scale = { .x = 1.0f, .y = 1.0f, .z = 1.0f };
  characterTransform2_.rotate = { .x = 0.0f, .y = DirectX::XMConvertToRadians(180.0f), .z = 0.0f };
  characterModel2_->SetTransform(characterTransform2_);
  characterModel2_->SetEnvironmentTexture(skyBox_->GetTextureIndex());

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

  boneTracker_->LinkBoneToEmitter("fire_left_link", "Node_16", "fire_left");

  // 武器モデルの初期化
  weaponModel_ = new Object3d();
  weaponModel_->Initialize();
  weaponModel_->SetModel("Sword.gltf"); // 仮のモデル、実際には剣などのモデルを使用
  weaponOffset_.translate = Vector3(0.0f, 0.0f, 0.0f); // アタッチメントオフセット
  weaponOffset_.scale = Vector3(0.3f, 0.3f, 0.3f); // アタッチメントスケール
  weaponOffset_.rotate = Vector3(0.0f, 0.0f, 0.0f); // アタッチメント回転
  weaponModel_->SetAttachmentTransform(weaponOffset_);

  // キャラクターにアタッチ　mixamorig:LeftHand　nodes[17]
  weaponModel_->AttachToJoint(characterModel_, "Node_16", weaponOffset_.translate); // 仮のJoint名

}

void GameScene::Finalize()
{
  delete terrain_;
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

  // アニメーション切り替えテスト（遷移時間付き）
  if (Input::GetInstance()->TriggerKey(DIK_1))
  {
    // 0.3秒の補間してAnim_0へ遷移
    characterModel_->GetModel()->SetAnimation("Anim_0", 0.3f);
  } else if (Input::GetInstance()->TriggerKey(DIK_2))
  {
    // 0.3秒の補間してAnim_1へ遷移
    characterModel_->GetModel()->SetAnimation("Anim_1", 0.3f);
  } else if (Input::GetInstance()->TriggerKey(DIK_3))
  {
    // 即座にAnim_0へ切り替え（遷移なし）
    characterModel_->GetModel()->SetAnimation("Anim_0");
  } else if (Input::GetInstance()->TriggerKey(DIK_4))
  {
    // 即座にAnim_1へ切り替え（遷移なし）
    characterModel_->GetModel()->SetAnimation("Anim_1");
  }

  skyBox_->Update();

  terrain_->SetTransform(terrainTransform_);
  terrain_->SetShininess(shininess_);
  terrain_->SetEnableLighting(isLighting_);
  terrain_->SetEnableHighlight(isHighlight_);
  terrain_->SetMaterialColor(terrainColor_);

  characterModel_->SetTransform(characterTransform_);
  characterModel_->SetShininess(shininess_);
  characterModel_->SetEnableLighting(isLighting_);
  characterModel_->SetEnableHighlight(isHighlight_);
  characterModel_->SetEnableEnvMap(enableEnvMap);
  characterModel_->SetEnvMapCoefficient(envMapCoefficient_);

  characterModel2_->SetTransform(characterTransform2_);
  characterModel2_->SetShininess(shininess_);
  characterModel2_->SetEnableLighting(isLighting_);
  characterModel2_->SetEnableHighlight(isHighlight_);
  characterModel2_->SetEnableEnvMap(enableEnvMap);
  characterModel2_->SetEnvMapCoefficient(envMapCoefficient_);

  weaponModel_->SetAttachmentTransform(weaponOffset_);

  terrain_->Update();
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
  terrain_->Draw();
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
  ImGui::Begin("terrain");
  SrvAllocateCount_ = SrvManager::GetInstance()->GetAllocatedCount();
  ImGui::Text("SRV Allocate Count : %d", SrvAllocateCount_);
  ImGui::DragFloat3("Scale", &terrainTransform_.scale.x, 0.01f, 0.1f, 50.0f);
  ImGui::DragFloat3("Position", &terrainTransform_.translate.x, 0.01f, -50.0f, 50.0f);
  ImGui::DragFloat3("Rotate", &terrainTransform_.rotate.x, 0.01f, DirectX::XMConvertToRadians(-180.0f), DirectX::XMConvertToRadians(180.0f));
  ImGui::ColorEdit4("Model Color", &terrainColor_.x);
  ImGui::End();

  ImGui::Begin("character1");
  ImGui::DragFloat3("Scale", &characterTransform_.scale.x, 0.01f, 0.1f, 50.0f);
  ImGui::DragFloat3("Position", &characterTransform_.translate.x, 0.01f, -50.0f, 50.0f);
  ImGui::DragFloat3("Rotate", &characterTransform_.rotate.x, 0.01f, DirectX::XMConvertToRadians(-180.0f), DirectX::XMConvertToRadians(180.0f));
  ImGui::End();

  ImGui::Begin("character2");
  ImGui::DragFloat3("Scale", &characterTransform2_.scale.x, 0.01f, 0.1f, 50.0f);
  ImGui::DragFloat3("Position", &characterTransform2_.translate.x, 0.01f, -50.0f, 50.0f);
  ImGui::DragFloat3("Rotate", &characterTransform2_.rotate.x, 0.01f, DirectX::XMConvertToRadians(-180.0f), DirectX::XMConvertToRadians(180.0f));
  ImGui::End();

  // 武器アタッチメントのデバッグUI
  ImGui::Begin("Weapon Attachment");

  // アタッチメント状態の表示
  bool isAttached = weaponModel_->IsAttached();
  ImGui::Text("Attachment Status: %s", isAttached ? "Attached" : "Detached");

  // Joint選択用のドロップダウン（実際のJoint名はモデルによって異なる）
  static int selectedJoint = 0;
  const char* jointNames[] = { "mixamorig:LeftHand", "mixamorig:RightHand" }; // 仮のJoint名リスト
  ImGui::Combo("Target Joint", &selectedJoint, jointNames, IM_ARRAYSIZE(jointNames));

  // オフセット調整
  ImGui::DragFloat3("Attachment Translate", &weaponOffset_.translate.x, 0.01f, -1.0f, 1.0f);
  ImGui::DragFloat3("Attachment Scale", &weaponOffset_.scale.x, 0.01f, 0.1f, 5.0f);
  ImGui::DragFloat3("Attachment Rotate", &weaponOffset_.rotate.x, 0.01f, DirectX::XMConvertToRadians(-180.0f), DirectX::XMConvertToRadians(180.0f));

  // アタッチ/デタッチボタン
  if (isAttached)
  {
    if (ImGui::Button("Detach Weapon"))
    {
      weaponModel_->DetachFromJoint();
    }
  } else
  {
    if (ImGui::Button("Attach Weapon"))
    {
      weaponModel_->AttachToJoint(characterModel2_, jointNames[selectedJoint], weaponOffset_.translate);
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
  } else
  {
    // アタッチ中はオフセットを更新
    weaponModel_->SetAttachmentTranslate(weaponOffset_.translate);
  }

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

  characterModel_->DrawImGui();
  characterModel2_->DrawImGui();

#endif // DEBUG
}

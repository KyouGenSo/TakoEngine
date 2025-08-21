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

  // ライトの設定
  Object3dBasic* obj3d = Object3dBasic::GetInstance();
  obj3d->SetDirectionalLight(lightDirection_, lightColor_, 0, lightIntensity_);
  obj3d->EnableShadow(true);
  obj3d->SetDirectionalLightShadowDistance(shadowDistance_);
  obj3d->SetSceneCenter(sceneCenter_);
  obj3d->SetAutoUpdatePosition(autoUpdateLightPos_);
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
  Object3dBasic* obj3d = Object3dBasic::GetInstance();
  obj3d->SetDirectionalLight(lightDirection_, lightColor_, 1, lightIntensity_);
  obj3d->EnableShadow(shadowEnabled_);
  obj3d->SetDirectionalLightShadowDistance(shadowDistance_);
  obj3d->SetAutoUpdatePosition(autoUpdateLightPos_);
  if (!autoUpdateLightPos_) {
    obj3d->SetDirectionalLightPosition(lightPosition_);
  }
  obj3d->SetSceneCenter(sceneCenter_);

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

  // シャドウマップ生成パス
  if (shadowEnabled_) {
    Object3dBasic::GetInstance()->BeginShadowMapRender();
    terrain_->Draw();
    characterModel_->Draw();
    characterModel2_->Draw();
    weaponModel_->Draw();
    Object3dBasic::GetInstance()->EndShadowMapRender();
  }

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

  // Shadow Mapping の設定
  ImGui::Begin("Shadow Mapping");
  ImGui::Checkbox("Enable Shadow", &shadowEnabled_);
  
  // シャドウ品質プリセット
  ImGui::Separator();
  ImGui::Text("Shadow Quality");
  static int shadowQuality = 2; // デフォルトはHigh
  const char* qualityNames[] = { "Low (512x512, PCF 3x3)", "Medium (1024x1024, PCF 5x5)", 
                                  "High (2048x2048, PCF 7x7)", "Ultra (4096x4096, PCF 9x9)" };
  if (ImGui::Combo("Quality Preset", &shadowQuality, qualityNames, IM_ARRAYSIZE(qualityNames))) {
    Object3dBasic::GetInstance()->SetShadowQuality(static_cast<ShadowMap::ShadowQuality>(shadowQuality));
  }
  
  // カスタム設定
  ImGui::Separator();
  ImGui::Text("Custom Settings");
  
  // シャドウマップ解像度
  static int shadowMapSize = Object3dBasic::GetInstance()->GetShadowMapSize();
  const char* sizeNames[] = { "256", "512", "1024", "2048", "4096", "8192" };
  int sizeValues[] = { 256, 512, 1024, 2048, 4096, 8192 };
  int currentSizeIndex = 3; // デフォルトは2048
  for (int i = 0; i < IM_ARRAYSIZE(sizeValues); i++) {
    if (sizeValues[i] == shadowMapSize) {
      currentSizeIndex = i;
      break;
    }
  }
  if (ImGui::Combo("Shadow Map Size", &currentSizeIndex, sizeNames, IM_ARRAYSIZE(sizeNames))) {
    shadowMapSize = sizeValues[currentSizeIndex];
    Object3dBasic::GetInstance()->SetShadowMapSize(shadowMapSize);
  }
  
  // PCFカーネルサイズ
  static int pcfKernelSize = Object3dBasic::GetInstance()->GetPCFKernelSize();
  const char* kernelNames[] = { "1x1 (No PCF)", "3x3", "5x5", "7x7", "9x9" };
  int kernelValues[] = { 1, 3, 5, 7, 9 };
  int currentKernelIndex = 1; // デフォルトは3x3
  for (int i = 0; i < IM_ARRAYSIZE(kernelValues); i++) {
    if (kernelValues[i] == pcfKernelSize) {
      currentKernelIndex = i;
      break;
    }
  }
  if (ImGui::Combo("PCF Kernel Size", &currentKernelIndex, kernelNames, IM_ARRAYSIZE(kernelNames))) {
    pcfKernelSize = kernelValues[currentKernelIndex];
    Object3dBasic::GetInstance()->SetPCFKernelSize(pcfKernelSize);
  }
  
  // バイアス設定
  ImGui::Separator();
  ImGui::Text("Bias Settings");
  ImGui::DragFloat("Shadow Bias", &shadowBias_, 0.00001f, 0.0f, 0.01f, "%.6f");
  
  static float normalOffsetBias = 0.01f;
  if (ImGui::DragFloat("Normal Offset Bias", &normalOffsetBias, 0.001f, 0.0f, 0.1f, "%.4f")) {
    Object3dBasic::GetInstance()->SetNormalOffsetBias(normalOffsetBias);
  }
  
  // その他の設定
  ImGui::Separator();
  ImGui::Text("Light Settings");
  ImGui::DragFloat("Shadow Distance", &shadowDistance_, 0.5f, 5.0f, 100.0f);
  ImGui::Checkbox("Auto Update Light Position", &autoUpdateLightPos_);
  if (!autoUpdateLightPos_) {
    ImGui::DragFloat3("Light Position", &lightPosition_.x, 0.1f, -50.0f, 50.0f);
  }
  ImGui::DragFloat3("Scene Center", &sceneCenter_.x, 0.1f, -50.0f, 50.0f);
  
  // パフォーマンス情報
  ImGui::Separator();
  ImGui::Text("Performance Info");
  ImGui::Text("Current Shadow Map Size: %dx%d", 
              Object3dBasic::GetInstance()->GetShadowMapSize(),
              Object3dBasic::GetInstance()->GetShadowMapSize());
  ImGui::Text("Current PCF Kernel: %dx%d", 
              Object3dBasic::GetInstance()->GetPCFKernelSize(),
              Object3dBasic::GetInstance()->GetPCFKernelSize());
  
  ImGui::End();

  characterModel_->DrawImGui();
  characterModel2_->DrawImGui();

#endif // DEBUG
}

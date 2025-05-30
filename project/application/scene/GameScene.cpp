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

#include "PostEffect.h"

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

  object3d_ = std::make_unique<Object3d>();
  object3d_->Initialize();
  object3d_->SetModel("terrain.obj");
  object3d_->SetTranslate({ 0.0f, 0.0f, 0.0f });

  GPUParticle* particleSystem = GPUParticle::GetInstance();

  emitterManager_ = std::make_unique<EmitterManager>(particleSystem);

  hitEffect1Sett_ = {
    .position = {.x = 0.0f, .y = 0.f, .z = 0.0f },
    .radius = 0.001f,
    .count = 500,
    .frequency = 0.9f,
    .scaleRangeX = {.x = 0.1f, .y = 0.1f},
    .scaleRangeY = {.x = 5.f, .y = 5.f},
    .velRangeX = {.x = 0.001, .y = 0.001},
    .velRangeY = {.x = 0.001, .y = 0.001},
    .velRangeZ = {.x = 0.001, .y = 0.001},
    .lifeTimeRange = {.x = 1.6f, .y = 1.6f},
    .isActive = false,
    .isRandomRotateZ = true
  };

  hitEffect2Sett_ = {
  .position = {.x = 0.0f, .y = 0.4f, .z = 0.0f },
  .radius = 0.001f,
  .count = 100,
  .frequency = 0.01f,
  .scaleRangeX = {.x = 2.3f, .y = 2.3f},
  .scaleRangeY = {.x = 2.3f, .y = 2.3f},
  .velRangeX = {.x = 0.0, .y = 0.0},
  .velRangeY = {.x = 0.0, .y = 0.0},
  .velRangeZ = {.x = 0.0, .y = 0.0},
  .lifeTimeRange = {.x = 0.1f, .y = 0.1f},
  .isActive = false,
  .isNormalize = true,
  .isRandomRotateZ = true,
  };

  // 球体エミッターの作成
  emitterManager_->CreateSphereEmitter("HitEffect1", hitEffect1Sett_.position, hitEffect1Sett_.radius, hitEffect1Sett_.count, hitEffect1Sett_.frequency);
  emitterManager_->SetEmitterColors("HitEffect1", { .x = 121.0f / 255.f, .y = 101.0f / 255.f, .z = 193.0f / 255.f, .w = 1.0f }, { .x = 141.0f / 255.f, .y = 216.0f / 255.f, .z = 255.0f / 255.f, .w = 1.0f });
  emitterManager_->SetEmitterScaleRange("HitEffect1", hitEffect1Sett_.scaleRangeX, hitEffect1Sett_.scaleRangeY);
  emitterManager_->SetEmitterVelocityRange("HitEffect1", hitEffect1Sett_.velRangeX, hitEffect1Sett_.velRangeY, hitEffect1Sett_.velRangeZ);
  emitterManager_->SetEmitterLifeTimeRange("HitEffect1", hitEffect1Sett_.lifeTimeRange);
  emitterManager_->SetEmitterRandomRotateZ("HitEffect1", hitEffect1Sett_.isRandomRotateZ);
  emitterManager_->SetEmitterActive("HitEffect1", hitEffect1Sett_.isActive);


  emitterManager_->CreateSphereEmitter("HitEffect2", hitEffect2Sett_.position, hitEffect2Sett_.radius, hitEffect2Sett_.count, hitEffect2Sett_.frequency);
  emitterManager_->SetEmitterColors("HitEffect2", { .x = 1.f, .y = 100.f / 255.f, .z = 100.f / 255.f, .w = 1.0f }, { .x = 1.f, .y = 0.f, .z = 0.f, .w = 1.0f });
  emitterManager_->SetEmitterScaleRange("HitEffect2", hitEffect2Sett_.scaleRangeX, hitEffect2Sett_.scaleRangeY);
  emitterManager_->SetEmitterVelocityRange("HitEffect2", hitEffect2Sett_.velRangeX, hitEffect2Sett_.velRangeY, hitEffect2Sett_.velRangeZ);
  emitterManager_->SetEmitterLifeTimeRange("HitEffect2", hitEffect2Sett_.lifeTimeRange);
  emitterManager_->SetEmitterRandomRotateZ("HitEffect2", hitEffect2Sett_.isRandomRotateZ);
  emitterManager_->SetEmitterActive("HitEffect2", hitEffect2Sett_.isActive);
  emitterManager_->SetEmitterNormalize("HitEffect2", hitEffect2Sett_.isNormalize);


  isHitEffect2_ = false;
  isRGBSplt_ = false;
  isRadialBlur_ = false;
  isBWFilter_ = false;

  radialBlurCenter = { 0.5f, 0.5f };
  radialBlurWidth = 0.0f;
  radialBlurSampleCount = 5;
  radialBlurWidthSpeed = 0.001f;
  radialBlurWidthMax = 0.01f;

  bwFilterThreshold = 0.5f;
  // 白黒フィルターの継続時間
  bwFilterDuration = 0.2f;

  rgbSplitIntensity = 0.f;
  redOffset = { 0.01f, 0.0f };
  greenOffset = { -0.01f, 0.0f };
  blueOffset = { 0.0f, 0.0f };
  rgbSplitIntensitySpeed = 0.01f;
  rgbSplitIntensityMax = 0.5f;
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

  object3d_->Update();

  emitterManager_->Update();

  emitterManager_->UpdateSphereEmitter("HitEffect1", hitEffect1Sett_.position, hitEffect1Sett_.radius, hitEffect1Sett_.count, hitEffect1Sett_.frequency);
  emitterManager_->SetEmitterScaleRange("HitEffect1", hitEffect1Sett_.scaleRangeX, hitEffect1Sett_.scaleRangeY);
  emitterManager_->SetEmitterVelocityRange("HitEffect1", hitEffect1Sett_.velRangeX, hitEffect1Sett_.velRangeY, hitEffect1Sett_.velRangeZ);
  emitterManager_->SetEmitterLifeTimeRange("HitEffect1", hitEffect1Sett_.lifeTimeRange);
  emitterManager_->SetEmitterRandomRotateZ("HitEffect1", hitEffect1Sett_.isRandomRotateZ);
  emitterManager_->SetEmitterActive("HitEffect1", hitEffect1Sett_.isActive);

  emitterManager_->UpdateSphereEmitter("HitEffect2", hitEffect2Sett_.position, hitEffect2Sett_.radius, hitEffect2Sett_.count, hitEffect2Sett_.frequency);
  emitterManager_->SetEmitterScaleRange("HitEffect2", hitEffect2Sett_.scaleRangeX, hitEffect2Sett_.scaleRangeY);
  emitterManager_->SetEmitterVelocityRange("HitEffect2", hitEffect2Sett_.velRangeX, hitEffect2Sett_.velRangeY, hitEffect2Sett_.velRangeZ);
  emitterManager_->SetEmitterLifeTimeRange("HitEffect2", hitEffect2Sett_.lifeTimeRange);
  emitterManager_->SetEmitterRandomRotateZ("HitEffect2", hitEffect2Sett_.isRandomRotateZ);
  emitterManager_->SetEmitterActive("HitEffect2", hitEffect2Sett_.isActive);
  emitterManager_->SetEmitterNormalize("HitEffect2", hitEffect2Sett_.isNormalize);


  if (Input::GetInstance()->TriggerKey(DIK_SPACE))
  {
    isHitEffect2_ = true;
  }

  PostEffect* postEffect = PostEffect::GetInstance();

  if (isHitEffect2_)
  {
    isRGBSplt_ = true;
    isHitEffect2_ = false;
    emitterManager_->CreateTemporaryEmitterFrom("HitEffect1", "spTemp", 1.0f);
  }

  if (isRGBSplt_)
  {
    postEffect->SetEffectType("RGBSplit");
    postEffect->SetRGBSplitOffsets(redOffset, greenOffset, blueOffset);

    rgbSplitIntensity += rgbSplitIntensitySpeed;
    postEffect->SetRGBSplitIntensity(rgbSplitIntensity);

    if (rgbSplitIntensity >= rgbSplitIntensityMax)
    {
      isRGBSplt_ = false;
      rgbSplitIntensity = 0.0f;
      isBWFilter_ = true;
    }

  }

  if (isRadialBlur_)
  {
    postEffect->SetEffectType("RadialBlur");
    postEffect->SetRadialBlurCenter(radialBlurCenter);
    postEffect->SetRadialBlurSampleCount(radialBlurSampleCount);

    radialBlurWidth += radialBlurWidthSpeed;
    radialBlurDuration -= FrameTimer::GetInstance()->GetDeltaTime();

    if (radialBlurWidth > radialBlurWidthMax)
    {
      radialBlurWidth = radialBlurWidthMax;
    }

    if (radialBlurDuration <= 0.0f)
    {
      isRadialBlur_ = false;
      radialBlurWidth = 0.0f;
      radialBlurDuration = 0.5f;
      postEffect->SetEffectType("NoEffect");
    }

    postEffect->SetRadialBlurWidth(radialBlurWidth);
  }

  if (isBWFilter_)
  {
    postEffect->SetEffectType("BWFilter");
    postEffect->SetBWFilterThreshold(bwFilterThreshold);
    bwFilterDuration -= FrameTimer::GetInstance()->GetDeltaTime();


    if (bwFilterDuration <= 0.0f)
    {
      emitterManager_->CreateTemporaryEmitterFrom("HitEffect2", "hit2Temp", 1.f);
      isBWFilter_ = false;
      bwFilterDuration = 0.2f;
      isRadialBlur_ = true;
    }
  }

  // シーン遷移
  if (Input::GetInstance()->TriggerKey(DIK_F2))
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

  object3d_->Draw();



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
    if (ImGui::BeginTabItem("HitEffect1Emitter"))
    {
      ImGui::DragFloat3("Position", &hitEffect1Sett_.position.x, 0.01f, -50.0f, 50.0f);
      ImGui::DragFloat("Radius", &hitEffect1Sett_.radius, 0.01f, 0.1f, 50.0f);
      int* count = reinterpret_cast<int*>(&hitEffect1Sett_.count);
      ImGui::DragInt("Count", count, 1, 1, 100);
      ImGui::DragFloat("Frequency", &hitEffect1Sett_.frequency, 0.01f, 0.1f, 10.0f);
      ImGui::DragFloat2("ScaleRangeX", &hitEffect1Sett_.scaleRangeX.x, 0.01f, -10.0f, 10.0f);
      ImGui::DragFloat2("ScaleRangeY", &hitEffect1Sett_.scaleRangeY.x, 0.01f, -10.0f, 10.0f);
      ImGui::DragFloat2("VelRangeX", &hitEffect1Sett_.velRangeX.x, 0.01f, -10.0f, 10.0f);
      ImGui::DragFloat2("VelRangeY", &hitEffect1Sett_.velRangeY.x, 0.01f, -10.0f, 10.0f);
      ImGui::DragFloat2("VelRangeZ", &hitEffect1Sett_.velRangeZ.x, 0.01f, -10.0f, 10.0f);
      ImGui::DragFloat2("LifeTimeRange", &hitEffect1Sett_.lifeTimeRange.x, 0.01f, 0.1f, 10.0f);
      ImGui::Checkbox("RandomRotateZ", &hitEffect1Sett_.isRandomRotateZ);
      ImGui::Checkbox("Active", &hitEffect1Sett_.isActive);

      if (ImGui::Button("Create HitEffect1 Temp Emitter"))
      {
        emitterManager_->CreateTemporaryEmitterFrom("HitEffect1", "spTemp", 1.0f);
      }

      ImGui::EndTabItem();
    }

    if (ImGui::BeginTabItem("HitEffect2Emitter"))
    {
      ImGui::DragFloat3("Position", &hitEffect2Sett_.position.x, 0.01f, -50.0f, 50.0f);
      ImGui::DragFloat("Radius", &hitEffect2Sett_.radius, 0.01f, 0.1f, 50.0f);
      int* count = reinterpret_cast<int*>(&hitEffect2Sett_.count);
      ImGui::DragInt("Count", count, 1, 1, 100);
      ImGui::DragFloat("Frequency", &hitEffect2Sett_.frequency, 0.01f, 0.1f, 10.0f);
      ImGui::DragFloat2("ScaleRangeX", &hitEffect2Sett_.scaleRangeX.x, 0.01f, -10.0f, 10.0f);
      ImGui::DragFloat2("ScaleRangeY", &hitEffect2Sett_.scaleRangeY.x, 0.01f, -10.0f, 10.0f);
      ImGui::DragFloat2("VelRangeX", &hitEffect2Sett_.velRangeX.x, 0.01f, -10.0f, 10.0f);
      ImGui::DragFloat2("VelRangeY", &hitEffect2Sett_.velRangeY.x, 0.01f, -10.0f, 10.0f);
      ImGui::DragFloat2("VelRangeZ", &hitEffect2Sett_.velRangeZ.x, 0.01f, -10.0f, 10.0f);
      ImGui::DragFloat2("LifeTimeRange", &hitEffect2Sett_.lifeTimeRange.x, 0.01f, 0.1f, 10.0f);
      ImGui::Checkbox("RandomRotateZ", &hitEffect2Sett_.isRandomRotateZ);
      ImGui::Checkbox("Normalize", &hitEffect2Sett_.isNormalize);
      ImGui::Checkbox("Active", &hitEffect2Sett_.isActive);

      if (ImGui::Button("Create HitEffect2 Temp Emitter"))
      {
        if (!isHitEffect2_ && !isRGBSplt_ && !isRadialBlur_ && !isBWFilter_)
        {
          isHitEffect2_ = true;
        }
      }

      ImGui::EndTabItem();
    }

    ImGui::EndTabBar();
  }


  ImGui::End();

#endif // DEBUG
}

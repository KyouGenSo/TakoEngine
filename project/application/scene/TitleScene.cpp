#include "TitleScene.h"
#include "SceneManager.h"
#include "TextureManager.h"
#include "Object3dBasic.h"
#include "SpriteBasic.h"
#include "ModelManager.h"
#include "Input.h"
#include "Draw2D.h"
#include "GlobalVariables.h"
#include "GPUParticle.h"

#ifdef _DEBUG
#include"ImGui.h"
#include "DebugCamera.h"
#endif

void TitleScene::Initialize()
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

  InitParticle();

  InitVariables();
}

void TitleScene::InitParticle()
{

 

}

void TitleScene::InitVariables()
{
  GlobalVariables::GetInstance()->CreateGroup("EmitterParam1");
  GlobalVariables::GetInstance()->CreateGroup("EmitterParam2");

  GlobalVariables::GetInstance()->AddItem("EmitterParam1", "color", emitterParam_.color_);
  GlobalVariables::GetInstance()->AddItem("EmitterParam1", "isVisualize", emitterParam_.isVisualize_);
  GlobalVariables::GetInstance()->AddItem("EmitterParam1", "isRandomColor", emitterParam_.isRandomColor_);
  GlobalVariables::GetInstance()->AddItem("EmitterParam1", "frequency", emitterParam_.frequency_);
  GlobalVariables::GetInstance()->AddItem("EmitterParam1", "count", emitterParam_.count_);
  GlobalVariables::GetInstance()->AddItem("EmitterParam1", "lifeTime", emitterParam_.lifeTime);
  GlobalVariables::GetInstance()->AddItem("EmitterParam1", "range max", emitterParam_.range_.max);
  GlobalVariables::GetInstance()->AddItem("EmitterParam1", "range min", emitterParam_.range_.min);
  GlobalVariables::GetInstance()->AddItem("EmitterParam1", "velocity", emitterParam_.velocity_);
  GlobalVariables::GetInstance()->AddItem("EmitterParam1", "scale", emitterParam_.transform_.scale);
  GlobalVariables::GetInstance()->AddItem("EmitterParam1", "translate", emitterParam_.transform_.translate);

  GlobalVariables::GetInstance()->AddItem("EmitterParam2", "color", emitterParam2_.color_);
  GlobalVariables::GetInstance()->AddItem("EmitterParam2", "isVisualize", emitterParam2_.isVisualize_);
  GlobalVariables::GetInstance()->AddItem("EmitterParam2", "isRandomColor", emitterParam2_.isRandomColor_);
  GlobalVariables::GetInstance()->AddItem("EmitterParam2", "frequency", emitterParam2_.frequency_);
  GlobalVariables::GetInstance()->AddItem("EmitterParam2", "count", emitterParam2_.count_);
  GlobalVariables::GetInstance()->AddItem("EmitterParam2", "lifeTime", emitterParam2_.lifeTime);
  GlobalVariables::GetInstance()->AddItem("EmitterParam2", "range max", emitterParam2_.range_.max);
  GlobalVariables::GetInstance()->AddItem("EmitterParam2", "range min", emitterParam2_.range_.min);
  GlobalVariables::GetInstance()->AddItem("EmitterParam2", "velocity", emitterParam2_.velocity_);
  GlobalVariables::GetInstance()->AddItem("EmitterParam2", "scale", emitterParam2_.transform_.scale);
  GlobalVariables::GetInstance()->AddItem("EmitterParam2", "translate", emitterParam2_.transform_.translate);

  GlobalVariables::GetInstance()->LoadFiles();
}

void TitleScene::Finalize()
{

  GlobalVariables::GetInstance()->DeleteGroup("EmitterParam1");
  GlobalVariables::GetInstance()->DeleteGroup("EmitterParam2");
}

void TitleScene::Update()
{
#ifdef _DEBUG
	if (Input::GetInstance()->TriggerKey(DIK_F1)) {
		Object3dBasic::GetInstance()->SetDebug(!Object3dBasic::GetInstance()->GetDebug());
		Draw2D::GetInstance()->SetDebug(!Draw2D::GetInstance()->GetDebug());
    GPUParticle::GetInstance()->SetIsDebug(!GPUParticle::GetInstance()->GetIsDebug());
		isDebug_ = !isDebug_;
	}

	if (isDebug_) {
		DebugCamera::GetInstance()->Update();
	}
#endif
	/// ================================== ///
	///              更新処理               ///
	/// ================================== ///

  ApplyGlobalVariables();


	if (Input::GetInstance()->TriggerKey(DIK_RETURN))
	{
		SceneManager::GetInstance()->ChangeScene("game");
	}
}

void TitleScene::Draw()
{
	/// ================================== ///
	///              描画処理               ///
	/// ================================== ///

  Draw2D::GetInstance()->DrawGrid(100.0f, 20.0f, Vector4(1.0f, 1.0f, 1.0f, 1.0f));

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

void TitleScene::DrawWithoutEffect()
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

void TitleScene::DrawImGui()
{
#ifdef _DEBUG

	/// ================================== ///
	///             ImGuiの描画              ///
	/// ================================== ///


#endif // _DEBUG
}

void TitleScene::ApplyGlobalVariables()
{
  GlobalVariables* globalVariables = GlobalVariables::GetInstance();
  const char* group1 = "EmitterParam1";
  const char* group2 = "EmitterParam2";

  //particleEmitter_->SetTranslate(globalVariables->GetValueVec3(group1, "translate"));
  //particleEmitter_->SetScale(globalVariables->GetValueVec3(group1, "scale"));
  //particleEmitter_->SetVelocity(globalVariables->GetValueVec3(group1, "velocity"));
  //AABB range1;
  //range1.min = globalVariables->GetValueVec3(group1, "range min");
  //range1.max = globalVariables->GetValueVec3(group1, "range max");
  //particleEmitter_->SetRange(range1);
  //particleEmitter_->SetLifeTime(globalVariables->GetValueFloat(group1, "lifeTime"));
  //particleEmitter_->SetCount(globalVariables->GetValueInt(group1, "count"));
  //particleEmitter_->SetFrequency(globalVariables->GetValueFloat(group1, "frequency"));
  //particleEmitter_->SetIsRandomColor(globalVariables->GetValueBool(group1, "isRandomColor"));
  //particleEmitter_->SetIsVisualize(globalVariables->GetValueBool(group1, "isVisualize"));
  //particleEmitter_->SetColor(globalVariables->GetValueVec4(group1, "color"));

  //particleEmitter2_->SetTranslate(globalVariables->GetValueVec3(group2, "translate"));
  //particleEmitter2_->SetScale(globalVariables->GetValueVec3(group2, "scale"));
  //particleEmitter2_->SetVelocity(globalVariables->GetValueVec3(group2, "velocity"));
  //AABB range2;
  //range2.min = globalVariables->GetValueVec3(group2, "range min");
  //range2.max = globalVariables->GetValueVec3(group2, "range max");
  //particleEmitter2_->SetRange(range2);
  //particleEmitter2_->SetLifeTime(globalVariables->GetValueFloat(group2, "lifeTime"));
  //particleEmitter2_->SetCount(globalVariables->GetValueInt(group2, "count"));
  //particleEmitter2_->SetFrequency(globalVariables->GetValueFloat(group2, "frequency"));
  //particleEmitter2_->SetIsRandomColor(globalVariables->GetValueBool(group2, "isRandomColor"));
  //particleEmitter2_->SetIsVisualize(globalVariables->GetValueBool(group2, "isVisualize"));
  //particleEmitter2_->SetColor(globalVariables->GetValueVec4(group2, "color"));
}

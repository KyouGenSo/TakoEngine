#include "GameScene.h"
#include "SceneManager.h"
#include"Audio.h"
#include"ModelManager.h"
#include"Object3dBasic.h"
#include"TextureManager.h"
#include"SpriteBasic.h"
#include"Input.h"
#include "DebugCamera.h"
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
#endif
	/// ================================== ///
	///              初期化処理              ///
	/// ================================== ///

	audio_ = Audio::GetInstance();
	bgmSH_ = audio_->LoadWaveFile("BGM/battle_bgm.wav");
	bgmVH_ = audio_->PlayWave(bgmSH_, true, 0.5f);

	Object3dBasic::GetInstance()->SetDirectionalLightIntensity(3.0f);

	PostEffect::GetInstance()->SetBloomIntensity(0.f);
	PostEffect::GetInstance()->SetBloomThreshold(0.75f);
	PostEffect::GetInstance()->SetBloomSigma(2.0f);

	// uiの初期化
	InitializeUI();

	input_ = Input::GetInstance();
	audio_ = Audio::GetInstance();

	// collisionMnagerの生成と初期化
	collisionManager_ = std::make_unique<CollisionManager>();
	collisionManager_->Initialize();

	ModelManager::GetInstance()->LoadModel("ground.obj");
	ModelManager::GetInstance()->LoadModel("skydome.obj");

	// プレイヤーのモデルの読み込み
	ModelManager::GetInstance()->LoadModel("float_Head.obj");
	ModelManager::GetInstance()->LoadModel("float_Body.obj");
	ModelManager::GetInstance()->LoadModel("float_L_arm.obj");
	ModelManager::GetInstance()->LoadModel("float_R_arm.obj");
	ModelManager::GetInstance()->LoadModel("weapon.obj");
	ModelManager::GetInstance()->LoadModel("playerBullet.obj");
	ModelManager::GetInstance()->LoadModel("efffect_ball.obj");

	playerHeadModel_ = std::make_unique<Object3d>();
	playerHeadModel_->Initialize();
	playerHeadModel_->SetModel("float_Head.obj");

	playerBodyModel_ = std::make_unique<Object3d>();
	playerBodyModel_->Initialize();
	playerBodyModel_->SetModel("float_Body.obj");

	playerL_armModel_ = std::make_unique<Object3d>();
	playerL_armModel_->Initialize();
	playerL_armModel_->SetModel("float_L_arm.obj");

	playerR_armModel_ = std::make_unique<Object3d>();
	playerR_armModel_->Initialize();
	playerR_armModel_->SetModel("float_R_arm.obj");

	playerWeaponModel_ = std::make_unique<Object3d>();
	playerWeaponModel_->Initialize();
	playerWeaponModel_->SetModel("weapon.obj");

	playerBulletModel_ = std::make_unique<Object3d>();
	playerBulletModel_->Initialize();
	playerBulletModel_->SetModel("playerBullet.obj");

	playerWeaponEffectModel_ = std::make_unique<Object3d>();
	playerWeaponEffectModel_->Initialize();
	playerWeaponEffectModel_->SetModel("efffect_ball.obj");

	playerModels_ = { playerHeadModel_.get(), playerBodyModel_.get(), playerL_armModel_.get(), playerR_armModel_.get(),
				 playerWeaponModel_.get(), playerBulletModel_.get(), playerWeaponEffectModel_.get() };

	// 敵のモデルの読み込み
	ModelManager::GetInstance()->LoadModel("boss.obj");
	ModelManager::GetInstance()->LoadModel("boss_block.obj");
	enemyBodyModel_ = std::make_unique<Object3d>();
	enemyBodyModel_->Initialize();
	enemyBodyModel_->SetModel("boss.obj");

	enemyBlockModel_ = std::make_unique<Object3d>();
	enemyBlockModel_->Initialize();
	enemyBlockModel_->SetModel("boss_block.obj");

	enemyModels_ = { enemyBodyModel_.get(), enemyBlockModel_.get() };

	// 天球
	skydome_ = std::make_unique<Skydome>();
	skydomeModel_ = std::make_unique<Object3d>();
	skydomeModel_->Initialize();
	skydomeModel_->SetModel("skydome.obj");
	skydome_->Initialize(skydomeModel_.get());

	skydomeModel_->SetEnableLighting(false);

	// 地面
	ground_ = std::make_unique<Ground>();
	groundModel_ = std::make_unique<Object3d>();
	groundModel_->Initialize();
	groundModel_->SetModel("ground.obj");
	ground_->Initialize(groundModel_.get());

	// LockOn system
	lockOn_ = std::make_unique<LockOn>();
	lockOn_->Initialize();

	// プレイヤーの初期化
	player_ = std::make_unique<Player>();
	player_->Initialize(playerModels_);
	player_->SetLockOn(lockOn_.get());

	// 敵の初期化
	enemy_ = std::make_unique<Enemy>();
	enemy_->Initialize(enemyModels_);

	// 追従カメラの初期化
	followCamera_ = std::make_unique<FollowCamera>();
	followCamera_->Initialize(Object3dBasic::GetInstance()->GetCamera());
	followCamera_->SetTarget(&player_->GetTransform());
	followCamera_->SetLockOn(lockOn_.get());
	// 追従カメラの更新
	followCamera_->Update();

	// SET------------------------------------------------------------

	// プレイヤーにカメラをセット
	player_->SetCamera(followCamera_->GetCamera());

	// LockOnにプレイヤーをセット
	lockOn_->SetPlayer(player_.get());

	// 敵にプレイヤーをセット
	enemy_->SetPlayer(player_.get());

	// 敵に追従カメラをセット
	enemy_->SetFollowCamera(followCamera_.get());

	// プレイヤーに敵をセット
	player_->SetEnemy(enemy_.get());

}

void GameScene::InitializeUI()
{
	// textureの読み込み
	TextureManager::GetInstance()->LoadTexture("white.png");
	TextureManager::GetInstance()->LoadTexture("red.png");
	TextureManager::GetInstance()->LoadTexture("player_Text.png");
	TextureManager::GetInstance()->LoadTexture("enemy_Text.png");
	TextureManager::GetInstance()->LoadTexture("A_Icon.png");
	TextureManager::GetInstance()->LoadTexture("B_Icon.png");
	TextureManager::GetInstance()->LoadTexture("X_Icon.png");
	TextureManager::GetInstance()->LoadTexture("LT_Icon.png");
	TextureManager::GetInstance()->LoadTexture("RT_Icon.png");
	TextureManager::GetInstance()->LoadTexture("dash_Text.png");
	TextureManager::GetInstance()->LoadTexture("jump_Text.png");
	TextureManager::GetInstance()->LoadTexture("attack_Text.png");
	TextureManager::GetInstance()->LoadTexture("aim_Text.png");
	TextureManager::GetInstance()->LoadTexture("shoot_Text.png");
	TextureManager::GetInstance()->LoadTexture("key_text.png");
	TextureManager::GetInstance()->LoadTexture("manual.png");

	// UIの初期化
	playerHP_ = std::make_unique<Sprite>();
	playerHP_->Initialize("white.png");
	playerHP_->SetPos(playerHP_Pos_);
	playerHP_->SetSize(playerHP_Size_);

	enemyHP_ = std::make_unique<Sprite>();
	enemyHP_->Initialize("red.png");
	enemyHP_->SetPos(enemyHP_Pos_);
	enemyHP_->SetSize(enemyHP_Size_);

	player_Text_ = std::make_unique<Sprite>();
	player_Text_->Initialize("player_Text.png");
	player_Text_->SetPos(player_Text_Pos_);
	player_Text_->SetSize(textSize_);

	enemy_Text_ = std::make_unique<Sprite>();
	enemy_Text_->Initialize("enemy_Text.png");
	enemy_Text_->SetPos(enemy_Text_Pos_);
	enemy_Text_->SetSize(textSize_);

	A_Icon_ = std::make_unique<Sprite>();
	A_Icon_->Initialize("A_Icon.png");
	A_Icon_->SetPos(A_Icon_Pos_);

	B_Icon_ = std::make_unique<Sprite>();
	B_Icon_->Initialize("B_Icon.png");
	B_Icon_->SetPos(B_Icon_Pos_);

	X_Icon_ = std::make_unique<Sprite>();
	X_Icon_->Initialize("X_Icon.png");
	X_Icon_->SetPos(X_Icon_Pos_);

	LT_Icon_ = std::make_unique<Sprite>();
	LT_Icon_->Initialize("LT_Icon.png");
	LT_Icon_->SetPos(LT_Icon_Pos_);

	RT_Icon_ = std::make_unique<Sprite>();
	RT_Icon_->Initialize("RT_Icon.png");
	RT_Icon_->SetPos(RT_Icon_Pos_);

	dash_Text_ = std::make_unique<Sprite>();
	dash_Text_->Initialize("dash_Text.png");
	dash_Text_->SetPos(dash_Text_Pos_);
	dash_Text_->SetSize(textSize_);

	jump_Text_ = std::make_unique<Sprite>();
	jump_Text_->Initialize("jump_Text.png");
	jump_Text_->SetPos(jump_Text_Pos_);
	jump_Text_->SetSize(textSize_);

	attack_Text_ = std::make_unique<Sprite>();
	attack_Text_->Initialize("attack_Text.png");
	attack_Text_->SetPos(attack_Text_Pos_);
	attack_Text_->SetSize(textSize_);

	aim_Text_ = std::make_unique<Sprite>();
	aim_Text_->Initialize("aim_Text.png");
	aim_Text_->SetPos(aim_Text_Pos_);
	aim_Text_->SetSize(textSize_);

	shoot_Text_ = std::make_unique<Sprite>();
	shoot_Text_->Initialize("shoot_Text.png");
	shoot_Text_->SetPos(shoot_Text_Pos_);
	shoot_Text_->SetSize(textSize_);

	key_Text_ = std::make_unique<Sprite>();
	key_Text_->Initialize("key_text.png");
	key_Text_->SetPos(key_Text_Pos_);
	key_Text_->SetSize(key_Text_Size_);

	manual_Text_ = std::make_unique<Sprite>();
	manual_Text_->Initialize("manual.png");
	manual_Text_->SetPos(manual_Text_Pos_);
}

void GameScene::Finalize()
{
	playerModels_.clear();
	enemyModels_.clear();
}

void GameScene::Update()
{
#ifdef _DEBUG
	if (Input::GetInstance()->TriggerKey(DIK_F1)) {
		Object3dBasic::GetInstance()->SetDebug(!Object3dBasic::GetInstance()->GetDebug());
		isDebug_ = !isDebug_;
	}

	if (isDebug_) {
		DebugCamera::GetInstance()->Update();
	}
#endif
	/// ================================== ///
	///              更新処理               ///
	/// ================================== ///

	if (input_->TriggerKey(DIK_TAB))
	{
		isPause_ = !isPause_;
	}

	key_Text_->SetPos(key_Text_Pos_);
	manual_Text_->SetPos(manual_Text_Pos_);

	if (isPause_)
	{
		return;
	}

	// プレイヤーの更新
	player_->Update();

	// 敵の更新
	enemy_->Update();

	// LockOnの更新
	lockOn_->Update(enemy_, *followCamera_->GetCamera());

	// 追従カメラの更新
	followCamera_->Update();

	// Skydomeの更新
	skydome_->Update();

	// 地面の更新
	ground_->Update();

	// デバッグ表示用にトランスフォームを更新
	collisionManager_->UpdateWorldTransform();

	UpdateUI();

	// 衝突判定と応答
	CheckAllCollisions();

	// ビネットエフェクト
	vignetteAlpha_ = (1.0f - std::clamp(player_->GetHP() / 100.0f, 0.0f, 1.0f)) * 0.55f;
	PostEffect::GetInstance()->SetVignettePower(vignetteAlpha_);

	// vignetteRadius根據player的HP從60到20變化
	vignetteRadius_ = 10.0f + (player_->GetHP() / 100.0f) * 50.0f;
	PostEffect::GetInstance()->SetVignetteRange(vignetteRadius_);

	 //シーン遷移
	if (player_->GetHP() <= 0) {
		SceneManager::GetInstance()->ChangeScene("over");
	}
	if (enemy_->GetHp() <= 0)
	{
		SceneManager::GetInstance()->ChangeScene("clear");
	}
}

void GameScene::UpdateUI()
{
	float playerHP = player_->GetHP();
	float enemyHP = enemy_->GetHp();
	playerHP_->SetSize({ playerHP * 5.0f, 30.0f });
	enemyHP_->SetSize({ enemyHP * 5.0f, 40.0f });

	playerHP_->Update();
	enemyHP_->Update();
	player_Text_->Update();
	enemy_Text_->Update();
	A_Icon_->Update();
	B_Icon_->Update();
	X_Icon_->Update();
	LT_Icon_->Update();
	RT_Icon_->Update();
	dash_Text_->Update();
	jump_Text_->Update();
	attack_Text_->Update();
	aim_Text_->Update();
	shoot_Text_->Update();
	key_Text_->Update();
	manual_Text_->Update();
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

	// 敵の描画
	enemy_->Draw();

	// Skydomeの描画
	skydome_->Draw();

	// 地面の描画
	ground_->Draw();

	// プレイヤーの描画
	player_->Draw();

	// 当たり判定の表示用モデルの描画
	collisionManager_->Draw();


	//-------------------Modelの描画-------------------//


	//------------------前景Spriteの描画------------------//
	// スプライト共通描画設定
	SpriteBasic::GetInstance()->SetCommonRenderSetting();

	lockOn_->Draw();

	// HPの描画
	DrawHP();

	// UIの描画
	A_Icon_->Draw();
	B_Icon_->Draw();
	X_Icon_->Draw();
	LT_Icon_->Draw();
	RT_Icon_->Draw();
	dash_Text_->Draw();
	jump_Text_->Draw();
	attack_Text_->Draw();
	aim_Text_->Draw();
	shoot_Text_->Draw();
	player_Text_->Draw();
	enemy_Text_->Draw();
	key_Text_->Draw();

	if (isPause_)
	{
		manual_Text_->Draw();
	}

	//--------------------------------------------------//
}

void GameScene::DrawHP()
{
	// プレイヤーのHPの描画
	playerHP_->Draw();
	// 敵のHPの描画
	enemyHP_->Draw();
}

void GameScene::DrawImGui()
{
#ifdef _DEBUG
	// プレイヤーのデバッグ表示
	player_->ImGuiDraw();
	//敵のデバッグ表示
	enemy_->ImGuiDraw();
	//当たり判定のデバッグ表示
	collisionManager_->ImGuiDraw();

	// vignetteのデバッグ表示
	ImGui::Begin("Vignette");
	ImGui::DragFloat("VignetteAlpha", &vignetteAlpha_, 0.01f, 0.0f, 1.0f);
	ImGui::DragFloat("VignetteRadius", &vignetteRadius_, 0.01f, 0.0f, 60.0f);
	ImGui::End();

	// デバッグ表示用のUI
	ImGui::Begin("GameScene");
	// key_Text_Pos_
	ImGui::DragFloat2("key_Text_Pos", &key_Text_Pos_.x, 1.0f, 0.0f, 1280.0f);
	// manual_Text_Pos_
	ImGui::DragFloat2("manual_Text_Pos", &manual_Text_Pos_.x, 1.0f, 0.0f, 1280.0f);
	ImGui::End();

#endif // DEBUG
}

void GameScene::CheckAllCollisions()
{
	collisionManager_->Reset();

	collisionManager_->AddCollider(player_.get());
	collisionManager_->AddCollider(enemy_.get());
	collisionManager_->AddCollider(player_->GetHammer());

	std::list<PlayerBullet*> bullets = player_->GetBullets();
	for (PlayerBullet* bullet : bullets) {
		collisionManager_->AddCollider(bullet);
	}

	std::list<EnemyBlock*> blocks_ = enemy_->GetBlocks();
	for (EnemyBlock* block : blocks_) {
		collisionManager_->AddCollider(block);
	}

	collisionManager_->CheckAllCollisions();
}

void GameScene::ResetAllObjects()
{
	player_->ReSet();
	enemy_->ReSet();
}

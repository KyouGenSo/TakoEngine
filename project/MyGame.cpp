#include "MyGame.h"

void MyGame::Initialize()
{
	winApp_ = new WinApp();
	winApp_->Initialize();

	dx12_ = new DX12Basic();
	dx12_->Initialize(winApp_);

#ifdef _DEBUG
	imguiManager_ = new ImGuiManager();
	imguiManager_->Initialize(winApp_, dx12_);
#endif

	srvManager_ = new SrvManager();
	srvManager_->Initialize(dx12_);

	TextureManager::GetInstance()->Initialize(dx12_, srvManager_);

	ModelManager::GetInstance()->Initialize(dx12_);

	object3dBasic_ = new Object3dBasic();
	object3dBasic_->Initialize(dx12_);

	spriteBasic_ = new SpriteBasic();
	spriteBasic_->Initialize(dx12_);

	defaultCamera_ = new Camera();
	defaultCamera_->SetRotate(Vector3(0.3f, 0.0f, 0.0f));
	defaultCamera_->SetTranslate(Vector3(0.0f, 4.0f, -10.0f));
	// デフォルトカメラを設定
	object3dBasic_->SetDefaultCamera(defaultCamera_);

	// 入力クラス
	input_ = new Input();
	input_->Initialize(winApp_);

	audio_ = new Audio();
	audio_->Initialize("resources/Sound/");

	// textureの読み込み
	TextureManager::GetInstance()->LoadTexture("resources/uvChecker.png");
	TextureManager::GetInstance()->LoadTexture("resources/checkerBoard.png");

	// modelの読み込み
	ModelManager::GetInstance()->LoadModel("teapot.obj");
	ModelManager::GetInstance()->LoadModel("plane.obj");

	// サウンドデータの読み込み
	soundDataHandle = audio_->LoadWaveFile("fanfare.wav");
	voiceHandle = 0;

	bgmSH = audio_->LoadWaveFile("playerBulletHit.wav");
	bgmVH = 0;

	// spriteの初期化
	for (uint32_t i = 0; i < spriteNum_; i++) {
		Sprite* sprite = new Sprite();
		if (i % 2 == 0)
			sprite->Initialize(spriteBasic_, "resources/uvChecker.png");
		else
			sprite->Initialize(spriteBasic_, "resources/checkerBoard.png");
		sprite->SetPos(Vector2(i * 500.0f, 0.0f));
		sprites_.push_back(sprite);
	}

	// object3dの初期化
	object3d_ = new Object3d();
	object3d_->Initialize(object3dBasic_);
	object3d_->SetModel("teapot.obj");
	object3d_->SetTranslate(Vector3(-2.0f, 0.0f, 0.0f));

	object3d2_ = new Object3d();
	object3d2_->Initialize(object3dBasic_);
	object3d2_->SetModel("plane.obj");
	object3d2_->SetTranslate(Vector3(2.0f, 0.0f, 0.0f));

}

void MyGame::Finalize()
{
}

void MyGame::Update()
{
}

void MyGame::Draw()
{
}

#include"Sprite.h"
#include"SpriteBasic.h"
#include "TextureManager.h"
#include "SrvManager.h"

void Sprite::Initialize(std::string texturePath)
{
	// Transformの初期化
	transform_.scale = Vector3(size_.x, size_.y, 1.0f);
	transform_.rotate = Vector3(0.0f, 0.0f, rotation_);
	transform_.translate = { pos_.x, pos_.y, 0.0f };

	// 頂点データを生成
	CreateVertexData();

	// マテリアルデータを生成
	CreateMaterialData();

	// 座標変換行列データを生成
	CreateTransformationMatrixData();

	// ファイルパスを保存
	texturePath_ = texturePath;

	// テクスチャインデックスを保存
	textureIndex_ = TextureManager::GetInstance()->GetSRVIndex(texturePath_);

	// 画像切り取り範囲をぴったりにする
	FitTexCutSize();

}

void Sprite::Update()
{
	// トランスフォームの更新
	transform_.translate = { pos_.x, pos_.y, 0.0f };
	transform_.rotate = Vector3(0.0f, 0.0f, rotation_);
	transform_.scale = Vector3(size_.x, size_.y, 1.0f);

	// アンカーポイントオフセット
	float left = 0.0f - anchorPoint_.x;  // 左
	float right = 1.0f - anchorPoint_.x; // 右
	float top = 0.0f - anchorPoint_.y;   // 上
	float bottom = 1.0f - anchorPoint_.y;// 下

	//左右反転
	if (isFlipX_)
	{
		left = -left;
		right = -right;
	}

	// 上下反転
	if (isFlipY_)
	{
		top = -top;
		bottom = -bottom;
		
	}

	// テクスチャ範囲指定
	const DirectX::TexMetadata& metadata = TextureManager::GetInstance()->GetMetaData(texturePath_);
	float texLeft = texTopLeft_.x / metadata.width;
	float texRight = (texTopLeft_.x + texCutSize_.x) / metadata.width;
	float texTop = texTopLeft_.y / metadata.height;
	float texBottom = (texTopLeft_.y + texCutSize_.y) / metadata.height;

	// 頂点リソースにデータを書き込む
	vertexData_[0].position = { left, bottom, 0.0f, 1.0f }; // 左下
	vertexData_[0].texcoord = { texLeft, texBottom };
	vertexData_[0].normal = { 0.0f, 0.0f, -1.0f };

	vertexData_[1].position = { left, top, 0.0f, 1.0f }; // 左上
	vertexData_[1].texcoord = { texLeft, texTop };
	vertexData_[1].normal = { 0.0f, 0.0f, -1.0f };

	vertexData_[2].position = { right, bottom, 0.0f, 1.0f }; // 右下
	vertexData_[2].texcoord = { texRight, texBottom };
	vertexData_[2].normal = { 0.0f, 0.0f, -1.0f };

	vertexData_[3].position = { right, top, 0.0f, 1.0f }; // 右上
	vertexData_[3].texcoord = { texRight, texTop };
	vertexData_[3].normal = { 0.0f, 0.0f, -1.0f };

	// 三角形のインデックスデータを作成
	indexData_[0] = 0; indexData_[1] = 1; indexData_[2] = 2;
	indexData_[3] = 3; indexData_[4] = 2; indexData_[5] = 1;

	// Spriteの座標変換
	Matrix4x4 worldMatrixSprite = Mat4x4::MakeAffine(transform_.scale, transform_.rotate, transform_.translate);
	Matrix4x4 viewMatrixSprite = Mat4x4::MakeIdentity();
	Matrix4x4 projectionMatrixSprite = Mat4x4::MakeOrtho(0.0f, 0.0f, float(WinApp::kClientWidth), float(WinApp::kClientHeight), 0.0f, 100.0f);
	Matrix4x4 wvpMatrixSprite = Mat4x4::Multiply(worldMatrixSprite, Mat4x4::Multiply(viewMatrixSprite, projectionMatrixSprite));

	transformationMatrixData_->WVP = wvpMatrixSprite;
	transformationMatrixData_->world = worldMatrixSprite;
}

void Sprite::Draw()
{
	// 頂点バッファビューの設定
	SpriteBasic::GetInstance()->GetDX12Basic()->GetCommandList()->IASetVertexBuffers(0, 1, &vertexBufferView_);

	// インデックスバッファビューの設定
	SpriteBasic::GetInstance()->GetDX12Basic()->GetCommandList()->IASetIndexBuffer(&indexBufferView_);

	// マテリアルCBufferの場所を設定
	SpriteBasic::GetInstance()->GetDX12Basic()->GetCommandList()->SetGraphicsRootConstantBufferView(0, materialResource_->GetGPUVirtualAddress());

	// 座標変換行列CBufferの場所を設定
	SpriteBasic::GetInstance()->GetDX12Basic()->GetCommandList()->SetGraphicsRootConstantBufferView(1, transformationMatrixResource_->GetGPUVirtualAddress());

	// SRVのDescriptorTableを設定,テクスチャを指定
	SrvManager::GetInstance()->SetRootDescriptorTable(2, textureIndex_);

	// 描画
	SpriteBasic::GetInstance()->GetDX12Basic()->GetCommandList()->DrawIndexedInstanced(6, 1, 0, 0, 0);
}

void Sprite::CreateVertexData()
{
	// 頂点リソースを生成
	vertexResource_ = SpriteBasic::GetInstance()->GetDX12Basic()->MakeBufferResource(sizeof(VertexData) * 4);

	// 頂点バッファビューを作成する
	vertexBufferView_.BufferLocation = vertexResource_->GetGPUVirtualAddress();
	vertexBufferView_.SizeInBytes = sizeof(VertexData) * 4;
	vertexBufferView_.StrideInBytes = sizeof(VertexData);

	// 頂点リソースをマップ
	vertexResource_->Map(0, nullptr, reinterpret_cast<void**>(&vertexData_));

	// インデックスリソースを生成
	indexResource_ = SpriteBasic::GetInstance()->GetDX12Basic()->MakeBufferResource(sizeof(uint32_t) * 6);

	// インデックスバッファビューを作成する
	indexBufferView_.BufferLocation = indexResource_->GetGPUVirtualAddress();
	indexBufferView_.SizeInBytes = sizeof(uint32_t) * 6;
	indexBufferView_.Format = DXGI_FORMAT_R32_UINT;

	// インデックスリソースをマップ
	indexResource_->Map(0, nullptr, reinterpret_cast<void**>(&indexData_));
}

void Sprite::CreateMaterialData()
{
	// マテリアルリソースを生成
	materialResource_ = SpriteBasic::GetInstance()->GetDX12Basic()->MakeBufferResource(sizeof(Material));

	// マテリアルリソースをマップ
	materialResource_->Map(0, nullptr, reinterpret_cast<void**>(&materialData_));

	// マテリアルデータの初期値を書き込む
	materialData_->color = Vector4(1.0f, 1.0f, 1.0f, 1.0f);
	materialData_->enableLighting = false;
	materialData_->uvTransform = Mat4x4::MakeIdentity();
}

void Sprite::CreateTransformationMatrixData()
{
	// 座標変換行列リソースを生成
	transformationMatrixResource_ = SpriteBasic::GetInstance()->GetDX12Basic()->MakeBufferResource(sizeof(TransformationMatrix));

	// 座標変換行列リソースをマップ
	transformationMatrixResource_->Map(0, nullptr, reinterpret_cast<void**>(&transformationMatrixData_));

	// 座標変換行列データの初期値を書き込む
	transformationMatrixData_->WVP = Mat4x4::MakeIdentity();
	transformationMatrixData_->world = Mat4x4::MakeIdentity();
}

void Sprite::FitTexCutSize()
{
	const DirectX::TexMetadata& metadata = TextureManager::GetInstance()->GetMetaData(texturePath_);

	texCutSize_.x = static_cast<float>(metadata.width);
	texCutSize_.y = static_cast<float>(metadata.height);

	// 画像サイズをテクスチャサイズに合わせる
	size_ = texCutSize_;
}

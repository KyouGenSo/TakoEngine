#include"Sprite.h"

#include"SpriteBasic.h"
#include "TextureManager.h"
#include "SrvManager.h"
#ifdef _DEBUG
#include "ImGuiManager.h"
#endif

namespace Tako {

  void Sprite::Initialize(const std::string& texturePath)
  {
    // Transform の初期化
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
    if (isFlipX_) {
      left = -left;
      right = -right;
    }

    // 上下反転
    if (isFlipY_) {
      top = -top;
      bottom = -bottom;
    }

    // テクスチャ範囲指定
    const DirectX::TexMetadata& metadata = TextureManager::GetInstance()->GetMetaData(textureIndex_);
    float texLeft = texTopLeft_.x / metadata.width;
    float texRight = (texTopLeft_.x + texCutSize_.x) / metadata.width;
    float texTop = texTopLeft_.y / metadata.height;
    float texBottom = (texTopLeft_.y + texCutSize_.y) / metadata.height;

    // 頂点リソースにデータを書き込む
    vertexData_[0].position = { left, bottom, 0.0f, 1.0f }; // 左下
    vertexData_[0].texCoord = { texLeft, texBottom };

    vertexData_[1].position = { left, top, 0.0f, 1.0f }; // 左上
    vertexData_[1].texCoord = { texLeft, texTop };

    vertexData_[2].position = { right, bottom, 0.0f, 1.0f }; // 右下
    vertexData_[2].texCoord = { texRight, texBottom };

    vertexData_[3].position = { right, top, 0.0f, 1.0f }; // 右上
    vertexData_[3].texCoord = { texRight, texTop };

    // Sprite の座標変換
    Matrix4x4 worldMatrixSprite = Mat4x4::MakeAffine(transform_.scale, transform_.rotate, transform_.translate);
    Matrix4x4 viewMat = SpriteBasic::GetInstance()->GetViewMatrix();
    Matrix4x4 projectionMat = SpriteBasic::GetInstance()->GetProjectionMatrix();
    Matrix4x4 wvpMatrixSprite = Mat4x4::Multiply(worldMatrixSprite, Mat4x4::Multiply(viewMat, projectionMat));

    transformationMatrixData_->WVP = wvpMatrixSprite;
    transformationMatrixData_->world = worldMatrixSprite;
  }

  void Sprite::Draw()
  {
    // 頂点バッファビューの設定
    SpriteBasic::GetInstance()->GetDX12Basic()->GetCommandList()->IASetVertexBuffers(0, 1, &vertexBufferView_);

    // インデックスバッファビューの設定
    SpriteBasic::GetInstance()->GetDX12Basic()->GetCommandList()->IASetIndexBuffer(&indexBufferView_);

    // マテリアル CBuffer の場所を設定
    SpriteBasic::GetInstance()->GetDX12Basic()->GetCommandList()->SetGraphicsRootConstantBufferView(0, materialResource_->GetGPUVirtualAddress());

    // 座標変換行列 CBuffer の場所を設定
    SpriteBasic::GetInstance()->GetDX12Basic()->GetCommandList()->SetGraphicsRootConstantBufferView(1, transformationMatrixResource_->GetGPUVirtualAddress());

    // SRV の DescriptorTable を設定,テクスチャを指定
    SrvManager::GetInstance()->SetGraphicsRootDescriptorTable(2, textureIndex_);

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

    // 三角形のインデックスデータを作成
    indexData_[0] = 0; indexData_[1] = 1; indexData_[2] = 2;
    indexData_[3] = 3; indexData_[4] = 2; indexData_[5] = 1;
  }

  void Sprite::CreateMaterialData()
  {
    // マテリアルリソースを生成
    materialResource_ = SpriteBasic::GetInstance()->GetDX12Basic()->MakeBufferResource(sizeof(Material));

    // マテリアルリソースをマップ
    materialResource_->Map(0, nullptr, reinterpret_cast<void**>(&materialData_));

    // マテリアルデータの初期値を書き込む
    materialData_->color = Vector4(1.0f, 1.0f, 1.0f, 1.0f);
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
    const DirectX::TexMetadata& metadata = TextureManager::GetInstance()->GetMetaData(textureIndex_);

    texCutSize_.x = static_cast<float>(metadata.width);
    texCutSize_.y = static_cast<float>(metadata.height);

    // 画像サイズをテクスチャサイズに合わせる
    size_ = texCutSize_;
  }

  void Sprite::SetTexture(const std::string& texturePath)
  {
    texturePath_ = texturePath;
    textureIndex_ = TextureManager::GetInstance()->GetSRVIndex(texturePath_);
    FitTexCutSize();
  }

  void Sprite::SetTextureIndex(uint32_t textureIndex)
  {
    textureIndex_ = textureIndex;
    texturePath_ = TextureManager::GetInstance()->GetFileName(textureIndex_);
    FitTexCutSize();
  }

  void Sprite::DrawImGui()
  {
#ifdef _DEBUG
    // Transform 設定
    ImGui::Text("Transform Settings");
    ImGui::DragFloat2("Position", &pos_.x, 1.0f);

    // 回転角度（度数表示）
    float rotationDegrees = rotation_ * 57.2958f; // ラジアンから度に変換
    if (ImGui::DragFloat("Rotation (degrees)", &rotationDegrees, 1.0f, -360.0f, 360.0f)) {
      rotation_ = rotationDegrees * 0.0174533f; // 度からラジアンに変換
    }

    ImGui::DragFloat2("Size", &size_.x, 1.0f, 0.0f, 2000.0f);

    ImGui::Separator();

    // 描画設定
    ImGui::Text("Rendering Settings");
    ImGui::DragFloat2("Anchor Point", &anchorPoint_.x, 0.01f, 0.0f, 1.0f);

    // 色設定
    if (materialData_) {
      ImGui::ColorEdit4("Color", &materialData_->color.x);
    }

    // フリップ設定
    ImGui::Checkbox("Flip X", &isFlipX_);
    ImGui::SameLine();
    ImGui::Checkbox("Flip Y", &isFlipY_);

    ImGui::Separator();

    // テクスチャ設定
    ImGui::Text("Texture Settings");
    ImGui::DragFloat2("Texture Top-Left", &texTopLeft_.x, 1.0f, 0.0f, 4096.0f);
    ImGui::DragFloat2("Texture Cut Size", &texCutSize_.x, 1.0f, 0.0f, 4096.0f);

    // テクスチャ情報の表示
    ImGui::Separator();
    ImGui::Text("Texture Info");
    ImGui::Text("Path: %s", texturePath_.c_str());
    ImGui::Text("Index: %u", textureIndex_);

    // 現在の Transform 情報の表示
    ImGui::Separator();
    ImGui::Text("Current Transform");
    ImGui::Text("Translate: (%.2f, %.2f, %.2f)", transform_.translate.x, transform_.translate.y, transform_.translate.z);
    ImGui::Text("Scale: (%.2f, %.2f, %.2f)", transform_.scale.x, transform_.scale.y, transform_.scale.z);
#endif // _DEBUG
  }

} // namespace Tako

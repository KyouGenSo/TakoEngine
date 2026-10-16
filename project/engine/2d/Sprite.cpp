#include"Sprite.h"

#include"SpriteBasic.h"
#include "TextureManager.h"
#include "SrvManager.h"
#include "ColorFunc.h"
#include <algorithm>
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

    // 枠の画面 px。合計がサイズを超える軸は比率を保って縮める
    const float borderLeft   = sliceBorder_.x * sliceScale_;
    const float borderTop    = sliceBorder_.y * sliceScale_;
    const float borderRight  = sliceBorder_.z * sliceScale_;
    const float borderBottom = sliceBorder_.w * sliceScale_;
    const float borderWidth  = borderLeft + borderRight;
    const float borderHeight = borderTop + borderBottom;
    const float fitX = (borderWidth > size_.x && borderWidth > 0.0f) ? (std::max)(size_.x, 0.0f) / borderWidth : 1.0f;
    const float fitY = (borderHeight > size_.y && borderHeight > 0.0f) ? (std::max)(size_.y, 0.0f) / borderHeight : 1.0f;

    // 頂点はサイズ 1 の単位空間なので、枠をサイズ比へ直す
    const float unitLeft   = size_.x > 0.0f ? borderLeft * fitX / size_.x : 0.0f;
    const float unitRight  = size_.x > 0.0f ? borderRight * fitX / size_.x : 0.0f;
    const float unitTop    = size_.y > 0.0f ? borderTop * fitY / size_.y : 0.0f;
    const float unitBottom = size_.y > 0.0f ? borderBottom * fitY / size_.y : 0.0f;

    // 4x4 グリッドの線（アンカーポイント基準）。枠が 0 なら内側の線は外周と重なる
    const float left   = 0.0f - anchorPoint_.x;
    const float right  = 1.0f - anchorPoint_.x;
    const float top    = 0.0f - anchorPoint_.y;
    const float bottom = 1.0f - anchorPoint_.y;
    const float xs[4]  = { left, left + unitLeft, right - unitRight, right };
    const float ys[4]  = { top, top + unitTop, bottom - unitBottom, bottom };

    const DirectX::TexMetadata& metadata = TextureManager::GetInstance()->GetMetaData(textureIndex_);
    const float texWidth  = static_cast<float>(metadata.width);
    const float texHeight = static_cast<float>(metadata.height);
    const float us[4] = {
      texTopLeft_.x / texWidth,
      (texTopLeft_.x + sliceBorder_.x) / texWidth,
      (texTopLeft_.x + texCutSize_.x - sliceBorder_.z) / texWidth,
      (texTopLeft_.x + texCutSize_.x) / texWidth,
    };
    const float vs[4] = {
      texTopLeft_.y / texHeight,
      (texTopLeft_.y + sliceBorder_.y) / texHeight,
      (texTopLeft_.y + texCutSize_.y - sliceBorder_.w) / texHeight,
      (texTopLeft_.y + texCutSize_.y) / texHeight,
    };

    // 反転はアンカーポイントを中心とした鏡映
    const float flipX = isFlipX_ ? -1.0f : 1.0f;
    const float flipY = isFlipY_ ? -1.0f : 1.0f;

    for (uint32_t row = 0; row < 4; ++row) {
      for (uint32_t col = 0; col < 4; ++col) {
        VertexData& vertex = vertexData_[row * 4 + col];
        vertex.position = { xs[col] * flipX, ys[row] * flipY, 0.0f, 1.0f };
        vertex.texCoord = { us[col], vs[row] };
      }
    }

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
    SpriteBasic::GetInstance()->GetDX12Basic()->GetCommandList()->SetGraphicsRootConstantBufferView(SpriteBasic::kMaterialParam, materialResource_->GetGPUVirtualAddress());

    // 座標変換行列 CBuffer の場所を設定
    SpriteBasic::GetInstance()->GetDX12Basic()->GetCommandList()->SetGraphicsRootConstantBufferView(SpriteBasic::kTransformParam, transformationMatrixResource_->GetGPUVirtualAddress());

    // SRV の DescriptorTable を設定,テクスチャを指定
    SrvManager::GetInstance()->SetGraphicsRootDescriptorTable(SpriteBasic::kTextureParam, textureIndex_);

    // 枠が無ければ外周 1 枚板だけ描く
    const bool isSliced = IsSliced();
    SpriteBasic::GetInstance()->GetDX12Basic()->GetCommandList()->DrawIndexedInstanced(
      isSliced ? kSliceIndexCount : kQuadIndexCount, 1, isSliced ? kQuadIndexCount : 0, 0, 0);
  }

  void Sprite::CreateVertexData()
  {
    // 頂点リソースを生成
    vertexResource_ = SpriteBasic::GetInstance()->GetDX12Basic()->MakeBufferResource(sizeof(VertexData) * kVertexCount);

    // 頂点バッファビューを作成する
    vertexBufferView_.BufferLocation = vertexResource_->GetGPUVirtualAddress();
    vertexBufferView_.SizeInBytes = sizeof(VertexData) * kVertexCount;
    vertexBufferView_.StrideInBytes = sizeof(VertexData);

    // 頂点リソースをマップ
    vertexResource_->Map(0, nullptr, reinterpret_cast<void**>(&vertexData_));

    // インデックスリソースを生成
    constexpr uint32_t kIndexCount = kQuadIndexCount + kSliceIndexCount;
    indexResource_ = SpriteBasic::GetInstance()->GetDX12Basic()->MakeBufferResource(sizeof(uint32_t) * kIndexCount);

    // インデックスバッファビューを作成する
    indexBufferView_.BufferLocation = indexResource_->GetGPUVirtualAddress();
    indexBufferView_.SizeInBytes = sizeof(uint32_t) * kIndexCount;
    indexBufferView_.Format = DXGI_FORMAT_R32_UINT;

    // インデックスリソースをマップ
    indexResource_->Map(0, nullptr, reinterpret_cast<void**>(&indexData_));

    // 頂点番号は row * 4 + col（row 0 が上）。三角形の並びは左下・左上・右下 / 右上・右下・左上
    auto writeQuad = [](uint32_t* out, uint32_t leftTop, uint32_t rightTop, uint32_t leftBottom, uint32_t rightBottom) {
      out[0] = leftBottom; out[1] = leftTop;     out[2] = rightBottom;
      out[3] = rightTop;   out[4] = rightBottom; out[5] = leftTop;
    };

    writeQuad(indexData_, 0, 3, 12, 15);

    uint32_t* sliceIndices = indexData_ + kQuadIndexCount;
    for (uint32_t row = 0; row < 3; ++row) {
      for (uint32_t col = 0; col < 3; ++col) {
        const uint32_t leftTop = row * 4 + col;
        writeQuad(sliceIndices, leftTop, leftTop + 1, leftTop + 4, leftTop + 5);
        sliceIndices += 6;
      }
    }
  }

  void Sprite::CreateMaterialData()
  {
    // マテリアルリソースを生成
    materialResource_ = SpriteBasic::GetInstance()->GetDX12Basic()->MakeBufferResource(sizeof(Material));

    // マテリアルリソースをマップ
    materialResource_->Map(0, nullptr, reinterpret_cast<void**>(&materialData_));

    // マテリアルデータの初期値を書き込む
    materialData_->color = Color::kWhite;
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
    ImGui::DragFloat4("Slice Border (LTRB)", &sliceBorder_.x, 1.0f, 0.0f, 4096.0f);

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

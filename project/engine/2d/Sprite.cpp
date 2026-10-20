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

    const DirectX::TexMetadata& metadata = TextureManager::GetInstance()->GetMetaData(textureIndex_);
    const SliceGrid grid = CalcSliceGrid(size_, sliceBorder_, sliceScale_, texTopLeft_, texCutSize_, { static_cast<float>(metadata.width), static_cast<float>(metadata.height) });

    // 反転はアンカーポイントを中心とした鏡映
    const float flipX = isFlipX_ ? -1.0f : 1.0f;
    const float flipY = isFlipY_ ? -1.0f : 1.0f;

    // 頂点の並びは左上・右上・左下・右下
    auto writeQuad = [&](uint32_t base, float left, float top, float right, float bottom, const Vector2& uvMin, const Vector2& uvMax) {
      const float xs[2] = { (left - anchorPoint_.x) * flipX, (right - anchorPoint_.x) * flipX };
      const float ys[2] = { (top - anchorPoint_.y) * flipY, (bottom - anchorPoint_.y) * flipY };
      for (uint32_t i = 0; i < 4; ++i) {
        const uint32_t col = i % 2;
        const uint32_t row = i / 2;
        vertexData_[base + i].position = { xs[col], ys[row], 0.0f, 1.0f };
        vertexData_[base + i].texCoord = { col == 0 ? uvMin.x : uvMax.x, row == 0 ? uvMin.y : uvMax.y };
      }
    };

    writeQuad(0, grid.xs[0], grid.ys[0], grid.xs[3], grid.ys[3], { grid.us[0], grid.vs[0] }, { grid.us[3], grid.vs[3] });

    // 区画は頂点を共有しない。別画像の区画はその画像全体（UV 0..1）を使うため
    for (int region = 0; region < kRegionCount; ++region) {
      const int     col     = region % 3;
      const int     row     = region / 3;
      const bool    isOwn   = regionTextureIndices_[region] != 0;
      const Vector2 uvMin   = isOwn ? Vector2{ 0.0f, 0.0f } : Vector2{ grid.us[col], grid.vs[row] };
      const Vector2 uvMax   = isOwn ? Vector2{ 1.0f, 1.0f } : Vector2{ grid.us[col + 1], grid.vs[row + 1] };
      writeQuad(4 + static_cast<uint32_t>(region) * 4, grid.xs[col], grid.ys[row], grid.xs[col + 1], grid.ys[row + 1], uvMin, uvMax);
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
    DX12Basic* dx12 = SpriteBasic::GetInstance()->GetDX12Basic();
    ID3D12GraphicsCommandList* commandList = dx12->GetCommandList();
    commandList->IASetVertexBuffers(0, 1, &vertexBufferView_);
    commandList->IASetIndexBuffer(&indexBufferView_);

    CommandContext& context = dx12->GetContext();
    CommandContext::ShaderBinder binder = context.Graphics();
    binder.BindCBV("gMaterial"_id, materialResource_->GetGPUVirtualAddress());
    binder.BindCBV("gTransformationMatrix"_id, transformationMatrixResource_->GetGPUVirtualAddress());

    if (!IsSliced()) {
      binder.BindSRV("gTexture"_id, textureIndex_);
      context.DrawIndexedInstanced(kQuadIndexCount, 1, 0, 0, 0);
      return;
    }

    // 表示する区画のうち、同じテクスチャが続くものは 1 回で描く（区画の設定が無ければ 9 枚を 1 回）
    constexpr uint32_t kIndicesPerRegion = kSliceIndexCount / kRegionCount;
    int                runStart          = -1;
    uint32_t           runTexture        = 0;
    auto flush = [&](int end) {
      if (runStart < 0) {
        return;
      }
      binder.BindSRV("gTexture"_id, runTexture);
      context.DrawIndexedInstanced(static_cast<uint32_t>(end - runStart) * kIndicesPerRegion, 1, kQuadIndexCount + static_cast<uint32_t>(runStart) * kIndicesPerRegion, 0, 0);
      runStart = -1;
    };
    for (int region = 0; region < kRegionCount; ++region) {
      const uint32_t texture = regionTextureIndices_[region] != 0 ? regionTextureIndices_[region] : textureIndex_;
      if (runStart >= 0 && (!regionVisible_[region] || texture != runTexture)) {
        flush(region);
      }
      if (regionVisible_[region] && runStart < 0) {
        runStart   = region;
        runTexture = texture;
      }
    }
    flush(kRegionCount);
  }

  Sprite::SliceGrid Sprite::CalcSliceGrid(const Vector2& size, const Vector4& borderTexPx, float scale, const Vector2& texLeftTop, const Vector2& texCutSize, const Vector2& texSize)
  {
    // 枠の画面 px。合計がサイズを超える軸は比率を保って縮める
    const float borderLeft   = borderTexPx.x * scale;
    const float borderTop    = borderTexPx.y * scale;
    const float borderRight  = borderTexPx.z * scale;
    const float borderBottom = borderTexPx.w * scale;
    const float borderWidth  = borderLeft + borderRight;
    const float borderHeight = borderTop + borderBottom;
    const float fitX = (borderWidth > size.x && borderWidth > 0.0f) ? (std::max)(size.x, 0.0f) / borderWidth : 1.0f;
    const float fitY = (borderHeight > size.y && borderHeight > 0.0f) ? (std::max)(size.y, 0.0f) / borderHeight : 1.0f;

    // 単位空間なので、枠をサイズ比へ直す。枠が 0 なら内側の線は外周と重なる
    const float unitLeft   = size.x > 0.0f ? borderLeft * fitX / size.x : 0.0f;
    const float unitRight  = size.x > 0.0f ? borderRight * fitX / size.x : 0.0f;
    const float unitTop    = size.y > 0.0f ? borderTop * fitY / size.y : 0.0f;
    const float unitBottom = size.y > 0.0f ? borderBottom * fitY / size.y : 0.0f;

    return SliceGrid{
      .xs = { 0.0f, unitLeft, 1.0f - unitRight, 1.0f },
      .ys = { 0.0f, unitTop, 1.0f - unitBottom, 1.0f },
      .us = {
        texLeftTop.x / texSize.x,
        (texLeftTop.x + borderTexPx.x) / texSize.x,
        (texLeftTop.x + texCutSize.x - borderTexPx.z) / texSize.x,
        (texLeftTop.x + texCutSize.x) / texSize.x,
      },
      .vs = {
        texLeftTop.y / texSize.y,
        (texLeftTop.y + borderTexPx.y) / texSize.y,
        (texLeftTop.y + texCutSize.y - borderTexPx.w) / texSize.y,
        (texLeftTop.y + texCutSize.y) / texSize.y,
      },
    };
  }

  void Sprite::SetRegionTexture(int region, const std::string& texturePath)
  {
    if (regionTextures_[region] == texturePath) {
      return;
    }
    // 読み込めなかったキーも覚えておき、毎フレーム引き直さない
    regionTextures_[region] = texturePath;
    TextureManager* textureManager = TextureManager::GetInstance();
    regionTextureIndices_[region] = (!texturePath.empty() && textureManager->IsLoaded(texturePath)) ? textureManager->GetSRVIndex(texturePath) : 0;
  }

  bool Sprite::IsSliced() const
  {
    const bool hasBorder = sliceBorder_.x > 0.0f || sliceBorder_.y > 0.0f || sliceBorder_.z > 0.0f || sliceBorder_.w > 0.0f;
    return hasBorder || std::ranges::find(regionVisible_, false) != regionVisible_.end() ||
      std::ranges::any_of(regionTextureIndices_, [](uint32_t index) { return index != 0; });
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

    // 四角形 1 枚は 4 頂点（左上・右上・左下・右下）。外周 1 枚板が先頭、続いて区画が行順。
    // 三角形の並びは左下・左上・右下 / 右上・右下・左上
    for (uint32_t quad = 0; quad < 1 + kRegionCount; ++quad) {
      const uint32_t base = quad * 4;
      uint32_t*      out  = indexData_ + quad * kQuadIndexCount;
      out[0] = base + 2; out[1] = base + 0; out[2] = base + 3;
      out[3] = base + 1; out[4] = base + 3; out[5] = base + 0;
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

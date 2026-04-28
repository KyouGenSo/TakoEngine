#include "Decal.h"
#include "DecalManager.h"
#include "DX12Basic.h"
#include "Camera.h"
#include "SrvManager.h"
#include "TextureManager.h"
#include "Mat4x4Func.h"

#ifdef _DEBUG
#include "ImGuiManager.h"
#include "Draw2D.h"
#include "OBB.h"
#endif

namespace Tako {

  Decal::~Decal()
  {
    // アプリ終了時の静的破棄順序でDecalManagerが先に消えている可能性に備える
    if (DecalManager::GetInstance()) {
      DecalManager::GetInstance()->RemoveDecal(this);
    }
  }

  void Decal::Initialize()
  {
    DX12Basic* dx12 = DecalManager::GetInstance()->GetDX12Basic();

    textureSrvIndex_ = TextureManager::GetInstance()->GetEngineDefaultSRVIndex("white.dds");

    // DecalData 定数バッファの作成
    decalDataBuffer_ = dx12->MakeBufferResource(sizeof(DecalDataGPU));
    decalDataBuffer_->Map(0, nullptr, reinterpret_cast<void**>(&decalDataMapped_));

    // DecalManager に自動登録
    DecalManager::GetInstance()->AddDecal(this);
  }

  void Decal::Update()
  {
    if (!isVisible_) return;

    // ワールド行列を構築
    Matrix4x4 worldMatrix = Mat4x4::MakeAffine(transform_.scale, transform_.rotate, transform_.translate);

    // 逆ワールド行列を計算
    Matrix4x4 worldInverse = Mat4x4::Inverse(worldMatrix);

    // WVP 行列を計算
    Matrix4x4 viewProjMatrix = DecalManager::GetInstance()->GetViewProjectionMatrix();
    Matrix4x4 wvpMatrix = worldMatrix * viewProjMatrix;

    // 定数バッファに書き込み
    decalDataMapped_->decalWorldInverse = worldInverse;
    decalDataMapped_->decalWVP = wvpMatrix;
    decalDataMapped_->color = color_;
    decalDataMapped_->shapeType = static_cast<int32_t>(shape_);
    decalDataMapped_->fanHalfAngle = fanHalfAngle_;
    decalDataMapped_->edgeSoftness = edgeSoftness_;
    decalDataMapped_->useTexture = useTexture_ ? 1 : 0;
  }

  void Decal::Draw()
  {
    if (!isVisible_) return;

    DX12Basic* dx12 = DecalManager::GetInstance()->GetDX12Basic();

    // DecalData CBV をバインド（RP#1）
    dx12->GetCommandList()->SetGraphicsRootConstantBufferView(
      1,
      decalDataBuffer_->GetGPUVirtualAddress()
    );

    // テクスチャモード時: デカールテクスチャ SRV をバインド（RP#3）
    if (useTexture_ && textureSrvIndex_ != 0) {
      SrvManager::GetInstance()->SetGraphicsRootDescriptorTable(3, textureSrvIndex_);
    }

    // インデックス付きドローコール（36 インデックス = 12 三角形のキューブ）
    dx12->GetCommandList()->DrawIndexedInstanced(36, 1, 0, 0, 0);
  }

  void Decal::SetTexture(const std::string& textureName)
  {
    useTexture_ = true;
    textureSrvIndex_ = TextureManager::GetInstance()->GetSRVIndex(textureName);
  }

  void Decal::ClearTexture()
  {
    useTexture_ = false;
    textureSrvIndex_ = 0;
  }

  void Decal::DrawImGui()
  {
#ifdef _DEBUG
    // === Transform ===
    ImGui::Text("Transform");
    ImGui::DragFloat3("Translate", &transform_.translate.x, 0.1f);

    // 回転角度（度数表示）
    Vector3 rotateDeg = {
      transform_.rotate.x * 57.2958f,
      transform_.rotate.y * 57.2958f,
      transform_.rotate.z * 57.2958f
    };
    if (ImGui::DragFloat3("Rotate (deg)", &rotateDeg.x, 1.0f, -360.0f, 360.0f)) {
      transform_.rotate = {
        rotateDeg.x * 0.0174533f,
        rotateDeg.y * 0.0174533f,
        rotateDeg.z * 0.0174533f
      };
    }

    ImGui::DragFloat3("Scale", &transform_.scale.x, 0.1f, 0.01f, 100.0f);

    ImGui::Separator();

    // === Rendering ===
    ImGui::Text("Rendering");
    ImGui::ColorEdit4("Color", &color_.x);
    ImGui::Checkbox("Visible", &isVisible_);
    ImGui::Checkbox("DebugViewVisible", &isDebugViewVisible_);

    ImGui::Separator();

    // === Shape ===
    ImGui::Text("Shape");
    const char* shapeNames[] = { "Circle", "Fan", "Rectangle" };
    int currentShape = static_cast<int>(shape_);
    if (ImGui::Combo("Shape", &currentShape, shapeNames, IM_ARRAYSIZE(shapeNames))) {
      shape_ = static_cast<DecalShape>(currentShape);
    }

    // 扇形パラメータ（Fan 選択時のみ表示）
    if (shape_ == DecalShape::Fan) {
      float halfAngleDeg = fanHalfAngle_ * 57.2958f;
      if (ImGui::DragFloat("Fan Half Angle (deg)", &halfAngleDeg, 1.0f, 0.0f, 180.0f)) {
        fanHalfAngle_ = halfAngleDeg * 0.0174533f;
      }
    }

    ImGui::DragFloat("Edge Softness", &edgeSoftness_, 0.01f, 0.0f, 1.0f);

    ImGui::Separator();

    // === Texture ===
    ImGui::Text("Texture");
    ImGui::Checkbox("Use Texture", &useTexture_);
    if (useTexture_) {
      ImGui::Text("SRV Index: %u", textureSrvIndex_);
    }
#endif // _DEBUG
  }

  void Decal::DrawDebug()
  {
#ifdef _DEBUG
    if (!isDebugViewVisible_) return;

    // OBB を構築
    OBB obb;
    obb.center = transform_.translate;
    obb.halfExtents = { transform_.scale.x * 0.5f, transform_.scale.y * 0.5f, transform_.scale.z * 0.5f };
    obb.orientation = Mat4x4::MakeRotateXYZ(transform_.rotate);

    // アルファ 1.0 で視認性確保
    Vector4 debugColor = { 1.0f, 1.0f, 1.0f, 1.0f };

    Draw2D::GetInstance()->DrawOBB(obb, debugColor);
#endif
  }

} // namespace Tako

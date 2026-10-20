#include "SkyBox.h"
#include "Object3dBasic.h"
#include "TextureManager.h"
#include "SrvManager.h"
#include "DX12Basic.h"
#include "Mat4x4Func.h"
#include "ColorFunc.h"

#ifdef _DEBUG
#include "DebugUIManager.h"
#endif

namespace Tako {

void SkyBox::Initialize(const std::string& texturePath)
{
  transform_.scale = { 500.0f, 500.0f, 500.0f };
  transform_.rotate = { 0.0f, 0.0f, 0.0f };
  transform_.translate = { 0.0f, 0.0f, 0.0f };

  worldMatrix_ = Mat4x4::MakeIdentity();

  dx12_ = Object3dBasic::GetInstance()->GetDX12Basic();

  CreatePipeline();
  CreateVertexData();
  CreateIndexData();
  CreateMaterialData();
  CreateTransformationMatrixData();

  textureIndex_ = TextureManager::GetInstance()->GetSRVIndex(texturePath);
}

void SkyBox::Update()
{
  worldMatrix_ = Mat4x4::MakeAffine(transform_.scale, transform_.rotate, transform_.translate);

  transformationMatrixData_->world = worldMatrix_;
}

void SkyBox::Draw()
{
  CommandContext& context = dx12_->GetContext();
  context.SetGraphicsPipeline(pipeline_);

  ID3D12GraphicsCommandList* commandList = dx12_->GetCommandList();
  commandList->IASetVertexBuffers(0, 1, &vertexBufferView_);
  commandList->IASetIndexBuffer(&indexBufferView_);

  CommandContext::ShaderBinder binder = context.Graphics();
  binder.BindCBV("gTransformationMatrix"_id, transformationMatrixResource_->GetGPUVirtualAddress());
  binder.BindSRV("gSkyBoxTexture"_id, textureIndex_);
  binder.BindCBV("gMaterial"_id, materialResource_->GetGPUVirtualAddress());
  // 本編/デバッグビューのどちらで呼ばれても現在の視点で描く
  binder.BindCBV("gView"_id, Object3dBasic::GetInstance()->GetViewAddress());

  context.DrawIndexedInstanced(36, 1, 0, 0, 0);
}

void SkyBox::CreatePipeline()
{
  ShaderLibrary& library = dx12_->GetShaderLibrary();
  pipeline_.vs = library.Get(L"SkyBox.VS.hlsl", ShaderStage::Vertex);
  pipeline_.ps = library.Get(L"SkyBox.PS.hlsl", ShaderStage::Pixel);
  pipeline_.vertexLayout = VertexLayout::Position;
  // 最遠面として描くので深度は書き込まない
  pipeline_.renderState.depthWrite = false;
}

void SkyBox::CreateVertexData()
{
  vertexResource_ = dx12_->MakeBufferResource(sizeof(VertexData) * 24);

  vertexResource_->Map(0, nullptr, reinterpret_cast<void**>(&vertexData_));

  // 右面。インデックス[0, 1, 2][2, 1, 3]
  vertexData_[0] = { Vector4(1.0f, 1.0f, 1.0f, 1.0f) };
  vertexData_[1] = { Vector4(1.0f, 1.0f, -1.0f, 1.0f) };
  vertexData_[2] = { Vector4(1.0f, -1.0f, 1.0f, 1.0f) };
  vertexData_[3] = { Vector4(1.0f, -1.0f, -1.0f, 1.0f) };
  // 左面。インデックス[4, 5, 6][6, 5, 7]
  vertexData_[4] = { Vector4(-1.0f, 1.0f, -1.0f, 1.0f) };
  vertexData_[5] = { Vector4(-1.0f, 1.0f, 1.0f, 1.0f) };
  vertexData_[6] = { Vector4(-1.0f, -1.0f, -1.0f, 1.0f) };
  vertexData_[7] = { Vector4(-1.0f, -1.0f, 1.0f, 1.0f) };
  // 前面。インデックス[8, 9, 10][10, 9, 11]
  vertexData_[8] = { Vector4(-1.0f, 1.0f, 1.0f, 1.0f) };
  vertexData_[9] = { Vector4(1.0f, 1.0f, 1.0f, 1.0f) };
  vertexData_[10] = { Vector4(-1.0f, -1.0f, 1.0f, 1.0f) };
  vertexData_[11] = { Vector4(1.0f, -1.0f, 1.0f, 1.0f) };
  // 後面。インデックス[12, 13, 14][14, 13, 15]
  vertexData_[12] = { Vector4(1.0f, 1.0f, -1.0f, 1.0f) };
  vertexData_[13] = { Vector4(-1.0f, 1.0f, -1.0f, 1.0f) };
  vertexData_[14] = { Vector4(1.0f, -1.0f, -1.0f, 1.0f) };
  vertexData_[15] = { Vector4(-1.0f, -1.0f, -1.0f, 1.0f) };
  // 上面。インデックス[16, 17, 18][18, 17, 19]
  vertexData_[16] = { Vector4(-1.0f, 1.0f, -1.0f, 1.0f) };
  vertexData_[17] = { Vector4(1.0f, 1.0f, -1.0f, 1.0f) };
  vertexData_[18] = { Vector4(-1.0f, 1.0f, 1.0f, 1.0f) };
  vertexData_[19] = { Vector4(1.0f, 1.0f, 1.0f, 1.0f) };
  // 下面。インデックス[20, 21, 22][22, 21, 23]
  vertexData_[20] = { Vector4(1.0f, -1.0f, -1.0f, 1.0f) };
  vertexData_[21] = { Vector4(-1.0f, -1.0f, -1.0f, 1.0f) };
  vertexData_[22] = { Vector4(1.0f, -1.0f, 1.0f, 1.0f) };
  vertexData_[23] = { Vector4(-1.0f, -1.0f, 1.0f, 1.0f) };

  vertexBufferView_.BufferLocation = vertexResource_->GetGPUVirtualAddress();
  vertexBufferView_.SizeInBytes = sizeof(VertexData) * 24;
  vertexBufferView_.StrideInBytes = sizeof(VertexData);
}

void SkyBox::CreateIndexData()
{
  indexResource_ = dx12_->MakeBufferResource(sizeof(uint32_t) * 36);

  indexResource_->Map(0, nullptr, reinterpret_cast<void**>(&indexData_));

  // 各面2三角形分のインデックス
  indexData_[0] = 0; indexData_[1] = 1; indexData_[2] = 2;
  indexData_[3] = 2; indexData_[4] = 1; indexData_[5] = 3;
  indexData_[6] = 4; indexData_[7] = 5; indexData_[8] = 6;
  indexData_[9] = 6; indexData_[10] = 5; indexData_[11] = 7;
  indexData_[12] = 8; indexData_[13] = 9; indexData_[14] = 10;
  indexData_[15] = 10; indexData_[16] = 9; indexData_[17] = 11;
  indexData_[18] = 12; indexData_[19] = 13; indexData_[20] = 14;
  indexData_[21] = 14; indexData_[22] = 13; indexData_[23] = 15;
  indexData_[24] = 16; indexData_[25] = 17; indexData_[26] = 18;
  indexData_[27] = 18; indexData_[28] = 17; indexData_[29] = 19;
  indexData_[30] = 20; indexData_[31] = 21; indexData_[32] = 22;
  indexData_[33] = 22; indexData_[34] = 21; indexData_[35] = 23;

  indexBufferView_.BufferLocation = indexResource_->GetGPUVirtualAddress();
  indexBufferView_.SizeInBytes = sizeof(uint32_t) * 36;
  indexBufferView_.Format = DXGI_FORMAT_R32_UINT;
}

void SkyBox::CreateMaterialData()
{
  materialResource_ = dx12_->MakeBufferResource(sizeof(Material));
  materialResource_->Map(0, nullptr, reinterpret_cast<void**>(&materialData_));
  materialData_->color = Color::kWhite;
}

void SkyBox::CreateTransformationMatrixData()
{
  transformationMatrixResource_ = dx12_->MakeBufferResource(sizeof(TransformationMatrix));
  transformationMatrixResource_->Map(0, nullptr, reinterpret_cast<void**>(&transformationMatrixData_));
  transformationMatrixData_->world = Mat4x4::MakeIdentity();
}

} // namespace Tako
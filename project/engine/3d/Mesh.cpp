#include "Mesh.h"
#include "DX12Basic.h"
#include "Mat4x4Func.h"
#include "ModelBasic.h"
#include "SrvManager.h"

///------------------------------------------------///
///                 PUBLIC METHODS                ///
///-----------------------------------------------///
void Mesh::Initialize(ModelBasic* modelBasic, const std::vector<VertexData>& vertices, const std::vector<uint32_t>& indices, const TextureData& textureData)
{
  modelBasic_ = modelBasic;
  dx12_ = modelBasic_->GetDX12Basic();

  vertices_ = vertices;
  indices_ = indices;
  textureData_ = textureData;

  CreateVertexData();
  CreateVertexBufferView();
  CreateIndexData();
  CreateMaterialData();
}

void Mesh::Draw(Matrix4x4 world, Matrix4x4 viewProjection)
{
  // 頂点バッファビューを設定
  dx12_->GetCommandList()->IASetVertexBuffers(0, 1, &vertexBufferView_);

  // インデックスバッファビューを設定
  dx12_->GetCommandList()->IASetIndexBuffer(&indexBufferView_);

  // マテリアルデータを設定
  dx12_->GetCommandList()->SetGraphicsRootConstantBufferView(0, materialResource_->GetGPUVirtualAddress());

  // テクスチャを設定
  SrvManager::GetInstance()->SetGraphicsRootDescriptorTable(2, textureData_.textureIndex);

  // 描画
  dx12_->GetCommandList()->DrawIndexedInstanced(static_cast<UINT>(indices_.size()), 1, 0, 0, 0);

}

///-----------------------------------------------///
///                PRIVATE METHODS                ///
///-----------------------------------------------///
void Mesh::CreateVertexData()
{
  // 頂点リソースを生成
  vertexResource_ = dx12_->MakeBufferResource(sizeof(VertexData) * vertices_.size());
  // 頂点リソースをマップ
  vertexResource_->Map(0, nullptr, reinterpret_cast<void**>(&vertexData_));
  memcpy(vertexData_, vertices_.data(), sizeof(VertexData) * vertices_.size());

  vertexSrvIndex_ = SrvManager::GetInstance()->Allocate();
  SrvManager::GetInstance()->CreateSRVForStructuredBuffer(vertexSrvIndex_, vertexResource_.Get(), static_cast<UINT>(vertices_.size()), sizeof(VertexData));
}

void Mesh::CreateVertexBufferView()
{
  // VertexBufferViewを作成
  vertexBufferView_.BufferLocation = vertexResource_->GetGPUVirtualAddress();
  vertexBufferView_.SizeInBytes = static_cast<UINT>(sizeof(VertexData) * vertices_.size());
  vertexBufferView_.StrideInBytes = sizeof(VertexData);
}

void Mesh::CreateIndexData()
{
  // インデックスリソースを生成
  indexResource_ = dx12_->MakeBufferResource(sizeof(uint32_t) * indices_.size());

  // インデックスバッファビューを作る
  indexBufferView_.BufferLocation = indexResource_->GetGPUVirtualAddress();
  indexBufferView_.SizeInBytes = static_cast<UINT>(sizeof(uint32_t) * indices_.size());
  indexBufferView_.Format = DXGI_FORMAT_R32_UINT;

  // インデックスリソースをマップ
  uint32_t* indexData = nullptr;
  indexResource_->Map(0, nullptr, reinterpret_cast<void**>(&indexData));
  memcpy(indexData, indices_.data(), sizeof(uint32_t) * indices_.size());

  indexResource_->Unmap(0, nullptr);
}

void Mesh::CreateMaterialData()
{
  // マテリアルリソースを生成
  materialResource_ = dx12_->MakeBufferResource(sizeof(Material));

  // マテリアルリソースをマップ
  materialResource_->Map(0, nullptr, reinterpret_cast<void**>(&materialData_));

  // マテリアルデータの初期値を書き込む
  materialData_->color = Vector4(1.0f, 1.0f, 1.0f, 1.0f);
  materialData_->enableLighting = true;
  materialData_->enableHighlight = true;
  materialData_->uvTransform = Mat4x4::MakeIdentity();
  materialData_->shininess = 15.0f;
}
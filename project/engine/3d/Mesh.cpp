#include "Mesh.h"
#include "DX12Basic.h"
#include "Mat4x4Func.h"
#include "ModelBasic.h"
#include "Object3dBasic.h"
#include "SrvManager.h"
#include "TextureManager.h"

//　デストラクタ
Mesh::~Mesh()
{
  // リソースの解放
  ReleaseSRVIndex();
}

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

  vertexSrvIndex_ = 0;
  influenceSrvIndex_ = 0;
  uavIndex_ = 0;

  CreateVertexData();
  CreateVertexBufferView();
  CreateIndexData();
  CreateMaterialData();
  CreateTransformation();
}

void Mesh::Draw()
{
  // スキニング対応版の描画処理
  D3D12_VERTEX_BUFFER_VIEW vbvToUse = hasSkinning_ ? skinnedVertexBufferView_ : vertexBufferView_;

  // 頂点バッファビューを設定
  dx12_->GetCommandList()->IASetVertexBuffers(0, 1, &vbvToUse);

  // インデックスバッファビューを設定
  dx12_->GetCommandList()->IASetIndexBuffer(&indexBufferView_);

  // マテリアルデータを設定
  if (!Object3dBasic::GetInstance()->IsRenderingShadowMap()) {
    dx12_->GetCommandList()->SetGraphicsRootConstantBufferView(0, materialResource_->GetGPUVirtualAddress());

    // テクスチャを設定
    SrvManager::GetInstance()->SetGraphicsRootDescriptorTable(2, textureData_.textureIndex);

    // 環境マップを使用する場合の設定
    if (materialData_->enableEnvMap && envTextureIndex_ != 0) {
      SrvManager::GetInstance()->SetGraphicsRootDescriptorTable(8, envTextureIndex_);
    } else {
      // 環境マップが無効またはテクスチャが設定されていない場合は、デフォルトテクスチャを設定
      uint32_t defaultTextureIndex = TextureManager::GetInstance()->GetSRVIndex("white.png");
      SrvManager::GetInstance()->SetGraphicsRootDescriptorTable(8, defaultTextureIndex);
    }
  }

  // 描画
  dx12_->GetCommandList()->DrawIndexedInstanced(static_cast<UINT>(indices_.size()), 1, 0, 0, 0);

}

void Mesh::DrawWithCurrentTransform()
{
  // スキニング対応版の描画処理
  D3D12_VERTEX_BUFFER_VIEW vbvToUse = hasSkinning_ ? skinnedVertexBufferView_ : vertexBufferView_;

  // 頂点バッファビューを設定
  dx12_->GetCommandList()->IASetVertexBuffers(0, 1, &vbvToUse);

  // インデックスバッファビューを設定
  dx12_->GetCommandList()->IASetIndexBuffer(&indexBufferView_);

  if (Object3dBasic::GetInstance()->IsRenderingShadowMap()) {
    // シャドウマップレンダリング時は座標変換行列のみ設定（パラメータ0）
    dx12_->GetCommandList()->SetGraphicsRootConstantBufferView(0, transformationResource_->GetGPUVirtualAddress());
  } else {
    // マテリアルデータを設定
    dx12_->GetCommandList()->SetGraphicsRootConstantBufferView(0, materialResource_->GetGPUVirtualAddress());

    // 座標変換行列データを設定
    dx12_->GetCommandList()->SetGraphicsRootConstantBufferView(1, transformationResource_->GetGPUVirtualAddress());

    // テクスチャを設定
    SrvManager::GetInstance()->SetGraphicsRootDescriptorTable(2, textureData_.textureIndex);

    // 環境マップテクスチャを設定
    if (materialData_->enableEnvMap && envTextureIndex_ != 0) {
      // 環境マップが有効で、テクスチャが設定されている場合
      SrvManager::GetInstance()->SetGraphicsRootDescriptorTable(8, envTextureIndex_);
    } else {
      // 環境マップが無効またはテクスチャが設定されていない場合は、デフォルトテクスチャを設定
      uint32_t defaultTextureIndex = TextureManager::GetInstance()->GetSRVIndex("white.png");
      SrvManager::GetInstance()->SetGraphicsRootDescriptorTable(8, defaultTextureIndex);
    }
  }

  // 描画
  dx12_->GetCommandList()->DrawIndexedInstanced(static_cast<UINT>(indices_.size()), 1, 0, 0, 0);
}

void Mesh::UpdateTransformation(const Matrix4x4& world, const Matrix4x4& viewProjection)
{
  // ワールド変換行列とビュープロジェクション行列からWVP行列を計算
  Matrix4x4 wvpMatrix = Mat4x4::Multiply(world, viewProjection);

  // 変換行列データを更新
  transformationData_->WVP = wvpMatrix;
  transformationData_->world = world;
  transformationData_->worldInvTranspose = Mat4x4::InverseTranspose(world);
}

Mesh* Mesh::Clone() const
{
  // メッシュのクローンを作成
  Mesh* newMesh = new Mesh();
  newMesh->Initialize(modelBasic_, vertices_, indices_, textureData_);
  return newMesh;
}

void Mesh::ReleaseSRVIndex()
{
  // SRVインデックスの解放
  if (vertexSrvIndex_ != 0)
  {
    // 解放前にアロケートされているか確認
    if (SrvManager::GetInstance()->IsAllocated(vertexSrvIndex_))
    {
      SrvManager::GetInstance()->Free(vertexSrvIndex_);
    }
    vertexSrvIndex_ = 0;
  }

  if (influenceSrvIndex_ != 0)
  {
    if (SrvManager::GetInstance()->IsAllocated(influenceSrvIndex_))
    {
      SrvManager::GetInstance()->Free(influenceSrvIndex_);
    }
    influenceSrvIndex_ = 0;
  }

  if (uavIndex_ != 0)
  {
    if (SrvManager::GetInstance()->IsAllocated(uavIndex_))
    {
      SrvManager::GetInstance()->Free(uavIndex_);
    }
    uavIndex_ = 0;
  }
}

void Mesh::InitializeSkinning(const std::map<std::string, JointWeightData>& skinClusterData, const std::map<std::string, int32_t>& jointMap)
{
  // スキニングフラグを設定
  hasSkinning_ = true;

  // 全頂点の影響度配列を初期化（MAX_INFLUENCE個の要素を持つ配列）
  vertexInfluences_.resize(vertices_.size());

  // すべての頂点影響度を0で初期化
  for (auto& influence : vertexInfluences_) {
    for (uint32_t i = 0; i < MAX_INFLUENCE; ++i) {
      influence.weights[i] = 0.0f;
      influence.jointIndices[i] = 0;
    }
  }

  // スキンクラスターデータから頂点影響度データを構築
  for (const auto& [jointName, jointWeightData] : skinClusterData) {
    // ジョイント名からインデックスを検索
    auto it = jointMap.find(jointName);
    if (it == jointMap.end()) {
      continue; // ジョイントが見つからない場合はスキップ
    }

    int32_t jointIndex = it->second;

    // このジョイントの影響を受ける頂点を処理
    for (const auto& vertexWeight : jointWeightData.vertexWeights) {
      uint32_t vertexIndex = vertexWeight.vertexIndex;
      float weight = vertexWeight.weight;

      // 頂点インデックスが範囲内かチェック
      if (vertexIndex >= vertexInfluences_.size()) {
        continue;
      }

      // この頂点の影響度データに追加
      auto& influence = vertexInfluences_[vertexIndex];

      // 空きスロットを探して重みとインデックスを設定
      for (uint32_t i = 0; i < MAX_INFLUENCE; ++i) {
        if (influence.weights[i] == 0.0f) {
          influence.weights[i] = weight;
          influence.jointIndices[i] = jointIndex;
          break;
        }
      }
    }
  }

  // 各頂点の重みを正規化（合計が1になるように）
  for (auto& influence : vertexInfluences_) {
    float totalWeight = 0.0f;
    for (uint32_t i = 0; i < MAX_INFLUENCE; ++i) {
      totalWeight += influence.weights[i];
    }

    // 重みの合計が0より大きい場合のみ正規化
    if (totalWeight > 0.0f) {
      float invTotalWeight = 1.0f / totalWeight;
      for (uint32_t i = 0; i < MAX_INFLUENCE; ++i) {
        influence.weights[i] *= invTotalWeight;
      }
    }
  }

  // UAVリソースを生成
  SetupSkinningUAV();
}

void Mesh::SkinningCompute()
{
  if (!hasSkinning_) return;
  
  // 既にこのフレームでスキニングが実行されていたらスキップ
  if (skinningComputedThisFrame_) return;
  
  // 現在のリソース状態を確認して適切に遷移
  // 初回以外は VERTEX_AND_CONSTANT_BUFFER → UAV への遷移が必要
  static bool isFirstCompute = true;
  if (!isFirstCompute) {
    dx12_->TransitionResourceState(
      D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER,
      D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
      uavVertexOutputResource_.Get());
  }
  isFirstCompute = false;

  SrvManager* srvManager = SrvManager::GetInstance();

  // 入力頂点バッファのSRV設定
  srvManager->SetComputeRootDescriptorTable(1, vertexSrvIndex_);

  // 頂点影響度データのSRV設定
  srvManager->SetComputeRootDescriptorTable(2, influenceSrvIndex_);

  // 出力頂点バッファのUAV設定
  srvManager->SetComputeRootDescriptorTable(3, uavIndex_);

  // スキニング情報の設定
  dx12_->GetCommandList()->SetComputeRootConstantBufferView(4, skinningInfoResource_->GetGPUVirtualAddress());

  // ComputeShaderの実行
  dx12_->GetCommandList()->Dispatch(
    static_cast<UINT>(vertices_.size() + 1023) / 1024, 1, 1);

  // バリア設定
  dx12_->TransitionResourceState(
    D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
    D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER,
    uavVertexOutputResource_.Get());
    
  // フラグを設定
  skinningComputedThisFrame_ = true;
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
  materialData_->color = textureData_.baseColor;
  materialData_->enableLighting = true;
  materialData_->enableHighlight = true;
  materialData_->uvTransform = Mat4x4::MakeIdentity();
  materialData_->shininess = 15.0f;
  materialData_->envMapCoefficient = 1.f;
  materialData_->enableEnvMap = false;
}

void Mesh::CreateTransformation()
{
  // 座標変換行列リソースを生成
  transformationResource_ = dx12_->MakeBufferResource(sizeof(Object3d::TransformationMatrix));
  // 座標変換行列リソースをマップ
  transformationResource_->Map(0, nullptr, reinterpret_cast<void**>(&transformationData_));

  // 座標変換行列データの初期値を書き込む
  transformationData_->WVP = Mat4x4::MakeIdentity();
  transformationData_->world = Mat4x4::MakeIdentity();
  transformationData_->worldInvTranspose = Mat4x4::MakeIdentity();
}

void Mesh::SetupSkinningUAV()
{
  // スキニングがない場合は何もしない
  if (!hasSkinning_) {
    return;
  }

  DX12Basic* dx12 = modelBasic_->GetDX12Basic();
  SrvManager* srvManager = SrvManager::GetInstance();

  // 1. 頂点バッファのSRVを作成
  if (vertexSrvIndex_ == 0) {
    vertexSrvIndex_ = srvManager->Allocate();
    srvManager->CreateSRVForStructuredBuffer(
      vertexSrvIndex_,
      vertexResource_.Get(),
      static_cast<UINT>(vertices_.size()),
      sizeof(VertexData)
    );
  }

  // 2. 影響度バッファのリソースを生成
  influenceResource_ = dx12->MakeBufferResource(sizeof(VertexInfluence) * vertexInfluences_.size());

  // 3. 影響度バッファに頂点影響度データをコピー
  VertexInfluence* mappedInfluences = nullptr;
  influenceResource_->Map(0, nullptr, reinterpret_cast<void**>(&mappedInfluences));
  std::memcpy(mappedInfluences, vertexInfluences_.data(), sizeof(VertexInfluence) * vertexInfluences_.size());
  influenceResource_->Unmap(0, nullptr);

  // 4. 影響度バッファのSRVを作成
  influenceSrvIndex_ = srvManager->Allocate();
  srvManager->CreateSRVForStructuredBuffer(
    influenceSrvIndex_,
    influenceResource_.Get(),
    static_cast<UINT>(vertexInfluences_.size()),
    sizeof(VertexInfluence)
  );

  // 5. 出力頂点バッファ（UAV）を生成
  dx12->CreateResourceForUAV(
    uavVertexOutputResource_,
    static_cast<UINT>(vertices_.size() * sizeof(VertexData))
  );

  // 6. UAVの作成
  uavIndex_ = srvManager->Allocate();
  srvManager->CreateUAV(
    uavIndex_,
    uavVertexOutputResource_.Get(),
    static_cast<UINT>(vertices_.size()),
    sizeof(VertexData)
  );

  // 7. スキニング情報リソースの生成
  dx12->CreateBufferResource(skinningInfoResource_, sizeof(SkinningInfo));
  skinningInfoResource_->Map(0, nullptr, reinterpret_cast<void**>(&skinningInfoData_));

  // 8. スキニング情報の初期化
  skinningInfoData_->numVertices = static_cast<uint32_t>(vertices_.size());

  // 9. スキニング用の頂点バッファビューを設定
  skinnedVertexBufferView_.BufferLocation = uavVertexOutputResource_->GetGPUVirtualAddress();
  skinnedVertexBufferView_.SizeInBytes = static_cast<UINT>(sizeof(VertexData) * vertices_.size());
  skinnedVertexBufferView_.StrideInBytes = sizeof(VertexData);
}
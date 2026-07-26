#include "Mesh.h"
#include "DX12Basic.h"
#include "Mat4x4Func.h"
#include "ModelBasic.h"
#include "Object3dBasic.h"
#include "ShadowRenderer.h"
#include "SrvManager.h"
#include "TextureManager.h"

#ifdef _DEBUG
#include "DebugUIManager.h"
#endif

namespace Tako {

  Mesh::~Mesh()
  {
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

    // ローカル AABB を計算 (Mesh エミッタの Inside/Surface/Edge スポーンで参照)
    if (!vertices_.empty()) {
      aabbLocalMin_.x = aabbLocalMax_.x = vertices_[0].position.x;
      aabbLocalMin_.y = aabbLocalMax_.y = vertices_[0].position.y;
      aabbLocalMin_.z = aabbLocalMax_.z = vertices_[0].position.z;
      for (const auto& v : vertices_) {
        if (v.position.x < aabbLocalMin_.x) aabbLocalMin_.x = v.position.x;
        if (v.position.y < aabbLocalMin_.y) aabbLocalMin_.y = v.position.y;
        if (v.position.z < aabbLocalMin_.z) aabbLocalMin_.z = v.position.z;
        if (v.position.x > aabbLocalMax_.x) aabbLocalMax_.x = v.position.x;
        if (v.position.y > aabbLocalMax_.y) aabbLocalMax_.y = v.position.y;
        if (v.position.z > aabbLocalMax_.z) aabbLocalMax_.z = v.position.z;
      }
    }
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
    if (!ShadowRenderer::GetInstance()->IsRenderingShadow()) {
      dx12_->GetCommandList()->SetGraphicsRootConstantBufferView(Object3dBasic::kMaterialParam, materialResource_->GetGPUVirtualAddress());

      // テクスチャを設定
      SrvManager::GetInstance()->SetGraphicsRootDescriptorTable(Object3dBasic::kTextureParam, textureData_.textureIndex);

      // 環境マップを使用する場合の設定
      if (materialData_->enableEnvMap && envTextureIndex_ != 0) {
        SrvManager::GetInstance()->SetGraphicsRootDescriptorTable(Object3dBasic::kEnvMapParam, envTextureIndex_);
      }
      else {
        // 環境マップが無効またはテクスチャが設定されていない場合は、デフォルトテクスチャを設定
        uint32_t defaultTextureIndex = TextureManager::GetInstance()->GetEngineDefaultSRVIndex("white.png");
        SrvManager::GetInstance()->SetGraphicsRootDescriptorTable(Object3dBasic::kEnvMapParam, defaultTextureIndex);
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

    if (ShadowRenderer::GetInstance()->IsRenderingShadow()) {
      // シャドウマップレンダリング時は座標変換行列のみ設定
      dx12_->GetCommandList()->SetGraphicsRootConstantBufferView(ShadowRenderer::kShadowPassTransformParam, transformationResource_->GetGPUVirtualAddress());
    }
    else {
      // マテリアルデータを設定
      dx12_->GetCommandList()->SetGraphicsRootConstantBufferView(Object3dBasic::kMaterialParam, materialResource_->GetGPUVirtualAddress());

      // 座標変換行列データを設定
      dx12_->GetCommandList()->SetGraphicsRootConstantBufferView(Object3dBasic::kTransformParam, transformationResource_->GetGPUVirtualAddress());

      // テクスチャを設定
      SrvManager::GetInstance()->SetGraphicsRootDescriptorTable(Object3dBasic::kTextureParam, textureData_.textureIndex);

      // 環境マップテクスチャを設定
      if (materialData_->enableEnvMap && envTextureIndex_ != 0) {
        // 環境マップが有効で、テクスチャが設定されている場合
        SrvManager::GetInstance()->SetGraphicsRootDescriptorTable(Object3dBasic::kEnvMapParam, envTextureIndex_);
      }
      else {
        // 環境マップが無効またはテクスチャが設定されていない場合は、デフォルトテクスチャを設定
        uint32_t defaultTextureIndex = TextureManager::GetInstance()->GetEngineDefaultSRVIndex("white.png");
        SrvManager::GetInstance()->SetGraphicsRootDescriptorTable(Object3dBasic::kEnvMapParam, defaultTextureIndex);
      }
    }

    // 描画
    dx12_->GetCommandList()->DrawIndexedInstanced(static_cast<UINT>(indices_.size()), 1, 0, 0, 0);
  }

  void Mesh::DrawInstanced(uint32_t instanceCount)
  {
    // インスタンシング描画（スキニングは未対応）
    if (hasSkinning_) {
#ifdef _DEBUG
      DebugUIManager::GetInstance()->AddLog(
        "Instanced drawing is not supported for skinned meshes",
        DebugUIManager::LogType::Warning);
#endif

      return;
    }

    // 頂点バッファビューを設定
    dx12_->GetCommandList()->IASetVertexBuffers(0, 1, &vertexBufferView_);

    // インデックスバッファビューを設定
    dx12_->GetCommandList()->IASetIndexBuffer(&indexBufferView_);

    // シャドウパス中はマテリアルとテクスチャの設定をスキップ
    if (!ShadowRenderer::GetInstance()->IsRenderingShadow()) {
      // 通常レンダリングパスの場合のみマテリアル・テクスチャを設定
      // マテリアルデータを設定
      dx12_->GetCommandList()->SetGraphicsRootConstantBufferView(Object3dBasic::kMaterialParam, materialResource_->GetGPUVirtualAddress());

      // テクスチャを設定
      SrvManager::GetInstance()->SetGraphicsRootDescriptorTable(Object3dBasic::kTextureParam, textureData_.textureIndex);

      // 環境マップテクスチャを設定
      if (materialData_->enableEnvMap && envTextureIndex_ != 0) {
        SrvManager::GetInstance()->SetGraphicsRootDescriptorTable(Object3dBasic::kEnvMapParam, envTextureIndex_);
      }
      else {
        uint32_t defaultTextureIndex = TextureManager::GetInstance()->GetEngineDefaultSRVIndex("white.png");
        SrvManager::GetInstance()->SetGraphicsRootDescriptorTable(Object3dBasic::kEnvMapParam, defaultTextureIndex);
      }
    }

    // インスタンシング描画
    dx12_->GetCommandList()->DrawIndexedInstanced(static_cast<UINT>(indices_.size()), instanceCount, 0, 0, 0);
  }

  void Mesh::UpdateTransformation(const Matrix4x4& world, const Matrix4x4& viewProjection)
  {
    // ワールド変換行列とビュープロジェクション行列から WVP 行列を計算
    Matrix4x4 wvpMatrix = Mat4x4::Multiply(world, viewProjection);

    // 変換行列データを更新
    transformationData_->WVP = wvpMatrix;
    transformationData_->world = world;
    transformationData_->worldInvTranspose = Mat4x4::InverseTranspose(world);
  }

  std::unique_ptr<Mesh> Mesh::Clone() const
  {
    // メッシュのクローンを作成
    auto newMesh = std::make_unique<Mesh>();
    newMesh->Initialize(modelBasic_, vertices_, indices_, textureData_);
    newMesh->name_ = name_;
    newMesh->isVisible_ = isVisible_;
    return newMesh;
  }

  void Mesh::SetTexture(const std::string& fileName)
  {
    // 同一パスなら何もしない
    if (fileName.empty() || fileName == textureData_.texturePath) {
      return;
    }

    TextureManager::GetInstance()->LoadTexture(fileName);
    textureData_.texturePath = fileName;
    textureData_.textureIndex = TextureManager::GetInstance()->GetSRVIndex(fileName);
  }

  void Mesh::ReleaseSRVIndex()
  {
    // SRV インデックスの解放
    if (vertexSrvIndex_ != 0) {
      // 解放前にアロケートされているか確認
      if (SrvManager::GetInstance()->IsAllocated(vertexSrvIndex_)) {
        SrvManager::GetInstance()->Free(vertexSrvIndex_);
      }
      vertexSrvIndex_ = 0;
    }

    if (influenceSrvIndex_ != 0) {
      if (SrvManager::GetInstance()->IsAllocated(influenceSrvIndex_)) {
        SrvManager::GetInstance()->Free(influenceSrvIndex_);
      }
      influenceSrvIndex_ = 0;
    }

    if (uavIndex_ != 0) {
      if (SrvManager::GetInstance()->IsAllocated(uavIndex_)) {
        SrvManager::GetInstance()->Free(uavIndex_);
      }
      uavIndex_ = 0;
    }

    if (indexSrvIndex_ != 0) {
      if (SrvManager::GetInstance()->IsAllocated(indexSrvIndex_)) {
        SrvManager::GetInstance()->Free(indexSrvIndex_);
      }
      indexSrvIndex_ = 0;
    }

    if (skinnedVertexSrvIndex_ != 0) {
      if (SrvManager::GetInstance()->IsAllocated(skinnedVertexSrvIndex_)) {
        SrvManager::GetInstance()->Free(skinnedVertexSrvIndex_);
      }
      skinnedVertexSrvIndex_ = 0;
    }
  }

  void Mesh::InitializeSkinning(const std::map<std::string, JointWeightData>& skinClusterData, const std::map<std::string, int32_t>& jointMap)
  {
    // スキニングフラグを設定
    hasSkinning_ = true;

    // 全頂点の影響度配列を初期化（MAX_INFLUENCE 個の要素を持つ配列）
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

    // UAV リソースを生成
    SetupSkinningUAV();
  }

  void Mesh::SkinningCompute()
  {
    if (!hasSkinning_) return;

    // 既にこのフレームでスキニングが実行されていたらスキップ
    if (skinningComputedThisFrame_) return;

    // 初回以外は (VBV | SRV) → UAV への遷移が必要
    static bool isFirstCompute = true;
    if (!isFirstCompute) {
      dx12_->TransitionResourceState(
        D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,
        D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
        uavVertexOutputResource_.Get());
    }
    isFirstCompute = false;

    SrvManager* srvManager = SrvManager::GetInstance();

    // 入力頂点バッファの SRV 設定
    srvManager->SetComputeRootDescriptorTable(ModelBasic::kVertexInputParam, vertexSrvIndex_);

    // 頂点影響度データの SRV 設定
    srvManager->SetComputeRootDescriptorTable(ModelBasic::kInfluenceParam, influenceSrvIndex_);

    // 出力頂点バッファの UAV 設定
    srvManager->SetComputeRootDescriptorTable(ModelBasic::kVertexOutputParam, uavIndex_);

    // スキニング情報の設定
    dx12_->GetCommandList()->SetComputeRootConstantBufferView(ModelBasic::kSkinningInfoParam, skinningInfoResource_->GetGPUVirtualAddress());

    // ComputeShader の実行
    dx12_->GetCommandList()->Dispatch(
      static_cast<UINT>(vertices_.size() + 1023) / 1024, 1, 1);

    // 描画 (VBV) とパーティクル emit (SRV) の両方で読めるよう OR 状態に遷移
    dx12_->TransitionResourceState(
      D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
      D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,
      uavVertexOutputResource_.Get());

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
    // VertexBufferView を作成
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

  uint32_t Mesh::GetIndexSrvIndex()
  {
    // 初回のみ index バッファに対する StructuredBuffer SRV を遅延生成してキャッシュする。
    if (indexSrvIndex_ == 0) {
      indexSrvIndex_ = SrvManager::GetInstance()->Allocate();
      SrvManager::GetInstance()->CreateSRVForStructuredBuffer(
        indexSrvIndex_, indexResource_.Get(), static_cast<UINT>(indices_.size()), sizeof(uint32_t));
    }
    return indexSrvIndex_;
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

    // 1. 頂点バッファの SRV を作成
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

    // 4. 影響度バッファの SRV を作成
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

    // 6. UAV の作成
    uavIndex_ = srvManager->Allocate();
    srvManager->CreateUAV(
      uavIndex_,
      uavVertexOutputResource_.Get(),
      static_cast<UINT>(vertices_.size()),
      sizeof(VertexData)
    );

    // 6b. 同一リソースに対する SRV (UAV と同時 bind せず、バリアで遷移して使う)
    skinnedVertexSrvIndex_ = srvManager->Allocate();
    srvManager->CreateSRVForStructuredBuffer(
      skinnedVertexSrvIndex_,
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

} // namespace Tako
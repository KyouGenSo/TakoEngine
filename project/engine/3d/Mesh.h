// ReSharper disable CppClangTidyClangDiagnosticInvalidUtf8
#pragma once
#include <d3d12.h>
#include <wrl.h>
#include "ModelStruct.h"
#include "Object3d.h"

class DX12Basic;
class ModelBasic;

class Mesh {

public: // メンバー関数

  // デストラクタ
  ~Mesh();

  // 初期化
  void Initialize(
    ModelBasic* modelBasic,
    const std::vector<VertexData>& vertices,
    const std::vector<uint32_t>& indices,
    const TextureData& textureData);

  // 描画
  void Draw();
  void DrawWithCurrentTransform();

  // 座標変換行列の更新
  void UpdateTransformation(const Matrix4x4& world, const Matrix4x4& viewProjection);

  // クローン
  Mesh* Clone() const;

  //------------------------------スキニング関連-------------------------------//
  // スキンニングの初期化
  void InitializeSkinning(
    const std::map<std::string,
    JointWeightData>& skinClusterData,
    const std::map<std::string,
    int32_t>& jointMap);

  void SkinningCompute();

  //-----------------------------Getters/Setters------------------------------//
  void SetShininess(float shininess) { materialData_->shininess = shininess; }
  void SetEnableLighting(bool enableLighting) { materialData_->enableLighting = enableLighting; }
  void SetEnableHighlight(bool enableHighlight) { materialData_->enableHighlight = enableHighlight; }
  void SetMaterialColor(const Vector4& color) { materialData_->color = color; }
  void SetEnvironmentTexture(uint32_t envTextureIndex) { envTextureIndex_ = envTextureIndex; }
  void SetEnableEnvMap(bool enableEnvMap) { materialData_->enableEnvMap = enableEnvMap; }
  void SetUvTransform(const Transform& transform)
  {
    materialData_->uvTransform = Mat4x4::MakeAffine(transform.scale, transform.rotate, transform.translate);
  }

  bool HasSkinning() const { return hasSkinning_; }
  ID3D12Resource* GetUAVVertexResource() { return uavVertexOutputResource_.Get(); }
  uint32_t GetVertexSrvIndex() { return vertexSrvIndex_; }
  uint32_t GetInfluenceSrvIndex() { return influenceSrvIndex_; }
  uint32_t GetUAVIndex() { return uavIndex_; }
  D3D12_GPU_VIRTUAL_ADDRESS GetSkinningInfoResourceGPUAddress() { return skinningInfoResource_->GetGPUVirtualAddress(); }
  UINT GetVertexCount() { return static_cast<UINT>(vertices_.size()); }

private: // プライベートメンバー関数

  // リソース生成メソッド
  void CreateVertexData();
  void CreateVertexBufferView();
  void CreateIndexData();
  void CreateMaterialData();
  void CreateTransformation();
  // スキニング関連のUAVリソース生成
  void SetupSkinningUAV();

  // リソースの解放
  void ReleaseSRVIndex();

  // メンバ変数
  ModelBasic* modelBasic_ = nullptr;
  DX12Basic* dx12_ = nullptr;

  std::vector<VertexData> vertices_;
  std::vector<uint32_t> indices_;
  TextureData textureData_;

  // リソース
  Microsoft::WRL::ComPtr<ID3D12Resource> vertexResource_;
  Microsoft::WRL::ComPtr<ID3D12Resource> indexResource_;
  Microsoft::WRL::ComPtr<ID3D12Resource> materialResource_;
  Microsoft::WRL::ComPtr<ID3D12Resource> transformationResource_;

  // バッファビュー
  D3D12_VERTEX_BUFFER_VIEW vertexBufferView_;
  D3D12_VERTEX_BUFFER_VIEW skinnedVertexBufferView_;
  D3D12_INDEX_BUFFER_VIEW indexBufferView_;

  // バッファリソース内のデータを指すポインタ
  VertexData* vertexData_ = nullptr;
  Material* materialData_ = nullptr;
  Object3d::TransformationMatrix* transformationData_ = nullptr;

  // SRVインデックス
  uint32_t vertexSrvIndex_ = 0;
  uint32_t envTextureIndex_ = 0;

  //-----------------スキニング関連--------------------//
      // スキニング関連のメンバ変数
  bool hasSkinning_ = false;
  std::vector<VertexInfluence> vertexInfluences_;

  // スキニングリソース
  Microsoft::WRL::ComPtr<ID3D12Resource> influenceResource_;
  Microsoft::WRL::ComPtr<ID3D12Resource> uavVertexOutputResource_;
  Microsoft::WRL::ComPtr<ID3D12Resource> skinningInfoResource_;

  uint32_t influenceSrvIndex_ = 0;
  uint32_t uavIndex_ = 0;

  SkinningInfo* skinningInfoData_ = nullptr;
};


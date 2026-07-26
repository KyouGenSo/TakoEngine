// ReSharper disable CppClangTidyClangDiagnosticInvalidUtf8
#pragma once
#include <d3d12.h>
#include <wrl.h>
#include "ModelStruct.h"
#include "Object3d.h"

namespace Tako {

  class DX12Basic;
  class ModelBasic;

  /// <summary>
  /// 3D モデルのメッシュデータとスキニングアニメーション処理を管理するクラス
  /// 頂点データ、インデックス、マテリアル、GPU スキニングを統合管理
  /// </summary>
  class Mesh {

  public: //メンバー関数

    /// <summary>
    /// デストラクタ
    /// </summary>
    ~Mesh();

    /// <summary>
    /// 初期化
    /// </summary>
    /// <param name="modelBasic">モデル基本システムへのポインタ</param>
    /// <param name="vertices">頂点データ配列</param>
    /// <param name="indices">インデックスデータ配列</param>
    /// <param name="textureData">テクスチャデータ</param>
    void Initialize(
      ModelBasic* modelBasic,
      const std::vector<VertexData>& vertices,
      const std::vector<uint32_t>& indices,
      const TextureData& textureData);

    /// <summary>
    /// 描画
    /// </summary>
    void Draw();

    /// <summary>
    /// 現在の変換行列で描画
    /// </summary>
    void DrawWithCurrentTransform();

    /// <summary>
    /// インスタンシング描画
    /// </summary>
    /// <param name="instanceCount">インスタンス数</param>
    void DrawInstanced(uint32_t instanceCount);

    /// <summary>
    /// 座標変換行列の更新
    /// </summary>
    /// <param name="world">ワールド行列</param>
    /// <param name="viewProjection">ビュープロジェクション行列</param>
    void UpdateTransformation(const Matrix4x4& world, const Matrix4x4& viewProjection);

    /// <summary>
    /// メッシュのクローンを作成
    /// </summary>
    /// <returns>クローンされたメッシュの unique_ptr</returns>
    std::unique_ptr<Mesh> Clone() const;

    //スキニング関連
    /// <summary>
    /// スキニングの初期化
    /// </summary>
    /// <param name="skinClusterData">スキンクラスターデータマップ</param>
    /// <param name="jointMap">ジョイント名とインデックスのマップ</param>
    void InitializeSkinning(
      const std::map<std::string,
      JointWeightData>& skinClusterData,
      const std::map<std::string,
      int32_t>& jointMap);

    /// <summary>
    /// スキニング計算を実行
    /// </summary>
    void SkinningCompute();

    /// <summary>
    /// フレーム開始時にスキニング状態をリセット
    /// </summary>
    void ResetSkinningState() { skinningComputedThisFrame_ = false; }

    //============================================================
    //Setter
    //============================================================
    void SetShininess(float shininess) { materialData_->shininess = shininess; }
    void SetEnableLighting(bool enableLighting) { materialData_->enableLighting = enableLighting; }
    void SetEnableHighlight(bool enableHighlight) { materialData_->enableHighlight = enableHighlight; }
    void SetMaterialColor(const Vector4& color) { materialData_->color = color; }
    void SetEnvironmentTexture(uint32_t envTextureIndex) { envTextureIndex_ = envTextureIndex; }
    void SetEnableEnvMap(bool enableEnvMap) { materialData_->enableEnvMap = static_cast<int32_t>(enableEnvMap); }
    void SetEnvMapCoefficient(float coefficient) { materialData_->envMapCoefficient = coefficient; }

    /// <summary>
    /// 通常テクスチャを差し替える（未ロードなら TextureManager 経由でロード）
    /// </summary>
    /// <param name="fileName">"resources/Texture/" 相対名、または "EngineResources/" プレフィックス付きパス</param>
    void SetTexture(const std::string& fileName);

    /// <summary>
    /// UV トランスフォームを設定
    /// </summary>
    /// <param name="transform">UV トランスフォーム情報</param>
    void SetUvTransform(const Transform& transform)
    {
      materialData_->uvTransform = Mat4x4::MakeAffine(transform.scale, transform.rotate, transform.translate);
    }

    void SetName(const std::string& name) { name_ = name; }
    void SetVisible(bool isVisible) { isVisible_ = isVisible; }

    //============================================================
    //Getter
    //============================================================
    Vector4 GetMaterialColor() const { return materialData_->color; }
    float GetShininess() const { return materialData_->shininess; }
    const TextureData& GetTextureData() const { return textureData_; }
    bool HasSkinning() const { return hasSkinning_; }
    ID3D12Resource* GetUAVVertexResource() { return uavVertexOutputResource_.Get(); }
    uint32_t GetVertexSrvIndex() { return vertexSrvIndex_; }
    uint32_t GetInfluenceSrvIndex() { return influenceSrvIndex_; }
    uint32_t GetUAVIndex() { return uavIndex_; }
    uint32_t GetSkinnedVertexSrvIndex() const { return skinnedVertexSrvIndex_; }

    /// <summary>
    /// スキニング情報リソースの GPU アドレスを取得
    /// </summary>
    /// <returns>スキニング情報リソースの GPU アドレス</returns>
    D3D12_GPU_VIRTUAL_ADDRESS GetSkinningInfoResourceGPUAddress() { return skinningInfoResource_->GetGPUVirtualAddress(); }

    /// <summary>
    /// 頂点数を取得
    /// </summary>
    /// <returns>頂点数</returns>
    UINT GetVertexCount() { return static_cast<UINT>(vertices_.size()); }

    ID3D12Resource* GetIndexResource() { return indexResource_.Get(); }

    /// <summary>
    /// インデックス数を取得 (三角形数 = GetIndexCount() / 3)
    /// </summary>
    UINT GetIndexCount() const { return static_cast<UINT>(indices_.size()); }

    /// <summary>
    /// インデックスバッファのSRVインデックスを取得する。
    /// 初回呼び出し時に遅延生成してキャッシュする。
    /// </summary>
    /// <returns>インデックス SRV インデックス</returns>
    uint32_t GetIndexSrvIndex();

    const std::vector<VertexData>& GetVertices() const { return vertices_; }
    const std::vector<uint32_t>& GetIndices() const { return indices_; }
    const Vector3& GetAABBLocalMin() const { return aabbLocalMin_; }
    const Vector3& GetAABBLocalMax() const { return aabbLocalMax_; }
    const std::string& GetName() const { return name_; }
    bool IsVisible() const { return isVisible_; }

  private: //非公開関数

    /// <summary>
    /// 頂点データを生成
    /// </summary>
    void CreateVertexData();

    /// <summary>
    /// 頂点バッファビューを生成
    /// </summary>
    void CreateVertexBufferView();

    /// <summary>
    /// インデックスデータを生成
    /// </summary>
    void CreateIndexData();

    /// <summary>
    /// マテリアルデータを生成
    /// </summary>
    void CreateMaterialData();

    /// <summary>
    /// 変換行列を生成
    /// </summary>
    void CreateTransformation();

    /// <summary>
    /// スキニング用 UAV リソースをセットアップ
    /// </summary>
    void SetupSkinningUAV();

    /// <summary>
    /// リソースの解放（SRV インデックスの解放）
    /// </summary>
    void ReleaseSRVIndex();

  private: //メンバー変数
    ModelBasic* modelBasic_ = nullptr;  ///< モデル基本システムへのポインタ
    DX12Basic*  dx12_       = nullptr;  ///< DirectX12基盤システムへのポインタ

    std::vector<VertexData> vertices_;     ///< 頂点データ配列
    std::vector<uint32_t>   indices_;      ///< インデックスデータ配列
    TextureData             textureData_;  ///< テクスチャデータ

    //リソース
    Microsoft::WRL::ComPtr<ID3D12Resource> vertexResource_;          ///< 頂点バッファリソース
    Microsoft::WRL::ComPtr<ID3D12Resource> indexResource_;           ///< インデックスバッファリソース
    Microsoft::WRL::ComPtr<ID3D12Resource> materialResource_;        ///< マテリアルバッファリソース
    Microsoft::WRL::ComPtr<ID3D12Resource> transformationResource_;  ///< 変換行列バッファリソース

    //バッファビュー
    D3D12_VERTEX_BUFFER_VIEW vertexBufferView_;         ///< 頂点バッファビュー
    D3D12_VERTEX_BUFFER_VIEW skinnedVertexBufferView_;  ///< スキニング済み頂点バッファビュー
    D3D12_INDEX_BUFFER_VIEW  indexBufferView_;          ///< インデックスバッファビュー

    //バッファリソース内のデータを指すポインタ
    VertexData*                     vertexData_         = nullptr;  ///< 頂点データへのポインタ
    Material*                       materialData_       = nullptr;  ///< マテリアルデータへのポインタ
    Object3d::TransformationMatrix* transformationData_ = nullptr;  ///< 変換行列データへのポインタ

    //SRV インデックス
    uint32_t vertexSrvIndex_  = 0;  ///< 頂点 SRV インデックス
    uint32_t indexSrvIndex_   = 0;  ///< インデックス SRV インデックス (GetIndexSrvIndex で遅延生成)
    uint32_t envTextureIndex_ = 0;  ///< 環境マップテクスチャインデックス

    //スキニング関連
    bool                         hasSkinning_      = false;  ///< スキニングの有無
    std::vector<VertexInfluence> vertexInfluences_;          ///< 頂点インフルエンス配列

    //スキニングリソース
    Microsoft::WRL::ComPtr<ID3D12Resource> influenceResource_;        ///< インフルエンスバッファリソース
    Microsoft::WRL::ComPtr<ID3D12Resource> uavVertexOutputResource_;  ///< UAV 頂点出力リソース
    Microsoft::WRL::ComPtr<ID3D12Resource> skinningInfoResource_;     ///< スキニング情報バッファリソース

    uint32_t influenceSrvIndex_     = 0;  ///< インフルエンス SRV インデックス
    uint32_t uavIndex_              = 0;  ///< UAV インデックス
    uint32_t skinnedVertexSrvIndex_ = 0;  ///< UAV と同一リソースに対する SRV ビュー

    SkinningInfo* skinningInfoData_ = nullptr;  ///< スキニング情報データへのポインタ

    bool skinningComputedThisFrame_ = false;  ///< スキニング済みフラグ（フレーム内で1回だけ実行）

    //GPU パーティクルの Mesh エミッタ用 AABB (頂点ローカル座標系)
    Vector3 aabbLocalMin_{ .x = 0.0f, .y = 0.0f, .z = 0.0f };
    Vector3 aabbLocalMax_{ .x = 0.0f, .y = 0.0f, .z = 0.0f };

    //表示制御・識別
    std::string name_;              ///< メッシュ名（Assimp aiMesh->mName 由来。表示制御の識別子）
    bool        isVisible_ = true;  ///< 表示フラグ（false で Model の描画ループからスキップ）
  };

} // namespace Tako

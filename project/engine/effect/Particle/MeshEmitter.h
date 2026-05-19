#pragma once
#include "GPUParticleEmitter.h"
#include "Matrix4x4.h"
#include <string>

namespace Tako {

  class Mesh;
  class Model;
  class Object3d;

  /// <summary>
  /// メッシュをスポーン形状として使うエミッター
  /// </summary>
  /// <remarks>
  /// Mesh の頂点バッファとインデックスバッファを SRV 経由でシェーダに渡し、
  /// SpawnLocation = Surface/Edge/Inside に応じてメッシュ表面・エッジ・内部から
  /// パーティクルをスポーンする。Mesh の動的追従は <c>BindMeshWorld()</c> または
  /// <c>SetMeshWorld()</c> で世界行列を更新する。
  ///
  /// 制約: 初期実装では同時メッシュエミッタ 1 個まで対応 (固定 SRV スロット)。
  /// 多重対応は indexable SRV 配列で将来フェーズに拡張予定。
  /// </remarks>
  class MeshEmitter : public GPUParticleEmitter {
  public:
    /// <summary>
    /// コンストラクタ
    /// </summary>
    /// <param name="particleSystem">GPU パーティクルシステムへのポインタ</param>
    /// <param name="mesh">スポーン形状として使う Mesh (非所有、ライフタイム責務は呼び出し側)</param>
    /// <param name="count">1 回の射出で生成するパーティクル数</param>
    /// <param name="frequency">射出間隔 (秒)</param>
    /// <remarks>
    /// emitterId は基底クラスで 0 として初期化される。実際の ID は <c>RegisterEmitter()</c> 時に割り当てられる。
    /// </remarks>
    MeshEmitter(GPUParticle* particleSystem, Mesh* mesh, uint32_t count, float frequency);

    /// <summary>
    /// Model を渡してマルチプリミティブ対応の MeshEmitter を生成
    /// </summary>
    /// <remarks>
    /// Mesh 数 > 1 のとき集約バッファに統合する。集約モードではスキニング動的同期は未対応。
    /// </remarks>
    MeshEmitter(GPUParticle* particleSystem, Model* model, uint32_t count, float frequency);

    /// <summary>
    /// Object3d を渡して MeshEmitter を生成 (JSON 永続化対応)
    /// </summary>
    /// <param name="particleSystem">GPU パーティクルシステムへのポインタ</param>
    /// <param name="obj3d">スポーン形状ソース (非所有、ライフタイム責務は呼び出し側)</param>
    /// <param name="count">1 回の射出で生成するパーティクル数</param>
    /// <param name="frequency">射出間隔 (秒)</param>
    /// <param name="object3dKey">JSON シリアライズ時に保存される識別キー (空文字で round-trip 不可)</param>
    /// <remarks>
    /// 内部で <c>obj3d-&gt;GetModel()</c> を取り出して Model* ctor に委譲する。
    /// 動的世界行列は <c>UpdateEmission()</c> 内で <c>obj3d-&gt;GetWorldMatrix()</c> から自動同期。
    /// </remarks>
    MeshEmitter(GPUParticle* particleSystem, Object3d* obj3d, uint32_t count, float frequency, std::string object3dKey);

    ~MeshEmitter() override = default;

    /// <summary>
    /// クローン作成
    /// </summary>
    std::shared_ptr<GPUParticleEmitter> Clone() const override;

    /// <summary>
    /// 射出更新 (基底実装 + Mesh world の動的同期)
    /// </summary>
    void UpdateEmission(float deltaTime) override;

    /// <summary>
    /// エミッタータイプを取得
    /// </summary>
    [[nodiscard]] EmitterType GetType() const override { return EmitterType::Mesh; }

    /// <summary>
    /// メッシュの世界行列を直接設定 (静的)
    /// </summary>
    /// <param name="world">世界行列</param>
    /// <remarks>動的バインドは <c>BindMeshWorld()</c> で行う</remarks>
    void SetMeshWorld(const Matrix4x4& world);

    /// <summary>
    /// メッシュの世界行列を動的にバインド
    /// </summary>
    /// <param name="worldPtr">毎フレーム読み取られる Matrix4x4 へのポインタ。ライフタイム管理は呼び出し側責務</param>
    /// <remarks>
    /// 非 nullptr のとき、<c>UpdateEmission()</c> 内で毎フレーム <c>*worldPtr</c> を <c>data_.meshWorld</c> に同期する。
    /// </remarks>
    void BindMeshWorld(const Matrix4x4* worldPtr) { boundMeshWorld_ = worldPtr; }

    /// <summary>
    /// 動的バインドを解除
    /// </summary>
    void UnbindMeshWorld() { boundMeshWorld_ = nullptr; }

    /// <summary>
    /// 参照中の Mesh ポインタを取得
    /// </summary>
    [[nodiscard]] Mesh* GetMesh() const { return mesh_; }

    /// <summary>
    /// バインドされている Object3d ポインタを取得
    /// </summary>
    [[nodiscard]] Object3d* GetBoundObject3d() const { return boundObject3d_; }

    /// <summary>
    /// JSON シリアライズ用の Object3d 識別キーを取得
    /// </summary>
    [[nodiscard]] const std::string& GetObject3dKey() const { return object3dKey_; }

    /// <summary>
    /// JSON シリアライズ用の Object3d 識別キーを設定
    /// </summary>
    void SetObject3dKey(const std::string& key) { object3dKey_ = key; }

    /// <summary>
    /// 動的バインドされた meshWorld を同期 (UpdateEmission から呼ばれる)
    /// </summary>
    /// <remarks>
    /// 優先順位: <c>boundObject3d_</c> &gt; <c>boundMeshWorld_</c>。
    /// Object3d がバインドされていれば <c>GetWorldMatrix()</c> で動的に世界行列を取得する。
    /// </remarks>
    void SyncMeshWorld();

  private:
    Mesh* mesh_ = nullptr;                            ///< 非所有参照
    const Matrix4x4* boundMeshWorld_ = nullptr;       ///< 動的バインド用 (非所有)
    Object3d* boundObject3d_ = nullptr;               ///< 動的バインド用 Object3d (非所有、Matrix4x4* より優先)
    std::string object3dKey_;                         ///< JSON シリアライズ用 Object3d 識別キー (空文字で round-trip 不可)
    uint32_t meshIndexSrvIndex_ = 0;                  ///< このエミッタ用に確保した index SRV インデックス

    // 三角形面積 Prefix Sum (Inversion Sampling)
    Microsoft::WRL::ComPtr<ID3D12Resource> areaPrefixSumResource_; ///< size = triCount + 1
    uint32_t meshAreaPrefixSumSrvIndex_ = 0;

    // マルチプリミティブ集約バッファ。index は mesh ごとの vertex base offset を加算済み。
    Microsoft::WRL::ComPtr<ID3D12Resource> aggregatedVertexResource_;
    Microsoft::WRL::ComPtr<ID3D12Resource> aggregatedIndexResource_;
    uint32_t aggregatedVertexSrvIndex_ = 0;
    uint32_t aggregatedIndexSrvIndex_ = 0;
  };

} // namespace Tako

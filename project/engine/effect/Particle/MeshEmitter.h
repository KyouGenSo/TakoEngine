#pragma once
#include "GPUParticleEmitter.h"
#include "Matrix4x4.h"

namespace Tako {

  class Mesh;

  /// <summary>
  /// メッシュをスポーン形状として使うエミッター (Stage D-1)
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
    /// メッシュの世界行列を動的にバインド (Stage D-1)
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
    /// 動的バインドされた meshWorld を同期 (UpdateEmission から呼ばれる)
    /// </summary>
    void SyncMeshWorld();

  private:
    Mesh* mesh_ = nullptr;                            ///< 非所有参照
    const Matrix4x4* boundMeshWorld_ = nullptr;       ///< 動的バインド用 (非所有)
    uint32_t meshIndexSrvIndex_ = 0;                  ///< このエミッタ用に確保した index SRV インデックス
  };

} // namespace Tako

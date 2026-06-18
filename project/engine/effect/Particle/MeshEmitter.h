#pragma once
#include "GPUParticleEmitter.h"
#include <d3d12.h>
#include <wrl.h>
#include <string>
#include <vector>

namespace Tako {

  class Mesh;
  class Model;
  class Object3d;
  struct VertexData;

  /// <summary>
  /// メッシュをスポーン形状として使うエミッター
  /// </summary>
  /// <remarks>
  /// スポーン位置は「メッシュローカル座標 × meshWorld」で決まる。
  /// meshWorld は毎フレーム「ローカルオフセット (position / offsetRotation / offsetScale) ×
  /// バインド先 Object3d の world 行列」を合成して更新される (非バインド時はオフセットのみ)。
  /// </remarks>
  class MeshEmitter : public GPUParticleEmitter {
  public: //メンバー関数
    /// <summary>
    /// コンストラクタ
    /// </summary>
    /// <param name="particleSystem">GPU パーティクルシステムへのポインタ</param>
    /// <param name="model">スポーン形状ソース (非所有、ライフタイム責務は呼び出し側)</param>
    /// <param name="count">1 回の射出で生成するパーティクル数</param>
    /// <param name="frequency">射出間隔 (秒)</param>
    /// <remarks>
    /// Mesh 数 1 ならスキニング対応のため Mesh の SRV を共有、複数なら集約バッファに統合する
    /// (集約モードではスキニング動的同期は未対応)。
    /// emitterId は基底クラスで 0 として初期化される。実際の ID は <c>RegisterEmitter()</c> 時に割り当てられる。
    /// </remarks>
    MeshEmitter(GPUParticle* particleSystem, Model* model, uint32_t count, float frequency);

    ~MeshEmitter() override = default;

    /// <summary>
    /// クローン作成 (スポーン形状の GPU リソースは immutable なので ComPtr 共有)
    /// </summary>
    std::shared_ptr<GPUParticleEmitter> Clone() const override;

    /// <summary>
    /// 型固有パラメータを json に書き出す
    /// </summary>
    /// <remarks>
    /// Object3d バインドは実行時情報のため永続化しない。復元時は
    /// <c>LoadPreset(presetName, newEmitterName, obj3d)</c> で呼び出し側が再バインドする。
    /// </remarks>
    void SerializeTypeSpecific(nlohmann::json& json) const override;

    /// <summary>
    /// JSON から MeshEmitter を構築
    /// </summary>
    /// <param name="particleSystem">GPU パーティクルシステムへのポインタ</param>
    /// <param name="json">読み込む JSON オブジェクト</param>
    /// <param name="bindTarget">バインド先 Object3d (nullptr で meshModelPath から自己完結復元)</param>
    /// <returns>構築されたエミッター (復元手段が無い場合 nullptr)</returns>
    static std::shared_ptr<GPUParticleEmitter> CreateFromJSON(
      GPUParticle* particleSystem, const nlohmann::json& json, Object3d* bindTarget);

    /// <summary>
    /// 射出更新 (基底実装 + meshWorld のオフセット合成)
    /// </summary>
    void UpdateEmission(float deltaTime) override;

    /// <summary>
    /// Object3d をバインドし、毎フレーム world 行列に追従させる
    /// </summary>
    /// <param name="obj3d">追従先 (非所有、ライフタイム責務は呼び出し側)</param>
    void BindObject3d(Object3d* obj3d) { boundObject3d_ = obj3d; }

    /// <summary>
    /// Object3d バインドを解除 (以後はローカルオフセットのみでワールド配置)
    /// </summary>
    void UnbindObject3d() { boundObject3d_ = nullptr; }

    //=============================================
    //Setter
    //=============================================
    void SetSpawnModelPath(const std::string& path) { spawnModelPath_ = path; }
    void SetOffsetRotation(const Vector3& rotation) { offsetRotation_ = rotation; }
    void SetOffsetScale(const Vector3& scale) { offsetScale_ = scale; }

    //=============================================
    //Getter
    //=============================================
    [[nodiscard]] EmitterType GetType() const override { return EmitterType::Mesh; }
    [[nodiscard]] Object3d* GetBoundObject3d() const { return boundObject3d_; }
    [[nodiscard]] Mesh* GetMesh() const { return mesh_; }
    [[nodiscard]] const std::string& GetSpawnModelPath() const { return spawnModelPath_; }
    [[nodiscard]] const Vector3& GetOffsetRotation() const { return offsetRotation_; }
    [[nodiscard]] const Vector3& GetOffsetScale() const { return offsetScale_; }

  private: //非公開関数
    /// <summary>
    /// スポーン形状の GPU リソースを構築 (ctor 本体)。model が null または mesh 0 個なら何もしない。
    /// </summary>
    void BuildFromModel(Model* model);

    /// <summary>
    /// 三角形面積の Prefix Sum を計算 (Inversion Sampling 用、size = triCount + 1)
    /// </summary>
    static std::vector<float> ComputeTriangleAreaPrefixSum(
      const std::vector<VertexData>& vertices, const std::vector<uint32_t>& indices);

    /// <summary>
    /// UPLOAD バッファを生成して srcData を書き込み、StructuredBuffer SRV を確保して index を返す
    /// </summary>
    uint32_t CreateStructuredBufferSrv(
      Microsoft::WRL::ComPtr<ID3D12Resource>& outResource,
      const void* srcData, size_t elementSize, uint32_t elementCount);

    /// <summary>
    /// ローカルオフセットとバインド先 world を合成して data_.meshWorld を更新 (毎フレーム)
    /// </summary>
    void SyncMeshWorld();

  private: //メンバー変数
    Mesh*       mesh_           = nullptr;  ///< 単一メッシュ時のみ非 null (スキニング/エディタ表示用、非所有)
    Object3d*   boundObject3d_  = nullptr;  ///< 追従先 Object3d (非所有)
    std::string spawnModelPath_;            ///< JSON 永続化用 スポーン形状モデルのパス (空=パス復元不可)

    Vector3 offsetRotation_ = { 0.0f, 0.0f, 0.0f };  ///< ローカルオフセット回転 (ラジアン Euler)
    Vector3 offsetScale_    = { 1.0f, 1.0f, 1.0f };  ///< ローカルオフセットスケール

    //スポーン形状の GPU リソース (構築後 immutable、Clone 間で ComPtr 共有)
    Microsoft::WRL::ComPtr<ID3D12Resource> areaPrefixSumResource_;     ///< 三角形面積 Prefix Sum (size = triCount + 1)
    Microsoft::WRL::ComPtr<ID3D12Resource> aggregatedVertexResource_;  ///< マルチプリミティブ集約頂点
    Microsoft::WRL::ComPtr<ID3D12Resource> aggregatedIndexResource_;   ///< 集約インデックス (vertex base offset 加算済み)
  };

} // namespace Tako

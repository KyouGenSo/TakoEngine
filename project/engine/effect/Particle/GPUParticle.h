#pragma once
#include <random>
#include <string>
#include <vector>
#include <memory>

#include "SrvManager.h"
#include "ParticleStruct.h"

namespace Tako {

  // 前方宣言
  class GPUParticleEmitter;
  class SphereEmitter;
  class BoxEmitter;
  class TriangleEmitter;
  class EmitterManager;
  class DX12Basic;
  class Camera;

  /// <summary>
  /// GPU パーティクルシステムクラス
  /// Compute Shader で最大10万パーティクルの高速処理を実現
  /// </summary>
  class GPUParticle
  {
  private: // シングルトン設定
    /// <summary>
    /// シングルトンインスタンス
    /// </summary>
    static std::unique_ptr<GPUParticle> instance_;
    GPUParticle() = default;
    ~GPUParticle() = default;

    friend struct std::default_delete<GPUParticle>;

  public:
    GPUParticle(const GPUParticle&) = delete;
    GPUParticle& operator=(const GPUParticle&) = delete;

  private:

  public: // メンバー関数

    /// <summary>
    /// インスタンスの取得
    /// </summary>
    /// <returns>GPUParticle システムのシングルトンインスタンス</returns>
    static GPUParticle* GetInstance();

    /// <summary>
    /// 初期化
    /// </summary>
    /// <param name="dx12">DirectX 12基盤クラスへのポインタ</param>
    /// <param name="camera">カメラへのポインタ</param>
    void Initialize(DX12Basic* dx12, Camera* camera);

    /// <summary>
    /// 更新
    /// </summary>
    void Update();

    /// <summary>
    /// 描画
    /// </summary>
    void Draw();

    /// <summary>
    /// 終了処理
    /// </summary>
    void Finalize();

    //-------------------------エミッター管理-------------------------//

    /// <summary>
    /// 既存のエミッターからコピーして一時的なエミッターを作成
    /// </summary>
    /// <param name="sourceEmitter">コピー元のエミッター</param>
    /// <param name="lifeTime">一時エミッターの寿命（秒）</param>
    /// <returns>作成された一時エミッター</returns>
    std::shared_ptr<GPUParticleEmitter> CreateTemporaryEmitterFrom(GPUParticleEmitter* sourceEmitter, float lifeTime);

    /// <summary>
    /// エミッターの登録
    /// </summary>
    /// <param name="emitter">登録するエミッター</param>
    void RegisterEmitter(std::shared_ptr<GPUParticleEmitter> emitter);

    /// <summary>
    /// エミッターの登録解除
    /// </summary>
    /// <param name="emitter">登録解除するエミッター</param>
    void UnregisterEmitter(std::shared_ptr<GPUParticleEmitter> emitter);

    /// <summary>
    /// エミッターの数を取得
    /// </summary>
    /// <returns>アクティブなエミッターの数</returns>
    [[nodiscard]] uint32_t GetEmitterCount() const { return static_cast<uint32_t>(activeEmitters_.size()); }

    //-------------------------フォースフィールド管理-------------------------//

    /// <summary>
    /// フォースフィールドの最大数
    /// </summary>
    static const uint32_t kMaxForceFields = 64;

    /// <summary>
    /// フォースフィールドを追加
    /// </summary>
    /// <param name="field">追加するフォースフィールド</param>
    /// <returns>追加されたフォースフィールドのインデックス（失敗時は -1）</returns>
    int32_t AddForceField(const ForceFieldData& field);

    /// <summary>
    /// フォースフィールドを削除
    /// </summary>
    /// <param name="index">削除するインデックス</param>
    void RemoveForceField(uint32_t index);

    /// <summary>
    /// フォースフィールドをすべて削除
    /// </summary>
    void ClearForceFields();

    /// <summary>
    /// 物理パラメータの速度減衰を設定
    /// </summary>
    /// <param name="damping">減衰係数（0.98-0.99 推奨）</param>
    void SetDamping(float damping) { physicsParamsData_->damping = damping; }

    /// <summary>
    /// 物理パラメータの反発係数を設定
    /// </summary>
    /// <param name="restitution">反発係数（0-1）</param>
    void SetCollisionRestitution(float restitution) { physicsParamsData_->collisionRestitution = restitution; }

    /// <summary>
    /// パーティクルの衝突半径を設定
    /// </summary>
    /// <param name="radius">衝突半径</param>
    void SetParticleRadius(float radius) { physicsParamsData_->particleRadius = radius; }

    /// <summary>
    /// フォースフィールドを更新
    /// </summary>
    /// <param name="index">更新するインデックス</param>
    /// <param name="field">新しいフォースフィールドデータ</param>
    void UpdateForceField(uint32_t index, const ForceFieldData& field);

    /// <summary>
    /// フォースフィールドリストを取得
    /// </summary>
    /// <returns>フォースフィールドリストの const 参照</returns>
    [[nodiscard]] const std::vector<ForceFieldData>& GetForceFields() const { return forceFields_; }

    /// <summary>
    /// 速度減衰を取得
    /// </summary>
    [[nodiscard]] float GetDamping() const { return physicsParamsData_ ? physicsParamsData_->damping : 0.99f; }

    /// <summary>
    /// 反発係数を取得
    /// </summary>
    [[nodiscard]] float GetCollisionRestitution() const { return physicsParamsData_ ? physicsParamsData_->collisionRestitution : 0.5f; }

    /// <summary>
    /// パーティクル衝突半径を取得
    /// </summary>
    [[nodiscard]] float GetParticleRadius() const { return physicsParamsData_ ? physicsParamsData_->particleRadius : 0.05f; }

    /// <summary>
    /// Curl Noise の空間スケールを設定
    /// </summary>
    /// <param name="scale">空間スケール（小さい値=大きな渦、大きい値=細かいディテール）</param>
    void SetNoiseScale(float scale) { physicsParamsData_->noiseScale = scale; }

    /// <summary>
    /// Curl Noise の空間スケールを取得
    /// </summary>
    /// <returns>空間スケール</returns>
    [[nodiscard]] float GetNoiseScale() const { return physicsParamsData_ ? physicsParamsData_->noiseScale : 3.0f; }

    void SetNoiseStrength(float strength) { physicsParamsData_->noiseStrength = strength; }
    [[nodiscard]] float GetNoiseStrength() const { return physicsParamsData_ ? physicsParamsData_->noiseStrength : 0.05f; }

    //-------------------------Getter/Setter-------------------------//
    /// <summary>
    /// インデックスによってエミッターを検索
    /// </summary>
    /// <param name="index">検索するインデックス</param>
    /// <returns>見つかったエミッター、見つからない場合は nullptr</returns>
    std::shared_ptr<GPUParticleEmitter> FindEmitterByIndex(size_t index);

    /// <summary>
    /// デバッグモードが有効か取得
    /// </summary>
    /// <returns>デバッグモードが有効な場合 true</returns>
    [[nodiscard]] bool GetIsDebug() const { return isDebug_; }

    /// <summary>
    /// カメラを設定
    /// </summary>
    /// <param name="camera">設定するカメラ</param>
    void SetCamera(Camera* camera) { m_camera_ = camera; }

    /// <summary>
    /// デバッグモードを設定
    /// </summary>
    /// <param name="isDebug">デバッグモードを有効にする場合 true</param>
    void SetIsDebug(bool isDebug) { isDebug_ = isDebug; }
    //-------------------------Getter/Setter-------------------------//

    // フレンドクラス宣言
    friend class GPUParticleEmitter;
    friend class SphereEmitter;
    friend class BoxEmitter;
    friend class TriangleEmitter;
    friend class EmitterManager;

  private: // プライベートメンバー関数
    /// <summary>
    /// emitter の更新
    /// </summary>
    void UpdateEmitter();

    /// <summary>
    /// PerView の更新
    /// </summary>
    void UpdatePerView();

    /// <summary>
    /// PerFrame の更新
    /// </summary>
    void UpdatePerFrame();

    /// <summary>
    /// CPU 側から GPU 側へのエミッターデータ同期
    /// </summary>
    void SyncEmitterData();

    //-----------リソース作成関連------------//
    /// <summary>
    /// ルートシグネチャの作成
    /// </summary>
    void CreateRS();

    /// <summary>
    /// パイプラインステートの生成
    /// </summary>
    void CreatePSO();

    /// <summary>
    /// InitCS ルートシグネチャの作成
    /// </summary>
    void CreateInitComputeRS();

    /// <summary>
    /// EmitParticleCS ルートシグネチャの作成
    /// </summary>
    void CreateEmitParticleComputeRS();

    /// <summary>
    /// UpdateParticleCS ルートシグネチャの作成
    /// </summary>
    void CreateUpdateParticleComputeRS();

    /// <summary>
    /// コンピュートシェーダーのパイプラインステートを生成
    /// </summary>
    /// <param name="RS">ルートシグネチャ</param>
    /// <param name="PSO">パイプラインステート</param>
    /// <param name="shaderName">シェーダーファイル名</param>
    void CreateComputeShaderPSO(Microsoft::WRL::ComPtr<ID3D12RootSignature>& RS, Microsoft::WRL::ComPtr<ID3D12PipelineState>& PSO, const std::wstring& shaderName);

    /// <summary>
    /// 頂点データの生成
    /// </summary>
    void CreateVertexData();

    /// <summary>
    /// PerView データの生成
    /// </summary>
    void CreatePerViewData();

    /// <summary>
    /// PerFrame データの生成
    /// </summary>
    void CreatePerFrameData();

    /// <summary>
    /// EmitterSphere データの生成
    /// </summary>
    void CreateEmitterData();

    /// <summary>
    /// CS パーティクルリソースの生成
    /// </summary>
    void CreateParticleResource();

    /// <summary>
    /// FreeList リソースの生成
    /// </summary>
    void CreateFreeListResource();

    /// <summary>
    /// IntegrateAll CS ルートシグネチャの作成
    /// </summary>
    void CreateIntegrateAllComputeRS();

    /// <summary>
    /// フォースフィールドリソースの生成
    /// </summary>
    void CreateForceFieldResource();

    /// <summary>
    /// 物理パラメータリソースの生成
    /// </summary>
    void CreatePhysicsParamsResource();

    /// <summary>
    /// 深度バッファ用 SRV の作成（深度衝突用）
    /// </summary>
    void CreateDepthSRV();

  public:
    /// <summary>
    /// ウィンドウリサイズ時の処理（深度 SRV の再作成）
    /// </summary>
    void OnResize();

  private:

    /// <summary>
    /// CPU 側から GPU 側へのフォースフィールドデータ同期
    /// </summary>
    void SyncForceFieldData();

    /// <summary>
    /// 物理パラメータの更新
    /// </summary>
    void UpdatePhysicsParams();

  private: //メンバー変数

    /// <summary>
    /// パーティクルの最大出力数
    /// </summary>
    static const uint32_t kNumMaxParticle;

    /// <summary>
    /// エミッターの最大数
    /// </summary>
    static const uint32_t kNumMaxEmitter;


    /// <summary>
    /// 初期化フラグ
    /// </summary>
    bool isInited_ = false;

    /// <summary>
    /// デバッグモードフラグ
    /// </summary>
    bool isDebug_ = false;

    /// <summary>
    /// DirectX 12基盤クラスへのポインタ
    /// </summary>
    DX12Basic* m_dx12_ = nullptr;

    /// <summary>
    /// SRV マネージャへのポインタ
    /// </summary>
    SrvManager* m_srvManager_ = nullptr;

    /// <summary>
    /// カメラへのポインタ
    /// </summary>
    Camera* m_camera_;

    /// <summary>
    /// モデルデータ
    /// </summary>
    ModelData modelData_;

    /// <summary>
    /// 描画用ルートシグネチャ
    /// </summary>
    Microsoft::WRL::ComPtr<ID3D12RootSignature> RS_;

    /// <summary>
    /// 初期化コンピュートシェーダー用ルートシグネチャ
    /// </summary>
    Microsoft::WRL::ComPtr<ID3D12RootSignature> initComputeRS_;

    /// <summary>
    /// パーティクル射出コンピュートシェーダー用ルートシグネチャ
    /// </summary>
    Microsoft::WRL::ComPtr<ID3D12RootSignature> emitParticleRS_;

    /// <summary>
    /// パーティクル更新コンピュートシェーダー用ルートシグネチャ（旧式、段階的に廃止）
    /// </summary>
    Microsoft::WRL::ComPtr<ID3D12RootSignature> updateParticleRS_;

    /// <summary>
    /// IntegrateAll コンピュートシェーダー用ルートシグネチャ
    /// </summary>
    Microsoft::WRL::ComPtr<ID3D12RootSignature> integrateAllRS_;

    /// <summary>
    /// 描画用パイプラインステート
    /// </summary>
    Microsoft::WRL::ComPtr<ID3D12PipelineState> PSO_;

    /// <summary>
    /// 初期化コンピュートシェーダー用パイプラインステート
    /// </summary>
    Microsoft::WRL::ComPtr<ID3D12PipelineState> initComputePSO_;

    /// <summary>
    /// パーティクル射出コンピュートシェーダー用パイプラインステート
    /// </summary>
    Microsoft::WRL::ComPtr<ID3D12PipelineState> emitParticlePSO_;

    /// <summary>
    /// パーティクル更新コンピュートシェーダー用パイプラインステート（旧式、段階的に廃止）
    /// </summary>
    Microsoft::WRL::ComPtr<ID3D12PipelineState> updateParticlePSO_;

    /// <summary>
    /// IntegrateAll コンピュートシェーダー用パイプラインステート
    /// </summary>
    Microsoft::WRL::ComPtr<ID3D12PipelineState> integrateAllPSO_;

    /// <summary>
    /// パーティクルデータ用 GPU リソース
    /// </summary>
    Microsoft::WRL::ComPtr<ID3D12Resource> particleResource_;

    /// <summary>
    /// パーティクルリソースの UAV インデックス
    /// </summary>
    uint32_t particleUavIndex_;

    /// <summary>
    /// パーティクルリソースの SRV インデックス
    /// </summary>
    uint32_t particleSrvIndex_;

    /// <summary>
    /// PerView 定数バッファリソース
    /// </summary>
    Microsoft::WRL::ComPtr<ID3D12Resource> perViewResource_;

    /// <summary>
    /// PerView データへのポインタ
    /// </summary>
    PerView* perViewData_;

    /// <summary>
    /// PerFrame 定数バッファリソース
    /// </summary>
    Microsoft::WRL::ComPtr<ID3D12Resource> perFrameResource_;

    /// <summary>
    /// PerFrame データへのポインタ
    /// </summary>
    PerFrame* perFrameData_;

    /// <summary>
    /// エミッターデータ用 GPU リソース
    /// </summary>
    Microsoft::WRL::ComPtr<ID3D12Resource> emitterResource_;

    /// <summary>
    /// エミッターリソースの SRV インデックス
    /// </summary>
    uint32_t emitterSrvIndex_;

    /// <summary>
    /// アクティブなエミッターのリスト
    /// </summary>
    std::vector<std::shared_ptr<GPUParticleEmitter>> activeEmitters_;

    /// <summary>
    /// FreeList インデックス用 GPU リソース
    /// </summary>
    Microsoft::WRL::ComPtr<ID3D12Resource> freeListIndexResource_;

    /// <summary>
    /// FreeList インデックスの UAV インデックス
    /// </summary>
    uint32_t freeListIndexUavIndex_;

    /// <summary>
    /// FreeList 用 GPU リソース
    /// </summary>
    Microsoft::WRL::ComPtr<ID3D12Resource> freeListResource_;

    /// <summary>
    /// FreeList の UAV インデックス
    /// </summary>
    uint32_t freeListUavIndex_;

    //-------------------------物理シミュレーション関連-------------------------//

    /// <summary>
    /// フォースフィールドデータ用 GPU リソース
    /// </summary>
    Microsoft::WRL::ComPtr<ID3D12Resource> forceFieldResource_;

    /// <summary>
    /// フォースフィールドリソースの SRV インデックス
    /// </summary>
    uint32_t forceFieldSrvIndex_ = 0;

    /// <summary>
    /// 深度バッファの SRV インデックス（深度衝突用）
    /// </summary>
    uint32_t depthSrvIndex_ = UINT32_MAX;

    /// <summary>
    /// 物理パラメータ定数バッファリソース
    /// </summary>
    Microsoft::WRL::ComPtr<ID3D12Resource> physicsParamsResource_;

    /// <summary>
    /// 物理パラメータデータへのポインタ
    /// </summary>
    PhysicsParamsData* physicsParamsData_ = nullptr;

    /// <summary>
    /// CPU 側のフォースフィールドリスト
    /// </summary>
    std::vector<ForceFieldData> forceFields_;

    /// <summary>
    /// 頂点データ用 GPU リソース
    /// </summary>
    Microsoft::WRL::ComPtr<ID3D12Resource> vertexResource_;

    /// <summary>
    /// 頂点データへのポインタ
    /// </summary>
    VertexData* vertexData_;

    /// <summary>
    /// 頂点バッファビュー
    /// </summary>
    D3D12_VERTEX_BUFFER_VIEW vertexBufferView_;
  };

} // namespace Tako


#pragma once
#include <random>
#include <string>
#include <vector>
#include <memory>
#include <unordered_map>

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
  class Model;
  class Mesh;

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

    /// <summary>
    /// ウィンドウリサイズ時の処理（深度 SRV の再作成）
    /// </summary>
    void OnResize();

    /// <summary>
    /// モデルを path からロードしてシステムが保持し、その先頭メッシュを返す。
    /// スポーン形状・描画モデルの両方で使う。GPU リソース寿命をシステム側で保証する
    /// (エミッターのクローン後も SRV が有効)。同一 path は一度だけロードしてキャッシュ。失敗時は nullptr。
    /// </summary>
    /// <param name="modelPath">モデルファイル名</param>
    /// <returns>モデルの先頭メッシュ (失敗時 nullptr)</returns>
    Mesh* AcquireModelMesh(const std::string& modelPath);

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

#ifdef _DEBUG
    /// <summary>
    /// GPU上で生存中のパーティクル数を取得
    /// </summary>
    /// <returns>アクティブなパーティクル数</returns>
    [[nodiscard]] uint32_t GetActiveParticleCount() const { return activeParticleCount_; }

    /// <summary>
    /// パーティクルの最大数を取得
    /// </summary>
    /// <returns>最大パーティクル数</returns>
    [[nodiscard]] static uint32_t GetMaxParticleCount() { return kNumMaxParticle; }
#endif

    //-------------------------フォースフィールド管理-------------------------//

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
    /// 描画用パイプラインステートの生成 (ブレンドモード別)
    /// </summary>
    /// <param name="blendDesc">ブレンドステート</param>
    /// <param name="outPSO">生成された PSO の出力先</param>
    void CreateDrawPSO(const D3D12_BLEND_DESC& blendDesc, Microsoft::WRL::ComPtr<ID3D12PipelineState>& outPSO);

    /// <summary>
    /// ブレンドモード値 (ParticleBlendMode) に対応する描画 PSO を取得
    /// </summary>
    /// <param name="blendMode">0=Add, 1=Screen, 2=Alpha</param>
    /// <returns>対応する PSO (不正値は Screen にフォールバック)</returns>
    ID3D12PipelineState* GetBlendPSO(uint32_t blendMode) const;

    /// <summary>
    /// InitCS ルートシグネチャの作成
    /// </summary>
    void CreateInitComputeRS();

    /// <summary>
    /// EmitParticleCS ルートシグネチャの作成
    /// </summary>
    void CreateEmitParticleComputeRS();

    /// <summary>
    /// コンピュートシェーダーのパイプラインステートを生成
    /// </summary>
    /// <param name="RS">ルートシグネチャ</param>
    /// <param name="PSO">パイプラインステート</param>
    /// <param name="shaderName">シェーダーファイル名</param>
    void CreateComputeShaderPSO(Microsoft::WRL::ComPtr<ID3D12RootSignature>& RS, Microsoft::WRL::ComPtr<ID3D12PipelineState>& PSO, const std::wstring& shaderName);

    /// <summary>
    /// 既定の描画モデル(板ポリ)を生成する。
    /// 頂点・インデックスを StructuredBuffer + SRV として作り、
    /// 描画モデル未指定エミッターの既定モデルとして全パーティクル共通で使う。
    /// </summary>
    void CreateDefaultQuadMesh();

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

    //-----------Indirect 描画 / コンパクション関連------------//
    /// <summary>
    /// Indirect 描画・コンパクション用 GPU リソースの生成
    /// (生存数カウンタ / 描画 index リスト / スキャッタカーソル / Indirect 引数)
    /// </summary>
    void CreateIndirectResources();

    /// <summary>
    /// ExecuteIndirect 用コマンドシグネチャの生成 。
    /// </summary>
    void CreateCommandSignature();

    /// <summary>
    /// ResetCounters CS のルートシグネチャ作成
    /// </summary>
    void CreateResetCountersRS();

    /// <summary>
    /// BuildDrawArgs CS のルートシグネチャ作成
    /// </summary>
    void CreateBuildDrawArgsRS();

    /// <summary>
    /// ScatterCompact CS のルートシグネチャ作成
    /// </summary>
    void CreateScatterCompactRS();

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

#ifdef _DEBUG
    /// <summary>
    /// FreeListIndex の Readback バッファを作成
    /// </summary>
    void CreateFreeListReadbackResource();

    /// <summary>
    /// FreeListIndex を Readback バッファにコピーし、アクティブパーティクル数を算出
    /// </summary>
    void ReadbackActiveParticleCount();
#endif

  private:

    /// <summary>
    /// CPU 側から GPU 側へのフォースフィールドデータ同期
    /// </summary>
    void SyncForceFieldData();

    /// <summary>
    /// 物理パラメータの更新
    /// </summary>
    void UpdatePhysicsParams();

  public: // 公開定数
    /// <summary>
    /// フォースフィールドの最大数
    /// </summary>
    static constexpr uint32_t kMaxForceFields = 64;

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

  public:
    /// <summary>
    /// SrvManager 参照を取得 (MeshEmitter が Mesh の index SRV を作成するために使用)
    /// </summary>
    SrvManager* GetSrvManager() const { return m_srvManager_; }

    /// <summary>
    /// DX12Basic を取得
    /// </summary>
    DX12Basic* GetDx12() const { return m_dx12_; }

  private:

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
    /// IntegrateAll コンピュートシェーダー用ルートシグネチャ
    /// </summary>
    Microsoft::WRL::ComPtr<ID3D12RootSignature> integrateAllRS_;

    /// <summary>
    /// 描画用パイプラインステート (ブレンドモード別: 加算 / スクリーン / アルファ)
    /// </summary>
    Microsoft::WRL::ComPtr<ID3D12PipelineState> psoAdd_;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> psoScreen_;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> psoAlpha_;

    /// <summary>
    /// 初期化コンピュートシェーダー用パイプラインステート
    /// </summary>
    Microsoft::WRL::ComPtr<ID3D12PipelineState> initComputePSO_;

    /// <summary>
    /// パーティクル射出コンピュートシェーダー用パイプラインステート
    /// </summary>
    Microsoft::WRL::ComPtr<ID3D12PipelineState> emitParticlePSO_;

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

#ifdef _DEBUG
    //-------------------------Readback: アクティブパーティクル数取得-------------------------//

    /// <summary>
    /// FreeListIndex の Readback バッファ（D3D12_HEAP_TYPE_READBACK）
    /// </summary>
    Microsoft::WRL::ComPtr<ID3D12Resource> freeListIndexReadbackResource_;

    /// <summary>
    /// CPU 側で読み取ったアクティブパーティクル数（表示用キャッシュ）
    /// </summary>
    uint32_t activeParticleCount_ = 0;

    /// <summary>
    /// Readback 間引きカウンタ（フレーム数）
    /// </summary>
    uint32_t readbackFrameCounter_ = 0;

    /// <summary>
    /// Readback 実行間隔（フレーム数）
    /// </summary>
    static const uint32_t kReadbackInterval = 10;
#endif

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

    //-------------------------デフォルトの描画モデル (板ポリ)-------------------------//

    /// <summary>
    /// 既定板ポリの頂点 StructuredBuffer
    /// </summary>
    Microsoft::WRL::ComPtr<ID3D12Resource> defaultQuadVertexResource_;
    uint32_t defaultQuadVertexSrvIndex_ = 0;

    /// <summary>
    /// 既定板ポリのインデックス StructuredBuffer
    /// </summary>
    Microsoft::WRL::ComPtr<ID3D12Resource> defaultQuadIndexResource_;
    uint32_t defaultQuadIndexSrvIndex_ = 0;
    uint32_t defaultQuadIndexCount_ = 0;

    //-------------------------Indirect 描画 / コンパクション関連-------------------------//

    /// <summary>
    /// ExecuteIndirect 用コマンドシグネチャ
    /// </summary>
    Microsoft::WRL::ComPtr<ID3D12CommandSignature> drawCommandSignature_;

    /// <summary>
    /// per-emitter の今フレーム生存パーティクル数バッファ
    /// </summary>
    Microsoft::WRL::ComPtr<ID3D12Resource> perEmitterCountResource_;
    uint32_t perEmitterCountUavIndex_ = 0;

    /// <summary>
    /// コンパクション済み生存パーティクル index リスト
    /// per-instance 頂点ストリーム (drawIndexVBV_) としてもバインドする
    /// </summary>
    Microsoft::WRL::ComPtr<ID3D12Resource> drawIndexResource_;
    uint32_t drawIndexUavIndex_ = 0;
    uint32_t drawIndexSrvIndex_ = 0;
    D3D12_VERTEX_BUFFER_VIEW drawIndexVBV_{};

    /// <summary>
    /// スキャッタ書き込みカーソル
    /// </summary>
    Microsoft::WRL::ComPtr<ID3D12Resource> scatterCursorResource_;
    uint32_t scatterCursorUavIndex_ = 0;

    /// <summary>
    /// Indirect 描画引数バッファ (D3D12_DRAW_ARGUMENTS 配列 × kNumMaxEmitter)
    /// </summary>
    Microsoft::WRL::ComPtr<ID3D12Resource> drawArgsResource_;
    uint32_t drawArgsUavIndex_ = 0;

    //-------------------------コンパクション用 CS の RS/PSO-------------------------//

    Microsoft::WRL::ComPtr<ID3D12RootSignature> resetCountersRS_;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> resetCountersPSO_;
    Microsoft::WRL::ComPtr<ID3D12RootSignature> buildDrawArgsRS_;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> buildDrawArgsPSO_;
    Microsoft::WRL::ComPtr<ID3D12RootSignature> scatterCompactRS_;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> scatterCompactPSO_;

    //-------------------------描画テンプレート-------------------------//

    /// <summary>
    /// per-emitter 描画テンプレート (CPU 書き込み: 描画モデルの index 数。既定板ポリは 6)
    /// BuildDrawArgs が DRAW 引数の VertexCountPerInstance に使う。
    /// </summary>
    Microsoft::WRL::ComPtr<ID3D12Resource> emitterDrawTemplateResource_;
    uint32_t emitterDrawTemplateSrvIndex_ = 0;
    uint32_t* emitterDrawTemplateData_ = nullptr;

    /// <summary>
    /// per-emitter 描画モデルのキャッシュ (path → Model)。
    /// 描画モデルの GPU リソース寿命をシステムが保持し、エミッターのクローン後も SRV を有効に保つ。
    /// shared_ptr のためデストラクタが型消去され、前方宣言 (不完全型) のままメンバにできる。
    /// </summary>
    std::unordered_map<std::string, std::shared_ptr<Model>> renderModels_;
  };

} // namespace Tako


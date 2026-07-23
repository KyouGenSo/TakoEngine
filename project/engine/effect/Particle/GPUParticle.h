#pragma once
#include <random>
#include <string>
#include <vector>
#include <memory>
#include <unordered_map>

#include "SrvManager.h"
#include "ParticleStruct.h"
#include "ModelStruct.h"

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
    struct Token {};  ///< 外部からの直接生成を防ぐ生成キー
    ~GPUParticle() = default;

    friend struct std::default_delete<GPUParticle>;

  public: //定数
    /// <summary>
    /// フォースフィールドの最大数
    /// </summary>
    static constexpr uint32_t kMaxForceFields = 64;

  public: //メンバー関数
    explicit GPUParticle(Token) {}
    GPUParticle(const GPUParticle&) = delete;
    GPUParticle& operator=(const GPUParticle&) = delete;

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
    /// モデルを path からロードしてシステムが保持し、その Model を返す。
    /// スポーン形状・描画モデルの両方で使う。GPU リソース寿命をシステム側で保証する
    /// (エミッターのクローン後も SRV が有効)。同一 path は一度だけロードしてキャッシュ。失敗時は nullptr。
    /// </summary>
    /// <param name="modelPath">モデルファイル名</param>
    /// <returns>キャッシュされた Model (失敗時 nullptr)</returns>
    Model* AcquireModel(const std::string& modelPath);

    /// <summary>
    /// AcquireModel の先頭メッシュ版。描画モデル (板ポリ代替) 用途で使う。失敗時は nullptr。
    /// </summary>
    /// <param name="modelPath">モデルファイル名</param>
    /// <returns>モデルの先頭メッシュ (失敗時 nullptr)</returns>
    Mesh* AcquireModelMesh(const std::string& modelPath);

    //============================================================
    //エミッター管理
    //============================================================
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

    //============================================================
    //フォースフィールド管理
    //============================================================
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

    //============================================================
    //Setter
    //============================================================
    void SetCamera(Camera* camera) { camera_ = camera; }
    void SetIsDebug(bool isDebug) { isDebug_ = isDebug; }

    //============================================================
    //Getter
    //============================================================
    [[nodiscard]] uint32_t GetEmitterCount() const { return static_cast<uint32_t>(activeEmitters_.size()); }

#ifdef _DEBUG
    [[nodiscard]] uint32_t GetActiveParticleCount() const { return activeParticleCount_; }
    [[nodiscard]] static uint32_t GetMaxParticleCount() { return kNumMaxParticle; }
#endif

    [[nodiscard]] const std::vector<ForceFieldData>& GetForceFields() const { return forceFields_; }
    [[nodiscard]] bool GetIsDebug() const { return isDebug_; }
    SrvManager* GetSrvManager() const { return srvManager_; }
    DX12Basic* GetDx12() const { return dx12_; }

    // フレンドクラス宣言
    friend class GPUParticleEmitter;
    friend class SphereEmitter;
    friend class BoxEmitter;
    friend class TriangleEmitter;
    friend class EmitterManager;

  private: //非公開関数
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

    /// <summary>
    /// 退役済みエミッタースロットのうち、寿命が尽きてパーティクルが全滅したものを解放する。
    /// </summary>
    void RetireExpiredSlots();

    //============================================================
    //リソース作成関連
    //============================================================
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
    /// ブレンドモード値に対応する描画 PSO を取得
    /// </summary>
    /// <param name="blendMode">0=Add, 1=Screen, 2=Alpha</param>
    /// <returns>対応する PSO </returns>
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

    //============================================================
    //Indirect 描画 / コンパクション関連
    //============================================================
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

    /// <summary>
    /// CPU 側から GPU 側へのフォースフィールドデータ同期
    /// </summary>
    void SyncForceFieldData();

    /// <summary>
    /// 物理パラメータの更新
    /// </summary>
    void UpdatePhysicsParams();

  private: //メンバー変数
    static const uint32_t     kNumMaxParticle;                   ///< パーティクルの最大出力数
    static const uint32_t     kNumMaxEmitter;                    ///< エミッターの最大数
    static constexpr uint32_t kInvalidMeshTarget = 0xFFFFFFFFu;  ///< HLSL 側 gTargetMeshEmitterId の無効値 (非 Mesh 一括 Dispatch を示す)

    //基本状態
    bool isInited_ = false;  ///< 初期化フラグ
    bool isDebug_  = false;  ///< デバッグモードフラグ

    //基盤
    DX12Basic*  dx12_       = nullptr;  ///< DirectX 12基盤クラスへのポインタ
    SrvManager* srvManager_ = nullptr;  ///< SRV マネージャへのポインタ
    Camera*     camera_;                ///< カメラへのポインタ
    ModelData   modelData_;               ///< モデルデータ

    //ルートシグネチャ
    Microsoft::WRL::ComPtr<ID3D12RootSignature> RS_;              ///< 描画用ルートシグネチャ
    Microsoft::WRL::ComPtr<ID3D12RootSignature> initComputeRS_;   ///< 初期化コンピュートシェーダー用ルートシグネチャ
    Microsoft::WRL::ComPtr<ID3D12RootSignature> emitParticleRS_;  ///< パーティクル射出コンピュートシェーダー用ルートシグネチャ
    Microsoft::WRL::ComPtr<ID3D12RootSignature> integrateAllRS_;  ///< IntegrateAll コンピュートシェーダー用ルートシグネチャ

    //描画用パイプラインステート (ブレンドモード別: 加算 / スクリーン / アルファ)
    Microsoft::WRL::ComPtr<ID3D12PipelineState> psoAdd_;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> psoScreen_;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> psoAlpha_;

    //コンピュートシェーダー用パイプラインステート
    Microsoft::WRL::ComPtr<ID3D12PipelineState> initComputePSO_;   ///< 初期化コンピュートシェーダー用パイプラインステート
    Microsoft::WRL::ComPtr<ID3D12PipelineState> emitParticlePSO_;  ///< パーティクル射出コンピュートシェーダー用パイプラインステート
    Microsoft::WRL::ComPtr<ID3D12PipelineState> integrateAllPSO_;  ///< IntegrateAll コンピュートシェーダー用パイプラインステート

    //パーティクルリソース
    Microsoft::WRL::ComPtr<ID3D12Resource> particleResource_;  ///< パーティクルデータ用 GPU リソース
    uint32_t                               particleUavIndex_;  ///< パーティクルリソースの UAV インデックス
    uint32_t                               particleSrvIndex_;  ///< パーティクルリソースの SRV インデックス

    //定数バッファ
    Microsoft::WRL::ComPtr<ID3D12Resource> perViewResource_;   ///< PerView 定数バッファリソース
    PerView*                               perViewData_;       ///< PerView データへのポインタ
    Microsoft::WRL::ComPtr<ID3D12Resource> perFrameResource_;  ///< PerFrame 定数バッファリソース
    PerFrame*                              perFrameData_;      ///< PerFrame データへのポインタ

    //エミッターリソース
    Microsoft::WRL::ComPtr<ID3D12Resource>           emitterResource_;   ///< エミッターデータ用 GPU リソース
    uint32_t                                         emitterSrvIndex_;   ///< エミッターリソースの SRV インデックス
    std::vector<std::shared_ptr<GPUParticleEmitter>> activeEmitters_;    ///< アクティブなエミッターのリスト。index = 安定スロット番号 (パーティクルの emitterId として焼き込まれる)。削除時に compaction せず、退役→寿命経過後に nullptr 穴にして freeEmitterSlots_ で再利用する。
    std::vector<uint32_t>                            freeEmitterSlots_;  ///< 再利用可能なスロット番号スタック。退役スロット解放時に返却し、RegisterEmitter で再利用する。
    std::vector<std::pair<uint32_t, float>>          retiringSlots_;     ///< 退役中スロット。射出停止後もパーティクル全滅まで描画継続するため保持する。

    //FreeList
    Microsoft::WRL::ComPtr<ID3D12Resource> freeListIndexResource_;  ///< FreeList インデックス用 GPU リソース
    uint32_t                               freeListIndexUavIndex_;  ///< FreeList インデックスの UAV インデックス
    Microsoft::WRL::ComPtr<ID3D12Resource> freeListResource_;       ///< FreeList 用 GPU リソース
    uint32_t                               freeListUavIndex_;       ///< FreeList の UAV インデックス

#ifdef _DEBUG
    //Readback: アクティブパーティクル数取得
    Microsoft::WRL::ComPtr<ID3D12Resource> freeListIndexReadbackResource_;       ///< FreeListIndex の Readback バッファ（D3D12_HEAP_TYPE_READBACK）
    uint32_t                               activeParticleCount_           = 0;   ///< CPU 側で読み取ったアクティブパーティクル数（表示用キャッシュ）
    uint32_t                               readbackFrameCounter_          = 0;   ///< Readback 間引きカウンタ（フレーム数）
    static const uint32_t                  kReadbackInterval              = 10;  ///< Readback 実行間隔（フレーム数）
#endif

    //物理シミュレーション関連
    Microsoft::WRL::ComPtr<ID3D12Resource> forceFieldResource_;                  ///< フォースフィールドデータ用 GPU リソース
    uint32_t                               forceFieldSrvIndex_    = 0;           ///< フォースフィールドリソースの SRV インデックス
    uint32_t                               depthSrvIndex_         = UINT32_MAX;  ///< 深度バッファの SRV インデックス（深度衝突用）
    Microsoft::WRL::ComPtr<ID3D12Resource> physicsParamsResource_;               ///< 物理パラメータ定数バッファリソース
    PhysicsParamsData*                     physicsParamsData_     = nullptr;     ///< 物理パラメータデータへのポインタ
    std::vector<ForceFieldData>            forceFields_;                         ///< CPU 側のフォースフィールドリスト

    //デフォルトの描画モデル (板ポリ)
    Microsoft::WRL::ComPtr<ID3D12Resource> defaultQuadVertexResource_;      ///< 既定板ポリの頂点 StructuredBuffer
    uint32_t                               defaultQuadVertexSrvIndex_ = 0;
    Microsoft::WRL::ComPtr<ID3D12Resource> defaultQuadIndexResource_;       ///< 既定板ポリのインデックス StructuredBuffer
    uint32_t                               defaultQuadIndexSrvIndex_  = 0;
    uint32_t                               defaultQuadIndexCount_     = 0;

    //Indirect 描画 / コンパクション関連
    Microsoft::WRL::ComPtr<ID3D12CommandSignature> drawCommandSignature_;         ///< ExecuteIndirect 用コマンドシグネチャ
    Microsoft::WRL::ComPtr<ID3D12Resource>         perEmitterCountResource_;      ///< per-emitter の今フレーム生存パーティクル数バッファ
    uint32_t                                       perEmitterCountUavIndex_ = 0;
    Microsoft::WRL::ComPtr<ID3D12Resource>         drawIndexResource_;            ///< コンパクション済み生存パーティクル index リスト。per-instance 頂点ストリーム (drawIndexVBV_) としてもバインドする
    uint32_t                                       drawIndexUavIndex_       = 0;
    uint32_t                                       drawIndexSrvIndex_       = 0;
    D3D12_VERTEX_BUFFER_VIEW                       drawIndexVBV_{};
    Microsoft::WRL::ComPtr<ID3D12Resource>         scatterCursorResource_;        ///< スキャッタ書き込みカーソル
    uint32_t                                       scatterCursorUavIndex_   = 0;
    Microsoft::WRL::ComPtr<ID3D12Resource>         drawArgsResource_;             ///< Indirect 描画引数バッファ (D3D12_DRAW_ARGUMENTS 配列 × kNumMaxEmitter)
    uint32_t                                       drawArgsUavIndex_        = 0;

    //コンパクション用 CS の RS/PSO
    Microsoft::WRL::ComPtr<ID3D12RootSignature> resetCountersRS_;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> resetCountersPSO_;
    Microsoft::WRL::ComPtr<ID3D12RootSignature> buildDrawArgsRS_;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> buildDrawArgsPSO_;
    Microsoft::WRL::ComPtr<ID3D12RootSignature> scatterCompactRS_;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> scatterCompactPSO_;

    //描画テンプレート
    Microsoft::WRL::ComPtr<ID3D12Resource>                  emitterIndexCountResource_;            ///< per-emitter 描画モデルのインデクスカウンタバッファ。BuildDrawArgs が DRAW 引数の VertexCountPerInstance に使う。
    uint32_t                                                emitterIndexCountSrvIndex_ = 0;
    uint32_t*                                               emitterIndexCountData_     = nullptr;
    std::unordered_map<std::string, std::shared_ptr<Model>> renderModels_;                         ///< per-emitter 描画モデルのキャッシュ。描画モデルの GPU リソース寿命をシステムが保持し、エミッターのクローン後も SRV を有効に保つ。shared_ptr のためデストラクタが型消去され、前方宣言 (不完全型) のままメンバにできる。
  };

} // namespace Tako

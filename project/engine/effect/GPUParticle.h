#pragma once
#include <random>
#include <string>
#include <vector>
#include <memory>
#include <unordered_map>

#include "SrvManager.h"
#include "ParticleStruct.h"

// 前方宣言
class GPUParticleEmitter;
class SphereEmitter;
class BoxEmitter;
class TriangleEmitter;
class EmitterManager;
class DX12Basic;
class Camera;

class GPUParticle
{
private: // シングルトン設定
  // インスタンス
  static GPUParticle* instance_;
  GPUParticle() = default;
  ~GPUParticle() = default;
  GPUParticle(GPUParticle&) = delete;
  GPUParticle& operator=(GPUParticle&) = delete;

public: // メンバー関数

  /// <summary>
  ///　インスタンスの取得
  ///	</summary>
  static GPUParticle* GetInstance();

  /// <summary>
  ///　初期化
  /// <summary>
  void Initialize(DX12Basic* dx12, Camera* camera);

  /// <summary>
  ///　更新
  /// </summary>
  void Update();

  /// <summary>
  ///　描画
  /// </summary>
  void Draw();

  /// <summary>
  /// 終了処理
  /// </summary>
  void Finalize();

  /// <summary>
  /// デバッグ情報表示
  /// </summary>
  void DebugInfo();

  //-------------------------エミッター管理-------------------------//

  /// <summary>
  /// 球体エミッター作成
  /// </summary>
  std::shared_ptr<SphereEmitter> CreateSphereEmitter(const Vector3& position, float radius, uint32_t count, float frequency);

  /// <summary>
  /// 箱型エミッター作成
  /// </summary>
  std::shared_ptr<BoxEmitter> CreateBoxEmitter(const Vector3& position, const Vector3& size, const Vector3& rotation, uint32_t count, float frequency);

  /// <summary>
  /// 三角形エミッター作成
  /// </summary>
  std::shared_ptr<TriangleEmitter> CreateTriangleEmitter(const Vector3& position, const Vector3& v1, const Vector3& v2, const Vector3& v3, uint32_t count, float frequency);

  /// <summary>
  /// エミッターパラメータ更新
  /// </summary>
  void UpdateEmitterParameters(uint32_t emitterId, const EmitterData& params);

  /// <summary>
  /// エミッターのID取得
  /// </summary>
  uint32_t GetEmitterCount() const { return activeEmitterCount_; }

  /// <summary>
  /// エミッター削除
  /// </summary>
  void RemoveEmitterById(uint32_t emitterId);

  //-------------------------Getter/Setter-------------------------//
  // EmitterDataの取得
  EmitterData GetEmitterData(uint32_t emitterId) const { return emitters_[emitterId]; }
  bool GetIsDebug() const { return isDebug_; }

  void SetCamera(Camera* camera) { m_camera_ = camera; }
  void SetIsDebug(bool isDebug) { isDebug_ = isDebug; }

  // フレンドクラス宣言
  friend class GPUParticleEmitter;
  friend class SphereEmitter;
  friend class BoxEmitter;
  friend class TriangleEmitter;
  friend class EmitterManager;

private: // プライベートメンバー関数
  /// <summary>
  /// エミッター内部作成関数
  /// </summary>
  uint32_t CreateSphereEmitterInternal(const Vector3& position, float radius, uint32_t count, float frequency);
  uint32_t CreateBoxEmitterInternal(const Vector3& position, const Vector3& size, const Vector3& rotation, uint32_t count, float frequency);
  uint32_t CreateTriangleEmitterInternal(const Vector3& position, const Vector3& v1, const Vector3& v2, const Vector3& v3, uint32_t count, float frequency);

  /// <summary>
  ///　emitterの更新
  /// </summary>
  void UpdateEmitter();

  /// <summary>
  /// PerViewの更新
  /// </summary>
  void UpdatePerView();

  /// <summary>
  /// PerFrameの更新
  /// </summary>
  void UpdatePerFrame();

  /// <summary>
  /// CPU側からGPU側へのエミッターデータ同期
  /// </summary>
  void SyncEmitterData();

  ///-----------リソース作成関連------------///
  ///<summary>
  /// ルートシグネチャの作成
  /// 	/// </summary>
  void CreateRS();

  ///<summary>
  /// パイプラインステートの生成
  /// </summary>
  void CreatePSO();

  ///<summary>
  /// InitCSルートシグネチャの作成
  ///</summary>
  void CreateInitComputeRS();

  ///<summary>
  /// EmitParticleCSルートシグネチャの作成
  ///</summary>
  void CreateEmitParticleComputeRS();

  ///<summary>
  /// UpdateParticleCSルートシグネチャの作成
  ///</summary>
  void CreateUpdateParticleComputeRS();

  ///<summary>
  /// パイプラインステートの生成
  /// </summary>
  void CreateComputeShaderPSO(Microsoft::WRL::ComPtr<ID3D12RootSignature>& RS, Microsoft::WRL::ComPtr<ID3D12PipelineState>& PSO, const std::wstring& shaderName);

  /// <summary>
  /// 頂点データの生成
  /// </summary>
  void CreateVertexData();

  /// <summary>
  /// PerViewデータの生成
  /// </summary>
  void CreatePerViewData();

  /// <summary>
  /// PerFrameデータの生成
  /// </summary>
  void CreatePerFrameData();

  /// <summary>
  /// EmitterSphereデータの生成
  /// </summary>
  void CreateEmitterData();

  /// <summary>
  /// CSパーティクルリソースの生成
  /// </summary>
  void CreateParticleResource();

  /// <summary>
  /// FreeListリソースの生成
  /// </summary>
  void CreateFreeListResource();

private: //メンバー変数

  // パーティクルの最大出力数
  static const uint32_t kNumMaxParticle_;

  // emitterの最大数
  static const uint32_t kNumMaxEmitter_;


  // 初期化フラグ
  bool isInited_ = false;
  bool isDebug_ = false;

  // DX12Basic
  DX12Basic* m_dx12_ = nullptr;

  // SRVマネージャ
  SrvManager* m_srvManager_ = nullptr;

  // カメラ
  Camera* m_camera_;

  // モデル
  ModelData modelData_;

  // ルートシグネチャ
  Microsoft::WRL::ComPtr<ID3D12RootSignature> RS_;
  Microsoft::WRL::ComPtr<ID3D12RootSignature> initComputeRS_;
  Microsoft::WRL::ComPtr<ID3D12RootSignature> emitParticleRS_;
  Microsoft::WRL::ComPtr<ID3D12RootSignature> updateParticleRS_;

  // パイプラインステート
  Microsoft::WRL::ComPtr<ID3D12PipelineState> PSO_;
  Microsoft::WRL::ComPtr<ID3D12PipelineState> initComputePSO_;
  Microsoft::WRL::ComPtr<ID3D12PipelineState> emitParticlePSO_;
  Microsoft::WRL::ComPtr<ID3D12PipelineState> updateParticlePSO_;

  // パーティクルリソース
  Microsoft::WRL::ComPtr<ID3D12Resource> particleResource_;
  uint32_t particleUavIndex_;
  uint32_t particleSrvIndex_;

  // PerViewの定数バッファ
  Microsoft::WRL::ComPtr<ID3D12Resource> perViewResource_;
  PerView* perViewData_;

  // PerFrameの定数バッファ
  Microsoft::WRL::ComPtr<ID3D12Resource> perFrameResource_;
  PerFrame* perFrameData_;

  // エミッターリソース
  Microsoft::WRL::ComPtr<ID3D12Resource> emitterResource_;
  uint32_t emitterSrvIndex_;
  std::vector<EmitterData> emitters_;
  uint32_t activeEmitterCount_ = 0;

  // FreeListリソース
  Microsoft::WRL::ComPtr<ID3D12Resource> freeListIndexResource_;
  uint32_t freeListIndexUavIndex_;
  Microsoft::WRL::ComPtr<ID3D12Resource> freeListResource_;
  uint32_t freeListUavIndex_;

  // 頂点バッファ
  Microsoft::WRL::ComPtr<ID3D12Resource> vertexResource_;
  VertexData* vertexData_;

  // 頂点バッファビュー
  D3D12_VERTEX_BUFFER_VIEW vertexBufferView_;
};


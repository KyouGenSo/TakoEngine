#pragma once
#include <random>
#include <string>

#include "SrvManager.h"
#include "ParticleStruct.h"

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

  void DebugInfo();

  //-------------------------Getter-------------------------//
  bool GetIsDebug() const { return isDebug_; }

  //-------------------------Setter-------------------------//
  void SetCamera(Camera* camera) { m_camera_ = camera; }
  void SetIsDebug(bool isDebug) { isDebug_ = isDebug; }

private: // プライベートメンバー関数

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
  void CreateEmitterSphereData();

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


  bool isInited_;

  bool isDebug_;

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

  // EmitterSphereの定数バッファ
  Microsoft::WRL::ComPtr<ID3D12Resource> emitterSphereResource_;
  EmitterSphere* emitterSphereData_;

  // PerFrameの定数バッファ
  Microsoft::WRL::ComPtr<ID3D12Resource> perFrameResource_;
  PerFrame* perFrameData_;

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


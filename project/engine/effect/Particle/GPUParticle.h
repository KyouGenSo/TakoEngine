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
/// GPUパーティクルシステムクラス
/// Compute Shaderで最大10万パーティクルの高速処理を実現
/// </summary>
class GPUParticle
{
private: // シングルトン設定
  /// <summary>
  /// シングルトンインスタンス
  /// </summary>
  static GPUParticle* instance_;
  GPUParticle() = default;
  ~GPUParticle() = default;
  GPUParticle(GPUParticle&) = delete;
  GPUParticle& operator=(GPUParticle&) = delete;

public: // メンバー関数

  /// <summary>
  /// インスタンスの取得
  /// </summary>
  /// <returns>GPUParticleシステムのシングルトンインスタンス</returns>
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

  //-------------------------Getter/Setter-------------------------//
  /// <summary>
  /// インデックスによってエミッターを検索
  /// </summary>
  /// <param name="index">検索するインデックス</param>
  /// <returns>見つかったエミッター、見つからない場合はnullptr</returns>
  std::shared_ptr<GPUParticleEmitter> FindEmitterByIndex(size_t index);

  /// <summary>
  /// デバッグモードが有効か取得
  /// </summary>
  /// <returns>デバッグモードが有効な場合true</returns>
  [[nodiscard]] bool GetIsDebug() const { return isDebug_; }

  /// <summary>
  /// カメラを設定
  /// </summary>
  /// <param name="camera">設定するカメラ</param>
  void SetCamera(Camera* camera) { m_camera_ = camera; }

  /// <summary>
  /// デバッグモードを設定
  /// </summary>
  /// <param name="isDebug">デバッグモードを有効にする場合true</param>
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
  /// emitterの更新
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
  /// InitCSルートシグネチャの作成
  /// </summary>
  void CreateInitComputeRS();

  /// <summary>
  /// EmitParticleCSルートシグネチャの作成
  /// </summary>
  void CreateEmitParticleComputeRS();

  /// <summary>
  /// UpdateParticleCSルートシグネチャの作成
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
  /// SRVマネージャへのポインタ
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
  /// パーティクル更新コンピュートシェーダー用ルートシグネチャ
  /// </summary>
  Microsoft::WRL::ComPtr<ID3D12RootSignature> updateParticleRS_;

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
  /// パーティクル更新コンピュートシェーダー用パイプラインステート
  /// </summary>
  Microsoft::WRL::ComPtr<ID3D12PipelineState> updateParticlePSO_;

  /// <summary>
  /// パーティクルデータ用GPU リソース
  /// </summary>
  Microsoft::WRL::ComPtr<ID3D12Resource> particleResource_;

  /// <summary>
  /// パーティクルリソースのUAVインデックス
  /// </summary>
  uint32_t particleUavIndex_;

  /// <summary>
  /// パーティクルリソースのSRVインデックス
  /// </summary>
  uint32_t particleSrvIndex_;

  /// <summary>
  /// PerView定数バッファリソース
  /// </summary>
  Microsoft::WRL::ComPtr<ID3D12Resource> perViewResource_;

  /// <summary>
  /// PerViewデータへのポインタ
  /// </summary>
  PerView* perViewData_;

  /// <summary>
  /// PerFrame定数バッファリソース
  /// </summary>
  Microsoft::WRL::ComPtr<ID3D12Resource> perFrameResource_;

  /// <summary>
  /// PerFrameデータへのポインタ
  /// </summary>
  PerFrame* perFrameData_;

  /// <summary>
  /// エミッターデータ用GPUリソース
  /// </summary>
  Microsoft::WRL::ComPtr<ID3D12Resource> emitterResource_;

  /// <summary>
  /// エミッターリソースのSRVインデックス
  /// </summary>
  uint32_t emitterSrvIndex_;

  /// <summary>
  /// アクティブなエミッターのリスト
  /// </summary>
  std::vector<std::shared_ptr<GPUParticleEmitter>> activeEmitters_;

  /// <summary>
  /// FreeListインデックス用GPUリソース
  /// </summary>
  Microsoft::WRL::ComPtr<ID3D12Resource> freeListIndexResource_;

  /// <summary>
  /// FreeListインデックスのUAVインデックス
  /// </summary>
  uint32_t freeListIndexUavIndex_;

  /// <summary>
  /// FreeList用GPUリソース
  /// </summary>
  Microsoft::WRL::ComPtr<ID3D12Resource> freeListResource_;

  /// <summary>
  /// FreeListのUAVインデックス
  /// </summary>
  uint32_t freeListUavIndex_;

  /// <summary>
  /// 頂点データ用GPUリソース
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


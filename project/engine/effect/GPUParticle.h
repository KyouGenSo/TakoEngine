#pragma once
#include <random>

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


private: // プライベートメンバー関数

  ///<summary>
  /// ルートシグネチャの作成
  /// 	/// </summary>
  void CreateRS();

  ///<summary>
  /// パイプラインステートの生成
  /// </summary>
  void CreatePSO();

  ///<summary>
  /// ルートシグネチャの作成
  /// 	/// </summary>
  void CreateInitComputeRS();

  ///<summary>
  /// パイプラインステートの生成
  /// </summary>
  void CreateInitComputePSO();

  /// <summary>
  /// 頂点データの生成
  /// </summary>
  void CreateVertexData();

  /// <summary>
  /// PerViewデータの生成
  /// </summary>
  void CreatePerViewData();

  /// <summary>
  /// CSパーティクルリソースの生成
  /// </summary>
  void CreateParticleResourceForCS();

private: //メンバー変数

  // パーティクルの最大出力数
  const uint32_t kNumMaxInstance_ = 1024;

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

  // パイプラインステート
  Microsoft::WRL::ComPtr<ID3D12PipelineState> PSO_;
  Microsoft::WRL::ComPtr<ID3D12PipelineState> initComputePSO_;

  // CS用のパーティクルリソース
  Microsoft::WRL::ComPtr<ID3D12Resource> particleResourceForCS_;
  uint32_t initParticleCSUavIndex_;
  uint32_t initParticleCSSrvIndex_;

  // PerViewの定数バッファ
  Microsoft::WRL::ComPtr<ID3D12Resource> perViewResource_;

  // 頂点バッファ
  Microsoft::WRL::ComPtr<ID3D12Resource> vertexResource_;

  // 頂点バッファビュー
  D3D12_VERTEX_BUFFER_VIEW vertexBufferView_;

  // 頂点データ
  VertexData* vertexData_;

  // PerViewのデータ
  PerView* perViewData_;

};


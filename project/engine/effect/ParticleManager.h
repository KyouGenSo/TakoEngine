#pragma once
#include <random>
#include "SrvManager.h"
#include "ParticleStruct.h"

class DX12Basic;

class Camera;

class ParticleManager {
private: // シングルトン設定
  // インスタンス
  static ParticleManager* instance_;

  ParticleManager() = default;
  ~ParticleManager() = default;
  ParticleManager(ParticleManager&) = delete;
  ParticleManager& operator=(ParticleManager&) = delete;

public: // メンバー関数

  /// <summary>
  ///　インスタンスの取得
  ///	</summary>
  static ParticleManager* GetInstance();

  /// <summary>
  ///　初期化
  /// </summary>
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
  /// パーティクルグループの生成
  /// </summary>
  void CreateParticleGroup(const std::string name, const std::string textureFilePath);

  /// <summary>
  ///　パーティクルを削除
  /// </summary>
  void DestroyParticle(const std::string name);

  /// <summary>
  /// エミット
  /// </summary>
  void Emit(const std::string name, const Vector3& position, const Vector3& scale, const Vector3& velocity, const AABB& range, uint32_t count, const Vector4& color, const float lifeTime, bool isRandomColor);

  // -----------------------------------Getters-----------------------------------//
  const std::unordered_map<std::string, ParticleGroup>& GetParticleGroups() const { return particleGroups; }
  const float GetDeltaTime() const { return kDeltaTime_; }
  const bool GetIsDebug() const { return isDebug_; }

  // -----------------------------------Setters-----------------------------------//
  void SetCamera(Camera* camera) { m_camera_ = camera; }
  void SetIsBillboard(bool isBillboard) { isBillboard_ = isBillboard; }
  void SetIsDebug(bool isDebug) { isDebug_ = isDebug; }

private: // プライベートメンバー関数

  ///<summary>
  /// ルートシグネチャの作成
  /// 	/// </summary>
  void CreateRootSignature();

  ///<summary>
  /// パイプラインステートの生成
  /// </summary>
  void CreatePSO();

  ///<summary>
  /// CSルートシグネチャの作成
  /// 	/// </summary>
  //void CreateRootSignatureForCS();

  ///<summary>
  /// CSパイプラインステートの生成
  /// </summary>
  //void CreatePSOForCS();

  /// <summary>
  /// 頂点データの生成
  /// </summary>
  void CreateVertexData();

  /// <summary>
  /// マテリアルデータの初期化
  /// </summary>
  void CreateMaterialData();

  /// <summary>
  /// CSパーティクルリソースの生成
  /// </summary>
  //void CreateParticleResourceForCS();

  /// <summary>
  /// パーティクル生成
  /// </summary>
  Particle MakeNewParticle(std::mt19937& randomEngine, const Vector3& translate, const Vector3& scale, const Vector3& velocity, const AABB& range, const Vector4& color, const float lifeTime, bool isRandomColor);

private: // メンバー変数

  bool isDebug_ = false;

  // DX12Basic
  DX12Basic* m_dx12_ = nullptr;

  SrvManager* srvManager_ = nullptr;

  // カメラ
  Camera* m_camera_;

  // モデル
  ModelData modelData_;

  // パーティクルグループ
  std::unordered_map<std::string, ParticleGroup> particleGroups;

  // パーティクルの最大出力数
  const uint32_t kNumMaxInstance_ = 1024;

  //とりあえず60fps固定してあるが、実時間を計測して可変fpsで動かせるようにしておくとなおよい
  const float kDeltaTime_ = 1.0f / 60.0f;

  // billboardMatrixのフラグ
  bool isBillboard_ = true;

  // ルートシグネチャ
  Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature_;
  //Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignatureCS_;

  // パイプラインステート
  Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineState_;
  //Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineStateCS_;

  // ランダムエンジン
  std::random_device seedGenerator;
  std::mt19937 randomEngine_;

  // CS用のパーティクルリソース
  Microsoft::WRL::ComPtr<ID3D12Resource> particleResource_;
  //uint32_t particleCSUavIndex_;

  // 頂点バッファ
  Microsoft::WRL::ComPtr<ID3D12Resource> vertexResource_;
  // マテリアルデータリソース
  Microsoft::WRL::ComPtr<ID3D12Resource> materialResource_;

  // 頂点バッファビュー
  D3D12_VERTEX_BUFFER_VIEW vertexBufferView_;
  // 頂点データ
  VertexData* vertexData_;

  // マテリアルデータ
  ParticleMaterial* materialData_;
};
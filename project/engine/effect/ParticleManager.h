#pragma once
#include <d3d12.h>
#include<wrl.h>
#include <random>
#include <vector>
#include <list>
#include <unordered_map>
#include "SrvManager.h"
#include "Matrix4x4.h"
#include "vector2.h"
#include "vector3.h"
#include "vector4.h"
#include "Transform.h"

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

public: // 構造体
    struct Transform
    {
        Vector3 scale;
        Vector3 rotate;
        Vector3 translate;

    };

    // 頂点データ
    struct VertexData
    {
        Vector4 position;
        Vector2 texcoord;
        Vector3 normal;
    };

    // GPU用のParticleデータ
    struct ParticleDataForGPU
    {
        Matrix4x4 WVP;
        Matrix4x4 world;
        Vector4 color;
    };

    // マテリアルデータ
    struct MaterialData {
        std::string texturePath;
        uint32_t textureIndex;
    };

    // モデルデータ
    struct ModelData {
        std::vector<VertexData> vertices;
        MaterialData material;
    };

    // マテリアル
    struct Material
    {
        Vector4 color;
        Matrix4x4 uvTransform;
    };

    //Particle構造体
    struct Particle {
        Transform transform;
        Vector3 velocity;
        Vector4 color;
        float lifeTime;
        float currentTime;
    };

    // パーティクルグループ構造体
    struct ParticleGroup {
        // texture
        MaterialData texture;
        // パーティクルのリスト
        std::list<Particle> particleList;
        // インスタンシングデータ用SRVインデックス
        int instancingSrvIndex;
        // GPU用のParticleデータリソース
        Microsoft::WRL::ComPtr<ID3D12Resource> particleDataForGPUResource_;
        // インスタンシングデータを書き込むためのポインタ
        ParticleDataForGPU* pParticleDataForGPU = nullptr;
        // インスタンス数
        UINT instanceCount = 0;
    };

    // エミッター構造体
    struct Emitter {
        Transform transform;
        uint32_t count;
        float frequency;
        float frequencyTime;
    };

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
    /// エミッター
    /// </summary>
    void Emit(const std::string name, const Vector3& position, const Vector3& scale, uint32_t count);
	void Emit(const std::string name, const Vector3& position, const Vector3& scale, uint32_t count, bool isRandomColor);
	void Emit(const std::string name, const Vector3& position, const Vector3& scale, uint32_t count, Vector4 color);
    void Emit(const std::string name, const Vector3& position, uint32_t count);


    // -----------------------------------Getters-----------------------------------//
	const std::unordered_map<std::string, ParticleGroup>& GetParticleGroups() const { return particleGroups; }

    // -----------------------------------Setters-----------------------------------//
    void SetCamera(Camera* camera) { m_camera_ = camera; }
	void SetIsBillboard(bool isBillboard) { isBillboard_ = isBillboard; }

private: // プライベートメンバー関数

    ///<summary>
    ///ルートシグネチャの作成
    /// 	/// </summary>
    void CreateRootSignature();

    ///<summary>
    ///パイプラインステートの生成
    /// </summary>
    void CreatePSO();

    /// <summary>
    /// 頂点データの生成
    /// </summary>
    void CreateVertexData();

    /// <summary>
    /// マテリアルデータの初期化
    /// </summary>
    void CreateMaterialData();

    /// <summary>
    /// パーティクル生成器
    /// </summary>
	Particle MakeNewParticle(std::mt19937& randomEngine, const Vector3& translate, const Vector3& scale);
	Particle MakeNewParticle(std::mt19937& randomEngine, const Vector3& translate, const Vector3& scale, bool isRandomColor);
	Particle MakeNewParticle(std::mt19937& randomEngine, const Vector3& translate, const Vector3& scale, Vector4 color);

private: // メンバー変数

    // DX12Basic
    DX12Basic* m_dx12_  = nullptr;

    SrvManager* srvManager_ = nullptr;

    // カメラ
    Camera* m_camera_;

    // モデル
    ModelData modelData_;

    // ルートシグネチャ
    Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature_;

    // パイプラインステート
    Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineState_;

    // ランダムエンジン
    std::random_device seedGenerator;
    std::mt19937 randomEngine_;

    // 頂点バッファ
    Microsoft::WRL::ComPtr<ID3D12Resource> vertexResource_;
    // マテリアルデータリソース
    Microsoft::WRL::ComPtr<ID3D12Resource> materialResource_;

    // 頂点バッファビュー
    D3D12_VERTEX_BUFFER_VIEW vertexBufferView_;
    // 頂点データ
    VertexData* vertexData_;

    // マテリアルデータ
    Material* materialData_;

    // パーティクルグループ
    std::unordered_map<std::string, ParticleGroup> particleGroups;

    // パーティクルの最大出力数
    const uint32_t kNumMaxInstance_ = 256;

    //とりあえず60fps固定してあるが、実時間を計測して可変fpsで動かせるようにしておくとなおよい
    const float kDeltaTime_ = 1.0f / 60.0f;

    // billboardMatrixのフラグ
    bool isBillboard_ = true;

};
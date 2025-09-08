#pragma once
#include <d3d12.h>
#include <wrl.h>
#include <vector>
#include <memory>
#include "ModelStruct.h"
#include "Transform.h"
#include "Vector4.h"
#include "Mat4x4Func.h"

class Model;
class Camera;
class ModelInstance;

class InstancedObject3d {
public:
    static constexpr uint32_t MAX_INSTANCES = 5000;  // 最大インスタンス数

    InstancedObject3d();
    ~InstancedObject3d();

    /// <summary>
    /// 初期化
    /// </summary>
    void Initialize(const std::string& modelFileName);

    /// <summary>
    /// 更新処理
    /// </summary>
    void Update();

    /// <summary>
    /// 描画処理
    /// </summary>
    void Draw();

    /// <summary>
    /// インスタンスを追加
    /// </summary>
    /// <param name="transform">初期トランスフォーム</param>
    /// <param name="color">インスタンスのカラー</param>
    /// <returns>インスタンスID（削除時に使用）</returns>
    uint32_t AddInstance(const Transform& transform, const Vector4& color = Vector4(1.0f, 1.0f, 1.0f, 1.0f));

    /// <summary>
    /// インスタンスを削除
    /// </summary>
    /// <param name="instanceId">削除するインスタンスのID</param>
    void RemoveInstance(uint32_t instanceId);

    /// <summary>
    /// インスタンスのトランスフォームを更新
    /// </summary>
    void UpdateInstance(uint32_t instanceId, const Transform& transform);

    /// <summary>
    /// インスタンスのカラーを更新
    /// </summary>
    void UpdateInstanceColor(uint32_t instanceId, const Vector4& color);

    /// <summary>
    /// 全インスタンスをGPUバッファに反映
    /// </summary>
    void UpdateAllInstances();

    /// <summary>
    /// 全インスタンスをクリア
    /// </summary>
    void ClearAllInstances();

    /// <summary>
    /// ModelInstanceハンドルを作成
    /// </summary>
    std::unique_ptr<ModelInstance> CreateInstance(const Transform& transform, const Vector4& color = Vector4(1.0f, 1.0f, 1.0f, 1.0f));

    // Getters
    uint32_t GetInstanceCount() const { return static_cast<uint32_t>(instances_.size()); }
    Model* GetModel() const { return model_; }
    const Transform& GetInstanceTransform(uint32_t instanceId) const;
    const Vector4& GetInstanceColor(uint32_t instanceId) const;

    // Setters
    void SetCamera(Camera** camera) { camera_ = camera; }
    void SetEnableLighting(bool enable);
    void SetShininess(float shininess);
    void SetEnvironmentTexture(uint32_t textureIndex);

    // シェーダー用カメラデータ構造
    struct CameraForGPU {
        Vector3 worldPos;
    };

private:
    /// <summary>
    /// インスタンスバッファの作成
    /// </summary>
    void CreateInstanceBuffer();

    /// <summary>
    /// カメラデータリソースの作成
    /// </summary>
    void CreateCameraForGPUData();

    /// <summary>
    /// ViewProjection行列バッファの作成
    /// </summary>
    void CreateViewProjectionBuffer();

    /// <summary>
    /// インスタンスデータを検証
    /// </summary>
    bool IsValidInstanceId(uint32_t instanceId) const;

private:
    // インスタンスデータの内部構造
    struct InternalInstanceData {
        Transform transform;
        Vector4 color;
        bool active;
        uint32_t id;
    };

    // モデル
    Model* model_ = nullptr;

    // カメラ
    Camera** camera_ = nullptr;

    // インスタンスデータ
    std::vector<InternalInstanceData> instances_;
    std::vector<uint32_t> freeIds_;  // 再利用可能なID
    uint32_t nextId_ = 0;

    // GPUリソース
    Microsoft::WRL::ComPtr<ID3D12Resource> instanceBuffer_;
    Microsoft::WRL::ComPtr<ID3D12Resource> cameraForGPUResource_;
    Microsoft::WRL::ComPtr<ID3D12Resource> viewProjResource_;
    
    // マップされたバッファ
    InstanceData* mappedInstanceData_ = nullptr;
    CameraForGPU* cameraForGPUData_ = nullptr;
    Matrix4x4* viewProjData_ = nullptr;

    // SRVインデックス
    uint32_t instanceSrvIndex_ = 0;

    // 更新フラグ
    bool needsUpdate_ = false;
};

// 軽量インスタンスハンドル
class ModelInstance {
public:
    ModelInstance(InstancedObject3d* parent, uint32_t instanceId);
    ~ModelInstance();

    // 移動・コピー禁止
    ModelInstance(const ModelInstance&) = delete;
    ModelInstance& operator=(const ModelInstance&) = delete;
    ModelInstance(ModelInstance&& other) noexcept;
    ModelInstance& operator=(ModelInstance&& other) noexcept;

    // トランスフォーム操作
    void SetTransform(const Transform& transform);
    void SetPosition(const Vector3& position);
    void SetRotation(const Vector3& rotation);
    void SetScale(const Vector3& scale);

    // カラー操作
    void SetColor(const Vector4& color);

    // Getters
    Transform GetTransform() const;
    Vector4 GetColor() const;
    bool IsValid() const { return parent_ != nullptr && instanceId_ != UINT32_MAX; }

private:
    InstancedObject3d* parent_ = nullptr;
    uint32_t instanceId_ = UINT32_MAX;
};
#pragma once
#include <d3d12.h>
#include <wrl.h>
#include <vector>
#include <memory>
#include "ModelStruct.h"
#include "Transform.h"
#include "Vector4.h"
#include "Mat4x4Func.h"

namespace Tako {

class Model;
class Camera;
class ModelInstance;

/// <summary>
/// インスタンシング描画管理クラス
/// 同一モデルの大量描画を効率的に処理
/// </summary>
class InstancedObject3d {
public:
    static constexpr uint32_t MAX_INSTANCES = 5000;  ///< 最大インスタンス数

    /// <summary>
    /// コンストラクタ
    /// </summary>
    InstancedObject3d();

    /// <summary>
    /// デストラクタ
    /// </summary>
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
    /// <summary>
    /// インスタンス数を取得
    /// </summary>
    /// <returns>現在のインスタンス数</returns>
    uint32_t GetInstanceCount() const { return static_cast<uint32_t>(instances_.size()); }

    /// <summary>
    /// モデルを取得
    /// </summary>
    /// <returns>モデルポインタ</returns>
    Model* GetModel() const { return model_.get(); }

    /// <summary>
    /// インスタンスのトランスフォームを取得
    /// </summary>
    /// <param name="instanceId">インスタンスID</param>
    /// <returns>トランスフォーム参照</returns>
    const Transform& GetInstanceTransform(uint32_t instanceId) const;

    /// <summary>
    /// インスタンスのカラーを取得
    /// </summary>
    /// <param name="instanceId">インスタンスID</param>
    /// <returns>カラー参照</returns>
    const Vector4& GetInstanceColor(uint32_t instanceId) const;

    // Setters
    /// <summary>
    /// カメラを設定
    /// </summary>
    /// <param name="camera">カメラポインタのポインタ</param>
    void SetCamera(Camera** camera) { camera_ = camera; }

    /// <summary>
    /// ライティング有効化設定
    /// </summary>
    /// <param name="enable">有効化フラグ</param>
    void SetEnableLighting(bool enable);

    /// <summary>
    /// 光沢度を設定
    /// </summary>
    /// <param name="shininess">光沢度</param>
    void SetShininess(float shininess);

    /// <summary>
    /// 環境テクスチャを設定
    /// </summary>
    /// <param name="textureIndex">テクスチャインデックス</param>
    void SetEnvironmentTexture(uint32_t textureIndex);

    /// <summary>
    /// シェーダー用カメラデータ構造
    /// </summary>
    struct CameraForGPU {
        Vector3 worldPos;  ///< カメラのワールド座標
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
    /// <summary>
    /// インスタンスデータの内部構造
    /// </summary>
    struct InternalInstanceData {
        Transform transform;  ///< トランスフォーム情報
        Vector4 color;  ///< カラー情報
        bool active;  ///< アクティブフラグ
        uint32_t id;  ///< 一意のID
    };

    std::unique_ptr<Model> model_;  ///< モデルポインタ

    Camera** camera_ = nullptr;  ///< カメラポインタのポインタ

    // インスタンスデータ
    std::vector<InternalInstanceData> instances_;  ///< インスタンスデータ配列
    std::vector<uint32_t> freeIds_;  ///< 再利用可能なIDリスト
    uint32_t nextId_ = 0;  ///< 次に割り当てるID

    // GPUリソース
    Microsoft::WRL::ComPtr<ID3D12Resource> instanceBuffer_;  ///< インスタンスバッファ
    Microsoft::WRL::ComPtr<ID3D12Resource> cameraForGPUResource_;  ///< カメラデータ用リソース
    Microsoft::WRL::ComPtr<ID3D12Resource> viewProjResource_;  ///< ViewProjection行列リソース

    // マップされたバッファ
    InstanceData* mappedInstanceData_ = nullptr;  ///< マップされたインスタンスデータ
    CameraForGPU* cameraForGPUData_ = nullptr;  ///< マップされたカメラデータ
    Matrix4x4* viewProjData_ = nullptr;  ///< マップされたViewProjection行列

    uint32_t instanceSrvIndex_ = 0;  ///< インスタンスバッファのSRVインデックス

    bool needsUpdate_ = false;  ///< 更新必要フラグ
};

/// <summary>
/// 軽量インスタンスハンドル
/// 個別のインスタンスを操作するためのクラス
/// </summary>
class ModelInstance {
public:
    /// <summary>
    /// コンストラクタ
    /// </summary>
    /// <param name="parent">親のInstancedObject3d</param>
    /// <param name="instanceId">インスタンスID</param>
    ModelInstance(InstancedObject3d* parent, uint32_t instanceId);

    /// <summary>
    /// デストラクタ
    /// </summary>
    ~ModelInstance();

    // 移動・コピー禁止
    ModelInstance(const ModelInstance&) = delete;
    ModelInstance& operator=(const ModelInstance&) = delete;

    /// <summary>
    /// ムーブコンストラクタ
    /// </summary>
    /// <param name="other">移動元のインスタンス</param>
    ModelInstance(ModelInstance&& other) noexcept;

    /// <summary>
    /// ムーブ代入演算子
    /// </summary>
    /// <param name="other">移動元のインスタンス</param>
    /// <returns>自身の参照</returns>
    ModelInstance& operator=(ModelInstance&& other) noexcept;

    // トランスフォーム操作
    /// <summary>
    /// トランスフォームを設定
    /// </summary>
    /// <param name="transform">新しいトランスフォーム</param>
    void SetTransform(const Transform& transform);

    /// <summary>
    /// 位置を設定
    /// </summary>
    /// <param name="position">新しい位置</param>
    void SetPosition(const Vector3& position);

    /// <summary>
    /// 回転を設定
    /// </summary>
    /// <param name="rotation">新しい回転（オイラー角）</param>
    void SetRotation(const Vector3& rotation);

    /// <summary>
    /// スケールを設定
    /// </summary>
    /// <param name="scale">新しいスケール</param>
    void SetScale(const Vector3& scale);

    // カラー操作
    /// <summary>
    /// カラーを設定
    /// </summary>
    /// <param name="color">新しいカラー</param>
    void SetColor(const Vector4& color);

    // Getters
    /// <summary>
    /// トランスフォームを取得
    /// </summary>
    /// <returns>現在のトランスフォーム</returns>
    Transform GetTransform() const;

    /// <summary>
    /// カラーを取得
    /// </summary>
    /// <returns>現在のカラー</returns>
    Vector4 GetColor() const;

    /// <summary>
    /// 有効性を確認
    /// </summary>
    /// <returns>有効な場合true</returns>
    bool IsValid() const { return parent_ != nullptr && instanceId_ != UINT32_MAX; }

private:
    InstancedObject3d* parent_ = nullptr;  ///< 親のInstancedObject3dポインタ
    uint32_t instanceId_ = UINT32_MAX;  ///< インスタンスID
};

} // namespace Tako
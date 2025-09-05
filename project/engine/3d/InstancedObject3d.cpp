#include "InstancedObject3d.h"
#include "DX12Basic.h"
#include "Object3dBasic.h"
#include "Model.h"
#include "ModelManager.h"
#include "Camera.h"
#include "SrvManager.h"
#include "Logger.h"
#include <algorithm>
#include <cassert>

InstancedObject3d::InstancedObject3d() {
}

InstancedObject3d::~InstancedObject3d() {
    if (instanceBuffer_) {
        instanceBuffer_->Unmap(0, nullptr);
    }
    if (cameraForGPUResource_) {
        cameraForGPUResource_->Unmap(0, nullptr);
    }
    if (instanceSrvIndex_ != 0) {
        SrvManager::GetInstance()->Free(instanceSrvIndex_);
    }
}

void InstancedObject3d::Initialize(const std::string& modelFileName) {
    // カメラを取得
    camera_ = Object3dBasic::GetInstance()->GetCamera();

    // モデルを取得
    model_ = ModelManager::GetInstance()->GetModel(modelFileName);

    // インスタンスバッファの作成
    CreateInstanceBuffer();

    // カメラデータの作成
    CreateCameraForGPUData();
}

void InstancedObject3d::CreateInstanceBuffer() {
    DX12Basic* dx12 = Object3dBasic::GetInstance()->GetDX12Basic();

    // インスタンスバッファの作成（StructuredBuffer）
    D3D12_HEAP_PROPERTIES heapProperties{};
    heapProperties.Type = D3D12_HEAP_TYPE_UPLOAD;

    D3D12_RESOURCE_DESC resourceDesc{};
    resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    resourceDesc.Width = sizeof(InstanceData) * MAX_INSTANCES;
    resourceDesc.Height = 1;
    resourceDesc.DepthOrArraySize = 1;
    resourceDesc.MipLevels = 1;
    resourceDesc.Format = DXGI_FORMAT_UNKNOWN;
    resourceDesc.SampleDesc.Count = 1;
    resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    resourceDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

    HRESULT hr = dx12->GetDevice()->CreateCommittedResource(
        &heapProperties,
        D3D12_HEAP_FLAG_NONE,
        &resourceDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&instanceBuffer_)
    );
    assert(SUCCEEDED(hr));

    // マップ
    hr = instanceBuffer_->Map(0, nullptr, reinterpret_cast<void**>(&mappedInstanceData_));
    assert(SUCCEEDED(hr));

    // 初期化
    for (uint32_t i = 0; i < MAX_INSTANCES; ++i) {
        mappedInstanceData_[i] = {};
        mappedInstanceData_[i].world = Mat4x4::MakeIdentity();
        mappedInstanceData_[i].worldInvTranspose = Mat4x4::MakeIdentity();
        mappedInstanceData_[i].color = Vector4(1.0f, 1.0f, 1.0f, 1.0f);
    }

    // SRVの作成
    instanceSrvIndex_ = SrvManager::GetInstance()->Allocate();
    
    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
    srvDesc.Format = DXGI_FORMAT_UNKNOWN;
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
    srvDesc.Buffer.FirstElement = 0;
    srvDesc.Buffer.NumElements = MAX_INSTANCES;
    srvDesc.Buffer.StructureByteStride = sizeof(InstanceData);
    srvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;

    D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle = SrvManager::GetInstance()->GetCPUDescriptorHandle(instanceSrvIndex_);
    dx12->GetDevice()->CreateShaderResourceView(instanceBuffer_.Get(), &srvDesc, cpuHandle);
}

void InstancedObject3d::CreateCameraForGPUData() {
    DX12Basic* dx12 = Object3dBasic::GetInstance()->GetDX12Basic();

    // カメラ用バッファの作成
    D3D12_HEAP_PROPERTIES heapProperties{};
    heapProperties.Type = D3D12_HEAP_TYPE_UPLOAD;

    D3D12_RESOURCE_DESC resourceDesc{};
    resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    resourceDesc.Width = sizeof(CameraForGPU);
    resourceDesc.Height = 1;
    resourceDesc.DepthOrArraySize = 1;
    resourceDesc.MipLevels = 1;
    resourceDesc.Format = DXGI_FORMAT_UNKNOWN;
    resourceDesc.SampleDesc.Count = 1;
    resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

    HRESULT hr = dx12->GetDevice()->CreateCommittedResource(
        &heapProperties,
        D3D12_HEAP_FLAG_NONE,
        &resourceDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&cameraForGPUResource_)
    );
    assert(SUCCEEDED(hr));

    // マップ
    hr = cameraForGPUResource_->Map(0, nullptr, reinterpret_cast<void**>(&cameraForGPUData_));
    assert(SUCCEEDED(hr));
}

uint32_t InstancedObject3d::AddInstance(const Transform& transform, const Vector4& color) {
    if (instances_.size() >= MAX_INSTANCES) {
        Logger::Log("Maximum instance count reached");
        return UINT32_MAX;
    }

    uint32_t id;
    if (!freeIds_.empty()) {
        id = freeIds_.back();
        freeIds_.pop_back();
    } else {
        id = nextId_++;
    }

    InternalInstanceData data;
    data.transform = transform;
    data.color = color;
    data.active = true;
    data.id = id;

    instances_.push_back(data);
    needsUpdate_ = true;

    return id;
}

void InstancedObject3d::RemoveInstance(uint32_t instanceId) {
    auto it = std::find_if(instances_.begin(), instances_.end(),
        [instanceId](const InternalInstanceData& data) {
            return data.id == instanceId && data.active;
        });

    if (it != instances_.end()) {
        it->active = false;
        freeIds_.push_back(instanceId);
        instances_.erase(it);
        needsUpdate_ = true;
    }
}

void InstancedObject3d::UpdateInstance(uint32_t instanceId, const Transform& transform) {
    auto it = std::find_if(instances_.begin(), instances_.end(),
        [instanceId](const InternalInstanceData& data) {
            return data.id == instanceId && data.active;
        });

    if (it != instances_.end()) {
        it->transform = transform;
        needsUpdate_ = true;
    }
}

void InstancedObject3d::UpdateInstanceColor(uint32_t instanceId, const Vector4& color) {
    auto it = std::find_if(instances_.begin(), instances_.end(),
        [instanceId](const InternalInstanceData& data) {
            return data.id == instanceId && data.active;
        });

    if (it != instances_.end()) {
        it->color = color;
        needsUpdate_ = true;
    }
}

void InstancedObject3d::UpdateAllInstances() {
    if (!needsUpdate_ || !mappedInstanceData_) {
        return;
    }

    // アクティブなインスタンスデータをGPUバッファに書き込む
    size_t activeCount = 0;
    for (const auto& instance : instances_) {
        if (instance.active && activeCount < MAX_INSTANCES) {
            Matrix4x4 worldMatrix = Mat4x4::MakeAffine(
                instance.transform.scale,
                instance.transform.rotate,
                instance.transform.translate
            );

            mappedInstanceData_[activeCount].world = worldMatrix;
            mappedInstanceData_[activeCount].worldInvTranspose = Mat4x4::InverseTranspose(worldMatrix);
            mappedInstanceData_[activeCount].color = instance.color;
            activeCount++;
        }
    }

    needsUpdate_ = false;
}

void InstancedObject3d::Update() {
    // モデルの更新
    if (model_) {
        model_->Update();
    }

    // インスタンスデータの更新
    UpdateAllInstances();

    // カメラデータの更新
    if (camera_ && *camera_) {
        cameraForGPUData_->worldPos = (*camera_)->GetTransform().translate;
    }
}

void InstancedObject3d::Draw() {
    if (!model_ || instances_.empty()) {
        return;
    }

    DX12Basic* dx12 = Object3dBasic::GetInstance()->GetDX12Basic();
    ID3D12GraphicsCommandList* commandList = dx12->GetCommandList();
    
    // インスタンシング用のレンダリング設定
    Object3dBasic::GetInstance()->SetInstancedRenderSetting();
    
    // ViewProjection行列をセット（ルートパラメータ1: b0）
    if (camera_ && *camera_) {
        // ViewProjection用の定数バッファを作成・設定
        // 注: 実際の実装では適切な定数バッファリソースを作成・管理する必要があります
        static Microsoft::WRL::ComPtr<ID3D12Resource> viewProjResource;
        if (!viewProjResource) {
            D3D12_HEAP_PROPERTIES heapProps{};
            heapProps.Type = D3D12_HEAP_TYPE_UPLOAD;
            
            D3D12_RESOURCE_DESC resDesc{};
            resDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
            resDesc.Width = sizeof(Matrix4x4);
            resDesc.Height = 1;
            resDesc.DepthOrArraySize = 1;
            resDesc.MipLevels = 1;
            resDesc.Format = DXGI_FORMAT_UNKNOWN;
            resDesc.SampleDesc.Count = 1;
            resDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
            
            dx12->GetDevice()->CreateCommittedResource(
                &heapProps, D3D12_HEAP_FLAG_NONE,
                &resDesc, D3D12_RESOURCE_STATE_GENERIC_READ,
                nullptr, IID_PPV_ARGS(&viewProjResource));
        }
        
        Matrix4x4* mappedData = nullptr;
        viewProjResource->Map(0, nullptr, reinterpret_cast<void**>(&mappedData));
        *mappedData = (*camera_)->GetViewProjectionMatrix();
        viewProjResource->Unmap(0, nullptr);
        
        commandList->SetGraphicsRootConstantBufferView(1, viewProjResource->GetGPUVirtualAddress());
    }

    // カメラデータをセット（ルートパラメータ4: b2）
    commandList->SetGraphicsRootConstantBufferView(4, cameraForGPUResource_->GetGPUVirtualAddress());
    
    // インスタンスバッファのSRVをセット（ルートパラメータ11: t5レジスタ）
    D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle = SrvManager::GetInstance()->GetGPUDescriptorHandle(instanceSrvIndex_);
    commandList->SetGraphicsRootDescriptorTable(11, gpuHandle);

    // モデルの各メッシュをインスタンシング描画
    model_->DrawInstanced(static_cast<uint32_t>(instances_.size()));
}

void InstancedObject3d::ClearAllInstances() {
    instances_.clear();
    freeIds_.clear();
    nextId_ = 0;
    needsUpdate_ = true;
    UpdateAllInstances();
}

std::unique_ptr<ModelInstance> InstancedObject3d::CreateInstance(const Transform& transform, const Vector4& color) {
    uint32_t id = AddInstance(transform, color);
    if (id == UINT32_MAX) {
        return nullptr;
    }
    return std::make_unique<ModelInstance>(this, id);
}

const Transform& InstancedObject3d::GetInstanceTransform(uint32_t instanceId) const {
    auto it = std::find_if(instances_.begin(), instances_.end(),
        [instanceId](const InternalInstanceData& data) {
            return data.id == instanceId && data.active;
        });

    static Transform defaultTransform{};
    if (it != instances_.end()) {
        return it->transform;
    }
    return defaultTransform;
}

const Vector4& InstancedObject3d::GetInstanceColor(uint32_t instanceId) const {
    auto it = std::find_if(instances_.begin(), instances_.end(),
        [instanceId](const InternalInstanceData& data) {
            return data.id == instanceId && data.active;
        });

    static Vector4 defaultColor(1.0f, 1.0f, 1.0f, 1.0f);
    if (it != instances_.end()) {
        return it->color;
    }
    return defaultColor;
}

void InstancedObject3d::SetEnableLighting(bool enable) {
    if (model_) {
        model_->SetEnableLighting(enable);
    }
}

void InstancedObject3d::SetShininess(float shininess) {
    if (model_) {
        model_->SetShininess(shininess);
    }
}

void InstancedObject3d::SetEnvironmentTexture(uint32_t textureIndex) {
    if (model_) {
        model_->SetEnvironmentTexture(textureIndex);
    }
}

bool InstancedObject3d::IsValidInstanceId(uint32_t instanceId) const {
    return std::any_of(instances_.begin(), instances_.end(),
        [instanceId](const InternalInstanceData& data) {
            return data.id == instanceId && data.active;
        });
}

// ModelInstance実装
ModelInstance::ModelInstance(InstancedObject3d* parent, uint32_t instanceId)
    : parent_(parent), instanceId_(instanceId) {
}

ModelInstance::~ModelInstance() {
    if (parent_ && instanceId_ != UINT32_MAX) {
        parent_->RemoveInstance(instanceId_);
    }
}

ModelInstance::ModelInstance(ModelInstance&& other) noexcept
    : parent_(other.parent_), instanceId_(other.instanceId_) {
    other.parent_ = nullptr;
    other.instanceId_ = UINT32_MAX;
}

ModelInstance& ModelInstance::operator=(ModelInstance&& other) noexcept {
    if (this != &other) {
        if (parent_ && instanceId_ != UINT32_MAX) {
            parent_->RemoveInstance(instanceId_);
        }
        parent_ = other.parent_;
        instanceId_ = other.instanceId_;
        other.parent_ = nullptr;
        other.instanceId_ = UINT32_MAX;
    }
    return *this;
}

void ModelInstance::SetTransform(const Transform& transform) {
    if (parent_ && instanceId_ != UINT32_MAX) {
        parent_->UpdateInstance(instanceId_, transform);
    }
}

void ModelInstance::SetPosition(const Vector3& position) {
    if (parent_ && instanceId_ != UINT32_MAX) {
        Transform transform = parent_->GetInstanceTransform(instanceId_);
        transform.translate = position;
        parent_->UpdateInstance(instanceId_, transform);
    }
}

void ModelInstance::SetRotation(const Vector3& rotation) {
    if (parent_ && instanceId_ != UINT32_MAX) {
        Transform transform = parent_->GetInstanceTransform(instanceId_);
        transform.rotate = rotation;
        parent_->UpdateInstance(instanceId_, transform);
    }
}

void ModelInstance::SetScale(const Vector3& scale) {
    if (parent_ && instanceId_ != UINT32_MAX) {
        Transform transform = parent_->GetInstanceTransform(instanceId_);
        transform.scale = scale;
        parent_->UpdateInstance(instanceId_, transform);
    }
}

void ModelInstance::SetColor(const Vector4& color) {
    if (parent_ && instanceId_ != UINT32_MAX) {
        parent_->UpdateInstanceColor(instanceId_, color);
    }
}

Transform ModelInstance::GetTransform() const {
    if (parent_ && instanceId_ != UINT32_MAX) {
        return parent_->GetInstanceTransform(instanceId_);
    }
    return Transform{};
}

Vector4 ModelInstance::GetColor() const {
    if (parent_ && instanceId_ != UINT32_MAX) {
        return parent_->GetInstanceColor(instanceId_);
    }
    return Vector4(1.0f, 1.0f, 1.0f, 1.0f);
}
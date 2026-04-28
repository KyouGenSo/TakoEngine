#include "GPUParticle.h"
#include "GPUParticleEmitter.h"
#include "SphereEmitter.h"
#include "BoxEmitter.h"
#include "TriangleEmitter.h"
#include "DX12Basic.h"
#include "Camera.h"
#include "TextureManager.h"
#include "FrameTimer.h"
#include "WinApp.h"
#include "EnginePaths.h"

#ifdef _DEBUG
#include "DebugUIManager.h"
#include "ImGuiManager.h"
#include "DebugCamera.h"
#endif // _DEBUG

#include <numbers>

namespace Tako {

  std::unique_ptr<GPUParticle> GPUParticle::instance_ = nullptr;

  const uint32_t GPUParticle::kNumMaxParticle = 1000000;

  const uint32_t GPUParticle::kNumMaxEmitter = 500;

  GPUParticle* GPUParticle::GetInstance()
  {
    if (!instance_) {
      instance_ = std::unique_ptr<GPUParticle>(new GPUParticle());
    }
    return instance_.get();
  }

  void GPUParticle::Initialize(DX12Basic* dx12, Camera* camera)
  {
    m_dx12_ = dx12;

    // カメラの設定
    m_camera_ = camera;

    m_srvManager_ = SrvManager::GetInstance();

    modelData_.textureData.texturePath = "circle.dds";
    modelData_.textureData.textureIndex = TextureManager::GetInstance()->GetEngineDefaultSRVIndex(modelData_.textureData.texturePath);

    isInited_ = false;

    isDebug_ = false;

    // ルートシグネチャの生成
    CreateRS();
    CreateInitComputeRS();
    CreateEmitParticleComputeRS();
    CreateIntegrateAllComputeRS();

    // PSO の生成
    CreatePSO();
    CreateComputeShaderPSO(initComputeRS_, initComputePSO_, L"InitParticle.CS.hlsl");
    CreateComputeShaderPSO(emitParticleRS_, emitParticlePSO_, L"EmitParticle.CS.hlsl");
    CreateComputeShaderPSO(integrateAllRS_, integrateAllPSO_, L"IntegrateAll.CS.hlsl");

    // PerView データの生成
    CreatePerViewData();

    // PerFrame データの生成
    CreatePerFrameData();

    // パーティクルリソースの生成
    CreateParticleResource();

    // EmitterSphere データの生成
    CreateEmitterData();

    // 頂点データの生成
    CreateVertexData();

    // FreeCounter リソースの生成
    CreateFreeListResource();

#ifdef _DEBUG
    // Readback バッファの生成（アクティブパーティクル数取得用）
    CreateFreeListReadbackResource();
#endif

    // フォースフィールドリソースの生成
    CreateForceFieldResource();

    // 物理パラメータリソースの生成
    CreatePhysicsParamsResource();

    // 深度バッファ用 SRV の作成
    CreateDepthSRV();
  }

  void GPUParticle::Update()
  {
    // PerFrame の更新
    UpdatePerFrame();

    // エミッターの更新
    UpdateEmitter();

    // PerView の更新
    UpdatePerView();

    SyncEmitterData();

    // 物理パラメータの更新
    UpdatePhysicsParams();

    // フォースフィールドデータの同期
    SyncForceFieldData();
  }

  void GPUParticle::Draw()
  {
    ID3D12GraphicsCommandList* commandList = m_dx12_->GetCommandList();

    /// ================================== ///
    ///            ComputerShader          ///
    /// ================================== ///

    //--------------------------------------初期化--------------------------------------//
    if (!isInited_) {
      // ルートシグネチャの設定
      commandList->SetComputeRootSignature(initComputeRS_.Get());

      // パイプラインステートの設定
      commandList->SetPipelineState(initComputePSO_.Get());

      // ParticleData の UAV の設定
      m_srvManager_->SetComputeRootDescriptorTable(0, particleUavIndex_);

      // FreeListIndex の UAV の設定
      m_srvManager_->SetComputeRootDescriptorTable(1, freeListIndexUavIndex_);

      // FreeList の UAV の設定
      m_srvManager_->SetComputeRootDescriptorTable(2, freeListUavIndex_);

      // ディスパッチ
      commandList->Dispatch(1024, 1, 1);

      isInited_ = true;
    }

    // リソースバリアの設定（UAV 同期）
    m_dx12_->SetUAVBarrier(particleResource_.Get());
    m_dx12_->SetUAVBarrier(freeListIndexResource_.Get());
    m_dx12_->SetUAVBarrier(freeListResource_.Get());

    //--------------------------------------射出--------------------------------------//
      // アクティブなエミッターがある場合のみ実行
    if (!activeEmitters_.empty()) {
      // ルートシグネチャの設定
      commandList->SetComputeRootSignature(emitParticleRS_.Get());

      // パイプラインステートの設定
      commandList->SetPipelineState(emitParticlePSO_.Get());

      // ParticleData の UAV の設定
      m_srvManager_->SetComputeRootDescriptorTable(0, particleUavIndex_);

      // FreeListIndex の UAV の設定
      m_srvManager_->SetComputeRootDescriptorTable(3, freeListIndexUavIndex_);

      // FreeList の UAV の設定
      m_srvManager_->SetComputeRootDescriptorTable(4, freeListUavIndex_);

      // エミッターリストの SRV の設定
      m_srvManager_->SetComputeRootDescriptorTable(1, emitterSrvIndex_);

      // PerFrame の設定
      commandList->SetComputeRootConstantBufferView(2, perFrameResource_->GetGPUVirtualAddress());

      // ディスパッチ（16スレッドごとにグループ化）
      uint32_t threadGroupsX = (static_cast<uint32_t>(activeEmitters_.size()) + 15) / 16;
      commandList->Dispatch(threadGroupsX, 1, 1);
    }

    // リソースバリアの設定（UAV 同期）
    m_dx12_->SetUAVBarrier(particleResource_.Get());
    m_dx12_->SetUAVBarrier(freeListIndexResource_.Get());
    m_dx12_->SetUAVBarrier(freeListResource_.Get());

    //--------------------------------------IntegrateAll--------------------------------------//

    // 深度バッファを NON_PIXEL_SHADER_RESOURCE に遷移（深度衝突用）
    m_dx12_->TransitionResourceWithTracking(
      m_dx12_->GetDepthStencilResource(),
      D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);

    // ルートシグネチャの設定
    commandList->SetComputeRootSignature(integrateAllRS_.Get());

    // パイプラインステートの設定
    commandList->SetPipelineState(integrateAllPSO_.Get());

    // ParticleData の UAV の設定 (u0)
    m_srvManager_->SetComputeRootDescriptorTable(0, particleUavIndex_);

    // FreeListIndex の UAV の設定 (u1)
    m_srvManager_->SetComputeRootDescriptorTable(1, freeListIndexUavIndex_);

    // FreeList の UAV の設定 (u2)
    m_srvManager_->SetComputeRootDescriptorTable(2, freeListUavIndex_);

    // ForceFields の SRV の設定 (t0)
    m_srvManager_->SetComputeRootDescriptorTable(3, forceFieldSrvIndex_);

    // DepthBuffer の SRV の設定 (t1) — 深度バッファ衝突用
    m_srvManager_->SetComputeRootDescriptorTable(4, depthSrvIndex_);

    // PerFrame の CBV の設定 (b0)
    commandList->SetComputeRootConstantBufferView(5, perFrameResource_->GetGPUVirtualAddress());

    // PhysicsParams の CBV の設定 (b1)
    commandList->SetComputeRootConstantBufferView(6, physicsParamsResource_->GetGPUVirtualAddress());

    // ディスパッチ（256スレッド/グループ × ceil(1M/256) = 3907グループ）
    uint32_t integrateGroups = (kNumMaxParticle + 255) / 256;
    commandList->Dispatch(integrateGroups, 1, 1);

    m_dx12_->SetUAVBarrier(particleResource_.Get());

#ifdef _DEBUG
    // アクティブパーティクル数の Readback（IntegrateAll 完了直後が最適）
    ReadbackActiveParticleCount();
#endif

    // 深度バッファを DEPTH_WRITE に復帰
    m_dx12_->TransitionResourceWithTracking(
      m_dx12_->GetDepthStencilResource(),
      D3D12_RESOURCE_STATE_DEPTH_WRITE);

    /// ======================== ///
    ///           描画    　     ///
    /// ======================= ///

      // ルートシグネチャの設定
    commandList->SetGraphicsRootSignature(RS_.Get());

    // パイプラインステートの設定
    commandList->SetPipelineState(PSO_.Get());

    // プリミティブトポロジを設定
    commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    // VBV を設定
    commandList->IASetVertexBuffers(0, 1, &vertexBufferView_);

    // ParticleData を SRV に変更
    m_dx12_->TransitionResourceState(D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, particleResource_.Get());

    // ParticleData の SRV を設定
    m_srvManager_->SetGraphicsRootDescriptorTable(0, particleSrvIndex_);

    // PerView の設定
    commandList->SetGraphicsRootConstantBufferView(1, perViewResource_->GetGPUVirtualAddress());

    // テクスチャの設定
    m_srvManager_->SetGraphicsRootDescriptorTable(2, modelData_.textureData.textureIndex);

    // 描画（インスタンス描画）
    commandList->DrawInstanced(static_cast<UINT>(modelData_.vertices.size()), kNumMaxParticle, 0, 0);

    // ParticleData を UAV に戻す
    m_dx12_->TransitionResourceState(D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, particleResource_.Get());
  }

  void GPUParticle::Finalize()
  {
    instance_.reset();
  }

  std::shared_ptr<GPUParticleEmitter> GPUParticle::CreateTemporaryEmitterFrom(GPUParticleEmitter* sourceEmitter, float lifeTime)
  {
    if (!sourceEmitter) return nullptr;

    // ソースエミッターのクローンを作成
    auto newEmitter = sourceEmitter->Clone();
    if (!newEmitter) return nullptr;

    // 重要なパラメータを強制的に設定
    // アクティブ化
    newEmitter->SetActive(true);

    // 射出をすぐに行うための設定
    newEmitter->SetFrequencyTime(newEmitter->GetFrequency());

    // 強制的に射出フラグを設定する
    newEmitter->SetEmitting(true);

    // 一時エミッター設定
    newEmitter->SetTemporary(true, lifeTime);

    // アクティブリストに追加
    RegisterEmitter(newEmitter);

    // デバッグ情報
#ifdef _DEBUG
    DebugUIManager::GetInstance()->AddLog("CreateTempEmitter: ID=" + std::to_string(newEmitter->GetEmitterId()) +
      ", Active=" + std::to_string(newEmitter->IsActive() ? 1 : 0) +
      ", Emit=" + std::to_string(newEmitter->IsEmitting() ? 1 : 0) +
      ", FreqTime=" + std::to_string(newEmitter->GetFrequencyTime()) +
      "/" + std::to_string(newEmitter->GetFrequency()), DebugUIManager::LogType::Info);
#endif

    return newEmitter;
  }

  void GPUParticle::RegisterEmitter(std::shared_ptr<GPUParticleEmitter> emitter)
  {
    if (activeEmitters_.size() >= kNumMaxEmitter) {
      return;
    }

    if (!emitter) {
      return;
    }

    activeEmitters_.push_back(emitter);
  }

  void GPUParticle::UnregisterEmitter(std::shared_ptr<GPUParticleEmitter> emitter)
  {
    if (activeEmitters_.empty()) {
      return;
    }

    auto it = std::ranges::find(activeEmitters_, emitter);
    if (it != activeEmitters_.end()) {
      activeEmitters_.erase(it);
    }
  }

  std::shared_ptr<GPUParticleEmitter> GPUParticle::FindEmitterByIndex(size_t index) {
    if (index < activeEmitters_.size()) {
      return activeEmitters_[index];
    }
    return nullptr;
  }

  //--------------------------------------Private--------------------------------------//

  void GPUParticle::UpdateEmitter()
  {
    float deltaTime = FrameTimer::GetInstance()->GetDeltaTime();

    // 削除予定のエミッターを格納するリスト
    std::vector<std::shared_ptr<GPUParticleEmitter>> emittersToRemove;

    // すべてのエミッターを更新
    for (auto& emitter : activeEmitters_) {
      // 非アクティブならスキップ
      if (!emitter->IsActive()) {
        continue;
      }

      // 一時的なエミッターの場合、寿命を更新
      if (emitter->IsTemporary()) {
        emitter->UpdateTemporaryLifeTime(deltaTime);

        // 寿命が尽きたら削除予定リストに追加
        if (emitter->IsLifeTimeExpired()) {
          emittersToRemove.push_back(emitter);
          continue;
        }
      }

      // 射出タイマーを更新
      emitter->UpdateEmission(deltaTime);
    }

    // 寿命が尽きたエミッターを削除
    for (auto& emitter : emittersToRemove) {
      UnregisterEmitter(emitter);
    }

    // GPU 側のデータを同期
    SyncEmitterData();
  }

  void GPUParticle::UpdatePerView()
  {

    Matrix4x4 cameraMatrix = Mat4x4::MakeAffine({ .x = 1.0f,.y = 1.0f,.z = 1.0f }, m_camera_->GetRotate(), m_camera_->GetTranslate());

#ifdef _DEBUG
    if (isDebug_) {
      cameraMatrix = Mat4x4::MakeAffine({ .x = 1.0f,.y = 1.0f,.z = 1.0f }, DebugCamera::GetInstance()->GetRotate(), DebugCamera::GetInstance()->GetTranslate());
    }
#endif

    const Matrix4x4 viewProjectionMatrix = Mat4x4::Multiply(Mat4x4::Inverse(cameraMatrix), m_camera_->GetProjectionMatrix());

    // ビルボード行列の生成
    const Matrix4x4 backToFrontMatrix = Mat4x4::MakeRotateY(std::numbers::pi_v<float>);

    Matrix4x4 billboardMatrix = Mat4x4::Multiply(backToFrontMatrix, cameraMatrix);
    billboardMatrix.m[3][0] = 0.0f;  //平行移動成分はいらない
    billboardMatrix.m[3][1] = 0.0f;
    billboardMatrix.m[3][2] = 0.0f;

    // PerView の更新
    perViewData_->billboardMatrix = billboardMatrix;
    perViewData_->viewProjection = viewProjectionMatrix;
  }

  void GPUParticle::UpdatePerFrame()
  {
    perFrameData_->time = FrameTimer::GetInstance()->GetGameTime();
    perFrameData_->deltaTime = FrameTimer::GetInstance()->GetDeltaTime();
  }

  // CPU→GPU 同期
  void GPUParticle::SyncEmitterData()
  {
    // GPU 側のエミッターバッファにマップ
    EmitterData* gpuEmitters = nullptr;
    emitterResource_->Map(0, nullptr, reinterpret_cast<void**>(&gpuEmitters));

    // バッファサイズが足りるか確認
    if (activeEmitters_.size() > kNumMaxEmitter) {
#ifdef _DEBUG
      DebugUIManager::GetInstance()->AddLog("Warning: Too many active emitters! Max: " + std::to_string(kNumMaxEmitter) +
        ", Current: " + std::to_string(activeEmitters_.size()), DebugUIManager::LogType::Warning);
#endif
    }

    // 各エミッターの GPU データを更新（統合構造体をそのままコピー）
    size_t emitterCount = min(activeEmitters_.size(), static_cast<size_t>(kNumMaxEmitter));
    for (size_t i = 0; i < emitterCount; i++) {
      if (activeEmitters_[i]) {
        gpuEmitters[i] = activeEmitters_[i]->GetData();
      }
    }

    // アンマップ
    emitterResource_->Unmap(0, nullptr);

    // PerFrame データにアクティブエミッター数を格納
    perFrameData_->activeEmitterCount = static_cast<uint32_t>(emitterCount);
  }

  void GPUParticle::CreateRS()
  {
    HRESULT hr;

    // rootSignature の生成
    D3D12_ROOT_SIGNATURE_DESC descriptionRootSignature;
    descriptionRootSignature.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

    // Sampler の設定
    D3D12_STATIC_SAMPLER_DESC samplerDesc[1]{};
    samplerDesc[0].Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR; // テクスチャの補間方法
    samplerDesc[0].AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP; // テクスチャの繰り返し方法
    samplerDesc[0].AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP; // テクスチャの繰り返し方法
    samplerDesc[0].AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP; // テクスチャの繰り返し方法
    samplerDesc[0].ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER; // 比較しない
    samplerDesc[0].MaxLOD = D3D12_FLOAT32_MAX; // ミップマップの最大 LOD
    samplerDesc[0].ShaderRegister = 0; // レジスタ番号
    samplerDesc[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL; // ピクセルシェーダーで使う
    descriptionRootSignature.pStaticSamplers = samplerDesc;
    descriptionRootSignature.NumStaticSamplers = _countof(samplerDesc);

    // DescriptorRange の設定。
    // Texture
    D3D12_DESCRIPTOR_RANGE descriptorRange_tex[1] = {};
    descriptorRange_tex[0].BaseShaderRegister = 0; // レジスタ番号
    descriptorRange_tex[0].NumDescriptors = 1; // ディスクリプタ数
    descriptorRange_tex[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV; // SRV を使う
    descriptorRange_tex[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND; // Offset を自動計算

    // Particle
    D3D12_DESCRIPTOR_RANGE descriptorRangeForParticle[1] = {};
    descriptorRangeForParticle[0].BaseShaderRegister = 0; // レジスタ番号
    descriptorRangeForParticle[0].NumDescriptors = 1; // ディスクリプタ数
    descriptorRangeForParticle[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV; // SRV を使う
    descriptorRangeForParticle[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND; // Offset を自動計算

    // RootParameter の設定。複数設定できるので配列
    D3D12_ROOT_PARAMETER rootParameters[3] = {};

    // Particle
    rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE; // ディスクリプタテーブルを使う
    rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX; // 頂点シェーダーで使う
    rootParameters[0].DescriptorTable.pDescriptorRanges = descriptorRangeForParticle; // ディスクリプタレンジを設定
    rootParameters[0].DescriptorTable.NumDescriptorRanges = _countof(descriptorRangeForParticle); // レンジの数

    // PerView
    rootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV; // 定数バッファビューを使う
    rootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX; // 頂点シェーダーで使う
    rootParameters[1].Descriptor.ShaderRegister = 0; // レジスタ番号とバインド

    // Texture
    rootParameters[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE; // ディスクリプタテーブルを使う
    rootParameters[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL; // ピクセルシェーダーで使う
    rootParameters[2].DescriptorTable.pDescriptorRanges = descriptorRange_tex; // ディスクリプタレンジを設定
    rootParameters[2].DescriptorTable.NumDescriptorRanges = _countof(descriptorRange_tex); // レンジの数

    descriptionRootSignature.pParameters = rootParameters;
    descriptionRootSignature.NumParameters = _countof(rootParameters);

    Microsoft::WRL::ComPtr<ID3DBlob> signatureBlob = nullptr;
    Microsoft::WRL::ComPtr<ID3DBlob> errorBlob = nullptr;

    hr = D3D12SerializeRootSignature(&descriptionRootSignature, D3D_ROOT_SIGNATURE_VERSION_1, &signatureBlob, &errorBlob);
    if (FAILED(hr)) {
#ifdef _DEBUG
      DebugUIManager::GetInstance()->AddLog(static_cast<char*>(errorBlob->GetBufferPointer()), DebugUIManager::LogType::Error);
#endif
      assert(false);
    }

    hr = m_dx12_->GetDevice()->CreateRootSignature(0, signatureBlob->GetBufferPointer(), signatureBlob->GetBufferSize(), IID_PPV_ARGS(RS_.GetAddressOf()));
    signatureBlob->GetBufferSize(), IID_PPV_ARGS(RS_.GetAddressOf());
    assert(SUCCEEDED(hr));
  }

  void GPUParticle::CreatePSO()
  {
    HRESULT hr;

    // InputLayout
    D3D12_INPUT_ELEMENT_DESC inputElementDescs[3] = {};
    inputElementDescs[0].SemanticName = "POSITION";
    inputElementDescs[0].SemanticIndex = 0;
    inputElementDescs[0].Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
    inputElementDescs[0].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;

    inputElementDescs[1].SemanticName = "TEXCOORD";
    inputElementDescs[1].SemanticIndex = 0;
    inputElementDescs[1].Format = DXGI_FORMAT_R32G32_FLOAT;
    inputElementDescs[1].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;

    inputElementDescs[2].SemanticName = "COLOR";
    inputElementDescs[2].SemanticIndex = 0;
    inputElementDescs[2].Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
    inputElementDescs[2].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;

    D3D12_INPUT_LAYOUT_DESC inputLayoutDesc{};
    inputLayoutDesc.pInputElementDescs = inputElementDescs;
    inputLayoutDesc.NumElements = _countof(inputElementDescs);

    // BlendState
    D3D12_BLEND_DESC blendDesc{};
    blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
    blendDesc.RenderTarget[0].BlendEnable = TRUE;
    blendDesc.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
    blendDesc.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
    blendDesc.RenderTarget[0].DestBlend = D3D12_BLEND_ONE;
    blendDesc.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
    blendDesc.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
    blendDesc.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ZERO;

    // RasterizerState
    D3D12_RASTERIZER_DESC rasterizerDesc{};
    // 三角形の中を塗りつぶす
    rasterizerDesc.FillMode = D3D12_FILL_MODE_SOLID;
    // 裏面を表示しない
    rasterizerDesc.CullMode = D3D12_CULL_MODE_BACK;

    // shader のコンパイル
    Microsoft::WRL::ComPtr<IDxcBlob> vertexShaderBlob = m_dx12_->CompileShader(EnginePaths::ShaderPath(L"GPUParticle.VS.hlsl"), L"vs_6_0");
    assert(vertexShaderBlob != nullptr);

    Microsoft::WRL::ComPtr<IDxcBlob> pixelShaderBlob = m_dx12_->CompileShader(EnginePaths::ShaderPath(L"GPUParticle.PS.hlsl"), L"ps_6_0");
    assert(pixelShaderBlob != nullptr);

    // DepthStencilState
    D3D12_DEPTH_STENCIL_DESC depthStencilDesc{};
    // depth の機能を有効化にする
    depthStencilDesc.DepthEnable = true;
    // 書き込みします
    depthStencilDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
    // 深度の比較方法
    depthStencilDesc.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;

    // PSO の生成
    D3D12_GRAPHICS_PIPELINE_STATE_DESC graphicsPipelineStateDesc{};
    graphicsPipelineStateDesc.pRootSignature = RS_.Get();
    graphicsPipelineStateDesc.InputLayout = inputLayoutDesc;
    graphicsPipelineStateDesc.VS = { vertexShaderBlob->GetBufferPointer(), vertexShaderBlob->GetBufferSize() };
    graphicsPipelineStateDesc.PS = { pixelShaderBlob->GetBufferPointer(), pixelShaderBlob->GetBufferSize() };
    graphicsPipelineStateDesc.BlendState = blendDesc;
    graphicsPipelineStateDesc.RasterizerState = rasterizerDesc;
    // 書き込む RTV の情報
    graphicsPipelineStateDesc.NumRenderTargets = 1;
    graphicsPipelineStateDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
    // 利用するトポロジ（形状）のタイプ。三角形
    graphicsPipelineStateDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    // どのように画面に色を打ち込むかの設定
    graphicsPipelineStateDesc.SampleDesc.Count = 1;
    graphicsPipelineStateDesc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;
    // DepthStencil の設定
    graphicsPipelineStateDesc.DepthStencilState = depthStencilDesc;
    graphicsPipelineStateDesc.DSVFormat = DXGI_FORMAT_D32_FLOAT;

    // 実際に生成
    hr = m_dx12_->GetDevice()->CreateGraphicsPipelineState(&graphicsPipelineStateDesc, IID_PPV_ARGS(&PSO_));
    assert(SUCCEEDED(hr));
  }

  void GPUParticle::CreateInitComputeRS()
  {
    HRESULT hr;
    // rootSignature の生成
    D3D12_ROOT_SIGNATURE_DESC descriptionRootSignature{};
    descriptionRootSignature.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

    // DescriptorRange の設定。
    D3D12_DESCRIPTOR_RANGE descriptorRangeForParticle[1] = {}; // Particle
    descriptorRangeForParticle[0].BaseShaderRegister = 0; // レジスタ番号
    descriptorRangeForParticle[0].NumDescriptors = 1; // ディスクリプタ数
    descriptorRangeForParticle[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV; // UAV を使う
    descriptorRangeForParticle[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND; // Offset を自動計算

    D3D12_DESCRIPTOR_RANGE descriptorRangeForFreeListIndex[1] = {}; // FreeListIndex
    descriptorRangeForFreeListIndex[0].BaseShaderRegister = 1; // レジスタ番号
    descriptorRangeForFreeListIndex[0].NumDescriptors = 1; // ディスクリプタ数
    descriptorRangeForFreeListIndex[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV; // UAV を使う
    descriptorRangeForFreeListIndex[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND; // Offset を自動計算

    D3D12_DESCRIPTOR_RANGE descriptorRangeForFreeList[1] = {}; // FreeList
    descriptorRangeForFreeList[0].BaseShaderRegister = 2; // レジスタ番号
    descriptorRangeForFreeList[0].NumDescriptors = 1; // ディスクリプタ数
    descriptorRangeForFreeList[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV; // UAV を使う
    descriptorRangeForFreeList[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND; // Offset を自動計算

    // RootParameter の設定。複数設定できるので配列
    D3D12_ROOT_PARAMETER rootParameters[3] = {};
    // Particle
    rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE; // ディスクリプタテーブルを使う
    rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL; // 全てのシェーダーで使う
    rootParameters[0].DescriptorTable.pDescriptorRanges = descriptorRangeForParticle; // ディスクリプタレンジを設定
    rootParameters[0].DescriptorTable.NumDescriptorRanges = _countof(descriptorRangeForParticle); // レンジの数

    // FreeListIndex
    rootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE; // ディスクリプタテーブルを使う
    rootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL; // 全てのシェーダーで使う
    rootParameters[1].DescriptorTable.pDescriptorRanges = descriptorRangeForFreeListIndex; // ディスクリプタレンジを設定
    rootParameters[1].DescriptorTable.NumDescriptorRanges = _countof(descriptorRangeForFreeListIndex); // レンジの数

    // FreeList
    rootParameters[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE; // ディスクリプタテーブルを使う
    rootParameters[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL; // 全てのシェーダーで使う
    rootParameters[2].DescriptorTable.pDescriptorRanges = descriptorRangeForFreeList; // ディスクリプタレンジを設定
    rootParameters[2].DescriptorTable.NumDescriptorRanges = _countof(descriptorRangeForFreeList); // レンジの数

    descriptionRootSignature.pParameters = rootParameters;
    descriptionRootSignature.NumParameters = _countof(rootParameters);

    Microsoft::WRL::ComPtr<ID3DBlob> signatureBlob = nullptr;
    Microsoft::WRL::ComPtr<ID3DBlob> errorBlob = nullptr;
    hr = D3D12SerializeRootSignature(&descriptionRootSignature, D3D_ROOT_SIGNATURE_VERSION_1, &signatureBlob, &errorBlob);
    if (FAILED(hr)) {
#ifdef _DEBUG
      DebugUIManager::GetInstance()->AddLog(reinterpret_cast<char*>(errorBlob->GetBufferPointer()), DebugUIManager::LogType::Error);
#endif
      assert(false);
    }
    hr = m_dx12_->GetDevice()->CreateRootSignature(0, signatureBlob->GetBufferPointer(), signatureBlob->GetBufferSize(), IID_PPV_ARGS(initComputeRS_.GetAddressOf()));
  }

  void GPUParticle::CreateEmitParticleComputeRS()
  {
    HRESULT hr;
    // rootSignature の生成
    D3D12_ROOT_SIGNATURE_DESC descriptionRootSignature{};
    descriptionRootSignature.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

    // DescriptorRange の設定。
    D3D12_DESCRIPTOR_RANGE descriptorRange_Particle[1] = {}; // Particle
    descriptorRange_Particle[0].BaseShaderRegister = 0; // レジスタ番号
    descriptorRange_Particle[0].NumDescriptors = 1; // ディスクリプタ数
    descriptorRange_Particle[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV; // UAV を使う
    descriptorRange_Particle[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND; // Offset を自動計算

    D3D12_DESCRIPTOR_RANGE descriptorRange_FreeListIndex[1] = {}; // FreeListIndex
    descriptorRange_FreeListIndex[0].BaseShaderRegister = 1; // レジスタ番号
    descriptorRange_FreeListIndex[0].NumDescriptors = 1; // ディスクリプタ数
    descriptorRange_FreeListIndex[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV; // UAV を使う
    descriptorRange_FreeListIndex[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND; // Offset を自動計算

    D3D12_DESCRIPTOR_RANGE descriptorRange_FreeList[1] = {}; // FreeList
    descriptorRange_FreeList[0].BaseShaderRegister = 2; // レジスタ番号
    descriptorRange_FreeList[0].NumDescriptors = 1; // ディスクリプタ数
    descriptorRange_FreeList[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV; // UAV を使う
    descriptorRange_FreeList[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND; // Offset を自動計算

    D3D12_DESCRIPTOR_RANGE descriptorRange_Emitter[1] = {}; // Emitter
    descriptorRange_Emitter[0].BaseShaderRegister = 0; // レジスタ番号
    descriptorRange_Emitter[0].NumDescriptors = 1; // ディスクリプタ数
    descriptorRange_Emitter[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV; // SRV を使う
    descriptorRange_Emitter[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND; // Offset を自動計算

    // RootParameter の設定。複数設定できるので配列
    D3D12_ROOT_PARAMETER rootParameters[5] = {};
    // Particle
    rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE; // ディスクリプタテーブルを使う
    rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL; // 全てのシェーダーで使う
    rootParameters[0].DescriptorTable.pDescriptorRanges = descriptorRange_Particle; // ディスクリプタレンジを設定
    rootParameters[0].DescriptorTable.NumDescriptorRanges = _countof(descriptorRange_Particle); // レンジの数

    // EmitterSphere
    rootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE; // ディスクリプタテーブルを使う
    rootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL; // 全てのシェーダーで使う
    rootParameters[1].DescriptorTable.pDescriptorRanges = descriptorRange_Emitter; // ディスクリプタレンジを設定
    rootParameters[1].DescriptorTable.NumDescriptorRanges = _countof(descriptorRange_Emitter); // レンジの数

    // PerFrame
    rootParameters[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV; // 定数バッファビューを使う
    rootParameters[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL; // 全てのシェーダーで使う
    rootParameters[2].Descriptor.ShaderRegister = 0; // レジスタ番号とバインド

    // FreeListIndex
    rootParameters[3].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE; // ディスクリプタテーブルを使う
    rootParameters[3].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL; // 全てのシェーダーで使う
    rootParameters[3].DescriptorTable.pDescriptorRanges = descriptorRange_FreeListIndex; // ディスクリプタレンジを設定
    rootParameters[3].DescriptorTable.NumDescriptorRanges = _countof(descriptorRange_FreeListIndex); // レンジの数

    // FreeList
    rootParameters[4].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE; // ディスクリプタテーブルを使う
    rootParameters[4].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL; // 全てのシェーダーで使う
    rootParameters[4].DescriptorTable.pDescriptorRanges = descriptorRange_FreeList; // ディスクリプタレンジを設定
    rootParameters[4].DescriptorTable.NumDescriptorRanges = _countof(descriptorRange_FreeList); // レンジの数

    descriptionRootSignature.pParameters = rootParameters;
    descriptionRootSignature.NumParameters = _countof(rootParameters);

    Microsoft::WRL::ComPtr<ID3DBlob> signatureBlob = nullptr;
    Microsoft::WRL::ComPtr<ID3DBlob> errorBlob = nullptr;
    hr = D3D12SerializeRootSignature(&descriptionRootSignature, D3D_ROOT_SIGNATURE_VERSION_1, &signatureBlob, &errorBlob);
    if (FAILED(hr)) {
#ifdef _DEBUG
      DebugUIManager::GetInstance()->AddLog(static_cast<char*>(errorBlob->GetBufferPointer()), DebugUIManager::LogType::Error);
#endif
      assert(false);
    }
    hr = m_dx12_->GetDevice()->CreateRootSignature(0, signatureBlob->GetBufferPointer(), signatureBlob->GetBufferSize(), IID_PPV_ARGS(emitParticleRS_.GetAddressOf()));
  }

  void GPUParticle::CreateComputeShaderPSO(Microsoft::WRL::ComPtr<ID3D12RootSignature>& RS, Microsoft::WRL::ComPtr<ID3D12PipelineState>& PSO, const std::wstring& shaderName)
  {
    Microsoft::WRL::ComPtr<IDxcBlob> csBlob = m_dx12_->CompileShader(EnginePaths::ShaderPath(shaderName), L"cs_6_0");

    D3D12_COMPUTE_PIPELINE_STATE_DESC computePipelineStateDesc{};
    computePipelineStateDesc.pRootSignature = RS.Get();
    computePipelineStateDesc.CS = { .pShaderBytecode = csBlob->GetBufferPointer(), .BytecodeLength = csBlob->GetBufferSize() };

    HRESULT hr = m_dx12_->GetDevice()->CreateComputePipelineState(&computePipelineStateDesc, IID_PPV_ARGS(&PSO));
    assert(SUCCEEDED(hr));
  }

  void GPUParticle::CreateVertexData()
  {
    modelData_.vertices.push_back({ .position = {.x = 1.0f, .y = 1.0f, .z = 0.0f, .w = 1.0f}, .texcoord = {.x = 0.0f, .y = 0.0f}, .normal = {.x = 0.0f, .y = 0.0f, .z = 1.0f} });
    modelData_.vertices.push_back({ .position = {.x = -1.0f, .y = 1.0f, .z = 0.0f, .w = 1.0f}, .texcoord = {.x = 1.0f, .y = 0.0f}, .normal = {.x = 0.0f, .y = 0.0f, .z = 1.0f} });
    modelData_.vertices.push_back({ .position = {.x = 1.0f, .y = -1.0f, .z = 0.0f, .w = 1.0f}, .texcoord = {.x = 0.0f, .y = 1.0f}, .normal = {.x = 0.0f, .y = 0.0f, .z = 1.0f} });
    modelData_.vertices.push_back({ .position = {.x = 1.0f, .y = -1.0f, .z = 0.0f, .w = 1.0f}, .texcoord = {.x = 0.0f, .y = 1.0f}, .normal = {.x = 0.0f, .y = 0.0f, .z = 1.0f} });
    modelData_.vertices.push_back({ .position = {.x = -1.0f, .y = 1.0f, .z = 0.0f, .w = 1.0f}, .texcoord = {.x = 1.0f, .y = 0.0f}, .normal = {.x = 0.0f, .y = 0.0f, .z = 1.0f} });
    modelData_.vertices.push_back({ .position = {.x = -1.0f, .y = -1.0f, .z = 0.0f, .w = 1.0f}, .texcoord = {.x = 1.0f, .y = 1.0f}, .normal = {.x = 0.0f, .y = 0.0f, .z = 1.0f} });

    // 頂点リソース生成
    vertexResource_ = m_dx12_->MakeBufferResource(sizeof(VertexData) * modelData_.vertices.size());

    // VertexBufferView の作成
    vertexBufferView_.BufferLocation = vertexResource_->GetGPUVirtualAddress(); // リソースの先頭のアドレスから使う
    vertexBufferView_.SizeInBytes = static_cast<UINT>(sizeof(VertexData) * modelData_.vertices.size());	// 使用するリソースのサイズは頂点のサイズ
    vertexBufferView_.StrideInBytes = sizeof(VertexData); // 1頂点あたりのサイズ

    // 頂点リソースをマップ
    [[maybe_unused]] HRESULT hr = vertexResource_->Map(0, nullptr, reinterpret_cast<void**>(&vertexData_));
    // 頂点データをリソースにコピー
    std::memcpy(vertexData_, modelData_.vertices.data(), sizeof(VertexData) * modelData_.vertices.size());
  }

  void GPUParticle::CreatePerViewData()
  {
    m_dx12_->CreateBufferResource(perViewResource_, sizeof(PerView));

    // map
    perViewResource_->Map(0, nullptr, reinterpret_cast<void**>(&perViewData_));

    // データの設定
    perViewData_->viewProjection = Mat4x4::MakeIdentity();
    perViewData_->billboardMatrix = Mat4x4::MakeIdentity();
  }

  void GPUParticle::CreatePerFrameData()
  {
    // PerFrame のリソースを生成
    m_dx12_->CreateBufferResource(perFrameResource_, sizeof(PerFrame));
    // PerFrame のデータをマップ
    perFrameResource_->Map(0, nullptr, reinterpret_cast<void**>(&perFrameData_));
    // PerFrame のデータを初期化
    perFrameData_->time = 0.0f;
    perFrameData_->deltaTime = 0.0f;
  }

  void GPUParticle::CreateEmitterData()
  {

    // エミッターリソースの生成
    m_dx12_->CreateBufferResource(emitterResource_, sizeof(EmitterData) * kNumMaxEmitter);

    // エミッターリソースの SRV を作成
    emitterSrvIndex_ = m_srvManager_->Allocate();
    m_srvManager_->CreateSRVForStructuredBuffer(emitterSrvIndex_, emitterResource_.Get(), kNumMaxEmitter, sizeof(EmitterData));

    // エミッター配列の初期化
    activeEmitters_.clear();

    // GPU 側の初期化
    EmitterData* gpuEmitters = nullptr;
    emitterResource_->Map(0, nullptr, reinterpret_cast<void**>(&gpuEmitters));
    ZeroMemory(gpuEmitters, sizeof(EmitterData) * kNumMaxEmitter);
    emitterResource_->Unmap(0, nullptr);
  }

  void GPUParticle::CreateParticleResource()
  {
    // ParticleCS のリソースを生成
    m_dx12_->CreateResourceForUAV(particleResource_, sizeof(ParticleCS) * kNumMaxParticle);

    // ParticleCS の UAV を生成
    particleUavIndex_ = m_srvManager_->Allocate();
    m_srvManager_->CreateUAV(particleUavIndex_, particleResource_.Get(), kNumMaxParticle, sizeof(ParticleCS));

    // ParticleCS の SRV を生成
    particleSrvIndex_ = m_srvManager_->Allocate();
    m_srvManager_->CreateSRVForStructuredBuffer(particleSrvIndex_, particleResource_.Get(), kNumMaxParticle, sizeof(ParticleCS));
  }

  void GPUParticle::CreateFreeListResource()
  {
    // FreeListIndex のリソースを生成
    m_dx12_->CreateResourceForUAV(freeListIndexResource_, sizeof(int32_t));

    // FreeListIndex の UAV を生成
    freeListIndexUavIndex_ = m_srvManager_->Allocate();
    m_srvManager_->CreateUAV(freeListIndexUavIndex_, freeListIndexResource_.Get(), 1, sizeof(int32_t));


    // FreeList のリソースを生成
    m_dx12_->CreateResourceForUAV(freeListResource_, sizeof(uint32_t) * kNumMaxParticle);

    // FreeList の UAV を生成
    freeListUavIndex_ = m_srvManager_->Allocate();
    m_srvManager_->CreateUAV(freeListUavIndex_, freeListResource_.Get(), kNumMaxParticle, sizeof(uint32_t));
  }

#ifdef _DEBUG
  void GPUParticle::CreateFreeListReadbackResource()
  {
    D3D12_RESOURCE_DESC bufferDesc = {};
    bufferDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    bufferDesc.Width = sizeof(int32_t);
    bufferDesc.Height = 1;
    bufferDesc.DepthOrArraySize = 1;
    bufferDesc.MipLevels = 1;
    bufferDesc.SampleDesc.Count = 1;
    bufferDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    bufferDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

    D3D12_HEAP_PROPERTIES heapProps = {};
    heapProps.Type = D3D12_HEAP_TYPE_READBACK;

    HRESULT hr = m_dx12_->GetDevice()->CreateCommittedResource(
      &heapProps,
      D3D12_HEAP_FLAG_NONE,
      &bufferDesc,
      D3D12_RESOURCE_STATE_COPY_DEST,
      nullptr,
      IID_PPV_ARGS(&freeListIndexReadbackResource_));
    assert(SUCCEEDED(hr));
  }

  void GPUParticle::ReadbackActiveParticleCount()
  {
    // 間引き制御: kReadbackInterval フレームに1回だけ実行
    readbackFrameCounter_++;
    if (readbackFrameCounter_ < kReadbackInterval) {
      return;
    }
    readbackFrameCounter_ = 0;

    ID3D12GraphicsCommandList* commandList = m_dx12_->GetCommandList();

    // freeListIndexResource_: UAV → COPY_SOURCE
    m_dx12_->TransitionResourceState(
      D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
      D3D12_RESOURCE_STATE_COPY_SOURCE,
      freeListIndexResource_.Get());

    // UAV バッファ → Readback バッファにコピー（4バイトのみ）
    commandList->CopyBufferRegion(
      freeListIndexReadbackResource_.Get(), 0,
      freeListIndexResource_.Get(), 0,
      sizeof(int32_t));

    // freeListIndexResource_: COPY_SOURCE → UAV
    m_dx12_->TransitionResourceState(
      D3D12_RESOURCE_STATE_COPY_SOURCE,
      D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
      freeListIndexResource_.Get());

    // Readback バッファから CPU 読み取り（前回コピー分の結果、表示用途では問題なし）
    int32_t* mappedData = nullptr;
    D3D12_RANGE readRange = { 0, sizeof(int32_t) };
    D3D12_RANGE writeRange = { 0, 0 };
    HRESULT hr = freeListIndexReadbackResource_->Map(0, &readRange, reinterpret_cast<void**>(&mappedData));
    if (SUCCEEDED(hr) && mappedData) {
      int32_t freeListIndex = *mappedData;
      int32_t active = static_cast<int32_t>(kNumMaxParticle) - 1 - freeListIndex;
      activeParticleCount_ = static_cast<uint32_t>((std::max)(0, active));
      freeListIndexReadbackResource_->Unmap(0, &writeRange);
    }
  }
#endif

  void GPUParticle::CreateIntegrateAllComputeRS()
  {
    HRESULT hr;
    D3D12_ROOT_SIGNATURE_DESC descriptionRootSignature{};
    descriptionRootSignature.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

    // DescriptorRange: UAV
    D3D12_DESCRIPTOR_RANGE rangeParticle[1] = {};
    rangeParticle[0].BaseShaderRegister = 0; // u0
    rangeParticle[0].NumDescriptors = 1;
    rangeParticle[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
    rangeParticle[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

    D3D12_DESCRIPTOR_RANGE rangeFreeListIndex[1] = {};
    rangeFreeListIndex[0].BaseShaderRegister = 1; // u1
    rangeFreeListIndex[0].NumDescriptors = 1;
    rangeFreeListIndex[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
    rangeFreeListIndex[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

    D3D12_DESCRIPTOR_RANGE rangeFreeList[1] = {};
    rangeFreeList[0].BaseShaderRegister = 2; // u2
    rangeFreeList[0].NumDescriptors = 1;
    rangeFreeList[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
    rangeFreeList[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

    // DescriptorRange: SRV
    D3D12_DESCRIPTOR_RANGE rangeForceFields[1] = {};
    rangeForceFields[0].BaseShaderRegister = 0; // t0
    rangeForceFields[0].NumDescriptors = 1;
    rangeForceFields[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    rangeForceFields[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

    D3D12_DESCRIPTOR_RANGE rangeDepthBuffer[1] = {};
    rangeDepthBuffer[0].BaseShaderRegister = 1; // t1
    rangeDepthBuffer[0].NumDescriptors = 1;
    rangeDepthBuffer[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    rangeDepthBuffer[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

    // RootParameter: 3 UAV + 2 SRV + 2 CBV = 7
    D3D12_ROOT_PARAMETER rootParameters[7] = {};

    // [0] Particles UAV (u0)
    rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    rootParameters[0].DescriptorTable.pDescriptorRanges = rangeParticle;
    rootParameters[0].DescriptorTable.NumDescriptorRanges = 1;

    // [1] FreeListIndex UAV (u1)
    rootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    rootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    rootParameters[1].DescriptorTable.pDescriptorRanges = rangeFreeListIndex;
    rootParameters[1].DescriptorTable.NumDescriptorRanges = 1;

    // [2] FreeList UAV (u2)
    rootParameters[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    rootParameters[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    rootParameters[2].DescriptorTable.pDescriptorRanges = rangeFreeList;
    rootParameters[2].DescriptorTable.NumDescriptorRanges = 1;

    // [3] ForceFields SRV (t0)
    rootParameters[3].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    rootParameters[3].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    rootParameters[3].DescriptorTable.pDescriptorRanges = rangeForceFields;
    rootParameters[3].DescriptorTable.NumDescriptorRanges = 1;

    // [4] DepthBuffer SRV (t1) — 深度バッファ衝突用
    rootParameters[4].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    rootParameters[4].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    rootParameters[4].DescriptorTable.pDescriptorRanges = rangeDepthBuffer;
    rootParameters[4].DescriptorTable.NumDescriptorRanges = 1;

    // [5] PerFrame CBV (b0)
    rootParameters[5].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    rootParameters[5].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    rootParameters[5].Descriptor.ShaderRegister = 0;

    // [6] PhysicsParams CBV (b1)
    rootParameters[6].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    rootParameters[6].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    rootParameters[6].Descriptor.ShaderRegister = 1;

    // Static Sampler: Point/Clamp（深度テクスチャサンプリング用）
    D3D12_STATIC_SAMPLER_DESC staticSampler{};
    staticSampler.Filter = D3D12_FILTER_MIN_MAG_MIP_POINT;
    staticSampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    staticSampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    staticSampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    staticSampler.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
    staticSampler.MaxLOD = D3D12_FLOAT32_MAX;
    staticSampler.ShaderRegister = 0; // s0
    staticSampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

    descriptionRootSignature.pParameters = rootParameters;
    descriptionRootSignature.NumParameters = _countof(rootParameters);
    descriptionRootSignature.pStaticSamplers = &staticSampler;
    descriptionRootSignature.NumStaticSamplers = 1;

    Microsoft::WRL::ComPtr<ID3DBlob> signatureBlob = nullptr;
    Microsoft::WRL::ComPtr<ID3DBlob> errorBlob = nullptr;
    hr = D3D12SerializeRootSignature(&descriptionRootSignature, D3D_ROOT_SIGNATURE_VERSION_1, &signatureBlob, &errorBlob);
    if (FAILED(hr)) {
#ifdef _DEBUG
      DebugUIManager::GetInstance()->AddLog(reinterpret_cast<char*>(errorBlob->GetBufferPointer()), DebugUIManager::LogType::Error);
#endif
      assert(false);
    }
    hr = m_dx12_->GetDevice()->CreateRootSignature(0, signatureBlob->GetBufferPointer(), signatureBlob->GetBufferSize(), IID_PPV_ARGS(integrateAllRS_.GetAddressOf()));
    assert(SUCCEEDED(hr));
  }

  void GPUParticle::CreateForceFieldResource()
  {
    // フォースフィールドリソースの生成（UPLOAD ヒープ — CPU から毎フレーム書き換え可能）
    m_dx12_->CreateBufferResource(forceFieldResource_, sizeof(ForceFieldData) * kMaxForceFields);

    // SRV を作成
    forceFieldSrvIndex_ = m_srvManager_->Allocate();
    m_srvManager_->CreateSRVForStructuredBuffer(forceFieldSrvIndex_, forceFieldResource_.Get(), kMaxForceFields, sizeof(ForceFieldData));

    // フォースフィールドリストの初期化
    forceFields_.clear();

    // GPU 側のゼロ初期化
    ForceFieldData* gpuForceFields = nullptr;
    forceFieldResource_->Map(0, nullptr, reinterpret_cast<void**>(&gpuForceFields));
    ZeroMemory(gpuForceFields, sizeof(ForceFieldData) * kMaxForceFields);
    forceFieldResource_->Unmap(0, nullptr);
  }

  void GPUParticle::CreatePhysicsParamsResource()
  {
    // 物理パラメータの定数バッファを生成
    m_dx12_->CreateBufferResource(physicsParamsResource_, sizeof(PhysicsParamsData));

    // データをマップ
    physicsParamsResource_->Map(0, nullptr, reinterpret_cast<void**>(&physicsParamsData_));

    // デフォルト値の設定
    physicsParamsData_->damping = 0.99f;
    physicsParamsData_->collisionRestitution = 0.5f;
    physicsParamsData_->particleRadius = 0.05f;
    physicsParamsData_->depthBias = 5.0f;  ///< ワールド空間の最大衝突距離（メートル単位）

    physicsParamsData_->gridOrigin = { .x = -50.0f, .y = -50.0f, .z = -50.0f };
    physicsParamsData_->gridCellSize = 1.5625f; // 100.0 / 64.0
    physicsParamsData_->gridDimX = 64;
    physicsParamsData_->gridDimY = 64;
    physicsParamsData_->gridDimZ = 64;
    physicsParamsData_->activeForceFieldCount = 0;

    physicsParamsData_->invViewProj = Mat4x4::MakeIdentity();
    physicsParamsData_->screenWidth = 1280.0f;
    physicsParamsData_->screenHeight = 720.0f;
    physicsParamsData_->noiseTime = 0.0f;
    physicsParamsData_->noiseScale = 3.0f;
    physicsParamsData_->noiseStrength = 0.05f;
    physicsParamsData_->pad[0] = 0.0f;
    physicsParamsData_->pad[1] = 0.0f;
    physicsParamsData_->pad[2] = 0.0f;

    // 深度バッファ衝突用
    physicsParamsData_->viewProj = Mat4x4::MakeIdentity();
    physicsParamsData_->cameraPos = { .x = 0.0f, .y = 0.0f, .z = 0.0f };
    physicsParamsData_->pad2 = 0.0f;
  }

  void GPUParticle::CreateDepthSRV()
  {
    // 既存の SRV を解放（リサイズ時の再作成対応）
    if (depthSrvIndex_ != UINT32_MAX) {
      m_srvManager_->Free(depthSrvIndex_);
      depthSrvIndex_ = UINT32_MAX;
    }
    depthSrvIndex_ = m_srvManager_->Allocate();
    m_srvManager_->CreateSRVForTexture2D(
      depthSrvIndex_,
      m_dx12_->GetDepthStencilResource(),
      DXGI_FORMAT_R32_FLOAT,
      1);
  }

  void GPUParticle::OnResize()
  {
    // 深度バッファが再作成されるため SRV を再構築
    CreateDepthSRV();
  }

  void GPUParticle::SyncForceFieldData()
  {
    // GPU 側のフォースフィールドバッファにマップ
    ForceFieldData* gpuForceFields = nullptr;
    forceFieldResource_->Map(0, nullptr, reinterpret_cast<void**>(&gpuForceFields));

    // フォースフィールドデータをコピー
    size_t count = min(forceFields_.size(), static_cast<size_t>(kMaxForceFields));
    if (count > 0) {
      std::memcpy(gpuForceFields, forceFields_.data(), sizeof(ForceFieldData) * count);
    }

    // 残りをゼロクリア
    if (count < kMaxForceFields) {
      ZeroMemory(&gpuForceFields[count], sizeof(ForceFieldData) * (kMaxForceFields - count));
    }

    forceFieldResource_->Unmap(0, nullptr);
  }

  void GPUParticle::UpdatePhysicsParams()
  {
    // アクティブなフォースフィールド数を更新
    physicsParamsData_->activeForceFieldCount = static_cast<uint32_t>(
      min(forceFields_.size(), static_cast<size_t>(kMaxForceFields)));

    // 時間を更新（Curl Noise 用）
    physicsParamsData_->noiseTime = FrameTimer::GetInstance()->GetGameTime();

    // --- カメラ行列の計算（深度衝突用） ---
    Matrix4x4 cameraMatrix = Mat4x4::MakeAffine(
      { .x = 1.0f, .y = 1.0f, .z = 1.0f },
      m_camera_->GetRotate(), m_camera_->GetTranslate());
    Vector3 camPos = m_camera_->GetTranslate();

#ifdef _DEBUG
    if (isDebug_) {
      cameraMatrix = Mat4x4::MakeAffine(
        { .x = 1.0f, .y = 1.0f, .z = 1.0f },
        DebugCamera::GetInstance()->GetRotate(),
        DebugCamera::GetInstance()->GetTranslate());
      camPos = DebugCamera::GetInstance()->GetTranslate();
    }
#endif

    Matrix4x4 vp = Mat4x4::Multiply(
      Mat4x4::Inverse(cameraMatrix), m_camera_->GetProjectionMatrix());

    physicsParamsData_->viewProj = vp;
    physicsParamsData_->invViewProj = Mat4x4::Inverse(vp);
    physicsParamsData_->cameraPos = camPos;

    // スクリーンサイズ（リサイズ対応）
    physicsParamsData_->screenWidth = static_cast<float>(WinApp::clientWidth);
    physicsParamsData_->screenHeight = static_cast<float>(WinApp::clientHeight);
  }

  int32_t GPUParticle::AddForceField(const ForceFieldData& field)
  {
    if (forceFields_.size() >= kMaxForceFields) {
      return -1;
    }
    forceFields_.push_back(field);
    return static_cast<int32_t>(forceFields_.size() - 1);
  }

  void GPUParticle::UpdateForceField(uint32_t index, const ForceFieldData& field)
  {
    if (index < forceFields_.size()) {
      forceFields_[index] = field;
    }
  }

  void GPUParticle::RemoveForceField(uint32_t index)
  {
    if (index < forceFields_.size()) {
      forceFields_.erase(forceFields_.begin() + index);
    }
  }

  void GPUParticle::ClearForceFields()
  {
    forceFields_.clear();
  }

} // namespace Tako

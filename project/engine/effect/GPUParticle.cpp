#include "GPUParticle.h"
#include "GPUParticleEmitter.h"
#include "SphereEmitter.h"
#include "BoxEmitter.h"
#include "TriangleEmitter.h"
#include "DX12Basic.h"
#include "Camera.h"
#include "TextureManager.h"
#include "Logger.h"
#include "FrameTimer.h"
#include "DebugCamera.h"

#ifdef _DEBUG
#include "ImGuiManager.h"
#endif // DEBUG

#include <numbers>

GPUParticle* GPUParticle::instance_ = nullptr;

const uint32_t GPUParticle::kNumMaxParticle = 20480;

const uint32_t GPUParticle::kNumMaxEmitter = 80;

GPUParticle* GPUParticle::GetInstance()
{
  if (instance_ == nullptr)
  {
    instance_ = new GPUParticle();
  }
  return instance_;
}

void GPUParticle::Initialize(DX12Basic* dx12, Camera* camera)
{
  m_dx12_ = dx12;

  // カメラの設定
  m_camera_ = camera;

  m_srvManager_ = SrvManager::GetInstance();

  modelData_.textureData.texturePath = "circle.png";
  modelData_.textureData.textureIndex = TextureManager::GetInstance()->GetSRVIndex(modelData_.textureData.texturePath);

  isInited_ = false;

  isDebug_ = false;

  // ルートシグネチャの生成
  CreateRS();
  CreateInitComputeRS();
  CreateEmitParticleComputeRS();
  CreateUpdateParticleComputeRS();

  // PSOの生成
  CreatePSO();
  CreateComputeShaderPSO(initComputeRS_, initComputePSO_, L"InitParticle.CS.hlsl");
  CreateComputeShaderPSO(emitParticleRS_, emitParticlePSO_, L"EmitParticle.CS.hlsl");
  CreateComputeShaderPSO(updateParticleRS_, updateParticlePSO_, L"UpdateParticle.CS.hlsl");

  // PerViewデータの生成
  CreatePerViewData();

  // PerFrameデータの生成
  CreatePerFrameData();

  // パーティクルリソースの生成
  CreateParticleResource();

  // EmitterSphereデータの生成
  CreateEmitterData();

  // 頂点データの生成
  CreateVertexData();

  // FreeCounterリソースの生成
  CreateFreeListResource();
}

void GPUParticle::Update()
{
  // PerFrameの更新
  UpdatePerFrame();

  // エミッターの更新
  UpdateEmitter();

  // PerViewの更新
  UpdatePerView();
}

void GPUParticle::Draw()
{
  ID3D12GraphicsCommandList* commandList = m_dx12_->GetCommandList();

  /// ================================== ///
  ///            ComputerShader          ///
  /// ================================== ///

  //--------------------------------------初期化--------------------------------------//
  if (!isInited_)
  {
    // ルートシグネチャの設定
    commandList->SetComputeRootSignature(initComputeRS_.Get());

    // パイプラインステートの設定
    commandList->SetPipelineState(initComputePSO_.Get());

    // ParticleDataのUAVの設定
    m_srvManager_->SetComputeRootDescriptorTable(0, particleUavIndex_);

    // FreeListIndexのUAVの設定
    m_srvManager_->SetComputeRootDescriptorTable(1, freeListIndexUavIndex_);

    // FreeListのUAVの設定
    m_srvManager_->SetComputeRootDescriptorTable(2, freeListUavIndex_);

    // ディスパッチ
    commandList->Dispatch(1024, 1, 1);

    isInited_ = true;
  }

  // リソースバリアの設定（UAV同期）
  m_dx12_->SetUAVBarrier(particleResource_.Get());
  m_dx12_->SetUAVBarrier(freeListIndexResource_.Get());
  m_dx12_->SetUAVBarrier(freeListResource_.Get());

  //--------------------------------------射出--------------------------------------//
    // アクティブなエミッターがある場合のみ実行
  if (!activeEmitters_.empty())
  {
    // ルートシグネチャの設定
    commandList->SetComputeRootSignature(emitParticleRS_.Get());

    // パイプラインステートの設定
    commandList->SetPipelineState(emitParticlePSO_.Get());

    // ParticleDataのUAVの設定
    m_srvManager_->SetComputeRootDescriptorTable(0, particleUavIndex_);

    // FreeListIndexのUAVの設定
    m_srvManager_->SetComputeRootDescriptorTable(3, freeListIndexUavIndex_);

    // FreeListのUAVの設定
    m_srvManager_->SetComputeRootDescriptorTable(4, freeListUavIndex_);

    // エミッターリストのSRVの設定
    m_srvManager_->SetComputeRootDescriptorTable(1, emitterSrvIndex_);

    // PerFrameの設定
    commandList->SetComputeRootConstantBufferView(2, perFrameResource_->GetGPUVirtualAddress());

    // ディスパッチ（16スレッドごとにグループ化）
    uint32_t threadGroupsX = (static_cast<uint32_t>(activeEmitters_.size()) + 15) / 16;
    commandList->Dispatch(threadGroupsX, 1, 1);
  }

  // リソースバリアの設定（UAV同期）
  m_dx12_->SetUAVBarrier(particleResource_.Get());
  m_dx12_->SetUAVBarrier(freeListIndexResource_.Get());
  m_dx12_->SetUAVBarrier(freeListResource_.Get());

  //--------------------------------------更新--------------------------------------//

  // ルートシグネチャの設定
  commandList->SetComputeRootSignature(updateParticleRS_.Get());

  // パイプラインステートの設定
  commandList->SetPipelineState(updateParticlePSO_.Get());

  // ParticleDataのUAVの設定
  m_srvManager_->SetComputeRootDescriptorTable(0, particleUavIndex_);

  //PerFrameのCBVの設定
  commandList->SetComputeRootConstantBufferView(1, perFrameResource_->GetGPUVirtualAddress());

  // FreeListIndexのUAVの設定
  m_srvManager_->SetComputeRootDescriptorTable(2, freeListIndexUavIndex_);

  // FreeListのUAVの設定
  m_srvManager_->SetComputeRootDescriptorTable(3, freeListUavIndex_);

  // ディスパッチ
  commandList->Dispatch(1024, 1, 1);

  m_dx12_->SetUAVBarrier(particleResource_.Get());

  /// ======================== ///
  ///           描画    　     ///
  /// ======================= ///

    // ルートシグネチャの設定
  commandList->SetGraphicsRootSignature(RS_.Get());

  // パイプラインステートの設定
  commandList->SetPipelineState(PSO_.Get());

  // プリミティブトポロジを設定
  commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

  // VBVを設定
  commandList->IASetVertexBuffers(0, 1, &vertexBufferView_);

  // ParticleDataをSRVに変更
  m_dx12_->TransitionResourceState(D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, particleResource_.Get());

  // ParticleDataのSRVを設定
  m_srvManager_->SetGraphicsRootDescriptorTable(0, particleSrvIndex_);

  // PerViewの設定
  commandList->SetGraphicsRootConstantBufferView(1, perViewResource_->GetGPUVirtualAddress());

  // テクスチャの設定
  m_srvManager_->SetGraphicsRootDescriptorTable(2, modelData_.textureData.textureIndex);

  // 描画（インスタンス描画）
  commandList->DrawInstanced(static_cast<UINT>(modelData_.vertices.size()), kNumMaxParticle, 0, 0);

  // ParticleDataをUAVに戻す
  m_dx12_->TransitionResourceState(D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, particleResource_.Get());
}

void GPUParticle::Finalize()
{
  if (instance_ != nullptr)
  {
    delete instance_;
    instance_ = nullptr;
  }
}

std::shared_ptr<GPUParticleEmitter> GPUParticle::CreateTemporaryEmitterFrom(GPUParticleEmitter* sourceEmitter, float lifeTime)
{
  if (!sourceEmitter) return nullptr;

  // ソースエミッターのクローンを作成
  auto newEmitter = sourceEmitter->Clone();
  if (!newEmitter) return nullptr;

  // 一時エミッター設定
  newEmitter->SetTemporary(true, lifeTime);

  // アクティブリストに追加
  RegisterEmitter(newEmitter);

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
  auto it = std::find(activeEmitters_.begin(), activeEmitters_.end(), emitter);
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

  // GPU側のデータを同期
  SyncEmitterData();
}

void GPUParticle::UpdatePerView()
{

  Matrix4x4 cameraMatrix = Mat4x4::MakeAffine({ .x = 1.0f,.y = 1.0f,.z = 1.0f }, m_camera_->GetRotate(), m_camera_->GetTranslate());

  if (isDebug_)
  {
    cameraMatrix = Mat4x4::MakeAffine({ .x = 1.0f,.y = 1.0f,.z = 1.0f }, DebugCamera::GetInstance()->GetRotate(), DebugCamera::GetInstance()->GetTranslate());
  }

  const Matrix4x4 viewProjectionMatrix = Mat4x4::Multiply(Mat4x4::Inverse(cameraMatrix), m_camera_->GetProjectionMatrix());

  // ビルボード行列の生成
  const Matrix4x4 backToFrontMatrix = Mat4x4::MakeRotateY(std::numbers::pi_v<float>);

  Matrix4x4 billboardMatrix = Mat4x4::Multiply(backToFrontMatrix, cameraMatrix);
  billboardMatrix.m[3][0] = 0.0f;  //平行移動成分はいらない
  billboardMatrix.m[3][1] = 0.0f;
  billboardMatrix.m[3][2] = 0.0f;

  // PerViewの更新
  perViewData_->billboardMatrix = billboardMatrix;
  perViewData_->viewProjection = viewProjectionMatrix;
}

void GPUParticle::UpdatePerFrame()
{
  perFrameData_->time = FrameTimer::GetInstance()->GetGameTime();
  perFrameData_->deltaTime = FrameTimer::GetInstance()->GetDeltaTime();
}

// CPU→GPU同期
void GPUParticle::SyncEmitterData()
{
  // GPU側のエミッターバッファにマップ
  EmitterGPUData* gpuEmitters = nullptr;
  emitterResource_->Map(0, nullptr, reinterpret_cast<void**>(&gpuEmitters));

  // バッファサイズが足りるか確認
  if (activeEmitters_.size() > kNumMaxEmitter) {
    Logger::Log("Warning: Too many active emitters! Max: %u, Current: %zu",
      kNumMaxEmitter, activeEmitters_.size());
  }

  // 各エミッターのGPUデータを更新
  size_t emitterCount = min(activeEmitters_.size(), static_cast<size_t>(kNumMaxEmitter));
  for (size_t i = 0; i < emitterCount; i++) {
    if (activeEmitters_[i]) {
      // エミッターにGPUデータのセットアップを依頼
      activeEmitters_[i]->SetupGPUData(gpuEmitters[i]);
    }
  }

  // アンマップ
  emitterResource_->Unmap(0, nullptr);

  // PerFrameデータにアクティブエミッター数を格納
  perFrameData_->activeEmitterCount = static_cast<uint32_t>(emitterCount);
}

void GPUParticle::CreateRS()
{
  HRESULT hr;

  // rootSignatureの生成
  D3D12_ROOT_SIGNATURE_DESC descriptionRootSignature;
  descriptionRootSignature.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

  // Samplerの設定
  D3D12_STATIC_SAMPLER_DESC samplerDesc[1]{};
  samplerDesc[0].Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR; // テクスチャの補間方法
  samplerDesc[0].AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP; // テクスチャの繰り返し方法
  samplerDesc[0].AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP; // テクスチャの繰り返し方法
  samplerDesc[0].AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP; // テクスチャの繰り返し方法
  samplerDesc[0].ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER; // 比較しない
  samplerDesc[0].MaxLOD = D3D12_FLOAT32_MAX; // ミップマップの最大LOD
  samplerDesc[0].ShaderRegister = 0; // レジスタ番号
  samplerDesc[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL; // ピクセルシェーダーで使う
  descriptionRootSignature.pStaticSamplers = samplerDesc;
  descriptionRootSignature.NumStaticSamplers = _countof(samplerDesc);

  // DescriptorRangeの設定。
  // Texture
  D3D12_DESCRIPTOR_RANGE descriptorRange_tex[1] = {};
  descriptorRange_tex[0].BaseShaderRegister = 0; // レジスタ番号
  descriptorRange_tex[0].NumDescriptors = 1; // ディスクリプタ数
  descriptorRange_tex[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV; // SRVを使う
  descriptorRange_tex[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND; // Offsetを自動計算

  // Particle
  D3D12_DESCRIPTOR_RANGE descriptorRangeForParticle[1] = {};
  descriptorRangeForParticle[0].BaseShaderRegister = 0; // レジスタ番号
  descriptorRangeForParticle[0].NumDescriptors = 1; // ディスクリプタ数
  descriptorRangeForParticle[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV; // SRVを使う
  descriptorRangeForParticle[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND; // Offsetを自動計算

  // RootParameterの設定。複数設定できるので配列
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
  if (FAILED(hr))
  {
    Logger::Log(static_cast<char*>(errorBlob->GetBufferPointer()));
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

  // shaderのコンパイル
  Microsoft::WRL::ComPtr<IDxcBlob> vertexShaderBlob = m_dx12_->CompileShader(L"resources/shaders/GPUParticle.VS.hlsl", L"vs_6_0");
  assert(vertexShaderBlob != nullptr);

  Microsoft::WRL::ComPtr<IDxcBlob> pixelShaderBlob = m_dx12_->CompileShader(L"resources/shaders/GPUParticle.PS.hlsl", L"ps_6_0");
  assert(pixelShaderBlob != nullptr);

  // DepthStencilState
  D3D12_DEPTH_STENCIL_DESC depthStencilDesc{};
  // depthの機能を有効化にする
  depthStencilDesc.DepthEnable = true;
  // 書き込みします
  depthStencilDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
  // 深度の比較方法
  depthStencilDesc.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;

  // PSOの生成
  D3D12_GRAPHICS_PIPELINE_STATE_DESC graphicsPipelineStateDesc{};
  graphicsPipelineStateDesc.pRootSignature = RS_.Get();
  graphicsPipelineStateDesc.InputLayout = inputLayoutDesc;
  graphicsPipelineStateDesc.VS = { vertexShaderBlob->GetBufferPointer(), vertexShaderBlob->GetBufferSize() };
  graphicsPipelineStateDesc.PS = { pixelShaderBlob->GetBufferPointer(), pixelShaderBlob->GetBufferSize() };
  graphicsPipelineStateDesc.BlendState = blendDesc;
  graphicsPipelineStateDesc.RasterizerState = rasterizerDesc;
  // 書き込むRTVの情報
  graphicsPipelineStateDesc.NumRenderTargets = 1;
  graphicsPipelineStateDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
  // 利用するトポロジ（形状）のタイプ。三角形
  graphicsPipelineStateDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
  // どのように画面に色を打ち込むかの設定
  graphicsPipelineStateDesc.SampleDesc.Count = 1;
  graphicsPipelineStateDesc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;
  // DepthStencilの設定
  graphicsPipelineStateDesc.DepthStencilState = depthStencilDesc;
  graphicsPipelineStateDesc.DSVFormat = DXGI_FORMAT_D32_FLOAT;

  // 実際に生成
  hr = m_dx12_->GetDevice()->CreateGraphicsPipelineState(&graphicsPipelineStateDesc, IID_PPV_ARGS(&PSO_));
  assert(SUCCEEDED(hr));
}

void GPUParticle::CreateInitComputeRS()
{
  HRESULT hr;
  // rootSignatureの生成
  D3D12_ROOT_SIGNATURE_DESC descriptionRootSignature{};
  descriptionRootSignature.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

  // DescriptorRangeの設定。
  D3D12_DESCRIPTOR_RANGE descriptorRangeForParticle[1] = {}; // Particle
  descriptorRangeForParticle[0].BaseShaderRegister = 0; // レジスタ番号
  descriptorRangeForParticle[0].NumDescriptors = 1; // ディスクリプタ数
  descriptorRangeForParticle[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV; // UAVを使う
  descriptorRangeForParticle[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND; // Offsetを自動計算

  D3D12_DESCRIPTOR_RANGE descriptorRangeForFreeListIndex[1] = {}; // FreeListIndex
  descriptorRangeForFreeListIndex[0].BaseShaderRegister = 1; // レジスタ番号
  descriptorRangeForFreeListIndex[0].NumDescriptors = 1; // ディスクリプタ数
  descriptorRangeForFreeListIndex[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV; // UAVを使う
  descriptorRangeForFreeListIndex[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND; // Offsetを自動計算

  D3D12_DESCRIPTOR_RANGE descriptorRangeForFreeList[1] = {}; // FreeList
  descriptorRangeForFreeList[0].BaseShaderRegister = 2; // レジスタ番号
  descriptorRangeForFreeList[0].NumDescriptors = 1; // ディスクリプタ数
  descriptorRangeForFreeList[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV; // UAVを使う
  descriptorRangeForFreeList[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND; // Offsetを自動計算

  // RootParameterの設定。複数設定できるので配列
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
  if (FAILED(hr))
  {
    Logger::Log(reinterpret_cast<char*>(errorBlob->GetBufferPointer()));
    assert(false);
  }
  hr = m_dx12_->GetDevice()->CreateRootSignature(0, signatureBlob->GetBufferPointer(), signatureBlob->GetBufferSize(), IID_PPV_ARGS(initComputeRS_.GetAddressOf()));
}

void GPUParticle::CreateEmitParticleComputeRS()
{
  HRESULT hr;
  // rootSignatureの生成
  D3D12_ROOT_SIGNATURE_DESC descriptionRootSignature{};
  descriptionRootSignature.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

  // DescriptorRangeの設定。
  D3D12_DESCRIPTOR_RANGE descriptorRange_Particle[1] = {}; // Particle
  descriptorRange_Particle[0].BaseShaderRegister = 0; // レジスタ番号
  descriptorRange_Particle[0].NumDescriptors = 1; // ディスクリプタ数
  descriptorRange_Particle[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV; // UAVを使う
  descriptorRange_Particle[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND; // Offsetを自動計算

  D3D12_DESCRIPTOR_RANGE descriptorRange_FreeListIndex[1] = {}; // FreeListIndex
  descriptorRange_FreeListIndex[0].BaseShaderRegister = 1; // レジスタ番号
  descriptorRange_FreeListIndex[0].NumDescriptors = 1; // ディスクリプタ数
  descriptorRange_FreeListIndex[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV; // UAVを使う
  descriptorRange_FreeListIndex[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND; // Offsetを自動計算

  D3D12_DESCRIPTOR_RANGE descriptorRange_FreeList[1] = {}; // FreeList
  descriptorRange_FreeList[0].BaseShaderRegister = 2; // レジスタ番号
  descriptorRange_FreeList[0].NumDescriptors = 1; // ディスクリプタ数
  descriptorRange_FreeList[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV; // UAVを使う
  descriptorRange_FreeList[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND; // Offsetを自動計算

  D3D12_DESCRIPTOR_RANGE descriptorRange_Emitter[1] = {}; // Emitter
  descriptorRange_Emitter[0].BaseShaderRegister = 0; // レジスタ番号
  descriptorRange_Emitter[0].NumDescriptors = 1; // ディスクリプタ数
  descriptorRange_Emitter[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV; // SRVを使う
  descriptorRange_Emitter[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND; // Offsetを自動計算

  // RootParameterの設定。複数設定できるので配列
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
  if (FAILED(hr))
  {
    Logger::Log(static_cast<char*>(errorBlob->GetBufferPointer()));
    assert(false);
  }
  hr = m_dx12_->GetDevice()->CreateRootSignature(0, signatureBlob->GetBufferPointer(), signatureBlob->GetBufferSize(), IID_PPV_ARGS(emitParticleRS_.GetAddressOf()));
}

void GPUParticle::CreateUpdateParticleComputeRS()
{
  HRESULT hr;
  // rootSignatureの生成
  D3D12_ROOT_SIGNATURE_DESC descriptionRootSignature{};
  descriptionRootSignature.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

  // DescriptorRangeの設定。
  D3D12_DESCRIPTOR_RANGE descriptorRangeForParticle[1] = {}; // Particle
  descriptorRangeForParticle[0].BaseShaderRegister = 0; // レジスタ番号
  descriptorRangeForParticle[0].NumDescriptors = 1; // ディスクリプタ数
  descriptorRangeForParticle[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV; // UAVを使う
  descriptorRangeForParticle[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND; // Offsetを自動計算

  D3D12_DESCRIPTOR_RANGE descriptorRangeForFreeListIndex[1] = {}; // FreeListIndex
  descriptorRangeForFreeListIndex[0].BaseShaderRegister = 1; // レジスタ番号
  descriptorRangeForFreeListIndex[0].NumDescriptors = 1; // ディスクリプタ数
  descriptorRangeForFreeListIndex[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV; // UAVを使う
  descriptorRangeForFreeListIndex[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND; // Offsetを自動計算

  D3D12_DESCRIPTOR_RANGE descriptorRangeForFreeList[1] = {}; // FreeList
  descriptorRangeForFreeList[0].BaseShaderRegister = 2; // レジスタ番号
  descriptorRangeForFreeList[0].NumDescriptors = 1; // ディスクリプタ数
  descriptorRangeForFreeList[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV; // UAVを使う
  descriptorRangeForFreeList[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND; // Offsetを自動計算

  // RootParameterの設定。複数設定できるので配列
  D3D12_ROOT_PARAMETER rootParameters[4] = {};
  // Particle
  rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE; // ディスクリプタテーブルを使う
  rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL; // 全てのシェーダーで使う
  rootParameters[0].DescriptorTable.pDescriptorRanges = descriptorRangeForParticle; // ディスクリプタレンジを設定
  rootParameters[0].DescriptorTable.NumDescriptorRanges = _countof(descriptorRangeForParticle); // レンジの数

  // PerFrame
  rootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV; // 定数バッファビューを使う
  rootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL; // 全てのシェーダーで使う
  rootParameters[1].Descriptor.ShaderRegister = 0; // レジスタ番号とバインド

  // FreeListIndex
  rootParameters[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE; // ディスクリプタテーブルを使う
  rootParameters[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL; // 全てのシェーダーで使う
  rootParameters[2].DescriptorTable.pDescriptorRanges = descriptorRangeForFreeListIndex; // ディスクリプタレンジを設定
  rootParameters[2].DescriptorTable.NumDescriptorRanges = _countof(descriptorRangeForFreeListIndex); // レンジの数

  // FreeList
  rootParameters[3].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE; // ディスクリプタテーブルを使う
  rootParameters[3].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL; // 全てのシェーダーで使う
  rootParameters[3].DescriptorTable.pDescriptorRanges = descriptorRangeForFreeList; // ディスクリプタレンジを設定
  rootParameters[3].DescriptorTable.NumDescriptorRanges = _countof(descriptorRangeForFreeList); // レンジの数

  descriptionRootSignature.pParameters = rootParameters;
  descriptionRootSignature.NumParameters = _countof(rootParameters);

  Microsoft::WRL::ComPtr<ID3DBlob> signatureBlob = nullptr;
  Microsoft::WRL::ComPtr<ID3DBlob> errorBlob = nullptr;
  hr = D3D12SerializeRootSignature(&descriptionRootSignature, D3D_ROOT_SIGNATURE_VERSION_1, &signatureBlob, &errorBlob);
  if (FAILED(hr))
  {
    Logger::Log(reinterpret_cast<char*>(errorBlob->GetBufferPointer()));
    assert(false);
  }
  hr = m_dx12_->GetDevice()->CreateRootSignature(0, signatureBlob->GetBufferPointer(), signatureBlob->GetBufferSize(), IID_PPV_ARGS(updateParticleRS_.GetAddressOf()));

}

void GPUParticle::CreateComputeShaderPSO(Microsoft::WRL::ComPtr<ID3D12RootSignature>& RS, Microsoft::WRL::ComPtr<ID3D12PipelineState>& PSO, const std::wstring& shaderName)
{
  Microsoft::WRL::ComPtr<IDxcBlob> csBlob = m_dx12_->CompileShader(L"resources/shaders/" + shaderName, L"cs_6_0");

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

  // VertexBufferViewの作成
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
  // PerFrameのリソースを生成
  m_dx12_->CreateBufferResource(perFrameResource_, sizeof(PerFrame));
  // PerFrameのデータをマップ
  perFrameResource_->Map(0, nullptr, reinterpret_cast<void**>(&perFrameData_));
  // PerFrameのデータを初期化
  perFrameData_->time = 0.0f;
  perFrameData_->deltaTime = 0.0f;
}

void GPUParticle::CreateEmitterData()
{

  // エミッターリソースの生成
  m_dx12_->CreateBufferResource(emitterResource_, sizeof(EmitterGPUData) * kNumMaxEmitter);

  // エミッターリソースのSRVを作成
  emitterSrvIndex_ = m_srvManager_->Allocate();
  m_srvManager_->CreateSRVForStructuredBuffer(emitterSrvIndex_, emitterResource_.Get(), kNumMaxEmitter, sizeof(EmitterGPUData));

  // エミッター配列の初期化
  activeEmitters_.clear();

  // GPU側の初期化
  EmitterGPUData* gpuEmitters = nullptr;
  emitterResource_->Map(0, nullptr, reinterpret_cast<void**>(&gpuEmitters));
  ZeroMemory(gpuEmitters, sizeof(EmitterGPUData) * kNumMaxEmitter);
  emitterResource_->Unmap(0, nullptr);
}

void GPUParticle::CreateParticleResource()
{
  // ParticleCSのリソースを生成
  m_dx12_->CreateResourceForUAV(particleResource_, sizeof(ParticleCS) * kNumMaxParticle);

  // ParticleCSのUAVを生成
  particleUavIndex_ = m_srvManager_->Allocate();
  m_srvManager_->CreateUAV(particleUavIndex_, particleResource_.Get(), kNumMaxParticle, sizeof(ParticleCS));

  // ParticleCSのSRVを生成
  particleSrvIndex_ = m_srvManager_->Allocate();
  m_srvManager_->CreateSRVForStructuredBuffer(particleSrvIndex_, particleResource_.Get(), kNumMaxParticle, sizeof(ParticleCS));
}

void GPUParticle::CreateFreeListResource()
{
  // FreeListIndexのリソースを生成
  m_dx12_->CreateResourceForUAV(freeListIndexResource_, sizeof(int32_t));

  // FreeListIndexのUAVを生成
  freeListIndexUavIndex_ = m_srvManager_->Allocate();
  m_srvManager_->CreateUAV(freeListIndexUavIndex_, freeListIndexResource_.Get(), 1, sizeof(int32_t));


  // FreeListのリソースを生成
  m_dx12_->CreateResourceForUAV(freeListResource_, sizeof(uint32_t) * kNumMaxParticle);

  // FreeListのUAVを生成
  freeListUavIndex_ = m_srvManager_->Allocate();
  m_srvManager_->CreateUAV(freeListUavIndex_, freeListResource_.Get(), kNumMaxParticle, sizeof(uint32_t));
}

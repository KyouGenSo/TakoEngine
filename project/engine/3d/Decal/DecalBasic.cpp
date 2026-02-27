#include "DecalBasic.h"
#include "DX12Basic.h"
#include "Camera.h"
#include "SrvManager.h"
#include "PostEffectManager.h"
#include "Mat4x4Func.h"
#include "WinApp.h"

#ifdef _DEBUG
#include "DebugUIManager.h"
#endif

namespace Tako {

  std::unique_ptr<DecalBasic> DecalBasic::instance_ = nullptr;

  DecalBasic* DecalBasic::GetInstance()
  {
    if (!instance_) {
      instance_ = std::unique_ptr<DecalBasic>(new DecalBasic());
    }
    return instance_.get();
  }

  void DecalBasic::Initialize(DX12Basic* dx12)
  {
    m_dx12_ = dx12;

    CreateCubeMesh();
    CreateViewDataBuffer();
    CreateDepthSRV();
    CreatePSO();
  }

  void DecalBasic::Finalize()
  {
    // 深度 SRV を解放
    if (depthSrvIndex_ != 0) {
      SrvManager::GetInstance()->Free(depthSrvIndex_);
      depthSrvIndex_ = 0;
    }

    instance_.reset();
  }

  void DecalBasic::OnResize()
  {
    // ウィンドウサイズが変更されたら深度 SRV を再作成
    CreateDepthSRV();
  }

  void DecalBasic::BeginDraw()
  {
    // ビュープロジェクション行列を更新
    viewProjectionMatrix_ = camera_->GetViewProjectionMatrix();

    // ViewData 定数バッファを更新
    viewDataMapped_->invViewProj = Mat4x4::Inverse(viewProjectionMatrix_);
    viewDataMapped_->screenWidth = static_cast<float>(WinApp::clientWidth);
    viewDataMapped_->screenHeight = static_cast<float>(WinApp::clientHeight);

    // 深度バッファを PIXEL_SHADER_RESOURCE に遷移
    m_dx12_->TransitionResourceWithTracking(
      m_dx12_->GetDepthStencilResource(),
      D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE
    );

    // RTV を DSV なしで再バインド
    D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = PostEffectManager::GetInstance()->GetCurrentRTVHandle();
    m_dx12_->GetCommandList()->OMSetRenderTargets(1, &rtvHandle, false, nullptr);

    // ビューポートとシザー矩形を設定
    m_dx12_->SetViewPort();

    // PSO / RootSignature をセット
    m_dx12_->GetCommandList()->SetGraphicsRootSignature(rootSignature_.Get());
    m_dx12_->GetCommandList()->SetPipelineState(pipelineState_.Get());
    m_dx12_->GetCommandList()->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    // キューブメッシュの VBV / IBV をバインド
    m_dx12_->GetCommandList()->IASetVertexBuffers(0, 1, &cubeVBV_);
    m_dx12_->GetCommandList()->IASetIndexBuffer(&cubeIBV_);

    // ViewData CBV をバインド（RP#0）
    m_dx12_->GetCommandList()->SetGraphicsRootConstantBufferView(0, viewDataBuffer_->GetGPUVirtualAddress());

    // 深度 SRV をバインド（RP#2）
    SrvManager::GetInstance()->SetGraphicsRootDescriptorTable(2, depthSrvIndex_);
  }

  void DecalBasic::EndDraw()
  {
    // 深度バッファを DEPTH_WRITE に復帰
    m_dx12_->TransitionResourceWithTracking(
      m_dx12_->GetDepthStencilResource(),
      D3D12_RESOURCE_STATE_DEPTH_WRITE
    );

    // RTV + DSV を再バインド
    D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = PostEffectManager::GetInstance()->GetCurrentRTVHandle();
    D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = m_dx12_->GetDSVHeapHandleStart();
    m_dx12_->GetCommandList()->OMSetRenderTargets(1, &rtvHandle, false, &dsvHandle);
  }

  // ルートシグネチャの作成
  void DecalBasic::CreateRootSignature()
  {
    HRESULT hr;

    D3D12_ROOT_SIGNATURE_DESC descriptionRootSignature{};
    descriptionRootSignature.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

    // サンプラーの設定（2つ）
    D3D12_STATIC_SAMPLER_DESC samplerDesc[2]{};
    // s0: Point / Clamp（深度テクスチャ用）
    samplerDesc[0].Filter = D3D12_FILTER_MIN_MAG_MIP_POINT;
    samplerDesc[0].AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    samplerDesc[0].AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    samplerDesc[0].AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    samplerDesc[0].ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
    samplerDesc[0].MaxLOD = D3D12_FLOAT32_MAX;
    samplerDesc[0].ShaderRegister = 0;
    samplerDesc[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

    // s1: Linear / Clamp（デカールテクスチャ用）
    samplerDesc[1].Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
    samplerDesc[1].AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    samplerDesc[1].AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    samplerDesc[1].AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    samplerDesc[1].ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
    samplerDesc[1].MaxLOD = D3D12_FLOAT32_MAX;
    samplerDesc[1].ShaderRegister = 1;
    samplerDesc[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

    descriptionRootSignature.pStaticSamplers = samplerDesc;
    descriptionRootSignature.NumStaticSamplers = _countof(samplerDesc);

    // DescriptorRange: 深度テクスチャ（t0）
    D3D12_DESCRIPTOR_RANGE descriptorRangeDepth[1] = {};
    descriptorRangeDepth[0].BaseShaderRegister = 0;
    descriptorRangeDepth[0].NumDescriptors = 1;
    descriptorRangeDepth[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    descriptorRangeDepth[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

    // DescriptorRange: デカールテクスチャ（t1）
    D3D12_DESCRIPTOR_RANGE descriptorRangeDecalTex[1] = {};
    descriptorRangeDecalTex[0].BaseShaderRegister = 1;
    descriptorRangeDecalTex[0].NumDescriptors = 1;
    descriptorRangeDecalTex[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    descriptorRangeDecalTex[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

    // RootParameter の設定（4パラメータ）
    D3D12_ROOT_PARAMETER rootParameters[4] = {};

    // RP#0: ViewData CBV（b0, ALL）
    rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    rootParameters[0].Descriptor.ShaderRegister = 0;

    // RP#1: DecalData CBV（b1, ALL）
    rootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    rootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    rootParameters[1].Descriptor.ShaderRegister = 1;

    // RP#2: 深度テクスチャ SRV Table（t0, PIXEL）
    rootParameters[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    rootParameters[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    rootParameters[2].DescriptorTable.pDescriptorRanges = descriptorRangeDepth;
    rootParameters[2].DescriptorTable.NumDescriptorRanges = _countof(descriptorRangeDepth);

    // RP#3: デカールテクスチャ SRV Table（t1, PIXEL）
    rootParameters[3].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    rootParameters[3].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    rootParameters[3].DescriptorTable.pDescriptorRanges = descriptorRangeDecalTex;
    rootParameters[3].DescriptorTable.NumDescriptorRanges = _countof(descriptorRangeDecalTex);

    descriptionRootSignature.pParameters = rootParameters;
    descriptionRootSignature.NumParameters = _countof(rootParameters);

    Microsoft::WRL::ComPtr<ID3DBlob> signatureBlob = nullptr;
    Microsoft::WRL::ComPtr<ID3DBlob> errorBlob = nullptr;

    hr = D3D12SerializeRootSignature(&descriptionRootSignature, D3D_ROOT_SIGNATURE_VERSION_1, &signatureBlob, &errorBlob);
    if (FAILED(hr)) {
#ifdef _DEBUG
      DebugUIManager::GetInstance()->AddLog(
        static_cast<char*>(errorBlob->GetBufferPointer()),
        DebugUIManager::LogType::Error);
#endif
      assert(false);
    }

    hr = m_dx12_->GetDevice()->CreateRootSignature(
      0,
      signatureBlob->GetBufferPointer(),
      signatureBlob->GetBufferSize(),
      IID_PPV_ARGS(rootSignature_.GetAddressOf())
    );
    assert(SUCCEEDED(hr));
  }

  // パイプラインステートの生成
  void DecalBasic::CreatePSO()
  {
    HRESULT hr;

    // RootSignature の生成
    CreateRootSignature();

    // InputLayout: POSITION (float4) のみ
    D3D12_INPUT_ELEMENT_DESC inputElementDescs[1] = {};
    inputElementDescs[0].SemanticName = "POSITION";
    inputElementDescs[0].SemanticIndex = 0;
    inputElementDescs[0].Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
    inputElementDescs[0].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;

    D3D12_INPUT_LAYOUT_DESC inputLayoutDesc{};
    inputLayoutDesc.pInputElementDescs = inputElementDescs;
    inputLayoutDesc.NumElements = _countof(inputElementDescs);

    // BlendState: SrcAlpha / InvSrcAlpha（標準アルファブレンド）
    D3D12_BLEND_DESC blendDesc{};
    blendDesc.RenderTarget[0].BlendEnable = true;
    blendDesc.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
    blendDesc.RenderTarget[0].DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
    blendDesc.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
    blendDesc.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
    blendDesc.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ZERO;
    blendDesc.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
    blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

    // RasterizerState: CullMode = NONE（キューブ内側からも描画）
    D3D12_RASTERIZER_DESC rasterizerDesc{};
    rasterizerDesc.FillMode = D3D12_FILL_MODE_SOLID;
    rasterizerDesc.CullMode = D3D12_CULL_MODE_NONE;

    // シェーダーのコンパイル
    Microsoft::WRL::ComPtr<IDxcBlob> vertexShaderBlob = m_dx12_->CompileShader(L"resources/shaders/Decal.VS.hlsl", L"vs_6_0");
    assert(vertexShaderBlob != nullptr);

    Microsoft::WRL::ComPtr<IDxcBlob> pixelShaderBlob = m_dx12_->CompileShader(L"resources/shaders/Decal.PS.hlsl", L"ps_6_0");
    assert(pixelShaderBlob != nullptr);

    // DepthStencilState: 深度テスト・書き込み無効
    D3D12_DEPTH_STENCIL_DESC depthStencilDesc{};
    depthStencilDesc.DepthEnable = false;
    depthStencilDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;

    // PSO の生成
    D3D12_GRAPHICS_PIPELINE_STATE_DESC graphicsPipelineStateDesc{};
    graphicsPipelineStateDesc.pRootSignature = rootSignature_.Get();
    graphicsPipelineStateDesc.InputLayout = inputLayoutDesc;
    graphicsPipelineStateDesc.VS = { vertexShaderBlob->GetBufferPointer(), vertexShaderBlob->GetBufferSize() };
    graphicsPipelineStateDesc.PS = { pixelShaderBlob->GetBufferPointer(), pixelShaderBlob->GetBufferSize() };
    graphicsPipelineStateDesc.BlendState = blendDesc;
    graphicsPipelineStateDesc.RasterizerState = rasterizerDesc;
    graphicsPipelineStateDesc.NumRenderTargets = 1;
    graphicsPipelineStateDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
    graphicsPipelineStateDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    graphicsPipelineStateDesc.SampleDesc.Count = 1;
    graphicsPipelineStateDesc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;
    graphicsPipelineStateDesc.DepthStencilState = depthStencilDesc;
    graphicsPipelineStateDesc.DSVFormat = DXGI_FORMAT_UNKNOWN; // DSV バインドなし

    hr = m_dx12_->GetDevice()->CreateGraphicsPipelineState(&graphicsPipelineStateDesc, IID_PPV_ARGS(&pipelineState_));
    assert(SUCCEEDED(hr));
  }

  // 単位キューブメッシュの作成（[-0.5, 0.5] 範囲の 8 頂点 / 36 インデックス）
  void DecalBasic::CreateCubeMesh()
  {
    // 8 頂点（float4: x, y, z, w）
    struct CubeVertex {
      float x, y, z, w;
    };

    CubeVertex vertices[8] = {
      { -0.5f, -0.5f, -0.5f, 1.0f }, // 0: 左下手前
      {  0.5f, -0.5f, -0.5f, 1.0f }, // 1: 右下手前
      {  0.5f,  0.5f, -0.5f, 1.0f }, // 2: 右上手前
      { -0.5f,  0.5f, -0.5f, 1.0f }, // 3: 左上手前
      { -0.5f, -0.5f,  0.5f, 1.0f }, // 4: 左下奥
      {  0.5f, -0.5f,  0.5f, 1.0f }, // 5: 右下奥
      {  0.5f,  0.5f,  0.5f, 1.0f }, // 6: 右上奥
      { -0.5f,  0.5f,  0.5f, 1.0f }, // 7: 左上奥
    };

    // 36 インデックス（12 三角形）
    uint16_t indices[36] = {
      // 手前面 (z-)
      0, 2, 1,  0, 3, 2,
      // 奥面 (z+)
      4, 5, 6,  4, 6, 7,
      // 左面 (x-)
      4, 7, 3,  4, 3, 0,
      // 右面 (x+)
      1, 2, 6,  1, 6, 5,
      // 下面 (y-)
      4, 0, 1,  4, 1, 5,
      // 上面 (y+)
      3, 7, 6,  3, 6, 2,
    };

    // 頂点バッファの作成
    cubeVertexBuffer_ = m_dx12_->MakeBufferResource(sizeof(vertices));
    CubeVertex* vertexData = nullptr;
    cubeVertexBuffer_->Map(0, nullptr, reinterpret_cast<void**>(&vertexData));
    memcpy(vertexData, vertices, sizeof(vertices));
    cubeVertexBuffer_->Unmap(0, nullptr);

    cubeVBV_.BufferLocation = cubeVertexBuffer_->GetGPUVirtualAddress();
    cubeVBV_.SizeInBytes = sizeof(vertices);
    cubeVBV_.StrideInBytes = sizeof(CubeVertex);

    // インデックスバッファの作成
    cubeIndexBuffer_ = m_dx12_->MakeBufferResource(sizeof(indices));
    uint16_t* indexData = nullptr;
    cubeIndexBuffer_->Map(0, nullptr, reinterpret_cast<void**>(&indexData));
    memcpy(indexData, indices, sizeof(indices));
    cubeIndexBuffer_->Unmap(0, nullptr);

    cubeIBV_.BufferLocation = cubeIndexBuffer_->GetGPUVirtualAddress();
    cubeIBV_.SizeInBytes = sizeof(indices);
    cubeIBV_.Format = DXGI_FORMAT_R16_UINT;
  }

  // ViewData 定数バッファの作成
  void DecalBasic::CreateViewDataBuffer()
  {
    viewDataBuffer_ = m_dx12_->MakeBufferResource(sizeof(ViewDataGPU));
    viewDataBuffer_->Map(0, nullptr, reinterpret_cast<void**>(&viewDataMapped_));
  }

  // 深度 SRV の作成
  void DecalBasic::CreateDepthSRV()
  {
    // 前回の SRV があれば解放
    if (depthSrvIndex_ != 0) {
      SrvManager::GetInstance()->Free(depthSrvIndex_);
    }

    depthSrvIndex_ = SrvManager::GetInstance()->Allocate();
    SrvManager::GetInstance()->CreateSRVForTexture2D(
      depthSrvIndex_,
      m_dx12_->GetDepthStencilResource(),
      DXGI_FORMAT_R32_FLOAT,
      1
    );
  }

} // namespace Tako

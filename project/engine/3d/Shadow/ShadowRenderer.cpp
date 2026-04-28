#include "ShadowRenderer.h"
#include "DX12Basic.h"
#include "Light.h"
#include "PostEffectManager.h"
#include "SrvManager.h"
#include <cassert>
#include "Mat4x4Func.h"
#include "EnginePaths.h"

#ifdef _DEBUG
#include "DebugUIManager.h"
#include "ImGuiManager.h"
#endif

namespace Tako {

// 静的メンバ変数の定義
std::unique_ptr<ShadowRenderer> ShadowRenderer::instance_ = nullptr;

ShadowRenderer* ShadowRenderer::GetInstance()
{
    if (!instance_)
    {
        instance_ = std::unique_ptr<ShadowRenderer>(new ShadowRenderer());
    }
    return instance_.get();
}

void ShadowRenderer::Initialize(DX12Basic* dx12)
{
    assert(dx12);
    
    dx12_ = dx12;
    
    // ShadowMap を内部で生成
    shadowMap_ = std::make_unique<ShadowMap>();
    shadowMap_->Initialize(dx12_);
    
    // シャドウ用のパイプラインを作成
    CreateShadowRootSignature();
    CreateShadowPipelineState();
    
    // インスタンシング用シャドウパイプラインを作成
    CreateShadowInstancedRootSignature();
    CreateShadowInstancedPipelineState();
    
    // 定数バッファを作成
    CreateConstantBuffer();
}

void ShadowRenderer::Update()
{
    if (!shadowConstantData_) return;
    
    // シャドウマップのフレーム開始処理（遅延リソース再作成）
    if (shadowMap_) {
        shadowMap_->BeginFrame();
    }
    
    // シャドウマップの更新
    if (shadowEnabled_ && camera_) {
        light_->UpdateDirectionalLightShadowMatrices(camera_, maxShadowDistance_);
        shadowConstantData_->lightViewProj = light_->GetDirectionalLight().viewProjMatrix;
        shadowConstantData_->enableShadow = 1;
        shadowConstantData_->shadowMapSize = {
            static_cast<float>(shadowMap_->GetShadowMapSize()), 
            static_cast<float>(shadowMap_->GetShadowMapSize())
        };
        shadowConstantData_->shadowBias = shadowBias_;
        shadowConstantData_->normalOffsetBias = normalOffsetBias_;
        shadowConstantData_->pcfKernelSize = static_cast<float>(shadowMap_->GetPCFKernelSize());
        shadowMap_->SetLightViewProjectionMatrix(light_->GetDirectionalLight().viewProjMatrix);
    } else {
        shadowConstantData_->enableShadow = 0;
    }
}

void ShadowRenderer::Finalize()
{
    if (shadowConstantBuffer_ && shadowConstantData_) {
        shadowConstantBuffer_->Unmap(0, nullptr);
        shadowConstantData_ = nullptr;
    }
    
    // ShadowMap を削除
    if (shadowMap_) {
        shadowMap_->Finalize();
        shadowMap_.reset();
    }
    
    // インスタンスを削除
    instance_.reset();
}

void ShadowRenderer::BeginShadowPass()
{
    if (!shadowEnabled_ || !shadowMap_) return;
    
    // シャドウマップレンダリング中フラグを設定
    isRenderingShadow_ = true;
    
    // 現在のレンダーターゲットとデプスバッファを保存
    savedRTVHandle_ = PostEffectManager::GetInstance()->GetCurrentRTVHandle();
    savedDSVHandle_ = dx12_->GetDSVHeapHandleStart();
    hasSavedRenderTargets_ = true;
    
    // シャドウマップレンダリングを開始
    shadowMap_->BeginShadowMapRender();
    
    // シャドウレンダリング設定を適用
    SetRenderState();
}

void ShadowRenderer::EndShadowPass()
{
    if (!shadowEnabled_ || !shadowMap_) return;
    
    // シャドウマップレンダリングを終了
    shadowMap_->EndShadowMapRender();
    
    // シャドウマップレンダリング中フラグをクリア
    isRenderingShadow_ = false;
    
    // 元のレンダーターゲットとデプスバッファを復元
    if (hasSavedRenderTargets_) {
        dx12_->GetCommandList()->OMSetRenderTargets(1, &savedRTVHandle_, FALSE, &savedDSVHandle_);
        hasSavedRenderTargets_ = false;
    }
    
    // ビューポートとシザー矩形を元に戻す
    dx12_->SetViewPort();
}

void ShadowRenderer::SetRenderState()
{
    // シャドウ用ルートシグネチャの設定
    dx12_->GetCommandList()->SetGraphicsRootSignature(shadowRootSignature_.Get());
    
    // シャドウ用パイプラインステートの設定
    dx12_->GetCommandList()->SetPipelineState(shadowPipelineState_.Get());
    
    // トポロジの設定
    dx12_->GetCommandList()->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    
    // シャドウ定数バッファの設定（パラメータ1、レジスタ b4）
    dx12_->GetCommandList()->SetGraphicsRootConstantBufferView(1, shadowConstantBuffer_->GetGPUVirtualAddress());
}

void ShadowRenderer::SetShadowForMainPass()
{
    // 通常レンダリング時のシャドウ設定
    if (!isRenderingShadow_ && shadowConstantBuffer_) {
        // シャドウ定数バッファの設定（ルートパラメータ9、レジスタ b4）
        dx12_->GetCommandList()->SetGraphicsRootConstantBufferView(9, shadowConstantBuffer_->GetGPUVirtualAddress());
    }
    
    // シャドウマップの設定（ルートパラメータ10、テクスチャ t4）
    if (!isRenderingShadow_ && shadowMap_) {
        SrvManager::GetInstance()->SetGraphicsRootDescriptorTable(10, shadowMap_->GetSrvIndex());
    }
}

void ShadowRenderer::CreateShadowRootSignature()
{
    HRESULT hr;
    
    // shadowRootSignature の生成
    D3D12_ROOT_SIGNATURE_DESC descriptionRootSignature{};
    descriptionRootSignature.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
    
    // Sampler の設定（シャドウマップ生成時は不要だが、最小限の設定）
    D3D12_STATIC_SAMPLER_DESC samplerDesc[1]{};
    samplerDesc[0].Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
    samplerDesc[0].AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    samplerDesc[0].AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    samplerDesc[0].AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    samplerDesc[0].ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
    samplerDesc[0].MaxLOD = D3D12_FLOAT32_MAX;
    samplerDesc[0].ShaderRegister = 0;
    samplerDesc[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    descriptionRootSignature.pStaticSamplers = samplerDesc;
    descriptionRootSignature.NumStaticSamplers = _countof(samplerDesc);
    
    // RootParameter の設定（シャドウマップ生成に必要な最小限のパラメータ）
    D3D12_ROOT_PARAMETER rootParameters[2] = {};
    
    // Parameter 0: TransformationMatrix (b0)
    rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
    rootParameters[0].Descriptor.ShaderRegister = 0;
    
    // Parameter 1: ShadowConstants (b4)
    rootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    rootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
    rootParameters[1].Descriptor.ShaderRegister = 4;
    
    descriptionRootSignature.pParameters = rootParameters;
    descriptionRootSignature.NumParameters = _countof(rootParameters);
    
    Microsoft::WRL::ComPtr<ID3DBlob> signatureBlob = nullptr;
    Microsoft::WRL::ComPtr<ID3DBlob> errorBlob = nullptr;
    
    hr = D3D12SerializeRootSignature(&descriptionRootSignature, D3D_ROOT_SIGNATURE_VERSION_1, &signatureBlob, &errorBlob);
    if (FAILED(hr))
    {
#ifdef _DEBUG
        DebugUIManager::GetInstance()->AddLog(
          static_cast<char*>(errorBlob->GetBufferPointer()),
          DebugUIManager::LogType::Error);
#endif

        assert(false);
    }

    hr = dx12_->GetDevice()->CreateRootSignature(0, signatureBlob->GetBufferPointer(),
        signatureBlob->GetBufferSize(), IID_PPV_ARGS(shadowRootSignature_.GetAddressOf()));
    assert(SUCCEEDED(hr));
}

void ShadowRenderer::CreateShadowPipelineState()
{
    HRESULT hr;
    
    // InputLayout（位置のみ）
    D3D12_INPUT_ELEMENT_DESC inputElementDescs[1] = {};
    inputElementDescs[0].SemanticName = "POSITION";
    inputElementDescs[0].SemanticIndex = 0;
    inputElementDescs[0].Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
    inputElementDescs[0].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;
    
    D3D12_INPUT_LAYOUT_DESC inputLayoutDesc{};
    inputLayoutDesc.pInputElementDescs = inputElementDescs;
    inputLayoutDesc.NumElements = _countof(inputElementDescs);
    
    // BlendState（深度のみなので不要だが設定）
    D3D12_BLEND_DESC blendDesc{};
    blendDesc.RenderTarget[0].RenderTargetWriteMask = 0; // カラー出力しない
    
    // RasterizerState（フロントフェースカリング）
    D3D12_RASTERIZER_DESC rasterizerDesc{};
    rasterizerDesc.FillMode = D3D12_FILL_MODE_SOLID;
    rasterizerDesc.CullMode = D3D12_CULL_MODE_FRONT; // シャドウアクネ対策
    rasterizerDesc.DepthBias = 100000; // 深度バイアス
    rasterizerDesc.SlopeScaledDepthBias = 1.0f; // スロープスケール深度バイアス
    
    // シェーダーのコンパイル
    Microsoft::WRL::ComPtr<IDxcBlob> vertexShaderBlob = dx12_->CompileShader(
        EnginePaths::ShaderPath(L"ShadowMap.VS.hlsl"), L"vs_6_0");
    assert(vertexShaderBlob != nullptr);
    
    // ピクセルシェーダーは不要（深度のみ）
    
    // DepthStencilState
    D3D12_DEPTH_STENCIL_DESC depthStencilDesc{};
    depthStencilDesc.DepthEnable = true;
    depthStencilDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
    depthStencilDesc.DepthFunc = D3D12_COMPARISON_FUNC_LESS;
    
    // シャドウ PSO の生成
    D3D12_GRAPHICS_PIPELINE_STATE_DESC graphicsPipelineStateDesc{};
    graphicsPipelineStateDesc.pRootSignature = shadowRootSignature_.Get();
    graphicsPipelineStateDesc.InputLayout = inputLayoutDesc;
    graphicsPipelineStateDesc.VS = { 
        vertexShaderBlob->GetBufferPointer(), 
        vertexShaderBlob->GetBufferSize() 
    };
    graphicsPipelineStateDesc.PS = { nullptr, 0 }; // ピクセルシェーダーなし
    graphicsPipelineStateDesc.BlendState = blendDesc;
    graphicsPipelineStateDesc.RasterizerState = rasterizerDesc;
    graphicsPipelineStateDesc.NumRenderTargets = 0; // カラーターゲットなし
    graphicsPipelineStateDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    graphicsPipelineStateDesc.SampleDesc.Count = 1;
    graphicsPipelineStateDesc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;
    graphicsPipelineStateDesc.DepthStencilState = depthStencilDesc;
    graphicsPipelineStateDesc.DSVFormat = DXGI_FORMAT_D32_FLOAT;
    
    // PSO を生成
    hr = dx12_->GetDevice()->CreateGraphicsPipelineState(&graphicsPipelineStateDesc, 
        IID_PPV_ARGS(&shadowPipelineState_));
    assert(SUCCEEDED(hr));
}

void ShadowRenderer::CreateConstantBuffer()
{
    // シャドウ用定数バッファの作成
    shadowConstantBuffer_ = dx12_->MakeBufferResource(sizeof(ShadowConstants));
    shadowConstantBuffer_->Map(0, nullptr, reinterpret_cast<void**>(&shadowConstantData_));
    
    // 初期値設定
    shadowConstantData_->lightViewProj = Mat4x4::MakeIdentity();
    shadowConstantData_->shadowBias = shadowBias_;
    shadowConstantData_->enableShadow = shadowEnabled_ ? 1 : 0;
    shadowConstantData_->shadowMapSize = {2048.0f, 2048.0f};
    shadowConstantData_->normalOffsetBias = normalOffsetBias_;
    shadowConstantData_->pcfKernelSize = 3.0f;
}

void ShadowRenderer::SetShadowQuality(int quality)
{
    if (shadowMap_) {
        shadowMap_->SetShadowQuality(static_cast<ShadowMap::ShadowQuality>(quality));
    }
}

void ShadowRenderer::SetShadowMapSize(uint32_t size)
{
    if (shadowMap_) {
        shadowMap_->SetShadowMapSize(size);
    }
}

void ShadowRenderer::SetPCFKernelSize(int kernelSize)
{
    if (shadowMap_) {
        shadowMap_->SetPCFKernelSize(kernelSize);
    }
}

void ShadowRenderer::SetInstancedRenderState()
{
    if (!shadowInstancedRootSignature_ || !shadowInstancedPipelineState_) return;
    
    // インスタンシング用シャドウルートシグネチャの設定
    dx12_->GetCommandList()->SetGraphicsRootSignature(shadowInstancedRootSignature_.Get());
    
    // インスタンシング用シャドウパイプラインステートの設定
    dx12_->GetCommandList()->SetPipelineState(shadowInstancedPipelineState_.Get());
    
    // トポロジの設定
    dx12_->GetCommandList()->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    
    // シャドウ定数バッファの設定（パラメータ1、レジスタ b4）
    dx12_->GetCommandList()->SetGraphicsRootConstantBufferView(1, shadowConstantBuffer_->GetGPUVirtualAddress());
}

void ShadowRenderer::CreateShadowInstancedRootSignature()
{
    HRESULT hr;
    
    // shadowInstancedRootSignature の生成
    D3D12_ROOT_SIGNATURE_DESC descriptionRootSignature{};
    descriptionRootSignature.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
    
    // Sampler の設定（シャドウマップ生成時は不要だが、最小限の設定）
    D3D12_STATIC_SAMPLER_DESC samplerDesc[1]{};
    samplerDesc[0].Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
    samplerDesc[0].AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    samplerDesc[0].AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    samplerDesc[0].AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    samplerDesc[0].ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
    samplerDesc[0].MaxLOD = D3D12_FLOAT32_MAX;
    samplerDesc[0].ShaderRegister = 0;
    samplerDesc[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    descriptionRootSignature.pStaticSamplers = samplerDesc;
    descriptionRootSignature.NumStaticSamplers = _countof(samplerDesc);
    
    // DescriptorRange の設定（インスタンスデータ用）
    D3D12_DESCRIPTOR_RANGE descriptorRangeInstance[1] = {};
    descriptorRangeInstance[0].BaseShaderRegister = 5;  // t5レジスタ
    descriptorRangeInstance[0].NumDescriptors = 1;
    descriptorRangeInstance[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    descriptorRangeInstance[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
    
    // RootParameter の設定（インスタンシング対応）
    D3D12_ROOT_PARAMETER rootParameters[3] = {};
    
    // Parameter 0: TransformationMatrix (b0) - 通常のシャドウパスとの互換性のため残す
    rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
    rootParameters[0].Descriptor.ShaderRegister = 0;
    
    // Parameter 1: ShadowConstants (b4)
    rootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    rootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
    rootParameters[1].Descriptor.ShaderRegister = 4;
    
    // Parameter 2: Instance Data (t5) - ディスクリプタテーブル
    rootParameters[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    rootParameters[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
    rootParameters[2].DescriptorTable.pDescriptorRanges = descriptorRangeInstance;
    rootParameters[2].DescriptorTable.NumDescriptorRanges = _countof(descriptorRangeInstance);
    
    descriptionRootSignature.pParameters = rootParameters;
    descriptionRootSignature.NumParameters = _countof(rootParameters);
    
    Microsoft::WRL::ComPtr<ID3DBlob> signatureBlob = nullptr;
    Microsoft::WRL::ComPtr<ID3DBlob> errorBlob = nullptr;
    
    hr = D3D12SerializeRootSignature(&descriptionRootSignature, D3D_ROOT_SIGNATURE_VERSION_1, &signatureBlob, &errorBlob);
    if (FAILED(hr))
    {
#ifdef _DEBUG
        DebugUIManager::GetInstance()->AddLog(
          static_cast<char*>(errorBlob->GetBufferPointer()),
          DebugUIManager::LogType::Error);
#endif

        assert(false);
    }

    hr = dx12_->GetDevice()->CreateRootSignature(0, signatureBlob->GetBufferPointer(),
        signatureBlob->GetBufferSize(), IID_PPV_ARGS(shadowInstancedRootSignature_.GetAddressOf()));
    assert(SUCCEEDED(hr));
}

void ShadowRenderer::CreateShadowInstancedPipelineState()
{
    HRESULT hr;
    
    // InputLayout（位置のみ）
    D3D12_INPUT_ELEMENT_DESC inputElementDescs[1] = {};
    inputElementDescs[0].SemanticName = "POSITION";
    inputElementDescs[0].SemanticIndex = 0;
    inputElementDescs[0].Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
    inputElementDescs[0].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;
    
    D3D12_INPUT_LAYOUT_DESC inputLayoutDesc{};
    inputLayoutDesc.pInputElementDescs = inputElementDescs;
    inputLayoutDesc.NumElements = _countof(inputElementDescs);
    
    // BlendState（深度のみなので不要だが設定）
    D3D12_BLEND_DESC blendDesc{};
    blendDesc.RenderTarget[0].RenderTargetWriteMask = 0; // カラー出力しない
    
    // RasterizerState（フロントフェースカリング）
    D3D12_RASTERIZER_DESC rasterizerDesc{};
    rasterizerDesc.FillMode = D3D12_FILL_MODE_SOLID;
    rasterizerDesc.CullMode = D3D12_CULL_MODE_FRONT; // シャドウアクネ対策
    rasterizerDesc.DepthBias = 100000; // 深度バイアス
    rasterizerDesc.SlopeScaledDepthBias = 1.0f; // スロープスケール深度バイアス
    
    // インスタンシング用頂点シェーダーのコンパイル
    Microsoft::WRL::ComPtr<IDxcBlob> vertexShaderBlob = dx12_->CompileShader(
        EnginePaths::ShaderPath(L"ShadowMapInstanced.VS.hlsl"), L"vs_6_0");
    assert(vertexShaderBlob != nullptr);
    
    // ピクセルシェーダーは不要（深度のみ）
    
    // DepthStencilState
    D3D12_DEPTH_STENCIL_DESC depthStencilDesc{};
    depthStencilDesc.DepthEnable = true;
    depthStencilDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
    depthStencilDesc.DepthFunc = D3D12_COMPARISON_FUNC_LESS;
    
    // インスタンシング用シャドウ PSO の生成
    D3D12_GRAPHICS_PIPELINE_STATE_DESC graphicsPipelineStateDesc{};
    graphicsPipelineStateDesc.pRootSignature = shadowInstancedRootSignature_.Get();
    graphicsPipelineStateDesc.InputLayout = inputLayoutDesc;
    graphicsPipelineStateDesc.VS = { 
        vertexShaderBlob->GetBufferPointer(), 
        vertexShaderBlob->GetBufferSize() 
    };
    graphicsPipelineStateDesc.PS = { nullptr, 0 }; // ピクセルシェーダーなし
    graphicsPipelineStateDesc.BlendState = blendDesc;
    graphicsPipelineStateDesc.RasterizerState = rasterizerDesc;
    graphicsPipelineStateDesc.NumRenderTargets = 0; // カラーターゲットなし
    graphicsPipelineStateDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    graphicsPipelineStateDesc.SampleDesc.Count = 1;
    graphicsPipelineStateDesc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;
    graphicsPipelineStateDesc.DepthStencilState = depthStencilDesc;
    graphicsPipelineStateDesc.DSVFormat = DXGI_FORMAT_D32_FLOAT;
    
    // PSO を生成
    hr = dx12_->GetDevice()->CreateGraphicsPipelineState(&graphicsPipelineStateDesc, 
        IID_PPV_ARGS(&shadowInstancedPipelineState_));
    assert(SUCCEEDED(hr));
}

void ShadowRenderer::DrawImGui()
{
#ifdef _DEBUG
    ImGui::Begin("Shadow Mapping");
    
    // シャドウの有効/無効
    ImGui::Checkbox("Enable Shadow", &shadowEnabled_);
    
    // シャドウ品質プリセット
    ImGui::Separator();
    ImGui::Text("Shadow Quality");
    static int shadowQuality = 2; // デフォルトは High
    const char* qualityNames[] = { "Low (512x512, NO PCF)", "Medium (1024x1024, PCF 3x3)", 
                                    "High (2048x2048, PCF 5x5)", "Ultra (4096x4096, PCF 7x7)",
                                    "Super (8192x8192, PCF 9x9)" };
    if (ImGui::Combo("Quality Preset", &shadowQuality, qualityNames, IM_ARRAYSIZE(qualityNames))) {
        SetShadowQuality(shadowQuality);
    }
    
    // カスタム設定
    ImGui::Separator();
    ImGui::Text("Custom Settings");
    
    // シャドウマップ解像度
    if (shadowMap_) {
        static int shadowMapSize = shadowMap_->GetShadowMapSize();
        const char* sizeNames[] = { "256", "512", "1024", "2048", "4096", "8192" };
        int sizeValues[] = { 256, 512, 1024, 2048, 4096, 8192 };
        int currentSizeIndex = 3; // デフォルトは2048
        for (int i = 0; i < IM_ARRAYSIZE(sizeValues); i++) {
            if (sizeValues[i] == shadowMapSize) {
                currentSizeIndex = i;
                break;
            }
        }
        if (ImGui::Combo("Shadow Map Size", &currentSizeIndex, sizeNames, IM_ARRAYSIZE(sizeNames))) {
            shadowMapSize = sizeValues[currentSizeIndex];
            SetShadowMapSize(shadowMapSize);
        }
        
        // PCF カーネルサイズ
        static int pcfKernelSize = shadowMap_->GetPCFKernelSize();
        const char* kernelNames[] = { "1x1 (No PCF)", "3x3", "5x5", "7x7", "9x9" };
        int kernelValues[] = { 1, 3, 5, 7, 9 };
        int currentKernelIndex = 1; // デフォルトは3x3
        for (int i = 0; i < IM_ARRAYSIZE(kernelValues); i++) {
            if (kernelValues[i] == pcfKernelSize) {
                currentKernelIndex = i;
                break;
            }
        }
        if (ImGui::Combo("PCF Kernel Size", &currentKernelIndex, kernelNames, IM_ARRAYSIZE(kernelNames))) {
            pcfKernelSize = kernelValues[currentKernelIndex];
            SetPCFKernelSize(pcfKernelSize);
        }
    }
    
    // バイアス設定
    ImGui::Separator();
    ImGui::Text("Bias Settings");
    if (ImGui::DragFloat("Shadow Bias", &shadowBias_, 0.00001f, 0.0f, 0.01f, "%.6f")) {
        // shadowBias_は既にメンバ変数なので直接変更される
    }
    
    if (ImGui::DragFloat("Normal Offset Bias", &normalOffsetBias_, 0.001f, 0.0f, 0.1f, "%.4f")) {
        // normalOffsetBias_は既にメンバ変数なので直接変更される
    }
    
    // ライト設定（Light クラスと連携）
    if (light_) {
        ImGui::Separator();
        ImGui::Text("Light Settings");
        
        // 最大シャドウ距離の設定
        if (ImGui::DragFloat("Max Shadow Distance", &maxShadowDistance_, 1.0f, 5.0f, 500.0f, "%.1f")) {
            // 値は既に更新されている
        }
        
        bool autoUpdatePos = light_->GetAutoUpdatePosition();
        if (ImGui::Checkbox("Auto Update Light Position", &autoUpdatePos)) {
            light_->SetAutoUpdatePosition(autoUpdatePos);
        }
        
        if (!autoUpdatePos) {
            Vector3 lightPos = light_->GetDirectionalLight().position;
            if (ImGui::DragFloat3("Light Position", &lightPos.x, 0.1f, -50.0f, 50.0f)) {
                light_->SetDirectionalLightPosition(lightPos);
            }
        }
        
        Vector3 sceneCenter = light_->GetSceneCenter();
        if (ImGui::DragFloat3("Scene Center", &sceneCenter.x, 0.1f, -50.0f, 50.0f)) {
            light_->SetSceneCenter(sceneCenter);
        }
    }
    
    // パフォーマンス情報
    ImGui::Separator();
    ImGui::Text("Performance Info");
    if (shadowMap_) {
        ImGui::Text("Current Shadow Map Size: %dx%d", 
                    shadowMap_->GetShadowMapSize(),
                    shadowMap_->GetShadowMapSize());
        ImGui::Text("Current PCF Kernel: %dx%d", 
                    shadowMap_->GetPCFKernelSize(),
                    shadowMap_->GetPCFKernelSize());
        
        // シャドウマップのメモリ使用量を表示
        uint32_t mapSize = shadowMap_->GetShadowMapSize();
        float memoryMB = (mapSize * mapSize * 4) / (1024.0f * 1024.0f); // 32bit depth
        ImGui::Text("Memory Usage: %.2f MB", memoryMB);
    }
    
    ImGui::End();
#endif // _DEBUG
}

} // namespace Tako
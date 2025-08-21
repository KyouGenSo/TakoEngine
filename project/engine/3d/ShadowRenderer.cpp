#include "ShadowRenderer.h"
#include "DX12Basic.h"
#include "ShadowMap.h"
#include "Light.h"
#include "Logger.h"
#include "PostEffectManager.h"
#include "SrvManager.h"
#include <cassert>

#include "Mat4x4Func.h"

void ShadowRenderer::Initialize(DX12Basic* dx12, ShadowMap* shadowMap, Light* light)
{
    assert(dx12);
    assert(shadowMap);
    assert(light);
    
    dx12_ = dx12;
    shadowMap_ = shadowMap;
    light_ = light;
    
    // シャドウ用のパイプラインを作成
    CreateShadowRootSignature();
    CreateShadowPipelineState();
    
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
    if (shadowEnabled_) {
        light_->UpdateDirectionalLightShadowMatrices();
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
    
    // シャドウ定数バッファの設定（パラメータ1、レジスタb4）
    dx12_->GetCommandList()->SetGraphicsRootConstantBufferView(1, shadowConstantBuffer_->GetGPUVirtualAddress());
}

void ShadowRenderer::SetShadowForMainPass()
{
    // 通常レンダリング時のシャドウ設定
    if (!isRenderingShadow_ && shadowConstantBuffer_) {
        // シャドウ定数バッファの設定（ルートパラメータ9、レジスタb4）
        dx12_->GetCommandList()->SetGraphicsRootConstantBufferView(9, shadowConstantBuffer_->GetGPUVirtualAddress());
    }
    
    // シャドウマップの設定（ルートパラメータ10、テクスチャt4）
    if (!isRenderingShadow_ && shadowMap_) {
#ifdef _DEBUG
        OutputDebugStringA("ShadowRenderer::SetShadowForMainPass() - Setting shadow map as shader resource\n");
#endif
        SrvManager::GetInstance()->SetGraphicsRootDescriptorTable(10, shadowMap_->GetSrvIndex());
    }
#ifdef _DEBUG
    else if (isRenderingShadow_) {
        OutputDebugStringA("ShadowRenderer::SetShadowForMainPass() - Skipping shadow map SRV (rendering shadow map)\n");
    }
#endif
}

void ShadowRenderer::CreateShadowRootSignature()
{
    HRESULT hr;
    
    // shadowRootSignatureの生成
    D3D12_ROOT_SIGNATURE_DESC descriptionRootSignature{};
    descriptionRootSignature.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
    
    // Samplerの設定（シャドウマップ生成時は不要だが、最小限の設定）
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
    
    // RootParameterの設定（シャドウマップ生成に必要な最小限のパラメータ）
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
        Logger::Log(static_cast<char*>(errorBlob->GetBufferPointer()));
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
        L"resources/shaders/ShadowMap.VS.hlsl", L"vs_6_0");
    assert(vertexShaderBlob != nullptr);
    
    // ピクセルシェーダーは不要（深度のみ）
    
    // DepthStencilState
    D3D12_DEPTH_STENCIL_DESC depthStencilDesc{};
    depthStencilDesc.DepthEnable = true;
    depthStencilDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
    depthStencilDesc.DepthFunc = D3D12_COMPARISON_FUNC_LESS;
    
    // シャドウPSOの生成
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
    
    // PSOを生成
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
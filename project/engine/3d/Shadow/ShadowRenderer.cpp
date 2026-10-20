#include "ShadowRenderer.h"
#include "DX12Basic.h"
#include "Object3dBasic.h"
#include "Light.h"
#include "SrvManager.h"
#include <cassert>
#include "Mat4x4Func.h"
#include "EnginePaths.h"

#ifdef _DEBUG
#include "DebugUIManager.h"
#include "ImGuiManager.h"
#endif

namespace Tako {

namespace {
    // --- 定数バッファ初期値 ---
    constexpr float kDefaultShadowMapSizeF = 2048.0f; ///< 解像度の初期値（DEFAULT_SHADOW_MAP_SIZE に対応）
    constexpr float kDefaultPcfKernelSizeF = 3.0f;    ///< PCF カーネルサイズの初期値
}

// 静的メンバ変数の定義
std::unique_ptr<ShadowRenderer> ShadowRenderer::instance_ = nullptr;

ShadowRenderer* ShadowRenderer::GetInstance()
{
    if (!instance_)
    {
        instance_ = std::make_unique<ShadowRenderer>(Token{});
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

    // シャドウ用のパイプラインを作成（通常 / インスタンシング）
    shadowPipeline_          = MakeShadowPipeline(L"ShadowMap.VS.hlsl");
    shadowInstancedPipeline_ = MakeShadowPipeline(L"ShadowMapInstanced.VS.hlsl");

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
    if (shadowEnabled_ && camera_ && light_) {
        light_->UpdateDirectionalLightShadowMatrices(camera_, maxShadowDistance_);
        shadowConstantData_->lightViewProj = light_->GetDirectionalLight().viewProjMatrix;
        shadowConstantData_->enableShadow = 1;
        shadowConstantData_->shadowMapSize = {
            static_cast<float>(shadowMap_->GetShadowMapSize()),
            static_cast<float>(shadowMap_->GetShadowMapSize())
        };
        shadowConstantData_->shadowBias = shadowBias_;
        shadowConstantData_->pcfKernelSize = static_cast<float>(shadowMap_->GetPCFKernelSize());
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

    // 本編/デバッグビュー/プレビューのどれから呼ばれても、パス開始前の描画先へ戻す
    dx12_->RestoreSceneRenderTarget();
}

void ShadowRenderer::SetRenderState()
{
    BindShadowPipeline(shadowPipeline_);
}

void ShadowRenderer::SetShadowForMainPass()
{
    if (isRenderingShadow_) {
        return;
    }
    CommandContext::ShaderBinder binder = dx12_->GetContext().Graphics();
    if (shadowConstantBuffer_) {
        binder.BindCBV("gShadowConstants"_id, shadowConstantBuffer_->GetGPUVirtualAddress());
    }
    if (shadowMap_) {
        binder.BindSRV("gShadowMap"_id, shadowMap_->GetSrvIndex());
    }
}

GraphicsPipelineDesc ShadowRenderer::MakeShadowPipeline(const wchar_t* vsFileName) const
{
    // フロントフェースカリングでピーターパニングを緩和する。深度だけを書くのでピクセルシェーダーは使わない
    GraphicsPipelineDesc desc;
    desc.vs = dx12_->GetShaderLibrary().Get(vsFileName, ShaderStage::Vertex);
    desc.vertexLayout = VertexLayout::Position;
    desc.renderState.cull = CullMode::Front;
    desc.renderState.depthFunc = D3D12_COMPARISON_FUNC_LESS;
    return desc;
}

void ShadowRenderer::BindShadowPipeline(const GraphicsPipelineDesc& pipeline)
{
    CommandContext& context = dx12_->GetContext();
    context.SetGraphicsPipeline(pipeline);
    context.Graphics().BindCBV("gShadowConstants"_id, shadowConstantBuffer_->GetGPUVirtualAddress());
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
    shadowConstantData_->shadowMapSize = { kDefaultShadowMapSizeF, kDefaultShadowMapSizeF };
    shadowConstantData_->pcfKernelSize = kDefaultPcfKernelSizeF;
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
    BindShadowPipeline(shadowInstancedPipeline_);
}

void ShadowRenderer::DrawImGui()
{
#ifdef _DEBUG
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
        int shadowMapSize = shadowMap_->GetShadowMapSize();
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
        int pcfKernelSize = shadowMap_->GetPCFKernelSize();
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
    }


    // ライト設定（Light クラスと連携）
    if (light_) {
        ImGui::Separator();
        ImGui::Text("Light Settings");

        // 最大シャドウ距離の設定
        if (ImGui::DragFloat("Max Shadow Distance", &maxShadowDistance_, 1.0f, 5.0f, 500.0f, "%.1f")) {
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

#endif // _DEBUG
}

} // namespace Tako

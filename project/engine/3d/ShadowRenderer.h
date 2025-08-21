#pragma once
#include <d3d12.h>
#include <wrl.h>
#include "Matrix4x4.h"
#include "Vector2.h"
#include "ShadowMap.h"

class DX12Basic;
class ShadowMap;
class Light;

class ShadowRenderer
{
private: // シングルトン設定
    // インスタンス
    static ShadowRenderer* instance_;
    
    ShadowRenderer() = default;
    ~ShadowRenderer() = default;
    ShadowRenderer(ShadowRenderer&) = delete;
    ShadowRenderer& operator=(ShadowRenderer&) = delete;

public:
    /// <summary>
    /// インスタンスの取得
    /// </summary>
    static ShadowRenderer* GetInstance();
    
    /// <summary>
    /// 初期化
    /// </summary>
    /// <param name="dx12">DirectX12基本オブジェクト</param>
    void Initialize(DX12Basic* dx12);
    
    /// <summary>
    /// Lightの参照を設定
    /// </summary>
    void SetLight(Light* light) { light_ = light; }

    /// <summary>
    /// 更新処理（定数バッファの更新）
    /// </summary>
    void Update();

    /// <summary>
    /// 終了処理
    /// </summary>
    void Finalize();

    /// <summary>
    /// シャドウパス開始
    /// </summary>
    void BeginShadowPass();

    /// <summary>
    /// シャドウパス終了
    /// </summary>
    void EndShadowPass();

    /// <summary>
    /// シャドウレンダリング設定を適用
    /// </summary>
    void SetRenderState();

    /// <summary>
    /// 通常レンダリング時のシャドウ設定を適用
    /// </summary>
    void SetShadowForMainPass();

    /// <summary>
    /// シャドウレンダリング中かどうか
    /// </summary>
    bool IsRenderingShadow() const { return isRenderingShadow_; }

    /// <summary>
    /// シャドウの有効/無効を設定
    /// </summary>
    void SetEnabled(bool enabled) { shadowEnabled_ = enabled; }

    /// <summary>
    /// シャドウが有効かどうか
    /// </summary>
    bool IsEnabled() const { return shadowEnabled_; }

    /// <summary>
    /// 定数バッファのGPUアドレスを取得
    /// </summary>
    D3D12_GPU_VIRTUAL_ADDRESS GetConstantBufferGPUAddress() const {
        return shadowConstantBuffer_ ? shadowConstantBuffer_->GetGPUVirtualAddress() : 0;
    }

    /// <summary>
    /// シャドウバイアスを設定
    /// </summary>
    void SetShadowBias(float bias) { shadowBias_ = bias; }

    /// <summary>
    /// 法線オフセットバイアスを設定
    /// </summary>
    void SetNormalOffsetBias(float bias) { normalOffsetBias_ = bias; }
    
    /// <summary>
    /// シャドウ品質を設定
    /// </summary>
    void SetShadowQuality(int quality);
    
    /// <summary>
    /// シャドウマップサイズを設定
    /// </summary>
    void SetShadowMapSize(uint32_t size);
    
    /// <summary>
    /// PCFカーネルサイズを設定
    /// </summary>
    void SetPCFKernelSize(int kernelSize);
    
    /// <summary>
    /// ShadowMapを取得
    /// </summary>
    ShadowMap* GetShadowMap() { return shadowMap_; }

    /// <summary>
    /// ImGuiでのデバッグ表示
    /// </summary>
    void DrawImGui();

private:
    /// <summary>
    /// シャドウ用ルートシグネチャの作成
    /// </summary>
    void CreateShadowRootSignature();

    /// <summary>
    /// シャドウ用パイプラインステートの作成
    /// </summary>
    void CreateShadowPipelineState();

    /// <summary>
    /// 定数バッファの作成
    /// </summary>
    void CreateConstantBuffer();

private:
    // DirectX12関連
    DX12Basic* dx12_ = nullptr;
    ShadowMap* shadowMap_ = nullptr;
    Light* light_ = nullptr;

    // シャドウ用ルートシグネチャとPSO
    Microsoft::WRL::ComPtr<ID3D12RootSignature> shadowRootSignature_;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> shadowPipelineState_;

    // シャドウ定数バッファ
    Microsoft::WRL::ComPtr<ID3D12Resource> shadowConstantBuffer_;
    
    struct ShadowConstants {
        Matrix4x4 lightViewProj;
        float shadowBias;
        int enableShadow;
        Vector2 shadowMapSize;
        float normalOffsetBias;
        float pcfKernelSize;
        float padding[2];  // 16バイトアライメント
    };
    ShadowConstants* shadowConstantData_ = nullptr;

    // シャドウ設定
    bool shadowEnabled_ = true;
    float shadowBias_ = 0.0001f;
    float normalOffsetBias_ = 0.01f;

    // レンダリング状態
    bool isRenderingShadow_ = false;
    
    // レンダーターゲット復元用
    D3D12_CPU_DESCRIPTOR_HANDLE savedRTVHandle_;
    D3D12_CPU_DESCRIPTOR_HANDLE savedDSVHandle_;
    bool hasSavedRenderTargets_ = false;
};
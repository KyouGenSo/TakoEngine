#pragma once
#include <d3d12.h>
#include <wrl.h>
#include "Vector3.h"
#include "Matrix4x4.h"

class DX12Basic;
class SrvManager;

class ShadowMap
{
public:
    // シャドウマップの品質設定
    enum class ShadowQuality {
        Low = 0,     // 512x512, PCF 1x1
        Medium = 1,  // 1024x1024, PCF 3x3
        High = 2,    // 2048x2048, PCF 5x5
        Ultra = 3,   // 4096x4096, PCF 7x7
        Super = 4    // 8192x8192, PCF 9x9
    };
    
    // デフォルトのシャドウマップサイズ
    static const uint32_t DEFAULT_SHADOW_MAP_SIZE = 2048;
    
public:
    /// <summary>
    /// 初期化
    /// </summary>
    void Initialize(DX12Basic* dx12);

    /// <summary>
    /// 終了処理
    /// </summary>
    void Finalize();

    /// <summary>
    /// フレーム開始時の処理（遅延リソース再作成）
    /// </summary>
    void BeginFrame();
    
    /// <summary>
    /// シャドウマップレンダリング開始
    /// </summary>
    void BeginShadowMapRender();

    /// <summary>
    /// シャドウマップレンダリング終了
    /// </summary>
    void EndShadowMapRender();

    /// <summary>
    /// ライトビュープロジェクション行列を設定
    /// </summary>
    void SetLightViewProjectionMatrix(const Matrix4x4& lightViewProj);

    /// <summary>
    /// シャドウマップをシェーダーリソースとして設定
    /// </summary>
    void SetShadowMapForShader();

    /// <summary>
    /// ライトビュープロジェクション行列を取得
    /// </summary>
    const Matrix4x4& GetLightViewProjectionMatrix() const { return lightViewProjectionMatrix_; }

    /// <summary>
    /// SRVインデックスを取得
    /// </summary>
    uint32_t GetSrvIndex() const { return srvIndex_; }

    /// <summary>
    /// 深度バイアス設定
    /// </summary>
    void SetDepthBias(int bias, float slopeScaledBias) {
        depthBias_ = bias;
        slopeScaledDepthBias_ = slopeScaledBias;
    }
    
    /// <summary>
    /// シャドウ品質の設定
    /// </summary>
    void SetShadowQuality(ShadowQuality quality);
    
    /// <summary>
    /// カスタム解像度の設定
    /// </summary>
    void SetShadowMapSize(uint32_t size);
    
    /// <summary>
    /// PCFカーネルサイズの設定（1, 3, 5, 7, 9のいずれか）
    /// </summary>
    void SetPCFKernelSize(int kernelSize);
    
    /// <summary>
    /// 法線オフセットバイアスの設定
    /// </summary>
    void SetNormalOffsetBias(float normalBias) { normalOffsetBias_ = normalBias; }
    
    /// <summary>
    /// 現在の解像度を取得
    /// </summary>
    uint32_t GetShadowMapSize() const { return shadowMapSize_; }
    
    /// <summary>
    /// 現在のPCFカーネルサイズを取得
    /// </summary>
    int GetPCFKernelSize() const { return pcfKernelSize_; }

private:
    /// <summary>
    /// シャドウマップリソースの作成
    /// </summary>
    void CreateShadowMapResource();

    /// <summary>
    /// 深度ステンシルビューの作成
    /// </summary>
    void CreateDepthStencilView();

    /// <summary>
    /// シェーダーリソースビューの作成
    /// </summary>
    void CreateShaderResourceView();

    /// <summary>
    /// 定数バッファの作成
    /// </summary>
    void CreateConstantBuffer();

private:
    // DirectX関連
    DX12Basic* dx12_;
    SrvManager* srvManager_;

    // シャドウマップリソース
    Microsoft::WRL::ComPtr<ID3D12Resource> shadowMapResource_;
    
    // 深度ステンシルビュー用のディスクリプタヒープ
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> dsvDescriptorHeap_;
    
    // DSVハンドル
    D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle_;
    
    // SRVインデックス
    uint32_t srvIndex_;
    
    // ライトビュープロジェクション行列
    Matrix4x4 lightViewProjectionMatrix_;
    
    // 定数バッファ
    Microsoft::WRL::ComPtr<ID3D12Resource> constantBuffer_;
    
    // 定数バッファのマップ先
    struct ShadowConstantBuffer
    {
        Matrix4x4 lightViewProjectionMatrix;
        float depthBias;
        float slopeScaledDepthBias;
        float normalOffsetBias;
        float pcfKernelSize;  // PCFカーネルサイズ
    };
    ShadowConstantBuffer* constantBufferData_;
    
    // 深度バイアス設定
    int depthBias_ = 100000;
    float slopeScaledDepthBias_ = 1.0f;
    float normalOffsetBias_ = 0.01f;
    
    // シャドウマップサイズとPCF設定
    uint32_t shadowMapSize_ = DEFAULT_SHADOW_MAP_SIZE;
    int pcfKernelSize_ = 3;  // デフォルトは3x3
    ShadowQuality currentQuality_ = ShadowQuality::High;
    bool needsRecreation_ = false;  // リソース再作成フラグ
    
    // 遅延リソース再作成用
    bool pendingRecreation_ = false;  // 次フレームで再作成
    uint32_t pendingShadowMapSize_ = DEFAULT_SHADOW_MAP_SIZE;
    
    // ビューポート
    D3D12_VIEWPORT viewport_;
    D3D12_RECT scissorRect_;
    
    // 初回フレームフラグ
    bool isFirstFrame_ = true;
};
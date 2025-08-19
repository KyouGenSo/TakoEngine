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
    // シャドウマップのサイズ
    static const uint32_t SHADOW_MAP_SIZE = 2048;
    
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
        float padding[2];
    };
    ShadowConstantBuffer* constantBufferData_;
    
    // 深度バイアス設定
    int depthBias_ = 100000;
    float slopeScaledDepthBias_ = 1.0f;
    
    // ビューポート
    D3D12_VIEWPORT viewport_;
    D3D12_RECT scissorRect_;
    
    // 初回フレームフラグ
    bool isFirstFrame_ = true;
};
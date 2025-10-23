#pragma once
#include <d3d12.h>
#include <wrl.h>
#include "Vector3.h"
#include "Matrix4x4.h"

class DX12Basic;
class SrvManager;

/// <summary>
/// シャドウマッピング用の深度バッファ管理クラス
/// PCF（Percentage Closer Filtering）対応の高品質なシャドウ生成をサポート
/// 動的な解像度変更、品質プリセット、深度バイアス調整機能を提供
/// </summary>
class ShadowMap
{
public:
    /// <summary>
    /// シャドウマップの品質プリセット
    /// 解像度とPCFカーネルサイズを組み合わせた設定
    /// </summary>
    enum class ShadowQuality {
        Low = 0,     ///< 512x512, PCF 1x1 - モバイル向け最低品質
        Medium = 1,  ///< 1024x1024, PCF 3x3 - 標準品質
        High = 2,    ///< 2048x2048, PCF 5x5 - 高品質（デフォルト）
        Ultra = 3,   ///< 4096x4096, PCF 7x7 - 超高品質
        Super = 4    ///< 8192x8192, PCF 9x9 - 最高品質（ハイエンドGPU向け）
    };

    static const uint32_t DEFAULT_SHADOW_MAP_SIZE = 2048; ///< デフォルトのシャドウマップ解像度（2048x2048）
    
public:
    /// <summary>
    /// 初期化
    /// </summary>
    /// <param name="dx12">DirectX12基盤システムへのポインタ</param>
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
    /// <param name="lightViewProj">ライトビュープロジェクション行列</param>
    void SetLightViewProjectionMatrix(const Matrix4x4& lightViewProj);

    /// <summary>
    /// ライトビュープロジェクション行列を取得
    /// </summary>
    /// <returns>ライトビュープロジェクション行列の参照</returns>
    const Matrix4x4& GetLightViewProjectionMatrix() const { return lightViewProjectionMatrix_; }

    /// <summary>
    /// SRVインデックスを取得
    /// </summary>
    /// <returns>シェーダーリソースビューのインデックス</returns>
    uint32_t GetSrvIndex() const { return srvIndex_; }

    /// <summary>
    /// 深度バイアス設定
    /// </summary>
    /// <param name="bias">深度バイアス値</param>
    /// <param name="slopeScaledBias">スロープスケール深度バイアス値</param>
    void SetDepthBias(int bias, float slopeScaledBias) {
        depthBias_ = bias;
        slopeScaledDepthBias_ = slopeScaledBias;
    }

    /// <summary>
    /// シャドウ品質の設定
    /// </summary>
    /// <param name="quality">品質プリセット</param>
    void SetShadowQuality(ShadowQuality quality);

    /// <summary>
    /// カスタム解像度の設定
    /// </summary>
    /// <param name="size">シャドウマップの解像度（512-8192推奨）</param>
    void SetShadowMapSize(uint32_t size);

    /// <summary>
    /// PCFカーネルサイズの設定（1, 3, 5, 7, 9のいずれか）
    /// </summary>
    /// <param name="kernelSize">PCFカーネルサイズ</param>
    void SetPCFKernelSize(int kernelSize);

    /// <summary>
    /// 法線オフセットバイアスの設定
    /// </summary>
    /// <param name="normalBias">法線オフセットバイアス値</param>
    void SetNormalOffsetBias(float normalBias) { normalOffsetBias_ = normalBias; }

    /// <summary>
    /// 現在の解像度を取得
    /// </summary>
    /// <returns>現在のシャドウマップ解像度</returns>
    uint32_t GetShadowMapSize() const { return shadowMapSize_; }

    /// <summary>
    /// 現在のPCFカーネルサイズを取得
    /// </summary>
    /// <returns>現在のPCFカーネルサイズ</returns>
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
    DX12Basic* dx12_; ///< DirectX12基盤システムへの参照
    SrvManager* srvManager_; ///< SRV管理システムへの参照

    Microsoft::WRL::ComPtr<ID3D12Resource> shadowMapResource_; ///< シャドウマップ用深度テクスチャリソース

    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> dsvDescriptorHeap_; ///< 深度ステンシルビュー用ディスクリプタヒープ

    D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle_; ///< 深度ステンシルビューのCPUハンドル

    uint32_t srvIndex_; ///< シェーダーリソースビューのインデックス（テクスチャとして読み取り用）

    Matrix4x4 lightViewProjectionMatrix_; ///< ライト空間のビュープロジェクション行列

    Microsoft::WRL::ComPtr<ID3D12Resource> constantBuffer_; ///< シャドウパラメータ用定数バッファ

    /// <summary>
    /// シャドウマップ用定数バッファ構造体
    /// GPU側に送信されるシャドウパラメータ
    /// </summary>
    struct ShadowConstantBuffer
    {
        Matrix4x4 lightViewProjectionMatrix; ///< ライトビュープロジェクション行列
        float depthBias;                     ///< 深度バイアス（シャドウアクネ防止）
        float slopeScaledDepthBias;          ///< スロープスケール深度バイアス
        float normalOffsetBias;              ///< 法線オフセットバイアス（ピーターパニング防止）
        float pcfKernelSize;                 ///< PCFカーネルサイズ（フィルタリング品質）
    };
    ShadowConstantBuffer* constantBufferData_; ///< 定数バッファのマップ済みポインタ

    int depthBias_ = 100000;                   ///< 深度バイアス値（デフォルト値）
    float slopeScaledDepthBias_ = 1.0f;        ///< スロープスケール深度バイアス値
    float normalOffsetBias_ = 0.01f;           ///< 法線オフセットバイアス値

    uint32_t shadowMapSize_ = DEFAULT_SHADOW_MAP_SIZE; ///< 現在のシャドウマップ解像度
    int pcfKernelSize_ = 3;                    ///< PCFカーネルサイズ（デフォルト3x3）
    ShadowQuality currentQuality_ = ShadowQuality::High; ///< 現在の品質設定
    bool needsRecreation_ = false;             ///< リソース再作成が必要かどうかのフラグ

    bool pendingRecreation_ = false;           ///< 次フレームで再作成を行うフラグ（遅延実行用）
    uint32_t pendingShadowMapSize_ = DEFAULT_SHADOW_MAP_SIZE; ///< 次フレームで適用する解像度

    D3D12_VIEWPORT viewport_;                  ///< シャドウマップレンダリング用ビューポート
    D3D12_RECT scissorRect_;                   ///< シャドウマップレンダリング用シザー矩形

    bool isFirstFrame_ = true;                 ///< 初回フレーム判定フラグ
};
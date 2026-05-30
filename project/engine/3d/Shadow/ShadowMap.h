#pragma once
#include <cstdint>
#include <d3d12.h>
#include <wrl.h>

namespace Tako {

class DX12Basic;
class SrvManager;

/// <summary>
/// シャドウマッピング用の深度バッファ管理クラス
/// 深度テクスチャ（DSV/SRV）・ビューポート・解像度ポリシー・リソース状態遷移を管理する。
/// 品質プリセットと PCF カーネルサイズによる動的な解像度変更（遅延再作成）に対応。
/// </summary>
class ShadowMap
{
public:
    /// <summary>
    /// シャドウマップの品質プリセット（解像度 + PCF カーネルサイズの組み合わせ）
    /// </summary>
    enum class ShadowQuality {
        Low = 0,     ///< 512x512, PCF 1x1 - モバイル向け最低品質
        Medium = 1,  ///< 1024x1024, PCF 3x3 - 標準品質
        High = 2,    ///< 2048x2048, PCF 5x5 - 高品質（デフォルト）
        Ultra = 3,   ///< 4096x4096, PCF 7x7 - 超高品質
        Super = 4    ///< 8192x8192, PCF 9x9 - 最高品質（ハイエンド GPU 向け）
    };

    static const uint32_t DEFAULT_SHADOW_MAP_SIZE = 2048; ///< デフォルトのシャドウマップ解像度（2048x2048）

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
    /// シャドウマップレンダリング開始（DEPTH_WRITE へ遷移し深度をクリア）
    /// </summary>
    void BeginShadowMapRender();

    /// <summary>
    /// シャドウマップレンダリング終了（PIXEL_SHADER_RESOURCE へ遷移）
    /// </summary>
    void EndShadowMapRender();

    /// <summary>
    /// SRV インデックスを取得（深度テクスチャ読み取り用、register t4）
    /// </summary>
    /// <returns>シェーダーリソースビューのインデックス</returns>
    uint32_t GetSrvIndex() const { return srvIndex_; }

    /// <summary>
    /// シャドウ品質の設定（解像度と PCF カーネルサイズをまとめて変更）
    /// </summary>
    /// <param name="quality">品質プリセット</param>
    void SetShadowQuality(ShadowQuality quality);

    /// <summary>
    /// カスタム解像度の設定（256-8192、2のべき乗にクランプ）
    /// </summary>
    /// <param name="size">シャドウマップの解像度</param>
    void SetShadowMapSize(uint32_t size);

    /// <summary>
    /// PCF カーネルサイズの設定（1, 3, 5, 7, 9 のいずれか）
    /// </summary>
    /// <param name="kernelSize">PCF カーネルサイズ</param>
    void SetPCFKernelSize(int kernelSize);

    /// <summary>
    /// 現在の解像度を取得
    /// </summary>
    /// <returns>現在のシャドウマップ解像度</returns>
    uint32_t GetShadowMapSize() const { return shadowMapSize_; }

    /// <summary>
    /// 現在の PCF カーネルサイズを取得
    /// </summary>
    /// <returns>現在の PCF カーネルサイズ</returns>
    int GetPCFKernelSize() const { return pcfKernelSize_; }

private:
    /// <summary>
    /// シャドウマップリソース（深度テクスチャ）の作成
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

private:
    DX12Basic* dx12_ = nullptr;        ///< DirectX12基盤システムへの参照
    SrvManager* srvManager_ = nullptr; ///< SRV 管理システムへの参照

    Microsoft::WRL::ComPtr<ID3D12Resource> shadowMapResource_;       ///< シャドウマップ用深度テクスチャリソース
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> dsvDescriptorHeap_; ///< 深度ステンシルビュー用ディスクリプタヒープ
    D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle_{};                        ///< 深度ステンシルビューの CPU ハンドル
    uint32_t srvIndex_ = UINT32_MAX;                                 ///< SRV インデックス（深度テクスチャ読み取り用）

    uint32_t shadowMapSize_ = DEFAULT_SHADOW_MAP_SIZE; ///< 現在のシャドウマップ解像度
    int pcfKernelSize_ = 3;                            ///< PCF カーネルサイズ（デフォルト3x3）

    bool pendingRecreation_ = false;                         ///< 次フレームで再作成を行うフラグ（遅延実行用）
    uint32_t pendingShadowMapSize_ = DEFAULT_SHADOW_MAP_SIZE; ///< 次フレームで適用する解像度

    D3D12_VIEWPORT viewport_{};  ///< シャドウマップレンダリング用ビューポート
    D3D12_RECT scissorRect_{};   ///< シャドウマップレンダリング用シザー矩形
};

} // namespace Tako

#pragma once
#include <d3d12.h>
#include <wrl.h>
#include <memory>
#include "Matrix4x4.h"
#include "Vector2.h"
#include "ShadowMap.h"

class DX12Basic;
class ShadowMap;

namespace Tako {

class Light;
class Camera;

/// <summary>
/// シャドウレンダリング統合管理クラス
/// シングルトンパターンで実装され、シャドウマップの生成と適用を制御
/// 通常描画とインスタンシング描画の両方でシャドウをサポート
/// PCF フィルタリング、動的品質調整、ImGui デバッグ UI 統合機能を提供
/// </summary>
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
    /// <returns>ShadowRenderer のシングルトンインスタンス</returns>
    static ShadowRenderer* GetInstance();

    /// <summary>
    /// 初期化
    /// </summary>
    /// <param name="dx12">DirectX12基盤システムへのポインタ</param>
    void Initialize(DX12Basic* dx12);

    /// <summary>
    /// Light の参照を設定
    /// </summary>
    /// <param name="light">ライトシステムへのポインタ</param>
    void SetLight(Light* light) { light_ = light; }

    /// <summary>
    /// Camera の参照を設定
    /// </summary>
    /// <param name="camera">カメラへのポインタ</param>
    void SetCamera(Camera* camera) { camera_ = camera; }

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
    /// <returns>シャドウレンダリング中の場合 true</returns>
    bool IsRenderingShadow() const { return isRenderingShadow_; }

    /// <summary>
    /// シャドウの有効/無効を設定
    /// </summary>
    /// <param name="enabled">シャドウを有効にするか</param>
    void SetEnabled(bool enabled) { shadowEnabled_ = enabled; }

    /// <summary>
    /// シャドウが有効かどうか
    /// </summary>
    /// <returns>シャドウが有効な場合 true</returns>
    bool IsEnabled() const { return shadowEnabled_; }

    /// <summary>
    /// 定数バッファの GPU アドレスを取得
    /// </summary>
    /// <returns>定数バッファの GPU アドレス</returns>
    D3D12_GPU_VIRTUAL_ADDRESS GetConstantBufferGPUAddress() const {
        return shadowConstantBuffer_ ? shadowConstantBuffer_->GetGPUVirtualAddress() : 0;
    }

    /// <summary>
    /// シャドウバイアスを設定
    /// </summary>
    /// <param name="bias">シャドウバイアス値</param>
    void SetShadowBias(float bias) { shadowBias_ = bias; }

    /// <summary>
    /// 法線オフセットバイアスを設定
    /// </summary>
    /// <param name="bias">法線オフセットバイアス値</param>
    void SetNormalOffsetBias(float bias) { normalOffsetBias_ = bias; }

    /// <summary>
    /// シャドウ品質を設定
    /// </summary>
    /// <param name="quality">品質レベル（0-4）</param>
    void SetShadowQuality(int quality);

    /// <summary>
    /// シャドウマップサイズを設定
    /// </summary>
    /// <param name="size">シャドウマップの解像度</param>
    void SetShadowMapSize(uint32_t size);

    /// <summary>
    /// PCF カーネルサイズを設定
    /// </summary>
    /// <param name="kernelSize">PCF カーネルサイズ</param>
    void SetPCFKernelSize(int kernelSize);

    /// <summary>
    /// 最大シャドウ距離を設定
    /// </summary>
    /// <param name="distance">最大シャドウ距離</param>
    void SetMaxShadowDistance(float distance) { maxShadowDistance_ = distance; }

    /// <summary>
    /// 最大シャドウ距離を取得
    /// </summary>
    /// <returns>最大シャドウ距離</returns>
    float GetMaxShadowDistance() const { return maxShadowDistance_; }

    /// <summary>
    /// ShadowMap を取得
    /// </summary>
    /// <returns>ShadowMap ポインタ</returns>
    ShadowMap* GetShadowMap() { return shadowMap_.get(); }

    /// <summary>
    /// ImGui でのデバッグ表示
    /// </summary>
    void DrawImGui();

    /// <summary>
    /// インスタンシング用シャドウレンダリング設定を適用
    /// </summary>
    void SetInstancedRenderState();

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

    /// <summary>
    /// インスタンシング用シャドウルートシグネチャの作成
    /// </summary>
    void CreateShadowInstancedRootSignature();

    /// <summary>
    /// インスタンシング用シャドウパイプラインステートの作成
    /// </summary>
    void CreateShadowInstancedPipelineState();

private:
    DX12Basic* dx12_ = nullptr;          ///< DirectX12基盤システムへの参照
    std::unique_ptr<ShadowMap> shadowMap_;     ///< シャドウマップ管理クラスへのポインタ
    Light* light_ = nullptr;             ///< ライトシステムへの参照（ライト位置・方向取得用）
    Camera* camera_ = nullptr;           ///< カメラへの参照（視錐台カリング用）

    Microsoft::WRL::ComPtr<ID3D12RootSignature> shadowRootSignature_;  ///< 通常シャドウ描画用ルートシグネチャ
    Microsoft::WRL::ComPtr<ID3D12PipelineState> shadowPipelineState_;   ///< 通常シャドウ描画用パイプラインステート

    Microsoft::WRL::ComPtr<ID3D12RootSignature> shadowInstancedRootSignature_; ///< インスタンシングシャドウ描画用ルートシグネチャ
    Microsoft::WRL::ComPtr<ID3D12PipelineState> shadowInstancedPipelineState_;  ///< インスタンシングシャドウ描画用パイプラインステート

    Microsoft::WRL::ComPtr<ID3D12Resource> shadowConstantBuffer_; ///< シャドウパラメータ用定数バッファ

    /// <summary>
    /// シャドウレンダリング用定数バッファ構造体
    /// GPU 側に送信されるシャドウ設定パラメータ
    /// </summary>
    struct ShadowConstants {
        Matrix4x4 lightViewProj; ///< ライト空間のビュープロジェクション行列
        float shadowBias;        ///< シャドウバイアス（シャドウアクネ防止）
        int enableShadow;        ///< シャドウ有効フラグ（0=無効, 1=有効）
        Vector2 shadowMapSize;   ///< シャドウマップ解像度（テクセルサイズ計算用）
        float normalOffsetBias;  ///< 法線オフセットバイアス（ピーターパニング防止）
        float pcfKernelSize;     ///< PCF カーネルサイズ（フィルタリング品質）
        float padding[2];        ///< 16バイトアライメント用パディング
    };
    ShadowConstants* shadowConstantData_ = nullptr; ///< 定数バッファのマップ済みポインタ

    bool shadowEnabled_ = true;              ///< シャドウ有効/無効フラグ
    float shadowBias_ = 0.0001f;             ///< シャドウバイアス値
    float normalOffsetBias_ = 0.01f;         ///< 法線オフセットバイアス値
    float maxShadowDistance_ = 50.0f;        ///< 影を表示する最大距離（カメラからの距離）

    bool isRenderingShadow_ = false;         ///< 現在シャドウパス中かどうかのフラグ

    D3D12_CPU_DESCRIPTOR_HANDLE savedRTVHandle_; ///< 保存された元のレンダーターゲットビューハンドル
    D3D12_CPU_DESCRIPTOR_HANDLE savedDSVHandle_; ///< 保存された元の深度ステンシルビューハンドル
    bool hasSavedRenderTargets_ = false;     ///< レンダーターゲットが保存されているかのフラグ
};

} // namespace Tako
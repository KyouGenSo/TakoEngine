#pragma once
#include <d3d12.h>
#include <wrl.h>
#include <memory>
#include "Matrix4x4.h"
#include "Vector2.h"
#include "ShadowMap.h"

namespace Tako {

  class DX12Basic;
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
    static std::unique_ptr<ShadowRenderer> instance_;

    ShadowRenderer() = default;
    ~ShadowRenderer() = default;
    ShadowRenderer(ShadowRenderer&) = delete;
    ShadowRenderer& operator=(ShadowRenderer&) = delete;

    friend struct std::default_delete<ShadowRenderer>;

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
    /// ImGui でのデバッグ表示
    /// </summary>
    void DrawImGui();

    /// <summary>
    /// インスタンシング用シャドウレンダリング設定を適用
    /// </summary>
    void SetInstancedRenderState();

  private:
    /// <summary>
    /// シャドウ用ルートシグネチャの作成（通常 / インスタンシング共通）
    /// </summary>
    /// <param name="instanced">true の場合、末尾にインスタンスデータ用 SRV テーブル（t5）を追加</param>
    /// <returns>生成したルートシグネチャ</returns>
    Microsoft::WRL::ComPtr<ID3D12RootSignature> CreateShadowRootSignature(bool instanced);

    /// <summary>
    /// シャドウ用パイプラインステート（深度のみ）の作成（通常 / インスタンシング共通）
    /// </summary>
    /// <param name="rootSignature">バインドするルートシグネチャ</param>
    /// <param name="vsFileName">使用する頂点シェーダーのファイル名</param>
    /// <returns>生成したパイプラインステート</returns>
    Microsoft::WRL::ComPtr<ID3D12PipelineState> CreateShadowPipelineState(
        ID3D12RootSignature* rootSignature, const wchar_t* vsFileName);

    /// <summary>
    /// 定数バッファの作成
    /// </summary>
    void CreateConstantBuffer();

  private: // 定数構造
    /// <summary>
    /// シャドウレンダリング用定数バッファ構造体（HLSL register b4 の唯一の対応構造体）
    /// GPU 側に送信されるシャドウ設定パラメータ。
    /// 注意: このレイアウトは ShadowMap.VS.hlsl / ShadowMapInstanced.VS.hlsl /
    /// Object3d.hlsli の cbuffer と一致させること（現状は lightViewProj のみが VS で参照される）。
    /// </summary>
    struct ShadowConstants {
      Matrix4x4 lightViewProj; ///< ライト空間のビュープロジェクション行列
      float shadowBias;        ///< シャドウバイアス（深度比較の最小マージン）
      int enableShadow;        ///< シャドウ有効フラグ（0=無効, 1=有効）
      Vector2 shadowMapSize;   ///< シャドウマップ解像度（テクセルサイズ計算用）
      float pcfKernelSize;     ///< PCF カーネルサイズ（フィルタリング品質）
      float padding[1];        ///< 16バイトアライメント用パディング
    };
    ShadowConstants* shadowConstantData_ = nullptr; ///< 定数バッファのマップ済みポインタ

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

    bool shadowEnabled_ = true;              ///< シャドウ有効/無効フラグ
    float shadowBias_ = 0.0001f;             ///< 深度比較の最小マージン（数値誤差吸収用のごく小さい固定値）

    float maxShadowDistance_ = 50.0f;        ///< 影を表示する最大距離（カメラからの距離）

    bool isRenderingShadow_ = false;         ///< 現在シャドウパス中かどうかのフラグ

    D3D12_CPU_DESCRIPTOR_HANDLE savedRTVHandle_; ///< 保存された元のレンダーターゲットビューハンドル
    D3D12_CPU_DESCRIPTOR_HANDLE savedDSVHandle_; ///< 保存された元の深度ステンシルビューハンドル
    bool hasSavedRenderTargets_ = false;     ///< レンダーターゲットが保存されているかのフラグ
  };

} // namespace Tako
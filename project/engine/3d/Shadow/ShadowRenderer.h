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

    struct Token {};  ///< 外部からの直接生成を防ぐ生成キー
    ~ShadowRenderer() = default;
    ShadowRenderer(ShadowRenderer&) = delete;
    ShadowRenderer& operator=(ShadowRenderer&) = delete;

    friend struct std::default_delete<ShadowRenderer>;

  public:
    explicit ShadowRenderer(Token) {}

  public: //定数
    // シャドウパス RS のルートパラメータ番号

  private: //構造体
    /// <summary>
    /// シャドウレンダリング用定数バッファ構造体
    /// GPU 側に送信されるシャドウ設定パラメータ。
    /// </summary>
    struct ShadowConstants {
      Matrix4x4 lightViewProj; ///< ライト空間のビュープロジェクション行列
      float shadowBias;        ///< シャドウバイアス（深度比較の最小マージン）
      int enableShadow;        ///< シャドウ有効フラグ（0=無効, 1=有効）
      Vector2 shadowMapSize;   ///< シャドウマップ解像度（テクセルサイズ計算用）
      float pcfKernelSize;     ///< PCF カーネルサイズ（フィルタリング品質）
      float padding[1];        ///< 16バイトアライメント用パディング
    };

  public: //メンバー関数
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
    /// ImGui でのデバッグ表示
    /// </summary>
    void DrawImGui();

    //=======================================
    //Setter
    //=======================================
    void SetLight(Light* light) { light_ = light; }
    void SetCamera(Camera* camera) { camera_ = camera; }

    /// <summary>
    /// 通常レンダリング時のシャドウ設定を適用
    /// </summary>
    void SetShadowForMainPass();

    void SetEnabled(bool enabled) { shadowEnabled_ = enabled; }

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

    void SetMaxShadowDistance(float distance) { maxShadowDistance_ = distance; }
    void SetShadowBias(float bias) { shadowBias_ = bias; }

    //=======================================
    //Getter
    //=======================================
    bool IsRenderingShadow() const { return isRenderingShadow_; }
    bool IsEnabled() const { return shadowEnabled_; }
    float GetMaxShadowDistance() const { return maxShadowDistance_; }
    float GetShadowBias() const { return shadowBias_; }

    /// <summary>
    /// 現在のシャドウマップ解像度（サイズ変更は次フレームで反映されるため、変更直後は旧値）
    /// </summary>
    uint32_t GetShadowMapSize() const { return shadowMap_ ? shadowMap_->GetShadowMapSize() : ShadowMap::DEFAULT_SHADOW_MAP_SIZE; }
    D3D12_GPU_VIRTUAL_ADDRESS GetShadowConstantsAddress() const { return shadowConstantBuffer_->GetGPUVirtualAddress(); }
    int GetPCFKernelSize() const { return shadowMap_ ? shadowMap_->GetPCFKernelSize() : 1; }

  private:

    /// <summary>
    /// 定数バッファの作成
    /// </summary>
    void CreateConstantBuffer();

  private:
    ShadowConstants* shadowConstantData_ = nullptr;  ///< 定数バッファのマップ済みポインタ

    DX12Basic*                 dx12_      = nullptr;  ///< DirectX12基盤システムへの参照
    std::unique_ptr<ShadowMap> shadowMap_;            ///< シャドウマップ管理クラスへのポインタ
    Light*                     light_     = nullptr;  ///< ライトシステムへの参照（ライト位置・方向取得用）
    Camera*                    camera_    = nullptr;  ///< カメラへの参照（視錐台カリング用）

    Microsoft::WRL::ComPtr<ID3D12Resource> shadowConstantBuffer_;  ///< シャドウパラメータ用定数バッファ

    bool  shadowEnabled_ = true;     ///< シャドウ有効/無効フラグ
    float shadowBias_    = 0.0001f;  ///< 深度比較の最小マージン（数値誤差吸収用のごく小さい固定値）

    float maxShadowDistance_ = 50.0f;  ///< 影を表示する最大距離（カメラからの距離）

    bool isRenderingShadow_ = false;  ///< 現在シャドウパス中かどうかのフラグ
  };

} // namespace Tako

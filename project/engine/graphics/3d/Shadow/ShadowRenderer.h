#pragma once
#include <d3d12.h>
#include <wrl.h>
#include <array>
#include <cstdint>
#include <memory>
#include <span>
#include <vector>
#include "Frustum.h"
#include "Matrix4x4.h"
#include "Vector2.h"
#include "Vector3.h"
#include "Vector4.h"
#include "ShadowMap.h"

namespace Tako {

  class DX12Basic;
  class Light;
  class Camera;

  /// <summary>
  /// 影の描画をまとめるクラス
  /// 平行光源のカスケード・スポットライト・点光源の 1 面を、どれも同じ「区画」として 2 枚のアトラスに並べる
  /// 区画ごとに、影を描く視点と、影を受ける側のシェーダーへ渡す定数を作る
  /// </summary>
  class TAKO_API ShadowRenderer
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
    static constexpr float    kDefaultMaxShadowDistance = 50.0f;
    static constexpr int32_t  kMaxCascades              = 4;   ///< 平行光源のカスケードの上限
    static constexpr uint32_t kMaxLocalSlices           = 16;  ///< スポットライト（1 枚）と点光源（6 枚）に使える区画の数

  private: //定数
    static constexpr uint32_t kFirstLocalSlice = kMaxCascades;  ///< 区画の並びは、カスケードの後ろにスポットライト・点光源
    static constexpr uint32_t kMaxSlices       = kFirstLocalSlice + kMaxLocalSlices;
    static constexpr uint32_t kCascadeGrid     = 2;  ///< カスケードが 2 枚以上のときの、アトラスの 1 辺の区画数
    static constexpr uint32_t kLocalGrid       = 4;  ///< スポットライト・点光源用のアトラスの 1 辺の区画数
    static constexpr uint32_t kPointLightFaces = 6;

  public: //構造体
    /// <summary>
    /// 影を描く視点 1 つ（区画 1 つぶん）
    /// </summary>
    struct ShadowView {
      D3D12_GPU_VIRTUAL_ADDRESS constants  = 0;      ///< gShadowCaster
      bool                      isLocal    = false;  ///< true ならスポットライト・点光源用のアトラスへ描く
      uint32_t                  x          = 0;      ///< 区画の左上（画素）
      uint32_t                  y          = 0;
      uint32_t                  size       = 0;      ///< 区画の 1 辺（画素）
      Frustum                   frustum{};           ///< 影を描く範囲。境界球がこの外にある描画物は描かない（平行光源は手前の面を使わない）
    };

  private: //構造体
    /// <summary>
    /// 影を受ける側が読む区画 1 つ（Shadow.hlsli の ShadowSlice と一致させること）
    /// </summary>
    struct ShadowSlice {
      Matrix4x4 worldToAtlas;  ///< ワールド → アトラスの UV と深度
      Vector4   uvBounds;      ///< 区画の UV の範囲（xy: 最小、zw: 最大）。隣の区画を読まないよう半テクセル内側
      Vector4   params;        ///< x: 1 テクセルのワールドでの大きさ（透視なら距離 1 での値）、y: 透視なら 1
    };

    /// <summary>
    /// 影を受ける側へ渡す定数（Shadow.hlsli の ShadowConstants と一致させること）
    /// </summary>
    struct ShadowConstants {
      ShadowSlice slices[kMaxSlices];            ///< 0〜3: カスケード、4〜19: スポットライト・点光源
      Vector4     cascadeSpheres[kMaxCascades];  ///< xyz: 中心、w: 半径の 2 乗
      Vector3     cameraPosition;
      float       maxDistance;
      Vector2     atlasTexel;
      Vector2     localAtlasTexel;
      int32_t     enableShadow;
      int32_t     cascadeCount;
      int32_t     pcfKernelSize;
      int32_t     debugCascades;
      float       depthBias;
      float       normalBias;
      float       padding[2];
    };
    static_assert(sizeof(ShadowConstants) == 2048);

    /// <summary>
    /// 影を描く側へ渡す定数（ShadowMap.VS.hlsl の gShadowCaster と一致させること）
    /// </summary>
    struct ShadowCasterConstants {
      Matrix4x4 lightViewProj;
      float     pancake;  ///< 1 なら near 面より手前の頂点を near 面で止める
      float     padding[3];
    };

  public: //メンバー関数
    /// <summary>
    /// インスタンスの取得
    /// </summary>
    static ShadowRenderer* GetInstance();

    void Initialize(DX12Basic* dx12);

    /// <summary>
    /// 解像度の変更で予約されたシャドウマップの作り直しを行う
    /// </summary>
    void Update();

    void Finalize();

    /// <summary>
    /// このフレームの影の視点と定数を作る。ライトの登録が済んだ後、影を描く前に 1 回呼ぶ
    /// </summary>
    void Prepare();

    /// <summary>
    /// view の区画へ描き始める（描画先・ビューポートを区画に合わせ、区画の深度を消す）
    /// </summary>
    void BeginView(const ShadowView& view);

    /// <summary>
    /// 影の描画を終え、呼び出し前の描画先へ戻す
    /// </summary>
    void EndShadowPass();

    /// <summary>
    /// ImGui でのデバッグ表示
    /// </summary>
    void DrawImGui();

    //===================================================
    //Setter
    //===================================================
    void SetLight(Light* light) { light_ = light; }
    void SetCamera(Camera* camera) { camera_ = camera; }

    /// <summary>
    /// 通常レンダリング時のシャドウ設定を適用
    /// </summary>
    /// <param name="receiveShadow">false なら影を受けない定数を渡す（本編のライトを使わない視点）</param>
    void SetShadowForMainPass(bool receiveShadow = true);

    void SetEnabled(bool enabled) { shadowEnabled_ = enabled; }

    /// <summary>
    /// シャドウ品質を設定
    /// </summary>
    /// <param name="quality">品質レベル（0-4）</param>
    void SetShadowQuality(int quality);

    /// <summary>
    /// 平行光源用のアトラスの解像度を設定（カスケードが 2 枚以上なら、1 枚はこの半分の辺）
    /// </summary>
    void SetShadowMapSize(uint32_t size);

    /// <summary>
    /// スポットライト・点光源用のアトラスの解像度を設定（区画 1 枚はこの 1/4 の辺）
    /// </summary>
    void SetLocalShadowMapSize(uint32_t size);

    void SetPCFKernelSize(int kernelSize);
    void SetMaxShadowDistance(float distance) { maxShadowDistance_ = distance; }

    /// <summary>
    /// カスケードの数を設定（1〜kMaxCascades にクランプ）
    /// </summary>
    void SetCascadeCount(int32_t count);

    /// <summary>
    /// カスケードの切り方を設定（0 で等間隔、1 で手前ほど細かい対数の間隔）
    /// </summary>
    void SetCascadeSplit(float split);

    /// <summary>
    /// 影を読む位置を光源の向きへずらす量を設定（テクセル単位）
    /// </summary>
    void SetDepthBias(float bias) { depthBias_ = bias; }

    /// <summary>
    /// 影を読む位置を法線の向きへずらす量を設定（テクセル単位）
    /// </summary>
    void SetNormalBias(float bias) { normalBias_ = bias; }

    /// <summary>
    /// 3 枚目以降のカスケードを 1 フレームおきに描き直すか（動く物の遠い影が 1 フレーム遅れる）
    /// </summary>
    void SetStaggerFarCascades(bool stagger) { staggerFarCascades_ = stagger; }

    //===================================================
    //Getter
    //===================================================
    bool IsRenderingShadow() const { return isRenderingShadow_; }
    bool IsEnabled() const { return shadowEnabled_; }
    float GetMaxShadowDistance() const { return maxShadowDistance_; }
    int32_t GetCascadeCount() const { return cascadeCount_; }
    float GetCascadeSplit() const { return cascadeSplit_; }
    float GetDepthBias() const { return depthBias_; }
    float GetNormalBias() const { return normalBias_; }
    bool IsStaggerFarCascades() const { return staggerFarCascades_; }

    /// <summary>
    /// 現在のシャドウマップ解像度（サイズ変更は次フレームで反映されるため、変更直後は旧値）
    /// </summary>
    uint32_t GetShadowMapSize() const { return shadowMap_ ? shadowMap_->GetShadowMapSize() : ShadowMap::DEFAULT_SHADOW_MAP_SIZE; }
    uint32_t GetLocalShadowMapSize() const { return localShadowMap_ ? localShadowMap_->GetShadowMapSize() : ShadowMap::DEFAULT_SHADOW_MAP_SIZE; }
    int GetPCFKernelSize() const { return shadowMap_ ? shadowMap_->GetPCFKernelSize() : 1; }

    /// <summary>
    /// このフレームに描く影の視点（Prepare で作る）
    /// </summary>
    std::span<const ShadowView> GetViews() const { return views_; }

  private: //非公開関数
    void CreateConstantBuffer();

    /// <summary>
    /// 平行光源のカスケードを、カメラの視錐台の切片ごとに作る
    /// </summary>
    void PrepareCascades();

    /// <summary>
    /// 影を落とすスポットライト・点光源へ、カメラに近い順に区画を割り当てる
    /// </summary>
    void PrepareLocalLights();

    /// <summary>
    /// 区画 1 つぶんの、影を受ける側の定数（slices[sliceIndex]）と、影を描く視点を作る
    /// </summary>
    /// <param name="tileIndex">アトラスの中の区画の番号（左上から右へ、次の段へ）</param>
    /// <param name="texelWorld">1 テクセルのワールドでの大きさ（透視なら距離 1 での値）</param>
    void AddSlice(uint32_t sliceIndex, const Matrix4x4& viewProj, bool isLocal, uint32_t tileIndex, float texelWorld, bool perspective);

  private: //メンバー変数
    ShadowConstants* shadowConstantData_ = nullptr;  ///< 定数バッファのマップ済みポインタ
    ShadowConstants  constants_{};                   ///< 定数の控え。描き直さなかったカスケードは前の値を残す（マップ先は読むと遅いので、ここで組んでから写す）

    DX12Basic*                 dx12_           = nullptr;
    std::unique_ptr<ShadowMap> shadowMap_;                 ///< 平行光源のカスケード用アトラス
    std::unique_ptr<ShadowMap> localShadowMap_;            ///< スポットライト・点光源用アトラス
    Light*                     light_          = nullptr;  ///< 本編のライト
    Camera*                    camera_         = nullptr;  ///< カスケードの範囲を決める本編カメラ

    Microsoft::WRL::ComPtr<ID3D12Resource> shadowConstantBuffer_;    ///< gShadowConstants
    Microsoft::WRL::ComPtr<ID3D12Resource> disabledConstantBuffer_;  ///< 影を受けない視点へ渡す、ゼロ埋めの gShadowConstants

    std::vector<ShadowView> views_;         ///< このフレームに描く視点
    std::vector<uint32_t>   requestOrder_;  ///< PrepareLocalLights の作業領域

    //設定
    bool    shadowEnabled_      = true;
    float   maxShadowDistance_  = kDefaultMaxShadowDistance;  ///< 影を表示する最大距離（カメラからの距離）。シーンの平行光源が決める
    int32_t cascadeCount_       = kMaxCascades;
    float   cascadeSplit_       = 0.5f;                       ///< 0: 等間隔、1: 対数の間隔
    float   depthBias_          = 1.0f;                       ///< テクセル単位
    float   normalBias_         = 2.0f;                       ///< テクセル単位
    bool    staggerFarCascades_ = true;
    bool    showCascades_       = false;                      ///< カスケードの色分け表示（保存しない）

    //描画の状態
    bool     isRenderingShadow_ = false;
    bool     cascadesDirty_     = true;   ///< true なら次の Prepare で全部のカスケードを描き直す
    int32_t  lastCascadeCount_  = 0;
    uint32_t lastCascadeTile_   = 0;
    uint32_t frameIndex_        = 0;
    bool     overflowWarned_    = false;  ///< 区画が足りない警告を出したか（1 回だけ出す）
  };

} // namespace Tako

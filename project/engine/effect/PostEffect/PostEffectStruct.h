#pragma once
#include <cstdint>
#include <variant>
#include <d3d12.h>
#include <wrl.h>

#include "Matrix4x4.h"
#include "Vector2.h"
#include "vector3.h"
#include "Vector4.h"

namespace Tako {

  class DX12Basic;

  /// <summary>
  /// レンダーターゲット構造体
  /// ポストエフェクトの中間バッファとして使用
  /// </summary>
  struct RenderTexture {
    Microsoft::WRL::ComPtr<ID3D12Resource> resource;
    D3D12_CPU_DESCRIPTOR_HANDLE            rtvHandle{};
    uint32_t                               rtvIndex  = 0;
    uint32_t                               srvIndex  = 0;

    /// <summary>
    /// リソース生成と RTV/SRV の確保・ビュー生成
    /// </summary>
    /// <param name="dx12">DirectX 12基盤システムのポインタ</param>
    /// <param name="width">テクスチャの幅（ピクセル）</param>
    /// <param name="height">テクスチャの高さ（ピクセル）</param>
    /// <param name="format">ピクセルフォーマット</param>
    /// <param name="clearColor">クリアカラー</param>
    void Create(DX12Basic* dx12, uint32_t width, uint32_t height, DXGI_FORMAT format, const Vector4& clearColor);

    /// <summary>
    /// リソース解放と RTV/SRV インデックスの返却
    /// </summary>
    void Release();
  };

  /// <summary>
  /// ビネット効果パラメータ
  /// 画面周辺を暗くする効果
  /// </summary>
  struct VignetteParam
  {
    float   power;
    float   range;       ///< 0.0〜1.0
    float   padding[2];  ///< 16バイトアライメント用
    Vector3 color;       ///< RGB
    float   padding2;
  };

  /// <summary>
  /// Bloom 効果パラメータ
  /// 明るい部分を光らせる効果
  /// </summary>
  struct BloomParam
  {
    float   intensity;
    float   threshold;   ///< 高輝度抽出のしきい値
    float   sigma;       ///< ガウシアンブラーのシグマ
    int     kernelSize;
    Vector2 direction;   ///< ブラー方向
    int     padding1;
    int     padding2;
  };

  struct NewBloomParam
  {
    float   intensity;
    float   threshold;
    float   sigma;
    Vector2 direction;
    Vector2 texelSize;
    int     sampleCount;
    int     iteration;
    int     padding3;
    int     padding4;
  };

  struct HighLumExtrcatParam
  {
    float threshold;
  };

  struct GaussianBlurParam
  {
    float   sigma;
    int     kernelSize;
    Vector2 direction;
  };

  struct BloomCombineParam
  {
    float intensity;
  };

  // Shader 用のカメラ
  struct CameraForGPU
  {
    float nearPlane;
    float farPlane;
  };

  struct FogParam
  {
    Vector4 color;
    float   density;
  };

  /// <summary>
  /// ラジアルブラーパラメータ
  /// 画面中心から放射状にブラーをかける効果
  /// </summary>
  struct RadialBlurParam
  {
    Vector2 center;       ///< ブラー中心（スクリーン空間0.0〜1.0）
    float   blurWidth;
    int32_t sampleCount;  ///< サンプリング数（多いほど高品質）
  };

  /// <summary>
  /// 白黒フィルターパラメータ
  /// しきい値による2値化処理
  /// </summary>
  struct BWFilterParam
  {
    float threshold;  ///< 白黒の境界しきい値（0.0〜1.0）
  };

  /// <summary>
  /// RGB カラー分離効果パラメータ
  /// 色収差のような効果を生成
  /// </summary>
  struct RGBSplitParam
  {
    Vector2 redOffset;    ///< R チャンネルのオフセット（ピクセル単位）
    Vector2 greenOffset;  ///< G チャンネルのオフセット（ピクセル単位）
    Vector2 blueOffset;   ///< B チャンネルのオフセット（ピクセル単位）
    float   intensity;    ///< エフェクトの全体強度（0.0〜1.0）
  };

  struct LuminanceOutlineParam
  {
    float outlineThickness;
  };

  struct DepthOutlineParam
  {
    Matrix4x4 projectionInverse;
    float     outlineThickness;
  };

  /// <summary>
  /// ディゾルブ効果パラメータ
  /// 画像を徐々に溶かす/消失させる効果
  /// </summary>
  struct DissolveParam
  {
    float   threshold;      ///< 溶解のしきい値（0.0〜1.0）
    float   edgeThickness;  ///< 縁取りの太さ
    float   padding[2];
    Vector4 edgeColor;      ///< RGBA
  };

  /// <summary>
  /// ホワイトノイズパラメータ
  /// ランダムノイズを生成
  /// </summary>
  struct WhiteNoiseParam
  {
    float time;  ///< 時間（ノイズの変化に使用）
  };

  /// <summary>
  /// ハーフトーン効果パラメータ
  /// 印刷物のようなドットパターン効果
  /// </summary>
  struct HalfToneParam
  {
    float   dotSize;     ///< ドットのサイズ（大きいほど粗い）
    float   contrast;
    float   angle;       ///< ドットグリッドの回転角度（ラジアン）
    int32_t dotPattern;  ///< 0=円, 1=四角, 2=ダイヤモンド
    Vector2 screenSize;  ///< スクリーン解像度（ピクセル）
    int32_t colorMode;   ///< 0=モノクロ, 1=CMYK 風
    float   threshold;   ///< 明暗の閾値
    float   padding;
  };

  using EffectParam = std::variant<
    VignetteParam,
    BloomParam,
    NewBloomParam,
    FogParam,
    RadialBlurParam,
    BWFilterParam,
    RGBSplitParam,
    LuminanceOutlineParam,
    DepthOutlineParam,
    CameraForGPU,
    DissolveParam,
    WhiteNoiseParam,
    HalfToneParam,
    GaussianBlurParam,
    HighLumExtrcatParam,
    BloomCombineParam
  >;

  /// <summary>
  /// イージング種別
  /// 一時エフェクトのフェードアウト曲線を指定
  /// </summary>
  enum class EasingType {
    Linear,     ///< 線形補間
    EaseOut,    ///< イーズアウト（減速）
    EaseIn,     ///< イーズイン（加速）
    EaseInOut   ///< イーズインアウト
  };

} // namespace Tako
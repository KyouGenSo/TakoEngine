#pragma once
#include <cstdint>

#include "Matrix4x4.h"
#include "Vector2.h"
#include "vector3.h"
#include "Vector4.h"

namespace Tako {

  /// <summary>
  /// レンダーターゲット構造体
  /// ポストエフェクトの中間バッファとして使用
  /// </summary>
  struct RenderTexture {
    Microsoft::WRL::ComPtr<ID3D12Resource> resource;  ///< テクスチャリソース
    D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle;            ///< レンダーターゲットビューハンドル
    uint32_t srvIndex;                                ///< シェーダーリソースビューインデックス
  };

  /// <summary>
  /// ビネット効果パラメータ
  /// 画面周辺を暗くする効果
  /// </summary>
  struct VignetteParam
  {
    float power;       ///< ビネット強度
    float range;       ///< ビネット範囲（0.0〜1.0）
    float padding[2];  ///< 16バイトアライメント用
    Vector3 color;     ///< ビネット色（RGB）
    float padding2;    ///< パディング
  };

  /// <summary>
  /// ビネット＋赤色 Bloom 効果パラメータ
  /// </summary>
  struct VignetteRedBloomParam
  {
    float power;      ///< ビネット強度
    float range;      ///< ビネット範囲
    float threshold;  ///< Bloom しきい値
  };

  /// <summary>
  /// Bloom 効果パラメータ
  /// 明るい部分を光らせる効果
  /// </summary>
  struct BloomParam
  {
    float intensity;   ///< Bloom 強度
    float threshold;   ///< 高輝度抽出のしきい値
    float sigma;       ///< ガウシアンブラーのシグマ値（ぼかし強度）
    int kernelSize;    ///< ブラーカーネルサイズ
    Vector2 direction; ///< ブラー方向ベクトル
    int padding1;      ///< パディング
    int padding2;      ///< パディング
  };

  struct NewBloomParam
  {
    float intensity;
    float threshold;
    float sigma;
    Vector2 direction;
    Vector2 texelSize;
    int sampleCount;
    int iteration;
    int padding3;        // パディング追加
    int padding4;        // パディング追加
  };

  struct HighLumExtrcatParam
  {
    float threshold;
  };

  struct GaussianBlurParam
  {
    float sigma;
    int kernelSize;
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
    float density;
  };

  /// <summary>
  /// ラジアルブラーパラメータ
  /// 画面中心から放射状にブラーをかける効果
  /// </summary>
  struct RadialBlurParam
  {
    Vector2 center;       ///< ブラー中心座標（スクリーン空間0.0〜1.0）
    float blurWidth;      ///< ブラーの幅
    int32_t sampleCount;  ///< サンプリング数（品質）
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
    Vector2 redOffset;   ///< R チャンネルのオフセット（ピクセル単位）
    Vector2 greenOffset; ///< G チャンネルのオフセット（ピクセル単位）
    Vector2 blueOffset;  ///< B チャンネルのオフセット（ピクセル単位）
    float intensity;     ///< エフェクトの全体強度（0.0〜1.0）
  };

  struct LuminanceOutlineParam
  {
    float outlineThickness;
  };

  struct DepthOutlineParam
  {
    Matrix4x4 projectionInverse;
    float outlineThickness;
  };

  /// <summary>
  /// ディゾルブ効果パラメータ
  /// 画像を徐々に溶かす/消失させる効果
  /// </summary>
  struct DissolveParam
  {
    float threshold;      ///< 溶解のしきい値（0.0〜1.0）
    float edgeThickness;  ///< エッジ（縁取り）の太さ
    float padding[2];     ///< パディング
    Vector4 edgeColor;    ///< エッジの色（RGBA）
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
    float dotSize;          ///< ドットのサイズ（大きいほど粗い）
    float contrast;         ///< コントラスト調整
    float angle;            ///< ドットグリッドの回転角度（ラジアン）
    int32_t dotPattern;     ///< ドットパターン (0=円, 1=四角, 2=ダイヤモンド)
    Vector2 screenSize;     ///< スクリーン解像度（ピクセル）
    int32_t colorMode;      ///< カラーモード (0=モノクロ, 1=CMYK 風)
    float threshold;        ///< 明暗の閾値調整
    float padding;          ///< パディング
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
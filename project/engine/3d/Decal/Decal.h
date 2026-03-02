#pragma once
#include <d3d12.h>
#include <wrl.h>
#include <cstdint>
#include <string>

#include "Transform.h"
#include "Vector4.h"
#include "Matrix4x4.h"

namespace Tako {

  /// <summary>
  /// デカール形状の種類
  /// </summary>
  enum class DecalShape {
    Circle = 0,    ///< 円形
    Fan = 1,       ///< 扇形
    Rectangle = 2, ///< 矩形
  };

  /// <summary>
  /// 個別デカールインスタンスクラス
  /// Projective Decal の位置、形状、カラーなどを管理
  /// </summary>
  class Decal {
  public:
    /// <summary>
    /// 初期化
    /// </summary>
    void Initialize();

    /// <summary>
    /// 更新（定数バッファの再計算）
    /// </summary>
    void Update();

    /// <summary>
    /// 描画
    /// </summary>
    void Draw();

    /// <summary>
    /// ImGui でパラメータをデバッグ表示・編集
    /// </summary>
    void DrawImGui();

    /// <summary>
    /// デバッグ描画（Draw2D による投影ボリュームのワイヤーフレーム表示）
    /// </summary>
    void DrawDebug();

    /// <summary>
    /// プロシージャルモードに戻す
    /// </summary>
    void ClearTexture();

    // ===== Getters =====
    const Transform& GetTransform() const { return transform_; }
    const Vector4& GetColor() const { return color_; }
    DecalShape GetShape() const { return shape_; }
    float GetFanHalfAngle() const { return fanHalfAngle_; }
    float GetEdgeSoftness() const { return edgeSoftness_; }
    bool IsVisible() const { return isVisible_; }
    bool IsUseTexture() const { return useTexture_; }

    // ===== Setters =====
    /// <summary>
    /// 位置を設定
    /// </summary>
    void SetTranslate(const Vector3& translate) { transform_.translate = translate; }

    /// <summary>
    /// 回転を設定
    /// </summary>
    void SetRotate(const Vector3& rotate) { transform_.rotate = rotate; }

    /// <summary>
    /// スケールを設定
    /// </summary>
    void SetScale(const Vector3& scale) { transform_.scale = scale; }

    /// <summary>
    /// カラーを設定
    /// </summary>
    void SetColor(const Vector4& color) { color_ = color; }

    /// <summary>
    /// 形状を設定
    /// </summary>
    void SetShape(DecalShape shape) { shape_ = shape; }

    /// <summary>
    /// 扇形の半角を設定（ラジアン）
    /// </summary>
    void SetFanHalfAngle(float halfAngle) { fanHalfAngle_ = halfAngle; }

    /// <summary>
    /// エッジのぼかし量を設定
    /// </summary>
    void SetEdgeSoftness(float softness) { edgeSoftness_ = softness; }

    /// <summary>
    /// 表示/非表示を設定
    /// </summary>
    void SetVisible(bool visible) { isVisible_ = visible; }

    /// <summary>
    /// デバッグ描画の表示/非表示を設定
    /// </summary>
    void SetDebugViewVisible(bool visible) { isDebugViewVisible_ = visible; }

    /// <summary>
    /// テクスチャモードに切り替え（TextureManager で事前ロード済みのテクスチャを使用）
    /// </summary>
    /// <param name="textureName">テクスチャ名</param>
    void SetTexture(const std::string& textureName);

  private:

    /// <summary>
    /// GPU に送る DecalData 構造体
    /// </summary>
    struct DecalDataGPU {
      Matrix4x4 decalWorldInverse; ///< ワールド→デカールローカル変換
      Matrix4x4 decalWVP;         ///< キューブの WVP
      Vector4 color;               ///< デカールカラー
      int32_t shapeType;           ///< 形状タイプ
      float fanHalfAngle;          ///< 扇形の半角
      float edgeSoftness;          ///< エッジのぼかし量
      int32_t useTexture;          ///< テクスチャモードフラグ
    };

    Transform transform_{ {1.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f} }; ///< 位置/回転/スケール

    Vector4 color_{ 1.0f, 0.2f, 0.1f, 0.5f }; ///< デカールカラー（デフォルト: 赤系半透明）

    DecalShape shape_ = DecalShape::Circle; ///< 形状

    float fanHalfAngle_ = 1.5708f; ///< 扇形の半角（デフォルト: π/2 = 90°）

    float edgeSoftness_ = 0.05f; ///< エッジのぼかし量

    bool useTexture_ = false; ///< テクスチャモードフラグ

    uint32_t textureSrvIndex_ = 0; ///< テクスチャの SRV インデックス

    bool isVisible_ = true; ///< 表示フラグ

    bool isDebugViewVisible_ = false;

    // 定数バッファ
    Microsoft::WRL::ComPtr<ID3D12Resource> decalDataBuffer_; ///< DecalData 定数バッファリソース
    DecalDataGPU* decalDataMapped_ = nullptr; ///< マップ済みポインタ
  };

} // namespace Tako

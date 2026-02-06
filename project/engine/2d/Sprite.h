#pragma once
#include"Vector4.h"
#include"Vector2.h"
#include"Vector3.h"
#include"Mat4x4Func.h"
#include "Transform.h"
#include<cstdint>
#include<string>
#include <d3d12.h>
#include<wrl.h>

namespace Tako {

/// <summary>
/// 2Dスプライト描画クラス。テクスチャの表示、切り抜き、反転などの機能を提供
/// </summary>
class Sprite {
private: // 構造体
  /// <summary>
  /// 頂点データ構造体
  /// </summary>
  struct VertexData
  {
    Vector4 position;
    Vector2 texcoord;
  };

  /// <summary>
  /// マテリアルデータ構造体
  /// </summary>
  struct Material
  {
    Vector4 color;
    Matrix4x4 uvTransform;
  };

  /// <summary>
  /// 座標変換行列データ構造体
  /// </summary>
  struct TransformationMatrix
  {
    Matrix4x4 WVP;
    Matrix4x4 world;
  };

public: // メンバー関数

  // ComPtrのエイリアス
  template<class T> using ComPtr = Microsoft::WRL::ComPtr<T>;

  /// <summary>
  /// 初期化
  /// </summary>
  /// <param name="texturePath">テクスチャファイルのパス</param>
  void Initialize(const std::string& texturePath);

  /// <summary>
  /// 更新
  /// </summary>
  void Update();

  /// <summary>
  /// 描画
  /// </summary>
  void Draw();

  /// <summary>
  /// ImGuiでパラメータ調整
  /// </summary>
  void DrawImGui();

  //-----------------------------------Getters-----------------------------------//
  /// <summary>
  /// トランスフォームの取得
  /// </summary>
  /// <returns>現在のトランスフォーム</returns>
  [[nodiscard]] Transform GetTransform() const { return transform_; }

  /// <summary>
  /// マテリアルデータの取得
  /// </summary>
  /// <returns>マテリアルデータへのポインタ</returns>
  [[nodiscard]] Material* GetMaterialData() const { return materialData_; }

  /// <summary>
  /// 色の取得
  /// </summary>
  /// <returns>現在の色（RGBA）</returns>
  [[nodiscard]] Vector4 GetColor() const { return materialData_->color; }

  /// <summary>
  /// 座標の取得
  /// </summary>
  /// <returns>現在の座標</returns>
  [[nodiscard]] Vector2 GetPos() const { return pos_; }

  /// <summary>
  /// 回転角度の取得
  /// </summary>
  /// <returns>現在の回転角度（ラジアン）</returns>
  [[nodiscard]] float GetRotation() const { return rotation_; }

  /// <summary>
  /// サイズの取得
  /// </summary>
  /// <returns>現在のサイズ</returns>
  [[nodiscard]] Vector2 GetSize() const { return size_; }

  /// <summary>
  /// アンカーポイントの取得
  /// </summary>
  /// <returns>現在のアンカーポイント</returns>
  [[nodiscard]] Vector2 GetAnchorPoint() const { return anchorPoint_; }

  /// <summary>
  /// 左右反転フラグの取得
  /// </summary>
  /// <returns>左右反転されている場合 true</returns>
  [[nodiscard]] bool GetIsFlipX() const { return isFlipX_; }

  /// <summary>
  /// 上下反転フラグの取得
  /// </summary>
  /// <returns>上下反転されている場合 true</returns>
  [[nodiscard]] bool GetIsFlipY() const { return isFlipY_; }

  /// <summary>
  /// テクスチャ切り取り左上座標の取得
  /// </summary>
  /// <returns>テクスチャ切り取り範囲の左上座標</returns>
  [[nodiscard]] Vector2 GetTexLeftTop() const { return texTopLeft_; }

  /// <summary>
  /// テクスチャ切り取りサイズの取得
  /// </summary>
  /// <returns>テクスチャ切り取りサイズ</returns>
  [[nodiscard]] Vector2 GetTexCutSize() const { return texCutSize_; }

  //-----------------------------------Setters-----------------------------------//
  /// <summary>
  /// トランスフォームの設定
  /// </summary>
  /// <param name="transform">設定するトランスフォーム</param>
  void SetTransform(const Transform& transform) { transform_ = transform; }

  /// <summary>
  /// 色の設定
  /// </summary>
  /// <param name="color">設定する色（RGBA）</param>
  void SetColor(const Vector4& color) { materialData_->color = color; }

  /// <summary>
  /// アルファ値の設定
  /// </summary>
  /// <param name="alpha">設定するアルファ値（0.0～1.0）</param>
  void SetAlpha(const float alpha) { materialData_->color.w = alpha; }

  /// <summary>
  /// 座標の設定
  /// </summary>
  /// <param name="pos">設定する座標</param>
  void SetPos(const Vector2& pos) { pos_ = pos; }

  /// <summary>
  /// 回転角度の設定
  /// </summary>
  /// <param name="rotation">設定する回転角度（ラジアン）</param>
  void SetRotation(const float rotation) { rotation_ = rotation; }

  /// <summary>
  /// サイズの設定
  /// </summary>
  /// <param name="size">設定するサイズ</param>
  void SetSize(const Vector2& size) { size_ = size; }

  /// <summary>
  /// アンカーポイントの設定
  /// </summary>
  /// <param name="anchorPoint">設定するアンカーポイント</param>
  void SetAnchorPoint(const Vector2& anchorPoint) { anchorPoint_ = anchorPoint; }

  /// <summary>
  /// 左右反転フラグの設定
  /// </summary>
  /// <param name="isFlipX">左右反転する場合 true</param>
  void SetIsFlipX(const bool isFlipX) { isFlipX_ = isFlipX; }

  /// <summary>
  /// 上下反転フラグの設定
  /// </summary>
  /// <param name="isFlipY">上下反転する場合 true</param>
  void SetIsFlipY(const bool isFlipY) { isFlipY_ = isFlipY; }

  /// <summary>
  /// テクスチャ切り取り左上座標の設定
  /// </summary>
  /// <param name="texTopLeft">テクスチャ切り取り範囲の左上座標</param>
  void SetTexLeftTop(const Vector2& texTopLeft) { texTopLeft_ = texTopLeft; }

  /// <summary>
  /// テクスチャ切り取りサイズの設定
  /// </summary>
  /// <param name="texCutSize">テクスチャ切り取りサイズ</param>
  void SetTexCutSize(const Vector2& texCutSize) { texCutSize_ = texCutSize; }

private: // プライベートメンバー関数

  /// <summary>
  /// 頂点データを生成
  /// </summary>
  void CreateVertexData();

  /// <summary>
  /// マテリアルデータを生成
  /// </summary>
  void CreateMaterialData();

  /// <summary>
  /// 座標変換行列データを生成
  /// </summary>
  void CreateTransformationMatrixData();

  ///　<summary>
  ///　画像切り取り範囲をぴったりにする
  /// </summary>
  void FitTexCutSize();

private:// メンバー変数

  ///< Transform
  Transform transform_ = {};

  ///< ファイルパス
  std::string texturePath_;

  ///< バッファリソース
  ComPtr<ID3D12Resource> vertexResource_;
  ComPtr<ID3D12Resource> indexResource_;
  ComPtr<ID3D12Resource> materialResource_;
  ComPtr<ID3D12Resource> transformationMatrixResource_;

  ///< バッファリソース内のデータを参照するためのポインタ
  VertexData* vertexData_ = nullptr;
  uint32_t* indexData_ = nullptr;
  Material* materialData_ = nullptr;
  TransformationMatrix* transformationMatrixData_ = nullptr;

  ///< 頂点バッファビュー
  D3D12_VERTEX_BUFFER_VIEW vertexBufferView_ = {};

  ///< インデックスバッファビュー
  D3D12_INDEX_BUFFER_VIEW indexBufferView_ = {};

  ///< テクスチャ番号
  uint32_t textureIndex_ = 0;

  ///< 座標
  Vector2 pos_ = {};

  ///< 回転
  float rotation_ = 0.0f;

  ///< サイズ
  Vector2 size_ = {};

  ///< アンカーポイント
  Vector2 anchorPoint_ = {};

  ///< テクスチャの左上座標
  Vector2 texTopLeft_ = {};

  ///< テクスチャの切り出しサイズ
  Vector2 texCutSize_ = {};

  ///< 左右反転
  bool isFlipX_ = false;

  ///< 上下反転
  bool isFlipY_ = false;
};

} // namespace Tako

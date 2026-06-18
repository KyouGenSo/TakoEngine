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
  /// 2D スプライト描画クラス。テクスチャの表示、切り抜き、反転などの機能を提供
  /// </summary>
  class Sprite {
  private: //構造体
    /// <summary>
    /// 頂点データ構造体
    /// </summary>
    struct VertexData
    {
      Vector4 position;
      Vector2 texCoord;
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

  public: //メンバー関数

    // ComPtr のエイリアス
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
    /// ImGui でパラメータ調整
    /// </summary>
    void DrawImGui();

    //==============================================
    //Setter
    //==============================================
    void SetTransform(const Transform& transform) { transform_ = transform; }
    void SetColor(const Vector4& color) { materialData_->color = color; }
    void SetAlpha(const float alpha) { materialData_->color.w = alpha; }
    void SetPos(const Vector2& pos) { pos_ = pos; }
    void SetRotation(const float rotation) { rotation_ = rotation; }
    void SetSize(const Vector2& size) { size_ = size; }
    void SetAnchorPoint(const Vector2& anchorPoint) { anchorPoint_ = anchorPoint; }
    void SetIsFlipX(const bool isFlipX) { isFlipX_ = isFlipX; }
    void SetIsFlipY(const bool isFlipY) { isFlipY_ = isFlipY; }
    void SetTexLeftTop(const Vector2& texTopLeft) { texTopLeft_ = texTopLeft; }
    void SetTexCutSize(const Vector2& texCutSize) { texCutSize_ = texCutSize; }

    /// <summary>
    /// テクスチャをファイルパスから変更する。切り取り範囲は新しいテクスチャ全体に自動フィット
    /// </summary>
    /// <param name="texturePath">変更先テクスチャのファイルパス</param>
    void SetTexture(const std::string& texturePath);

    /// <summary>
    /// テクスチャを SRVIndex から変更する。切り取り範囲は新しいテクスチャ全体に自動フィット
    /// </summary>
    /// <param name="textureIndex">変更先テクスチャの SRV インデックス</param>
    void SetTextureIndex(uint32_t textureIndex);

    //==============================================
    //Getter
    //==============================================
    [[nodiscard]] Transform GetTransform() const { return transform_; }
    [[nodiscard]] Material* GetMaterialData() const { return materialData_; }
    [[nodiscard]] Vector4 GetColor() const { return materialData_->color; }
    [[nodiscard]] Vector2 GetPos() const { return pos_; }
    [[nodiscard]] float GetRotation() const { return rotation_; }
    [[nodiscard]] Vector2 GetSize() const { return size_; }
    [[nodiscard]] Vector2 GetAnchorPoint() const { return anchorPoint_; }
    [[nodiscard]] bool GetIsFlipX() const { return isFlipX_; }
    [[nodiscard]] bool GetIsFlipY() const { return isFlipY_; }
    [[nodiscard]] Vector2 GetTexLeftTop() const { return texTopLeft_; }
    [[nodiscard]] Vector2 GetTexCutSize() const { return texCutSize_; }

  private: //非公開関数

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

    /// <summary>
    /// 画像切り取り範囲をぴったりにする
    /// </summary>
    void FitTexCutSize();

  private: //メンバー変数

    Transform transform_ = {};  ///< Transform

    std::string texturePath_;  ///< ファイルパス

    //バッファリソース
    ComPtr<ID3D12Resource> vertexResource_;
    ComPtr<ID3D12Resource> indexResource_;
    ComPtr<ID3D12Resource> materialResource_;
    ComPtr<ID3D12Resource> transformationMatrixResource_;

    //バッファリソース内のデータを参照するためのポインタ
    VertexData*           vertexData_               = nullptr;
    uint32_t*             indexData_                = nullptr;
    Material*             materialData_             = nullptr;
    TransformationMatrix* transformationMatrixData_ = nullptr;

    D3D12_VERTEX_BUFFER_VIEW vertexBufferView_ = {};  ///< 頂点バッファビュー

    D3D12_INDEX_BUFFER_VIEW indexBufferView_ = {};  ///< インデックスバッファビュー

    uint32_t textureIndex_ = 0;  ///< テクスチャ番号

    Vector2 pos_ = {};  ///< 座標

    float rotation_ = 0.0f;  ///< 回転

    Vector2 size_ = {};  ///< サイズ

    Vector2 anchorPoint_ = {};  ///< アンカーポイント

    Vector2 texTopLeft_ = {};  ///< テクスチャの左上座標

    Vector2 texCutSize_ = {};  ///< テクスチャの切り出しサイズ

    bool isFlipX_ = false;  ///< 左右反転

    bool isFlipY_ = false;  ///< 上下反転
  };

} // namespace Tako

#pragma once
#include"Vector4.h"
#include"Vector2.h"
#include"Vector3.h"
#include"Mat4x4Func.h"
#include "Transform.h"
#include<array>
#include<cstdint>
#include<string>
#include <d3d12.h>
#include<wrl.h>
#include <memory>

#include "Material.h"

namespace Tako {

  /// <summary>
  /// 2D スプライト描画クラス。テクスチャの表示、切り抜き、反転などの機能を提供
  /// </summary>
  class Sprite {
  public: //定数
    static constexpr int kRegionCount = 9;  ///< 9-slice の区画数。並びは行順（0:左上 … 4:中央 … 8:右下）

  private: //定数
    static constexpr uint32_t kVertexCount     = 40;  ///< 外周 1 枚板の 4 頂点 + 区画ごとに独立した 4 頂点 x 9
    static constexpr uint32_t kQuadIndexCount  = 6;   ///< 外周 1 枚板（インデックスバッファ先頭）
    static constexpr uint32_t kSliceIndexCount = 54;  ///< 9 枚分（外周 1 枚板の後ろ）

  public: //構造体
    /// <summary>
    /// 9-slice の 4x4 の格子。位置はサイズ 1 の単位空間（左上 0、右下 1）、UV はテクスチャ全体を 0..1 とした値
    /// </summary>
    struct SliceGrid {
      float xs[4];
      float ys[4];
      float us[4];
      float vs[4];
    };

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
    struct MaterialConstants
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
    /// 描画リストへ登録する（登録順に描かれる）
    /// </summary>
    void Draw();

    /// <summary>
    /// ImGui でパラメータ調整
    /// </summary>
    void DrawImGui();

    /// <summary>
    /// 9-slice の格子を求める。枠の合計がサイズを超える軸は、比率を保って枠を縮める（中央は 0 になる）
    /// </summary>
    /// <param name="size">描画サイズ（画面 px）</param>
    /// <param name="borderTexPx">枠幅（テクスチャ px、x=左 y=上 z=右 w=下）</param>
    /// <param name="scale">枠の画面 px ÷ テクスチャ px</param>
    /// <param name="texLeftTop">切り出し範囲の左上（テクスチャ px）</param>
    /// <param name="texCutSize">切り出し範囲のサイズ（テクスチャ px）</param>
    /// <param name="texSize">テクスチャ全体のサイズ（px）</param>
    static SliceGrid CalcSliceGrid(const Vector2& size, const Vector4& borderTexPx, float scale, const Vector2& texLeftTop, const Vector2& texCutSize, const Vector2& texSize);

    //============================================================
    //Setter
    //============================================================
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
    /// 同じ描画キューの中での描画順（小さいほど先）
    /// </summary>
    void SetSortingOrder(int32_t sortingOrder) { sortingOrder_ = sortingOrder; }

    /// <summary>
    /// 9-slice の枠幅を設定する。四隅は伸縮せず、辺と中央だけが伸びる。全要素 0 で通常描画
    /// </summary>
    /// <param name="borderTexPx">枠幅（テクスチャ px、x=左 y=上 z=右 w=下）</param>
    /// <param name="scale">枠の画面 px ÷ テクスチャ px</param>
    void SetSlice(const Vector4& borderTexPx, float scale = 1.0f) { sliceBorder_ = borderTexPx; sliceScale_ = scale; }

    void SetRegionVisible(int region, bool visible) { regionVisible_[region] = visible; }

    /// <summary>
    /// 9-slice の区画に別の画像を使う。その画像全体を区画いっぱいに引き伸ばす。読み込み済みでなければ本体の画像の区画のまま
    /// </summary>
    /// <param name="region">区画（kRegionCount の並び）</param>
    /// <param name="texturePath">TextureManager のキー。空なら本体の画像の区画に戻す</param>
    void SetRegionTexture(int region, const std::string& texturePath);

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

    //============================================================
    //Getter
    //============================================================
    [[nodiscard]] Transform GetTransform() const { return transform_; }
    [[nodiscard]] MaterialConstants* GetMaterialData() const { return materialData_; }
    [[nodiscard]] Material& GetMaterial() const { return *material_; }
    [[nodiscard]] Vector4 GetColor() const { return materialData_->color; }
    [[nodiscard]] Vector2 GetPos() const { return pos_; }
    [[nodiscard]] float GetRotation() const { return rotation_; }
    [[nodiscard]] Vector2 GetSize() const { return size_; }
    [[nodiscard]] Vector2 GetAnchorPoint() const { return anchorPoint_; }
    [[nodiscard]] bool GetIsFlipX() const { return isFlipX_; }
    [[nodiscard]] bool GetIsFlipY() const { return isFlipY_; }
    [[nodiscard]] Vector2 GetTexLeftTop() const { return texTopLeft_; }
    [[nodiscard]] Vector2 GetTexCutSize() const { return texCutSize_; }
    [[nodiscard]] Vector4 GetSliceBorder() const { return sliceBorder_; }
    [[nodiscard]] const std::string& GetRegionTexture(int region) const { return regionTextures_[region]; }

  private: //非公開関数
    /// <summary>
    /// 区画ごとに描くか（枠・非表示の区画・別画像の区画のどれかがある）。false なら外周 1 枚板で描く
    /// </summary>
    bool IsSliced() const;

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
    ComPtr<ID3D12Resource> transformationMatrixResource_;

    //バッファリソース内のデータを参照するためのポインタ
    VertexData*           vertexData_               = nullptr;
    uint32_t*             indexData_                = nullptr;
    MaterialConstants*    materialData_             = nullptr;  ///< material_ の定数
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

    Vector4 sliceBorder_ = {};    ///< 9-slice の枠幅（テクスチャ px、左上右下）
    float   sliceScale_  = 1.0f;  ///< 枠の画面 px ÷ テクスチャ px

    std::unique_ptr<Material> material_;
    int32_t                   sortingOrder_ = 0;

    //9-slice の区画
    std::array<bool, kRegionCount>        regionVisible_        = { true, true, true, true, true, true, true, true, true };
    std::array<std::string, kRegionCount> regionTextures_;                                                                   ///< 区画ごとの別画像のキー（空なら本体の画像）
    std::array<uint32_t, kRegionCount>    regionTextureIndices_ = {};                                                        ///< regionTextures_ の SRV 番号。0 なら本体の画像
  };

} // namespace Tako

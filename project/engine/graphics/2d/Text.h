#pragma once
#include "Material.h"
#include "Matrix4x4.h"
#include "Vector2.h"
#include "ColorFunc.h"
#include "Vector4.h"
#include <cstdint>
#include <d3d12.h>
#include <memory>
#include <string>
#include <wrl.h>

namespace Tako {

  class Font;

  /// <summary>
  /// 文字列 1 つを、フォントのアトラスの字を並べた四角で描く。長さの単位は呼び出し側が決め、拡大で画面 px に合わせる
  /// </summary>
  class TAKO_API Text {
  public: //構造体
    /// <summary>
    /// 字の並びを決める値。変わったときだけ頂点を作り直す
    /// </summary>
    struct Layout {
      std::string string;               ///< UTF-8
      std::string font;                 ///< Font::Get のキー（空なら既定のフォント）
      float       size        = 32.0f;  ///< em の大きさ
      Vector2     boxSize     = {};     ///< 折り返しと揃えの基準にする箱
      Vector2     pivot       = {};     ///< 箱の中の原点 (0..1)。位置と回転の中心になる
      Vector2     align       = {};     ///< 箱の中の寄せ方 (0..1)。x は 0 で左・1 で右、y は 0 で上・1 で下
      float       lineSpacing = 1.0f;   ///< 行送りの倍率
      bool        wrap        = true;   ///< 箱の幅で折り返すか（収まらない 1 語ははみ出す）

      bool operator==(const Layout&) const = default;
    };

  private: //構造体
    struct VertexData {
      Vector4 position;
      Vector2 texCoord;
    };

    /// <summary>
    /// Text.PS.hlsl の gMaterial
    /// </summary>
    struct MaterialConstants {
      Color   color;
      Color   outlineColor;
      Vector2 unitRange;     ///< SDF の値 0〜1 が表す距離（アトラスの UV）
      float   outline;       ///< 縁取りの太さ（SDF の値）
      float   padding;
    };

    struct TransformationMatrix {
      Matrix4x4 WVP;
      Matrix4x4 world;
    };

  public: //メンバー関数
    Text();
    ~Text();

    /// <summary>
    /// 字の並びが変わっていれば頂点を作り直し、変換行列と定数を更新する。頂点バッファを作り直すので描画中には呼ばない
    /// </summary>
    void Update();

    /// <summary>
    /// 描画リストへ登録する（登録順に描かれる）
    /// </summary>
    void Draw();

    //================================================
    //Setter
    //================================================
    void SetLayout(const Layout& layout);
    void SetPos(const Vector2& pos) { pos_ = pos; }
    void SetRotation(float rotation) { rotation_ = rotation; }

    /// <summary>
    /// Layout の長さ → 画面 px の倍率
    /// </summary>
    void SetScale(const Vector2& scale) { scale_ = scale; }

    void SetColor(const Color& color) { color_ = color; }

    /// <summary>
    /// 縁取り。太さは Layout の size と同じ単位で、0 なら付けない（上限は size × Font::kSpread ÷ Font::kBakeSize 弱）
    /// </summary>
    void SetOutline(const Color& color, float width) { outlineColor_ = color; outlineWidth_ = width; }

    /// <summary>
    /// 同じ描画キューの中での描画順（小さいほど先）
    /// </summary>
    void SetSortingOrder(int32_t sortingOrder) { sortingOrder_ = sortingOrder; }

    //================================================
    //Getter
    //================================================
    Material& GetMaterial() const { return *material_; }

  private: //非公開関数
    /// <summary>
    /// 行に分けて箱の中へ揃え、字ごとの四角を頂点バッファへ書く。まだ作れていない字があれば、次の Update でやり直す
    /// </summary>
    void Rebuild();

    /// <summary>
    /// 四角 quadCount 枚が入る大きさの頂点・インデックスバッファにする（足りないときだけ作り直す）
    /// </summary>
    void Reserve(uint32_t quadCount);

  private: //メンバー変数
    //字の並び
    Layout   layout_;
    Font*    font_      = nullptr;  ///< 非所有（Font::Get の物）
    bool     isDirty_   = true;
    uint32_t quadCount_ = 0;
    uint32_t capacity_  = 0;        ///< バッファに入る四角の数

    //毎フレーム変えてよい値
    Vector2 pos_          = {};
    float   rotation_     = 0.0f;                        ///< ラジアン
    Vector2 scale_        = { 1.0f, 1.0f };
    Color   color_        = { 1.0f, 1.0f, 1.0f, 1.0f };
    Color   outlineColor_ = { 0.0f, 0.0f, 0.0f, 1.0f };
    float   outlineWidth_ = 0.0f;
    int32_t sortingOrder_ = 0;

    //バッファ
    Microsoft::WRL::ComPtr<ID3D12Resource> vertexResource_;
    Microsoft::WRL::ComPtr<ID3D12Resource> indexResource_;
    Microsoft::WRL::ComPtr<ID3D12Resource> transformationMatrixResource_;
    VertexData*                            vertexData_                   = nullptr;
    TransformationMatrix*                  transformationMatrixData_     = nullptr;
    D3D12_VERTEX_BUFFER_VIEW               vertexBufferView_             = {};
    D3D12_INDEX_BUFFER_VIEW                indexBufferView_              = {};

    std::unique_ptr<Material> material_;
  };

} // namespace Tako

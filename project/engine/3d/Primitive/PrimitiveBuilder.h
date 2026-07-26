#pragma once
#include <cstdint>
#include <memory>

namespace Tako {

  class Model;

  /// <summary>
  /// コードから動的にプリミティブ (Cube/Sphere/Plane/Ring/Cylinder/Torus) のメッシュデータを生成し
  /// 1メッシュ Model としてラップして返す静的ファクトリ。
  /// 返却された Model は Object3d::SetModel(std::unique_ptr&lt;Model&gt;) に直接渡せる。
  /// デフォルトテクスチャ "white.dds" を自動で割り当てる。
  /// </summary>
  class PrimitiveBuilder
  {
  public: //構造体

    /// <summary>
    /// 立方体生成パラメータ
    /// </summary>
    struct CubeParams
    {
      float size = 1.0f;  ///< 1辺の長さ（原点中心、[-size/2, +size/2]）
    };

    /// <summary>
    /// 球生成パラメータ
    /// </summary>
    struct SphereParams
    {
      float radius = 0.5f;       ///< 半径
      uint32_t lonDiv = 16;      ///< 経度分割数（最小 3）
      uint32_t latDiv = 8;       ///< 緯度分割数（最小 2）
    };

    /// <summary>
    /// 平面生成パラメータ
    /// </summary>
    struct PlaneParams
    {
      float width = 1.0f;        ///< X方向の幅
      float height = 1.0f;       ///< Z方向の奥行き
      uint32_t xSeg = 1;         ///< X方向の分割数（最小 1）
      uint32_t ySeg = 1;         ///< Z方向の分割数（最小 1）
    };

    /// <summary>
    /// 平面リング生成パラメータ
    /// </summary>
    struct RingParams
    {
      float    innerRadius   = 0.3f;    ///< 内半径（0 で円盤になる）
      float    outerRadius   = 0.5f;    ///< 外半径
      uint32_t segments      = 32;      ///< 円周方向の分割数（最小 3）
      uint32_t ringSeg       = 1;       ///< 半径方向の分割数（最小 1）
      float    startAngleDeg = 0.0f;    ///< 開始角。+X 軸を 0 とし +Z 方向へ増加
      float    sweepAngleDeg = 360.0f;  ///< 掃引角。360 未満で扇形リング、負値は逆回り
    };

    /// <summary>
    /// 円柱/円錐台生成パラメータ（topRadius=0 で円錐になる）
    /// </summary>
    struct CylinderParams
    {
      float    topRadius    = 0.5f;   ///< 上面半径（0 で円錐）
      float    bottomRadius = 0.5f;   ///< 底面半径
      float    height       = 1.0f;   ///< 高さ（Y軸中心、[-height/2, +height/2]）
      uint32_t radialDiv    = 16;     ///< 円周方向の分割数（最小 3）
      uint32_t heightDiv    = 1;      ///< 高さ方向の分割数（最小 1）
      bool     capTop       = true;   ///< 上面キャップを生成するか（半径 0 なら自動省略）
      bool     capBottom    = true;   ///< 底面キャップを生成するか（半径 0 なら自動省略）
    };

    /// <summary>
    /// トーラス生成パラメータ
    /// </summary>
    struct TorusParams
    {
      float    majorRadius = 0.5f;    ///< 主半径（チューブ中心円の半径）
      float    minorRadius = 0.15f;   ///< 副半径（チューブの太さ）
      uint32_t majorDiv    = 24;      ///< 主円周方向の分割数（最小 3）
      uint32_t minorDiv    = 12;      ///< チューブ断面方向の分割数（最小 3）
    };

  public: //メンバー関数

    /// <summary>
    /// 立方体 Model を生成
    /// </summary>
    /// <param name="p">立方体パラメータ</param>
    /// <returns>1メッシュ Model（所有権付き）</returns>
    static std::unique_ptr<Model> CreateCube(const CubeParams& p = {});

    /// <summary>
    /// 球 Model を生成（UV球、緯度経度分割）
    /// </summary>
    /// <param name="p">球パラメータ</param>
    /// <returns>1メッシュ Model（所有権付き）</returns>
    static std::unique_ptr<Model> CreateSphere(const SphereParams& p = {});

    /// <summary>
    /// 平面 Model を生成（XZ 平面、上向き法線）
    /// </summary>
    /// <param name="p">平面パラメータ</param>
    /// <returns>1メッシュ Model（所有権付き）</returns>
    static std::unique_ptr<Model> CreatePlane(const PlaneParams& p = {});

    /// <summary>
    /// 平面リング Model を生成（XZ 平面、上向き法線）
    /// </summary>
    /// <param name="p">リングパラメータ</param>
    /// <returns>1メッシュ Model（所有権付き）</returns>
    static std::unique_ptr<Model> CreateRing(const RingParams& p = {});

    /// <summary>
    /// 円柱/円錐台 Model を生成（Y軸中心、topRadius=0 で円錐）
    /// </summary>
    /// <param name="p">円柱パラメータ</param>
    /// <returns>1メッシュ Model（所有権付き）</returns>
    static std::unique_ptr<Model> CreateCylinder(const CylinderParams& p = {});

    /// <summary>
    /// トーラス Model を生成（XZ 平面に主円、Y軸中心）
    /// </summary>
    /// <param name="p">トーラスパラメータ</param>
    /// <returns>1メッシュ Model（所有権付き）</returns>
    static std::unique_ptr<Model> CreateTorus(const TorusParams& p = {});
  };

} // namespace Tako

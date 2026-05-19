#include "PrimitiveBuilder.h"

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

#include "Mesh.h"
#include "Model.h"
#include "ModelBasic.h"
#include "ModelManager.h"
#include "ModelStruct.h"
#include "TextureManager.h"

namespace Tako {

  namespace {

    /// <summary>
    /// 頂点配列とインデックス配列から 1メッシュ Model を組み立てる共通ヘルパ
    /// </summary>
    std::unique_ptr<Model> BuildModel(
      const std::vector<VertexData>& vertices,
      const std::vector<uint32_t>& indices,
      const std::string& debugName)
    {
      ModelBasic* basic = ModelManager::GetInstance()->GetModelBasic();

      // デフォルトテクスチャは white.dds（エンジン起動時に LoadEngineDefault 済み）
      TextureData textureData{};
      textureData.texturePath = "";
      textureData.textureIndex = TextureManager::GetInstance()->GetEngineDefaultSRVIndex("white.dds");
      textureData.baseColor = Vector4(1.0f, 1.0f, 1.0f, 1.0f);

      auto mesh = std::make_unique<Mesh>();
      mesh->Initialize(basic, vertices, indices, textureData);

      auto model = std::make_unique<Model>();
      model->InitializeFromPrimitive(basic, std::move(mesh), debugName);
      return model;
    }

    /// <summary>
    /// Cube の1面分の頂点（4頂点）とインデックス（6個=2三角形）を末尾追加する
    /// 法線方向から見て CCW (外向き) になるよう (-u,-v) → (+u,-v) → (+u,+v) → (-u,+v) の順
    /// </summary>
    void AppendCubeFace(
      float cx, float cy, float cz,        // 面中心座標
      float ux, float uy, float uz,        // u 軸方向ベクトル（単位）
      float vx, float vy, float vz,        // v 軸方向ベクトル（単位）
      float nx, float ny, float nz,        // 外向き法線（単位）
      float halfSize,
      std::vector<VertexData>& outVerts,
      std::vector<uint32_t>& outIndices)
    {
      const uint32_t base = static_cast<uint32_t>(outVerts.size());

      // ローカル u, v ∈ {-1, +1} から position を生成するラムダ
      auto makeVertex = [&](float su, float sv, float uvU, float uvV) {
        VertexData vd{};
        vd.position = Vector4(
          cx + (ux * su + vx * sv) * halfSize,
          cy + (uy * su + vy * sv) * halfSize,
          cz + (uz * su + vz * sv) * halfSize,
          1.0f);
        vd.normal = Vector3(nx, ny, nz);
        vd.texcoord = Vector2(uvU, uvV);
        return vd;
      };

      outVerts.push_back(makeVertex(-1.0f, -1.0f, 0.0f, 1.0f)); // 0: 左下
      outVerts.push_back(makeVertex(+1.0f, -1.0f, 1.0f, 1.0f)); // 1: 右下
      outVerts.push_back(makeVertex(+1.0f, +1.0f, 1.0f, 0.0f)); // 2: 右上
      outVerts.push_back(makeVertex(-1.0f, +1.0f, 0.0f, 0.0f)); // 3: 左上

      // 三角形 (0,1,2) + (0,2,3) — 法線方向から見て CCW
      outIndices.push_back(base + 0);
      outIndices.push_back(base + 1);
      outIndices.push_back(base + 2);
      outIndices.push_back(base + 0);
      outIndices.push_back(base + 2);
      outIndices.push_back(base + 3);
    }

    /// <summary>
    /// 立方体の頂点・インデックスを生成（24頂点、36インデックス）
    /// </summary>
    void GenerateCube(
      const PrimitiveBuilder::CubeParams& params,
      std::vector<VertexData>& outVertices,
      std::vector<uint32_t>& outIndices)
    {
      outVertices.clear();
      outIndices.clear();
      outVertices.reserve(24);
      outIndices.reserve(36);

      const float h = params.size * 0.5f;

      // 6面: 面中心 = normal * h、各面に u/v 軸を割り当てて4頂点+6インデックスを生成
      AppendCubeFace(+h, 0, 0,   0, 0,-1,   0, 1, 0,   +1, 0, 0,  h, outVertices, outIndices); // +X
      AppendCubeFace(-h, 0, 0,   0, 0,+1,   0, 1, 0,   -1, 0, 0,  h, outVertices, outIndices); // -X
      AppendCubeFace( 0,+h, 0,  +1, 0, 0,   0, 0,+1,    0,+1, 0,  h, outVertices, outIndices); // +Y
      AppendCubeFace( 0,-h, 0,  +1, 0, 0,   0, 0,-1,    0,-1, 0,  h, outVertices, outIndices); // -Y
      AppendCubeFace( 0, 0,+h,  +1, 0, 0,   0,+1, 0,    0, 0,+1,  h, outVertices, outIndices); // +Z
      AppendCubeFace( 0, 0,-h,  -1, 0, 0,   0,+1, 0,    0, 0,-1,  h, outVertices, outIndices); // -Z
    }

    /// <summary>
    /// 球の頂点・インデックスを生成（UV 球、緯度経度分割）
    /// </summary>
    void GenerateSphere(
      const PrimitiveBuilder::SphereParams& params,
      std::vector<VertexData>& outVertices,
      std::vector<uint32_t>& outIndices)
    {
      outVertices.clear();
      outIndices.clear();

      const uint32_t lon = std::max<uint32_t>(params.lonDiv, 3u);
      const uint32_t lat = std::max<uint32_t>(params.latDiv, 2u);
      const float radius = params.radius;
      constexpr float kPi = 3.14159265358979323846f;

      outVertices.reserve((lon + 1) * (lat + 1));
      outIndices.reserve(lon * lat * 6);

      // 頂点生成: 北極 (phi=0) から南極 (phi=π) へ、経度方向に lon+1 個（UV境界の継ぎ目用に重複）
      for (uint32_t j = 0; j <= lat; ++j) {
        const float phi = static_cast<float>(j) / static_cast<float>(lat) * kPi;
        const float sinPhi = std::sin(phi);
        const float cosPhi = std::cos(phi);

        for (uint32_t i = 0; i <= lon; ++i) {
          const float theta = static_cast<float>(i) / static_cast<float>(lon) * (2.0f * kPi);
          const float sinTheta = std::sin(theta);
          const float cosTheta = std::cos(theta);

          // 単位球面上の方向ベクトル（= 法線）
          const float nx = sinPhi * cosTheta;
          const float ny = cosPhi;
          const float nz = sinPhi * sinTheta;

          VertexData vd{};
          vd.position = Vector4(nx * radius, ny * radius, nz * radius, 1.0f);
          vd.normal = Vector3(nx, ny, nz);
          vd.texcoord = Vector2(
            static_cast<float>(i) / static_cast<float>(lon),
            static_cast<float>(j) / static_cast<float>(lat));
          outVertices.push_back(vd);
        }
      }

      // インデックス生成: 各クアッドを2三角形に分割。外向き法線方向から見て CCW
      for (uint32_t j = 0; j < lat; ++j) {
        for (uint32_t i = 0; i < lon; ++i) {
          const uint32_t a = j * (lon + 1) + i;
          const uint32_t b = a + 1;
          const uint32_t c = a + (lon + 1);
          const uint32_t d = c + 1;

          outIndices.push_back(a);
          outIndices.push_back(c);
          outIndices.push_back(b);

          outIndices.push_back(b);
          outIndices.push_back(c);
          outIndices.push_back(d);
        }
      }
    }

    /// <summary>
    /// 平面の頂点・インデックスを生成（XZ 平面、上向き法線 +Y）
    /// </summary>
    void GeneratePlane(
      const PrimitiveBuilder::PlaneParams& params,
      std::vector<VertexData>& outVertices,
      std::vector<uint32_t>& outIndices)
    {
      outVertices.clear();
      outIndices.clear();

      const uint32_t xs = std::max<uint32_t>(params.xSeg, 1u);
      const uint32_t ys = std::max<uint32_t>(params.ySeg, 1u);

      outVertices.reserve((xs + 1) * (ys + 1));
      outIndices.reserve(xs * ys * 6);

      // XZ 平面上に (xs+1) × (ys+1) のグリッド頂点を配置、法線は +Y 固定
      for (uint32_t j = 0; j <= ys; ++j) {
        const float v = static_cast<float>(j) / static_cast<float>(ys);
        const float pz = (v - 0.5f) * params.height;

        for (uint32_t i = 0; i <= xs; ++i) {
          const float u = static_cast<float>(i) / static_cast<float>(xs);
          const float px = (u - 0.5f) * params.width;

          VertexData vd{};
          vd.position = Vector4(px, 0.0f, pz, 1.0f);
          vd.normal = Vector3(0.0f, 1.0f, 0.0f);
          vd.texcoord = Vector2(u, v);
          outVertices.push_back(vd);
        }
      }

      // 上向き法線 (+Y) から見て CCW
      for (uint32_t j = 0; j < ys; ++j) {
        for (uint32_t i = 0; i < xs; ++i) {
          const uint32_t a = j * (xs + 1) + i;
          const uint32_t b = a + 1;
          const uint32_t c = a + (xs + 1);
          const uint32_t d = c + 1;

          outIndices.push_back(a);
          outIndices.push_back(c);
          outIndices.push_back(b);

          outIndices.push_back(b);
          outIndices.push_back(c);
          outIndices.push_back(d);
        }
      }
    }

  } // anonymous namespace

  ///------------------------------------------------///
  ///                 PUBLIC METHODS                 ///
  ///------------------------------------------------///

  std::unique_ptr<Model> PrimitiveBuilder::CreateCube(const CubeParams& p)
  {
    std::vector<VertexData> vertices;
    std::vector<uint32_t> indices;
    GenerateCube(p, vertices, indices);
    return BuildModel(vertices, indices, "<Primitive_Cube>");
  }

  std::unique_ptr<Model> PrimitiveBuilder::CreateSphere(const SphereParams& p)
  {
    std::vector<VertexData> vertices;
    std::vector<uint32_t> indices;
    GenerateSphere(p, vertices, indices);
    return BuildModel(vertices, indices, "<Primitive_Sphere>");
  }

  std::unique_ptr<Model> PrimitiveBuilder::CreatePlane(const PlaneParams& p)
  {
    std::vector<VertexData> vertices;
    std::vector<uint32_t> indices;
    GeneratePlane(p, vertices, indices);
    return BuildModel(vertices, indices, "<Primitive_Plane>");
  }

} // namespace Tako

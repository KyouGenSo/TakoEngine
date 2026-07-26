#include "PrimitiveBuilder.h"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <format>
#include <fstream>
#include <numbers>
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
    /// u×v = n となる右手系の軸を渡すこと。三角形は u×v 方向が表（外向き法線に右手系 CCW）
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

      // 三角形 (0,1,2) + (0,2,3) — u×v 方向（=外向き法線）に対し右手系 CCW
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

      // 6面: 面中心 = normal * h、各面に u×v = n となる右手系の u/v 軸を割り当てて4頂点+6インデックスを生成
      AppendCubeFace(+h, 0, 0,   0, 0,-1,   0, 1, 0,   +1, 0, 0,  h, outVertices, outIndices); // +X
      AppendCubeFace(-h, 0, 0,   0, 0,+1,   0, 1, 0,   -1, 0, 0,  h, outVertices, outIndices); // -X
      AppendCubeFace( 0,+h, 0,  +1, 0, 0,   0, 0,-1,    0,+1, 0,  h, outVertices, outIndices); // +Y
      AppendCubeFace( 0,-h, 0,  +1, 0, 0,   0, 0,+1,    0,-1, 0,  h, outVertices, outIndices); // -Y
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

      outVertices.reserve((lon + 1) * (lat + 1));
      outIndices.reserve(lon * lat * 6);

      // 頂点生成: 北極 (phi=0) から南極 (phi=π) へ、経度方向に lon+1 個（UV境界の継ぎ目用に重複）
      for (uint32_t j = 0; j <= lat; ++j) {
        const float phi = static_cast<float>(j) / static_cast<float>(lat) * std::numbers::pi_v<float>;
        const float sinPhi = std::sin(phi);
        const float cosPhi = std::cos(phi);

        for (uint32_t i = 0; i <= lon; ++i) {
          const float theta = static_cast<float>(i) / static_cast<float>(lon) * (2.0f * std::numbers::pi_v<float>);
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

      // インデックス生成: 各クアッドを2三角形に分割。外向き法線に対し右手系 CCW（Plane/Ring と同じ向き）
      for (uint32_t j = 0; j < lat; ++j) {
        for (uint32_t i = 0; i < lon; ++i) {
          const uint32_t a = j * (lon + 1) + i;
          const uint32_t b = a + 1;
          const uint32_t c = a + (lon + 1);
          const uint32_t d = c + 1;

          outIndices.push_back(a);
          outIndices.push_back(b);
          outIndices.push_back(c);

          outIndices.push_back(b);
          outIndices.push_back(d);
          outIndices.push_back(c);
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

    /// <summary>
    /// 平面リングの頂点・インデックスを生成（XZ 平面、上向き法線 +Y）
    /// </summary>
    void GenerateRing(
      const PrimitiveBuilder::RingParams& params,
      std::vector<VertexData>& outVertices,
      std::vector<uint32_t>& outIndices)
    {
      outVertices.clear();
      outIndices.clear();

      const uint32_t seg = std::max<uint32_t>(params.segments, 3u);
      const uint32_t rs = std::max<uint32_t>(params.ringSeg, 1u);

      const float rInner = (std::max)(0.0f, (std::min)(params.innerRadius, params.outerRadius));
      const float rOuter = (std::max)(0.0f, (std::max)(params.innerRadius, params.outerRadius));

      // 負の掃引角は開始角をずらして正方向へ揃える（そのままだと三角形が裏返る）
      float startDeg = params.startAngleDeg;
      float sweepDeg = params.sweepAngleDeg;
      if (sweepDeg < 0.0f) {
        startDeg += sweepDeg;
        sweepDeg = -sweepDeg;
      }
      constexpr float kDegToRad = std::numbers::pi_v<float> / 180.0f;
      const float startRad = startDeg * kDegToRad;
      const float sweepRad = sweepDeg * kDegToRad;

      outVertices.reserve((seg + 1) * (rs + 1));
      outIndices.reserve(seg * rs * 6);

      // 角度方向は全周でも UV の継ぎ目を割るため seg+1 個
      for (uint32_t j = 0; j <= rs; ++j) {
        const float v = static_cast<float>(j) / static_cast<float>(rs);
        const float radius = rInner + (rOuter - rInner) * v;

        for (uint32_t i = 0; i <= seg; ++i) {
          const float u = static_cast<float>(i) / static_cast<float>(seg);
          const float theta = startRad + sweepRad * u;

          VertexData vd{};
          vd.position = Vector4(std::cos(theta) * radius, 0.0f, std::sin(theta) * radius, 1.0f);
          vd.normal = Vector3(0.0f, 1.0f, 0.0f);
          vd.texcoord = Vector2(u, v);
          outVertices.push_back(vd);
        }
      }

      // i=接線方向 / j=径方向なので cross(Δi, Δj) が +Y。GeneratePlane とは逆順になる
      for (uint32_t j = 0; j < rs; ++j) {
        for (uint32_t i = 0; i < seg; ++i) {
          const uint32_t a = j * (seg + 1) + i;
          const uint32_t b = a + 1;
          const uint32_t c = a + (seg + 1);
          const uint32_t d = c + 1;

          outIndices.push_back(a);
          outIndices.push_back(b);
          outIndices.push_back(c);

          outIndices.push_back(b);
          outIndices.push_back(d);
          outIndices.push_back(c);
        }
      }
    }

    /// <summary>
    /// 円柱キャップ（中心+リムの三角形ファン）の頂点とインデックスを末尾追加する
    /// 半径 0 以下なら何もしない（円錐の頂点側を自動省略）
    /// </summary>
    void AppendCapFan(
      float y, float radius, uint32_t radialDiv, bool facingUp,
      std::vector<VertexData>& outVerts,
      std::vector<uint32_t>& outIndices)
    {
      if (radius <= 0.0f) {
        return;
      }

      const uint32_t base = static_cast<uint32_t>(outVerts.size());
      const float ny = facingUp ? 1.0f : -1.0f;

      // 中心頂点。側面と法線が不連続なため頂点は共有しない（ハードエッジ）
      VertexData center{};
      center.position = Vector4(0.0f, y, 0.0f, 1.0f);
      center.normal = Vector3(0.0f, ny, 0.0f);
      center.texcoord = Vector2(0.5f, 0.5f);
      outVerts.push_back(center);

      // リムは角度ループを閉じるため radialDiv+1 個。UV は平面マッピング
      for (uint32_t i = 0; i <= radialDiv; ++i) {
        const float theta = static_cast<float>(i) / static_cast<float>(radialDiv) * (2.0f * std::numbers::pi_v<float>);
        const float cosTheta = std::cos(theta);
        const float sinTheta = std::sin(theta);

        VertexData vd{};
        vd.position = Vector4(cosTheta * radius, y, sinTheta * radius, 1.0f);
        vd.normal = Vector3(0.0f, ny, 0.0f);
        vd.texcoord = Vector2(0.5f + 0.5f * cosTheta, 0.5f + 0.5f * sinTheta);
        outVerts.push_back(vd);
      }

      // +Y 面は GenerateRing の円盤 (innerRadius=0) と同じ (center, rim_{i+1}, rim_i)、-Y 面は逆順
      for (uint32_t i = 0; i < radialDiv; ++i) {
        const uint32_t rim0 = base + 1 + i;
        const uint32_t rim1 = base + 2 + i;

        outIndices.push_back(base);
        if (facingUp) {
          outIndices.push_back(rim1);
          outIndices.push_back(rim0);
        }
        else {
          outIndices.push_back(rim0);
          outIndices.push_back(rim1);
        }
      }
    }

    /// <summary>
    /// 円柱/円錐台の頂点・インデックスを生成（Y軸中心、topRadius=0 で円錐）
    /// </summary>
    void GenerateCylinder(
      const PrimitiveBuilder::CylinderParams& params,
      std::vector<VertexData>& outVertices,
      std::vector<uint32_t>& outIndices)
    {
      outVertices.clear();
      outIndices.clear();

      const uint32_t rad = std::max<uint32_t>(params.radialDiv, 3u);
      const uint32_t hs = std::max<uint32_t>(params.heightDiv, 1u);
      const float rTop = (std::max)(0.0f, params.topRadius);
      const float rBottom = (std::max)(0.0f, params.bottomRadius);
      const float h = (std::max)(0.0f, params.height);
      const float halfH = h * 0.5f;

      outVertices.reserve((rad + 1) * (hs + 1) + (rad + 2) * 2);
      outIndices.reserve(rad * hs * 6 + rad * 3 * 2);

      // 側面プロファイル線 (rBottom,-h/2)→(rTop,+h/2) の外向き法線を r-y 平面で求める
      // 円柱時は水平放射、円錐時は斜めになる。縮退時 (高さ0かつ同半径) は水平にフォールバック
      const float slopeLen = std::sqrt(h * h + (rBottom - rTop) * (rBottom - rTop));
      const float nR = (slopeLen > 0.0f) ? h / slopeLen : 1.0f;
      const float nY = (slopeLen > 0.0f) ? (rBottom - rTop) / slopeLen : 0.0f;

      // 側面: 上端 (j=0) から下端 (j=hs) へ、周方向は UV 継ぎ目用に rad+1 個
      // topRadius=0 のとき j=0 行は頂点に縮退するが、GenerateSphere の極と同じ扱いで問題ない
      for (uint32_t j = 0; j <= hs; ++j) {
        const float v = static_cast<float>(j) / static_cast<float>(hs);
        const float y = halfH - h * v;
        const float radius = rTop + (rBottom - rTop) * v;

        for (uint32_t i = 0; i <= rad; ++i) {
          const float theta = static_cast<float>(i) / static_cast<float>(rad) * (2.0f * std::numbers::pi_v<float>);
          const float cosTheta = std::cos(theta);
          const float sinTheta = std::sin(theta);

          VertexData vd{};
          vd.position = Vector4(cosTheta * radius, y, sinTheta * radius, 1.0f);
          vd.normal = Vector3(cosTheta * nR, nY, sinTheta * nR);
          vd.texcoord = Vector2(static_cast<float>(i) / static_cast<float>(rad), v);
          outVertices.push_back(vd);
        }
      }

      // 行=下方向 / 列=θ+ のトポロジが GenerateSphere と一致するため同じ巻き順（外向き法線に対し右手系 CCW）
      for (uint32_t j = 0; j < hs; ++j) {
        for (uint32_t i = 0; i < rad; ++i) {
          const uint32_t a = j * (rad + 1) + i;
          const uint32_t b = a + 1;
          const uint32_t c = a + (rad + 1);
          const uint32_t d = c + 1;

          outIndices.push_back(a);
          outIndices.push_back(b);
          outIndices.push_back(c);

          outIndices.push_back(b);
          outIndices.push_back(d);
          outIndices.push_back(c);
        }
      }

      if (params.capTop) {
        AppendCapFan(+halfH, rTop, rad, true, outVertices, outIndices);
      }
      if (params.capBottom) {
        AppendCapFan(-halfH, rBottom, rad, false, outVertices, outIndices);
      }
    }

    /// <summary>
    /// トーラスの頂点・インデックスを生成（XZ 平面に主円、Y軸中心）
    /// </summary>
    void GenerateTorus(
      const PrimitiveBuilder::TorusParams& params,
      std::vector<VertexData>& outVertices,
      std::vector<uint32_t>& outIndices)
    {
      outVertices.clear();
      outIndices.clear();

      const uint32_t majorDiv = std::max<uint32_t>(params.majorDiv, 3u);
      const uint32_t minorDiv = std::max<uint32_t>(params.minorDiv, 3u);
      const float majorR = (std::max)(0.0f, params.majorRadius);
      const float minorR = (std::max)(0.0f, params.minorRadius);

      outVertices.reserve((majorDiv + 1) * (minorDiv + 1));
      outIndices.reserve(majorDiv * minorDiv * 6);

      // 行=断面角 ψ / 列=主円周角 φ、両方向とも UV 継ぎ目用に +1 個重複
      // ψ=0 が外周赤道、法線はチューブ中心円からの放射方向
      for (uint32_t j = 0; j <= minorDiv; ++j) {
        const float psi = static_cast<float>(j) / static_cast<float>(minorDiv) * (2.0f * std::numbers::pi_v<float>);
        const float cosPsi = std::cos(psi);
        const float sinPsi = std::sin(psi);

        for (uint32_t i = 0; i <= majorDiv; ++i) {
          const float phi = static_cast<float>(i) / static_cast<float>(majorDiv) * (2.0f * std::numbers::pi_v<float>);
          const float cosPhi = std::cos(phi);
          const float sinPhi = std::sin(phi);

          VertexData vd{};
          vd.position = Vector4(
            (majorR + minorR * cosPsi) * cosPhi,
            minorR * sinPsi,
            (majorR + minorR * cosPsi) * sinPhi,
            1.0f);
          vd.normal = Vector3(cosPsi * cosPhi, sinPsi, cosPsi * sinPhi);
          vd.texcoord = Vector2(
            static_cast<float>(i) / static_cast<float>(majorDiv),
            static_cast<float>(j) / static_cast<float>(minorDiv));
          outVertices.push_back(vd);
        }
      }

      // 外周赤道 (ψ=0) で行方向 ψ+ が +Y を向き GenerateSphere (行方向 -Y) と逆のため巻き順も逆（外向き法線に対し右手系 CCW）
      for (uint32_t j = 0; j < minorDiv; ++j) {
        for (uint32_t i = 0; i < majorDiv; ++i) {
          const uint32_t a = j * (majorDiv + 1) + i;
          const uint32_t b = a + 1;
          const uint32_t c = a + (majorDiv + 1);
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

  std::unique_ptr<Model> PrimitiveBuilder::CreateRing(const RingParams& p)
  {
    std::vector<VertexData> vertices;
    std::vector<uint32_t> indices;
    GenerateRing(p, vertices, indices);
    return BuildModel(vertices, indices, "<Primitive_Ring>");
  }

  std::unique_ptr<Model> PrimitiveBuilder::CreateCylinder(const CylinderParams& p)
  {
    std::vector<VertexData> vertices;
    std::vector<uint32_t> indices;
    GenerateCylinder(p, vertices, indices);
    return BuildModel(vertices, indices, "<Primitive_Cylinder>");
  }

  std::unique_ptr<Model> PrimitiveBuilder::CreateTorus(const TorusParams& p)
  {
    std::vector<VertexData> vertices;
    std::vector<uint32_t> indices;
    GenerateTorus(p, vertices, indices);
    return BuildModel(vertices, indices, "<Primitive_Torus>");
  }

  bool PrimitiveBuilder::ExportObj(const Model& model, const std::string& fileBaseName, const std::string& directory)
  {
    if (model.GetMeshCount() == 0) {
      return false;
    }

    std::filesystem::create_directories(directory);
    std::ofstream obj(directory + fileBaseName + ".obj");
    std::ofstream mtl(directory + fileBaseName + ".mtl");
    if (!obj.is_open() || !mtl.is_open()) {
      return false;
    }

    obj << "mtllib " << fileBaseName << ".mtl\n";

    // OBJ のインデックスは 1 始まり。複数メッシュは頂点番号を通し番号で連結する
    uint32_t vertexOffset = 1;
    for (size_t m = 0; m < model.GetMeshCount(); ++m) {
      const Mesh* mesh = model.GetMesh(m);
      const std::vector<VertexData>& verts = mesh->GetVertices();
      const std::vector<uint32_t>& indices = mesh->GetIndices();
      const std::string matName = "material_" + std::to_string(m);

      obj << "o " << fileBaseName << "_" << m << "\n";

      // ローダ (Model.cpp) が位置/法線に {-x,y,z}、UV に FlipUVs を適用するため逆変換して書く
      for (const VertexData& v : verts) {
        obj << std::format("v {} {} {}\n", -v.position.x, v.position.y, v.position.z);
      }
      for (const VertexData& v : verts) {
        obj << std::format("vt {} {}\n", v.texcoord.x, 1.0f - v.texcoord.y);
      }
      for (const VertexData& v : verts) {
        obj << std::format("vn {} {} {}\n", -v.normal.x, v.normal.y, v.normal.z);
      }

      obj << "usemtl " << matName << "\n";
      obj << "s off\n";

      // FlipWindingOrder の逆変換: (i0,i1,i2) を (i0,i2,i1) で書くと読込反転後 (i1,i2,i0) = 元の巡回置換に戻る
      for (size_t i = 0; i + 2 < indices.size(); i += 3) {
        const uint32_t a = indices[i] + vertexOffset;
        const uint32_t b = indices[i + 2] + vertexOffset;
        const uint32_t c = indices[i + 1] + vertexOffset;
        obj << std::format("f {0}/{0}/{0} {1}/{1}/{1} {2}/{2}/{2}\n", a, b, c);
      }
      vertexOffset += static_cast<uint32_t>(verts.size());

      // エンジンローダが読み戻すのは Kd と map_Kd のみ。Ns/d は外部 DCC (Blender 等) 互換のため出力
      const Vector4 col = mesh->GetMaterialColor();
      mtl << "newmtl " << matName << "\n";
      mtl << std::format("Ns {}\n", mesh->GetShininess());
      mtl << std::format("Kd {} {} {}\n", col.x, col.y, col.z);
      mtl << std::format("d {}\n", col.w);
      if (!mesh->GetTextureData().texturePath.empty()) {
        mtl << "map_Kd " << mesh->GetTextureData().texturePath << "\n";
      }
    }
    return true;
  }

} // namespace Tako

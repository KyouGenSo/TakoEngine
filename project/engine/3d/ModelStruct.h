#pragma once
#include <string>
#include <vector>
#include <map>
#include <optional>
#include <span>
#include <array>

#include "vector2.h"
#include "vector3.h"
#include "vector4.h"
#include "Matrix4x4.h"
#include "Quaternion.h"
#include "Transform.h"

// ノードデータ
struct Node
{
    QuatTransform transform;
    Matrix4x4 localMatrix;
    std::string name;
    std::vector<Node> children;
    std::vector<int> meshIndices;
};

// 頂点データ
struct VertexData
{
    Vector4 position;
    Vector2 texcoord;
    Vector3 normal;
};

// マテリアルデータ
struct TextureData {
    std::string texturePath;
    uint32_t textureIndex;
};

struct VertexWeightData
{
  float weight;
  uint32_t vertexIndex;
};

struct JointWeightData
{
  Matrix4x4 inverseBindMatrix;
  std::vector<VertexWeightData> vertexWeights;
};

struct ModelData
{
  std::vector<VertexData> vertices;
  TextureData textureData;
};

// スキンニングありのモデルデータ
struct SkinnigModelData {
  std::map<std::string, JointWeightData> skinClusterData;
  std::vector<VertexData> vertices;
	std::vector<uint32_t> indices;
  TextureData textureData;
  Node rootNode;
};

// マテリアル
struct Material
{
    Vector4 color;
    bool enableLighting;
    float padding1[3];
    Matrix4x4 uvTransform;
    float shininess;
    bool enableHighlight;
    float padding2[3];
};

// アニメーションデータたち
template<typename tValue>
struct KeyFrame
{
    float time;
    tValue value;
};

using KeyFrameVector3 = KeyFrame<Vector3>;
using KeyFrameQuaternion = KeyFrame<Quaternion>;


template<typename tValue>
struct AnimationCurve
{
    std::vector<KeyFrame<tValue>> keyFrames;
};

struct NodeAnimetion
{
    AnimationCurve<Vector3> translate;
    AnimationCurve<Quaternion> rotate;
    AnimationCurve<Vector3> scale;
};

struct Animation 
{
    float duration; // アニメーションの長さ(秒)
    std::map<std::string, NodeAnimetion> nodeAnimations;
};

// 骨データ
struct Joint
{
    QuatTransform transform;
    Matrix4x4 localMatrix;
    Matrix4x4 skeletonSpaceMatrix;
    std::string name;
    std::vector<int32_t> childrenIndex; // 子Jointのインデックスのリスト
    int32_t index;                      // Jointのインデックス
    std::optional<int32_t> parentIndex; // 親Jointのインデックス
};

// スケルトンデータ
struct Skeleton
{
    int32_t root;
    std::map<std::string, int32_t> jointMap;
    std::vector<Joint> joints;
};

const uint32_t MAX_INFLUENCE = 4;
struct VertexInfluence
{
  std::array<float, MAX_INFLUENCE> weights;
  std::array<uint32_t, MAX_INFLUENCE> jointIndices;
};

struct WellForGPU
{
  Matrix4x4 skeletonSpaceMat;                     // 位置用
  Matrix4x4 skeletonSpaceMatrixInvTransposeMat;   // 法線用
};

struct SkinningInfo
{
  uint32_t numVertices;
};

struct SkinCluster
{
  std::vector<Matrix4x4> inverseBindMatrices;
  Microsoft::WRL::ComPtr<ID3D12Resource> influenceResource;
  std::span<VertexInfluence> mappedInfluences;
  uint32_t influenceSrvIndex;
  Microsoft::WRL::ComPtr<ID3D12Resource> paletteResource;
  std::span<WellForGPU> mappedPalette;
  uint32_t paletteSrvIndex;
  std::pair<D3D12_CPU_DESCRIPTOR_HANDLE, D3D12_GPU_DESCRIPTOR_HANDLE> paletteSrvHandle;
};

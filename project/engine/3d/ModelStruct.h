#pragma once
#include <string>
#include <vector>
#include <map>
#include <optional>
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
};

// 頂点データ
struct VertexData
{
    Vector4 position;
    Vector2 texcoord;
    Vector3 normal;
};

// マテリアルデータ
struct MaterialData {
    std::string texturePath;
    uint32_t textureIndex;
};

// モデルデータ
struct ModelData {
    std::vector<VertexData> vertices;
    MaterialData material;
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

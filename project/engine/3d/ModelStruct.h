#pragma once
#include <string>
#include <vector>
#include <map>
#include "vector2.h"
#include "vector3.h"
#include "vector4.h"
#include "Matrix4x4.h"
#include "Quaternion.h"

// ノードデータ
struct Node
{
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

// アニメーションデータ
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
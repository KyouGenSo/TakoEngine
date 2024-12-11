#pragma once
#include <d3d12.h>
#include<wrl.h>
#include "ModelStruct.h"

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

class ModelBasic;

class Model
{
public: // メンバー関数
	/// <summary>
	/// 初期化
	/// </summary>
	void Initialize(ModelBasic* modelBasic, const std::string& fileName, bool hasAnimation, bool hasSkeleton);

	/// <summary>
	/// 更新
	/// </summary>
	void Update();

	/// <summary>
	/// 描画
	/// </summary>
	void Draw(Matrix4x4 world, Matrix4x4 viewProjection);

	/// <summary>
	/// Skeletonのデバグ用描画
	/// </summary>
	void DrawSkeleton(Matrix4x4 world, Matrix4x4 viewProjection);

	/// <summary>
	/// objファイルの読み込む
	///	</summary>
	void LoadModelFile(const std::string& directoryPath, const std::string& fileName);

	/// <summary>
	/// アニメーションの読み込み
	/// </summary>
	Animation LoadAnimationFile(const std::string& directoryPath, const std::string& fileName);

	// -----------------------------------Getters-----------------------------------//
	// nodeのlocalMatrixを取得
	const Matrix4x4& GetLocalMatrix() const { return modelData_.rootNode.localMatrix; }
	// アニメーションの有無を取得
	bool HasAnimation() const { return hasAnimation_; }
	// Skeletonの有無を取得
	bool HasSkeleton() const { return hasSkeleton_; }

	// -----------------------------------Setters-----------------------------------//
	void SetShininess(float shininess) { materialData_->shininess = shininess; }
	void SetEnableLighting(bool enableLighting) { materialData_->enableLighting = enableLighting; }
	void SetEnableHighlight(bool enableHighlight) { materialData_->enableHighlight = enableHighlight; }
	void SetAlpha(float alpha) { materialData_->color.w = alpha; }

private: // プライベートメンバー関数
	/// <summary>
	/// 頂点データの生成
	/// </summary>
	void CreateVertexData();

	/// <summary>
	/// 頂点インデクスの生成
	/// </summary>
	void CreateIndexData();

	/// <summary>
	/// マテリアルデータの生成
	/// </summary>
	void CreateMaterialData();

	/// <summary>
	/// ノード読み込み
	/// <summary>
	Node ReadNode(aiNode* node);

	/// <summary>
	/// Jointの生成
	/// </summary>
	int32_t CreateJoint(const Node& node, const std::optional<int32_t>& parentIndex, std::vector<Joint>& joints);

	/// <summary>
	/// Skeletonの生成
	/// </summary>
	Skeleton CreateSkeleton(const Node& rootNode);

	/// <summary>
	/// キーフレームの値を計算
	/// <summary>
	Vector3 CalcKeyFrameValue(const std::vector<KeyFrameVector3>& keyFrames, float time);
	Quaternion CalcKeyFrameValue(const std::vector<KeyFrameQuaternion>& keyFrames, float time);

	/// <summary>
	/// アニメーションの更新
	/// </summary>
	void UpdateAnimation(float deltaTime);

	/// <summary>
	/// skeletonの更新
	/// </summary>
	void UpdateSkeleton();

	/// <summary>
	/// アニメーションを適用
	/// </summary>
	void ApplyAnimation(float time);

private: // メンバ変数

	ModelBasic* m_modelBasic_;

	std::string directoryFolderName_;
	std::string ModelFolderName_;

	// モデルデータ
	ModelData modelData_;

	// アニメーションデータ
	Animation animationData_;
	bool hasAnimation_ = false;
	float animationTime_ = 0.0f;

	// skeleton
	Skeleton skeleton_;
	bool hasSkeleton_ = false;

	// バッファリソース
	Microsoft::WRL::ComPtr<ID3D12Resource> vertexResource_;
	Microsoft::WRL::ComPtr<ID3D12Resource> indexResource_;
	Microsoft::WRL::ComPtr<ID3D12Resource> materialResource_;

	// バッファリソース内のデータを指すポインタ
	VertexData* vertexData_ = nullptr;
	Material* materialData_ = nullptr;

	// バッファビュー
	D3D12_VERTEX_BUFFER_VIEW vertexBufferView_;
	D3D12_INDEX_BUFFER_VIEW indexBufferView_;

};
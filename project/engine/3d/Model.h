#pragma once
#include <d3d12.h>
#include <unordered_map>
#include<wrl.h>
#include "ModelStruct.h"

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

#include "Mesh.h"

class ModelBasic;
class DX12Basic;

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
	/// objファイルの読み込む
	///	</summary>
	void LoadModelFile(const std::string& directoryPath, const std::string& fileName);

	// -----------------------------------Getters-----------------------------------//
	// nodeのlocalMatrixを取得
	const Matrix4x4& GetLocalMatrix() const { return rootNode_.localMatrix; }
	// アニメーションの有無を取得
	bool HasAnimation() const { return hasAnimation_; }
	// Skeletonの有無を取得
	bool HasSkeleton() const { return hasSkeleton_; }

	// -----------------------------------Setters-----------------------------------//
  void SetShininess(float shininess);
  void SetEnableLighting(bool enableLighting);
  void SetEnableHighlight(bool enableHighlight);
  void SetMaterialColor(const Vector4& color);

private: // プライベートメンバー関数
  /// <summary>
  /// Skeletonのデバグ用描画
  /// </summary>
  void DrawSkeleton(Matrix4x4 world, Matrix4x4 viewProjection);

  /// <summary>
  /// アニメーションの読み込み
  /// </summary>
  Animation LoadAnimationFile(const std::string& directoryPath, const std::string& fileName);

  /// <summary>
  /// スキニング処理
  /// </summary>
  void UpdateSkinning();
  void PrepareSkinning();
  void ExecuteSkinning();
  void InitializeMatrixPalette();

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
  /// SkinClusterの生成
  /// </summary>
  //SkinCluster CreateSkinCluster();

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
  /// SkinClusterの更新
  /// </summary>
  void UpdateSkinCluster();

	/// <summary>
	/// アニメーションを適用
	/// </summary>
	void UpdateSkeletonAnimation(float time);

private: // メンバ変数

	ModelBasic* m_modelBasic_;
  DX12Basic* m_dx12_;

	std::string directoryFolderName_;
	std::string ModelFolderName_;

	// モデルデータ
	//SkinnigModelData modelData_;
  std::vector<Mesh> meshes_;

  // ノードデータ
  Node rootNode_;

  // テクスチャキャッシュ
  std::unordered_map<std::string, TextureData> textureCache_;

	// スケルトン・アニメーション関連
	Animation animationData_;
  Skeleton skeleton_;
	bool hasAnimation_ = false;
	float animationTime_ = 0.0f;
  bool hasSkeleton_ = false;

  // スキニング関連
  std::vector<Matrix4x4> inverseBindMatrices_;
  Microsoft::WRL::ComPtr<ID3D12Resource> paletteResource_;
  std::span<WellForGPU> mappedPalette_;
  uint32_t paletteSrvIndex_;
  std::pair<D3D12_CPU_DESCRIPTOR_HANDLE, D3D12_GPU_DESCRIPTOR_HANDLE> paletteSrvHandle_;
  std::map<std::string, JointWeightData> skinClusterData_;

  // skinCluster
  //SkinCluster skinCluster_;

	// バッファリソース
	//Microsoft::WRL::ComPtr<ID3D12Resource> vertexResource_;
	//Microsoft::WRL::ComPtr<ID3D12Resource> indexResource_;
	//Microsoft::WRL::ComPtr<ID3D12Resource> materialResource_;

  //Microsoft::WRL::ComPtr<ID3D12Resource> uavVertexOutputResource_;
  //Microsoft::WRL::ComPtr<ID3D12Resource> skinningInfoResource_;

  //uint32_t vertexSrvIndex_ = 0;
  //uint32_t uavIndex_ = 0;

	// バッファリソース内のデータを指すポインタ
	//VertexData* vertexData_ = nullptr;
	//Material* materialData_ = nullptr;
  //SkinningInfo* skinningInfoData_ = nullptr;

	// バッファビュー
	//D3D12_VERTEX_BUFFER_VIEW vertexBufferView_;
	//D3D12_INDEX_BUFFER_VIEW indexBufferView_;

};
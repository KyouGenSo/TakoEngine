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
  /// 終了処理
  /// </summary>
  void Finalize();

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

  /// <summary>
  /// クローン
  ///	</summary>
  Model* Clone() const;

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
  void SetUvTransform(const Transform& uvTransform);
  void SetEnvironmentTexture(uint32_t textureIndex);
  void SetEnableEnvMap(bool enableEnvMap);
  void SetEnvMapCoefficient(float coefficient);

private: // プライベートメンバー関数
  /// <summary>
  /// ノード階層で座標変換行列を処理して描画
  /// </summary>
  void ProcessNodeHierarchy(const Node& node, const Matrix4x4& parentGlobalMatrix, Matrix4x4 world, Matrix4x4 viewProjection);

  /// <summary>
  /// Skeletonのデバグ用描画
  /// </summary>
  void DrawSkeleton(Matrix4x4 world, Matrix4x4 viewProjection);

  /// <summary>
  /// アニメーションの読み込み
  /// </summary>
  Animation LoadAnimationFile(const std::string& directoryPath, const std::string& fileName);

  /// <summary>
  /// スキニング処理関連
  /// </summary>
  void InitializeMatrixPalette();
  void PrepareSkinning();
  void ExecuteSkinning();

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
	/// アニメーションを適用
	/// </summary>
	void UpdateSkeletonAnimation(float time);

  /// <summary>
  /// スキニング関連リソースの解放
  /// </summary>
  void ReleaseSkinningSRVIndex();

private: // メンバ変数

	ModelBasic* m_modelBasic_;
  DX12Basic* m_dx12_;

	std::string directoryFolderName_;
	std::string ModelFolderName_;

	// モデルデータ
  std::vector<Mesh*> meshes_;

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
  uint32_t paletteSrvIndex_ = 0;
  std::pair<D3D12_CPU_DESCRIPTOR_HANDLE, D3D12_GPU_DESCRIPTOR_HANDLE> paletteSrvHandle_;
  std::map<std::string, JointWeightData> skinClusterData_;
};
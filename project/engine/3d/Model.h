#pragma once
#include <d3d12.h>
#include<wrl.h>
#include <string>
#include <vector>
#include "vector2.h"
#include "vector3.h"
#include "vector4.h"
#include "Mat4x4Func.h"

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

class ModelBasic;

class Model
{
public: // 構造体
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

public: // メンバー関数
	/// <summary>
	/// 初期化
	/// </summary>
	void Initialize(ModelBasic* modelBasic, const std::string& fileName);

	/// <summary>
	/// 描画
	/// </summary>
	void Draw();

	///<summary>
	///objファイルの読み込む
	///	</summary>
	void LoadModelFile(const std::string& directoryPath, const std::string& fileName);

	///<summary>
	///mtlファイルの読み込む
	/// </summary>
	void LoadMtlFile(const std::string& directoryPath, const std::string& fileName);

	// -----------------------------------Getters-----------------------------------//
	// nodeのlocalMatrixを取得
	const Matrix4x4& GetLocalMatrix() const { return modelData_.rootNode.localMatrix; }

	// -----------------------------------Setters-----------------------------------//
	void SetShininess(float shininess) { materialData_->shininess = shininess; }
	void SetEnableLighting(bool enableLighting) { materialData_->enableLighting = enableLighting; }
	void SetEnableHighlight(bool enableHighlight) { materialData_->enableHighlight = enableHighlight; }

private: // プライベートメンバー関数
	/// <summary>
	/// 頂点データの生成
	/// </summary>
	void CreateVertexData();

	/// <summary>
	/// マテリアルデータの生成
	/// </summary>
	void CreateMaterialData();

	/// <summary>
	/// ノード読み込み
	/// <summary>
	Node ReadNode(aiNode* node);

private: // メンバ変数
	
	ModelBasic* m_modelBasic_;

	std::string directoryFolderName_;
	std::string ModelFolderName_;

	// モデルデータ
	ModelData modelData_;

	// バッファリソース
	Microsoft::WRL::ComPtr<ID3D12Resource> vertexResource_;
	Microsoft::WRL::ComPtr<ID3D12Resource> materialResource_;

	// バッファリソース内のデータを指すポインタ
	VertexData* vertexData_ = nullptr;
	Material* materialData_ = nullptr;

	// バッファビュー
	D3D12_VERTEX_BUFFER_VIEW vertexBufferView_;

};
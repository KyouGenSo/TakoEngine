#pragma once
#include <d3d12.h>
#include<wrl.h>
#include <string>
#include <vector>
#include "vector2.h"
#include "vector3.h"
#include "vector4.h"
#include "Mat4x4Func.h"

class Object3dBasic;

class Object3d {

public: // 構造体

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
	};

	// マテリアル
	struct Material
	{
		Vector4 color;
		bool enableLighting;
		float padding[3];
		Matrix4x4 uvTransform;
	};

	// 座標変換行列データ
	struct TransformationMatrix
	{
		Matrix4x4 WVP;
		Matrix4x4 world;
	};

	// 平行光源データ
	struct DirectionalLight
	{
		Vector4 color;
		Vector3 direction;
		int32_t lightType;
		float intensity;
	};

public: // メンバー関数
	///<summary>
	///初期化
	/// </summary>
	void Initialize(Object3dBasic* object3dBasic);

	///<summary>
	///更新
	/// </summary>
	void Update();

	///<summary>
	///描画
	/// </summary>
	void Draw();

	///<summary>
	///objファイルの読み込む
	///	</summary>
	void LoadObjFile(const std::string& directoryPath, const std::string& fileName);

	///<summary>
	///mtlファイルの読み込む
	/// </summary>
	void LoadMtlFile(const std::string& directoryPath, const std::string& fileName);

private: // プライベートメンバー関数

	///<summary>
	///頂点データの生成
	/// </summary>
	void CreateVertexData();

	///<summary>
	///マテリアルデータの生成
	/// </summary>
	void CreateMaterialData();

	///<summary>
	///座標変換行列データの生成
	/// </summary>
	void CreateTransformationMatrixData();

	///<summary>
	///平行光源データの生成
	/// </summary>
	void CreateDirectionalLightData();

private: // メンバー変数

	// 3dオブジェクトの基本クラス
	Object3dBasic* obj3dBasic_ = nullptr;

	// モデルデータ
	ModelData modelData_;

	// トランスフォーム
	Transform transform_;
	
	//カメラのトランスフォーム
	Transform cameraTransform_;

	// バッファリソース
	Microsoft::WRL::ComPtr<ID3D12Resource> vertexResource_;
	Microsoft::WRL::ComPtr<ID3D12Resource> materialResource_;
	Microsoft::WRL::ComPtr<ID3D12Resource> transformationMatResource_;
	Microsoft::WRL::ComPtr<ID3D12Resource> directionalLightResource_;

	// バッファリソース内のデータを指すポインタ
	VertexData* vertexData_ = nullptr;
	Material* materialData_ = nullptr;
	TransformationMatrix* transformationMatData_ = nullptr;
	DirectionalLight* directionalLightData_ = nullptr;

	// バッファビュー
	D3D12_VERTEX_BUFFER_VIEW vertexBufferView_;

};

#pragma once
#include <string>
#include <vector>
#include "vector2.h"
#include "vector3.h"
#include "vector4.h"

class Object3dBasic;

class Object3d {

public: // 構造体
	struct VertexData
	{
		Vector4 position;
		Vector2 texcoord;
		Vector3 normal;
	};

	struct MaterialData {
		std::string texturePath;
	};

	struct ModelData {
		std::vector<VertexData> vertices;
		MaterialData material;
	};

	
public:
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
	static ModelData LoadObjFile(const std::string& directoryPath, const std::string& fileName);

	///<summary>
	///mtlファイルの読み込む
	/// </summary>
	static MaterialData LoadMtlFile(const std::string& directoryPath, const std::string& fileName);

private: // メンバー変数

	// 3dオブジェクトの基本クラス
	Object3dBasic* object3dBasic_ = nullptr;

	// モデルデータ
	ModelData modelData_;

};

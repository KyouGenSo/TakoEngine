#include "Object3d.h"
#include "Object3dbasic.h"
#include "DX12Basic.h"
#include "TextureManager.h"
#include <cassert>
#include<string>
#include<fstream>
#include<sstream>

void Object3d::Initialize(Object3dBasic* obj3dBasic)
{
	// Object3dBasicクラスのインスタンスを参照
	obj3dBasic_ = obj3dBasic;

	// トランスフォームに初期化値を設定
	transform_ = { Vector3(1.0f, 1.0f, 1.0f), Vector3(0.0f, 0.0f, 0.0f), Vector3(0.0f, 0.0f, 0.0f) };
	cameraTransform_ = { Vector3(1.0f, 1.0f, 1.0f), Vector3(0.3f, 0.0f, 0.0f), Vector3(0.0f, 4.0f, -10.0f) };

	// objファイルの読み込み
	LoadObjFile("resources", "bunny.obj");

	// 頂点データの生成
	CreateVertexData(); 

	// マテリアルデータの生成
	CreateMaterialData();

	// 座標変換行列データの生成
	CreateTransformationMatrixData();

	// 平行光源データの生成
	CreateDirectionalLightData();

	// テクスチャの読み込み
	TextureManager::GetInstance()->LoadTexture(modelData_.material.texturePath);

	// テクスチャインデックスを保存
	modelData_.material.textureIndex = TextureManager::GetInstance()->GetTextureIndex(modelData_.material.texturePath);
}

void Object3d::Update()
{
	// トランスフォームでワールド行列を作る
	Matrix4x4 worldMatrix = Mat4x4::MakeAffine(transform_.scale, transform_.rotate, transform_.translate);

	// ビュー行列を作る
	Matrix4x4 cameraMatrix = Mat4x4::MakeAffine(cameraTransform_.scale, cameraTransform_.rotate, cameraTransform_.translate);
	Matrix4x4 viewMatrix = Mat4x4::Inverse(cameraMatrix);

	// プロジェクション行列を作る
	Matrix4x4 projectionMatrix = Mat4x4::MakePerspective(0.45f, static_cast<float>(WinApp::kClientWidth) / static_cast<float>(WinApp::kClientHeight), 0.1f, 100.0f);

	// 座標変換行列データに書き込む
	transformationMatData_->WVP = Mat4x4::Multiply(worldMatrix, Mat4x4::Multiply(viewMatrix, projectionMatrix));
	transformationMatData_->world = worldMatrix;

}

void Object3d::Draw()
{
	// 頂点バッファビューを設定
	obj3dBasic_->GetDX12Basic()->GetCommandList()->IASetVertexBuffers(0, 1, &vertexBufferView_);

	// マテリアルCBufferの場所を設定
	obj3dBasic_->GetDX12Basic()->GetCommandList()->SetGraphicsRootConstantBufferView(0, materialResource_->GetGPUVirtualAddress());

	// 座標変換行列CBufferの場所を設定
	obj3dBasic_->GetDX12Basic()->GetCommandList()->SetGraphicsRootConstantBufferView(1, transformationMatResource_->GetGPUVirtualAddress());

	// 平行光源CBufferの場所を設定
	obj3dBasic_->GetDX12Basic()->GetCommandList()->SetGraphicsRootConstantBufferView(3, directionalLightResource_->GetGPUVirtualAddress());

	// SRVのDescriptorTableを設定,テクスチャを指定
	obj3dBasic_->GetDX12Basic()->GetCommandList()->SetGraphicsRootDescriptorTable(2, TextureManager::GetInstance()->GetSRVGpuHandle(modelData_.material.textureIndex));

	// 描画
	obj3dBasic_->GetDX12Basic()->GetCommandList()->DrawInstanced(UINT(modelData_.vertices.size()), 1, 0, 0);
}

void Object3d::LoadObjFile(const std::string& directoryPath, const std::string& fileName)
{
	VertexData triangleVertices[3];
	std::vector<Vector4> positions;
	std::vector<Vector2> texcoords;
	std::vector<Vector3> normals;
	std::string line;

	std::ifstream file(directoryPath + "/" + fileName);
	assert(file.is_open());

	while (std::getline(file, line))
	{
		std::string identifier;
		std::istringstream s(line);
		s >> identifier;

		if (identifier == "v") {
			Vector4 position;
			s >> position.x >> position.y >> position.z;
			position.w = 1.0f;
			positions.push_back(position);

		} else if (identifier == "vt") {
			Vector2 texcoord;
			s >> texcoord.x >> texcoord.y;
			texcoords.push_back(texcoord);

		} else if (identifier == "vn") {
			Vector3 normal;
			s >> normal.x >> normal.y >> normal.z;
			normals.push_back(normal);

		} else if (identifier == "f") {

			for (int32_t facevertex = 0; facevertex < 3; facevertex++) {
				std::string vertexDefiniton;
				s >> vertexDefiniton;

				std::istringstream v(vertexDefiniton);
				uint32_t elementIndices[3];

				for (int32_t element = 0; element < 3; element++) {
					std::string index;
					std::getline(v, index, '/');
					elementIndices[element] = std::stoi(index);
				}

				Vector4 position = positions[elementIndices[0] - 1];
				Vector2 texcoord = texcoords[elementIndices[1] - 1];
				Vector3 normal = normals[elementIndices[2] - 1];

				position.z *= -1.0f;
				normal.z *= -1.0f;
				texcoord.y = 1.0f - texcoord.y;

				triangleVertices[facevertex] = { position, texcoord, normal };

			}

			// 三角形の頂点データを追加
			modelData_.vertices.push_back(triangleVertices[2]);
			modelData_.vertices.push_back(triangleVertices[1]);
			modelData_.vertices.push_back(triangleVertices[0]);

		} else if (identifier == "mtllib") {
			std::string mtlFileName;
			s >> mtlFileName;
			LoadMtlFile(directoryPath, mtlFileName);
		}
	}

}

void Object3d::Object3d::LoadMtlFile(const std::string& directoryPath, const std::string& fileName)
{
	std::string line;

	std::ifstream file(directoryPath + "/" + fileName);
	assert(file.is_open());

	while (std::getline(file, line))
	{
		std::string identifier;
		std::istringstream s(line);
		s >> identifier;

		if (identifier == "map_Kd") {
			std::string textureFileName;
			s >> textureFileName;
			modelData_.material.texturePath = directoryPath + "/" + textureFileName;
		}
	}
}

void Object3d::CreateVertexData()
{
	// 頂点リソースを生成
	vertexResource_ = obj3dBasic_->GetDX12Basic()->MakeBufferResource(sizeof(VertexData) * modelData_.vertices.size());

	// 頂点バッファビューを作る
	vertexBufferView_.BufferLocation = vertexResource_->GetGPUVirtualAddress();
	vertexBufferView_.SizeInBytes = UINT(sizeof(VertexData) * modelData_.vertices.size());
	vertexBufferView_.StrideInBytes = sizeof(VertexData);

	// 頂点リソースをマップ
	vertexResource_->Map(0, nullptr, reinterpret_cast<void**>(&vertexData_));
	memcpy(vertexData_, modelData_.vertices.data(), sizeof(VertexData) * modelData_.vertices.size());
}

void Object3d::CreateMaterialData()
{
	// マテリアルリソースを生成
	materialResource_ = obj3dBasic_->GetDX12Basic()->MakeBufferResource(sizeof(Material));

	// マテリアルリソースをマップ
	materialResource_->Map(0, nullptr, reinterpret_cast<void**>(&materialData_));

	// マテリアルデータの初期値を書き込む
	materialData_->color = Vector4(1.0f, 1.0f, 1.0f, 1.0f);
	materialData_->enableLighting = false;
	materialData_->uvTransform = Mat4x4::MakeIdentity();
}

void Object3d::CreateTransformationMatrixData()
{
	// 座標変換行列リソースを生成
	transformationMatResource_ = obj3dBasic_->GetDX12Basic()->MakeBufferResource(sizeof(TransformationMatrix));

	// 座標変換行列リソースをマップ
	transformationMatResource_->Map(0, nullptr, reinterpret_cast<void**>(&transformationMatData_));

	// 座標変換行列データの初期値を書き込む
	transformationMatData_->WVP = Mat4x4::MakeIdentity();
	transformationMatData_->world = Mat4x4::MakeIdentity();
}

void Object3d::CreateDirectionalLightData()
{
	// 平行光源リソースを生成
	directionalLightResource_ = obj3dBasic_->GetDX12Basic()->MakeBufferResource(sizeof(DirectionalLight));

	// 平行光源リソースをマップ
	directionalLightResource_->Map(0, nullptr, reinterpret_cast<void**>(&directionalLightData_));

	// 平行光源データの初期値を書き込む
	directionalLightData_->direction = Vector3(0.0f, -1.0f, 0.0f); // ライトの方向
	
	directionalLightData_->color = { 1.0f, 1.0f, 1.0f, 1.0f };     // ライトの色
	
	directionalLightData_->lightType = 0;                          // ライトのタイプ 0:Lambert 1:Half-Lambert
	
	directionalLightData_->intensity = 1.0f;                       // 輝度
}

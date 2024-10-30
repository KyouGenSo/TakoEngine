#include "Model.h"
#include "ModelBasic.h"
#include "DX12Basic.h"
#include "TextureManager.h"
#include "SrvManager.h"
#include <cassert>
#include<fstream>
#include<sstream>

void Model::Initialize(ModelBasic* modelBasic, const std::string& fileName)
{
	m_modelBasic_ = modelBasic;

	directoryFolderName_ = m_modelBasic_->GetDirectoryFolderName();

	ModelFolderName_ = m_modelBasic_->GetModelFolderName();

	// objファイルの読み込み
	LoadObjFile(directoryFolderName_ + "/" + ModelFolderName_, fileName);

	// 頂点データの生成
	CreateVertexData();

	// マテリアルデータの生成
	CreateMaterialData();

	// テクスチャの読み込み
	TextureManager::GetInstance()->LoadTexture(modelData_.material.texturePath);

	// テクスチャインデックスを保存
	modelData_.material.textureIndex = TextureManager::GetInstance()->GetSRVIndex(modelData_.material.texturePath);
}

void Model::Draw()
{
	// 頂点バッファビューを設定
	m_modelBasic_->GetDX12Basic()->GetCommandList()->IASetVertexBuffers(0, 1, &vertexBufferView_);

	// マテリアルデータを設定
	m_modelBasic_->GetDX12Basic()->GetCommandList()->SetGraphicsRootConstantBufferView(0, materialResource_->GetGPUVirtualAddress());

	// SRVのDescriptorTableを設定,テクスチャを指定
	SrvManager::GetInstance()->SetRootDescriptorTable(2, modelData_.material.textureIndex);

	// 描画
	m_modelBasic_->GetDX12Basic()->GetCommandList()->DrawInstanced(UINT(modelData_.vertices.size()), 1, 0, 0);
}

void Model::LoadObjFile(const std::string& directoryPath, const std::string& fileName)
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
			LoadMtlFile(directoryFolderName_, mtlFileName);
		}
	}

}

void Model::LoadMtlFile(const std::string& directoryPath, const std::string& fileName)
{
	std::string line;

	std::ifstream file(directoryPath + "/" + ModelFolderName_ + "/" + fileName);
	assert(file.is_open());

	while (std::getline(file, line))
	{
		std::string identifier;
		std::istringstream s(line);
		s >> identifier;

		if (identifier == "map_Kd") {
			std::string textureFileName;
			s >> textureFileName;
			modelData_.material.texturePath = textureFileName;
		}
	}
}

void Model::CreateVertexData()
{
	// 頂点リソースを生成
	vertexResource_ = m_modelBasic_->GetDX12Basic()->MakeBufferResource(sizeof(VertexData) * modelData_.vertices.size());

	// 頂点バッファビューを作る
	vertexBufferView_.BufferLocation = vertexResource_->GetGPUVirtualAddress();
	vertexBufferView_.SizeInBytes = UINT(sizeof(VertexData) * modelData_.vertices.size());
	vertexBufferView_.StrideInBytes = sizeof(VertexData);

	// 頂点リソースをマップ
	vertexResource_->Map(0, nullptr, reinterpret_cast<void**>(&vertexData_));
	memcpy(vertexData_, modelData_.vertices.data(), sizeof(VertexData) * modelData_.vertices.size());
}

void Model::CreateMaterialData()
{
	// マテリアルリソースを生成
	materialResource_ = m_modelBasic_->GetDX12Basic()->MakeBufferResource(sizeof(Material));

	// マテリアルリソースをマップ
	materialResource_->Map(0, nullptr, reinterpret_cast<void**>(&materialData_));

	// マテリアルデータの初期値を書き込む
	materialData_->color = Vector4(1.0f, 1.0f, 1.0f, 1.0f);
	materialData_->enableLighting = true;
	materialData_->uvTransform = Mat4x4::MakeIdentity();
}

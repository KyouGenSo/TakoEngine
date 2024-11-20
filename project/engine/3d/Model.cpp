#include "Model.h"
#include "ModelBasic.h"
#include "DX12Basic.h"
#include "TextureManager.h"
#include "SrvManager.h"
#include <cassert>
#include <fstream>
#include <sstream>

void Model::Initialize(ModelBasic* modelBasic, const std::string& fileName)
{
	m_modelBasic_ = modelBasic;

	directoryFolderName_ = m_modelBasic_->GetDirectoryFolderName();

	ModelFolderName_ = m_modelBasic_->GetModelFolderName();

	// objファイルの読み込み
	LoadModelFile(directoryFolderName_ + "/" + ModelFolderName_, fileName);

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

void Model::LoadModelFile(const std::string& directoryPath, const std::string& fileName)
{
	Assimp::Importer importer;
	std::string filePath = directoryPath + "/" + fileName;
	const aiScene* scene = importer.ReadFile(filePath.c_str(), aiProcess_FlipWindingOrder | aiProcess_FlipUVs );
	assert(scene->HasMeshes()); // メッシュがない場合はエラー

	// メッシュの解析
	for (uint32_t meshIndex = 0; meshIndex < scene->mNumMeshes; meshIndex++)
	{
		aiMesh* mesh = scene->mMeshes[meshIndex];
		assert(mesh->HasTextureCoords(0) && mesh->HasNormals()); // テクスチャ座標と法線がない場合はエラー

		for (uint32_t faceIndex = 0; faceIndex < mesh->mNumFaces; ++faceIndex) {
			aiFace& face = mesh->mFaces[faceIndex];
			assert(face.mNumIndices == 3); // 三角形以外はエラー

			for (uint32_t element = 0; element < face.mNumIndices; ++element) {
				uint32_t vertexIndex = face.mIndices[element];
				aiVector3D position = mesh->mVertices[vertexIndex];
				aiVector3D texcoord = mesh->mTextureCoords[0][vertexIndex];
				aiVector3D normal = mesh->mNormals[vertexIndex];

				VertexData vertex;
				vertex.position = Vector4(position.x, position.y, position.z, 1.0f);
				vertex.texcoord = Vector2(texcoord.x, texcoord.y);
				vertex.normal = Vector3(normal.x, normal.y, normal.z);

				vertex.position.z *= -1.0f;
				vertex.normal.z *= -1.0f;

				modelData_.vertices.push_back(vertex);
			}
		}

	}

	// マテリアルファイルの読み込み
	for (uint32_t materialIndex = 0; materialIndex < scene->mNumMaterials; materialIndex++)
	{
		aiMaterial* material = scene->mMaterials[materialIndex];
		if (material->GetTextureCount(aiTextureType_DIFFUSE) != 0)
		{
			aiString texturePath;
			material->GetTexture(aiTextureType_DIFFUSE, 0, &texturePath);
			modelData_.material.texturePath = texturePath.C_Str();
		}
	}

	// ノードの読み込み
	modelData_.rootNode = ReadNode(scene->mRootNode);
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
	materialData_->enableHighlight = true;
	materialData_->uvTransform = Mat4x4::MakeIdentity();
	materialData_->shininess = 15.0f;
}

Model::Node Model::ReadNode(aiNode* node)
{
	Node result;

	aiMatrix4x4 aiLocalMatrix = node->mTransformation; // ノードのローカル変換行列を取得
	aiLocalMatrix.Transpose(); // 列ベクトルを行ベクトルに変換

	result.localMatrix.m[0][0] = aiLocalMatrix[0][0];
	result.localMatrix.m[0][1] = aiLocalMatrix[0][1];
	result.localMatrix.m[0][2] = aiLocalMatrix[0][2];
	result.localMatrix.m[0][3] = aiLocalMatrix[0][3];

	result.localMatrix.m[1][0] = aiLocalMatrix[1][0];
	result.localMatrix.m[1][1] = aiLocalMatrix[1][1];
	result.localMatrix.m[1][2] = aiLocalMatrix[1][2];
	result.localMatrix.m[1][3] = aiLocalMatrix[1][3];

	result.localMatrix.m[2][0] = aiLocalMatrix[2][0];
	result.localMatrix.m[2][1] = aiLocalMatrix[2][1];
	result.localMatrix.m[2][2] = aiLocalMatrix[2][2];
	result.localMatrix.m[2][3] = aiLocalMatrix[2][3];

	result.localMatrix.m[3][0] = aiLocalMatrix[3][0];
	result.localMatrix.m[3][1] = aiLocalMatrix[3][1];
	result.localMatrix.m[3][2] = aiLocalMatrix[3][2];
	result.localMatrix.m[3][3] = aiLocalMatrix[3][3];

	result.name = node->mName.C_Str(); // ノードの名前を取得

	result.children.resize(node->mNumChildren); // 子ノードの数だけリサイズ
	for (uint32_t childIndex = 0; childIndex < node->mNumChildren; ++childIndex)
	{
		result.children[childIndex] = ReadNode(node->mChildren[childIndex]); // 再帰的に子ノードを読み込む
	}

	return result;
}

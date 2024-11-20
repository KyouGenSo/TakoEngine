#include "Model.h"
#include "ModelBasic.h"
#include "DX12Basic.h"
#include "TextureManager.h"
#include "SrvManager.h"
#include "Mat4x4Func.h"
#include "QuatFunc.h"
#include <cassert>
#include <fstream>
#include <sstream>

void Model::Initialize(ModelBasic* modelBasic, const std::string& fileName, bool hasAnimation)
{
	m_modelBasic_ = modelBasic;

	directoryFolderName_ = m_modelBasic_->GetDirectoryFolderName();

	ModelFolderName_ = m_modelBasic_->GetModelFolderName();

	hasAnimation_ = hasAnimation;

	// objファイルの読み込み
	LoadModelFile(directoryFolderName_ + "/" + ModelFolderName_, fileName);

	// アニメーションの読み込み
	if (hasAnimation_)
	{
		animationData_ = LoadAnimationFile(directoryFolderName_ + "/" + ModelFolderName_, fileName); 
	}

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

Animation Model::LoadAnimationFile(const std::string& directoryPath, const std::string& fileName)
{
	Animation animation;
	Assimp::Importer importer;
	std::string filePath = directoryPath + "/" + fileName;
	const aiScene* scene = importer.ReadFile(filePath.c_str(), 0);

	// アニメーションがない場合はエラー
	assert(scene->HasAnimations());

	// アニメーションの解析
	aiAnimation* aiAnimation = scene->mAnimations[0]; // 一旦最初のアニメーションだけ対応
	animation.duration = float(aiAnimation->mDuration / aiAnimation->mTicksPerSecond); // アニメーションの長さを取得,秒に変換

	// ノードアニメーションの解析
	for (uint32_t channelIndex = 0; channelIndex < aiAnimation->mNumChannels; ++channelIndex)
	{
		aiNodeAnim* aiNodeAnim = aiAnimation->mChannels[channelIndex];
		NodeAnimetion& nodeAnimetion = animation.nodeAnimations[aiNodeAnim->mNodeName.C_Str()]; // ノード名をキーにしてノードアニメーションを取得

		// 位置アニメーションの解析
		for (uint32_t keyIndex = 0; keyIndex < aiNodeAnim->mNumPositionKeys; ++keyIndex)
		{
			aiVectorKey& aiKey = aiNodeAnim->mPositionKeys[keyIndex];
			KeyFrameVector3 keyFrame;
			keyFrame.time = float(aiKey.mTime / aiAnimation->mTicksPerSecond); // 時間を秒に変換
			keyFrame.value = Vector3(-aiKey.mValue.x, aiKey.mValue.y, aiKey.mValue.z);
			nodeAnimetion.translate.keyFrames.push_back(keyFrame);
		}

		// 回転アニメーションの解析
		for (uint32_t keyIndex = 0; keyIndex < aiNodeAnim->mNumRotationKeys; ++keyIndex)
		{
			aiQuatKey& aiKey = aiNodeAnim->mRotationKeys[keyIndex];
			KeyFrameQuaternion keyFrame;
			keyFrame.time = float(aiKey.mTime / aiAnimation->mTicksPerSecond); // 時間を秒に変換
			keyFrame.value = Quaternion(aiKey.mValue.x, -aiKey.mValue.y, -aiKey.mValue.z, aiKey.mValue.w); // クォータニオンのy,z成分を反転,右手系から左手系に変換
			nodeAnimetion.rotate.keyFrames.push_back(keyFrame);
		}

		// スケールアニメーションの解析
		for (uint32_t keyIndex = 0; keyIndex < aiNodeAnim->mNumScalingKeys; ++keyIndex)
		{
			aiVectorKey& aiKey = aiNodeAnim->mScalingKeys[keyIndex];
			KeyFrameVector3 keyFrame;
			keyFrame.time = float(aiKey.mTime / aiAnimation->mTicksPerSecond); // 時間を秒に変換
			keyFrame.value = Vector3(aiKey.mValue.x, aiKey.mValue.y, aiKey.mValue.z);
			nodeAnimetion.scale.keyFrames.push_back(keyFrame);
		}
	}

	return animation;
}

void Model::UpdateAnimation(float deltaTime)
{
	animationTime_ += deltaTime; // アニメーション時間を更新
	animationTime_ = std::fmod(animationTime_, animationData_.duration); // アニメーション時間がアニメーションの長さを超えたらループ
	NodeAnimetion& nodeAnimetion = animationData_.nodeAnimations[modelData_.rootNode.name]; // ルートノードのアニメーションを取得

	// 位置アニメーションの計算
	Vector3 translate = CalcKeyFrameValue(nodeAnimetion.translate.keyFrames, animationTime_);
	// 回転アニメーションの計算
	Quaternion rotate = CalcKeyFrameValue(nodeAnimetion.rotate.keyFrames, animationTime_);
	// スケールアニメーションの計算
	Vector3 scale = CalcKeyFrameValue(nodeAnimetion.scale.keyFrames, animationTime_);

	// ローカル変換行列を生成
	Matrix4x4 localMatrix = Mat4x4::MakeAffine(scale, rotate, translate);
	modelData_.rootNode.localMatrix = localMatrix; // ルートノードのローカル変換行列を更新
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

Node Model::ReadNode(aiNode* node)
{
	Node result;

	aiVector3D scale, position;
	aiQuaternion rotate;
	node->mTransformation.Decompose(scale, rotate, position); // スケール,回転,平行移動を取得
	
	result.transform.scale = { scale.x, scale.y, scale.z }; // スケールを取得
	result.transform.rotate = { rotate.x, -rotate.y, -rotate.z, rotate.w }; // 回転を取得,右手系から左手系に変換
	result.transform.translate = { -position.x, position.y, position.z }; // 平行移動を取得,x軸を反転

	result.localMatrix = Mat4x4::MakeAffine(result.transform.scale, result.transform.rotate, result.transform.translate); // ローカル変換行列を生成

	result.name = node->mName.C_Str(); // ノードの名前を取得

	result.children.resize(node->mNumChildren); // 子ノードの数だけリサイズ
	for (uint32_t childIndex = 0; childIndex < node->mNumChildren; ++childIndex)
	{
		result.children[childIndex] = ReadNode(node->mChildren[childIndex]); // 再帰的に子ノードを読み込む
	}

	return result;
}

Vector3 Model::CalcKeyFrameValue(const std::vector<KeyFrameVector3>& keyFrames, float time)
{
	assert(!keyFrames.empty()); // キーフレームがない場合はエラー
	if (keyFrames.size() == 1 || time <= keyFrames[0].time)
	{
		return keyFrames[0].value; // 最初のキーフレームの値を返す
	}

	for (uint32_t keyIndex = 0; keyIndex < keyFrames.size() - 1; ++keyIndex)
	{
		uint32_t nextKeyIndex = keyIndex + 1;
		if ( keyFrames[keyIndex].time <= time && time <= keyFrames[nextKeyIndex].time)
		{
			float t = (time - keyFrames[keyIndex].time) / (keyFrames[nextKeyIndex].time - keyFrames[keyIndex].time); // 補間係数を計算
			return Vec3::Lerp(keyFrames[keyIndex].value, keyFrames[nextKeyIndex].value, t); // 線形補間
		}
	}

	return (*keyFrames.rbegin()).value; // 最後のキーフレームの値を返す
}

Quaternion Model::CalcKeyFrameValue(const std::vector<KeyFrameQuaternion>& keyFrames, float time)
{
	assert(!keyFrames.empty()); // キーフレームがない場合はエラー
	if (keyFrames.size() == 1 || time <= keyFrames[0].time)
	{
		return keyFrames[0].value; // 最初のキーフレームの値を返す
	}

	for (uint32_t keyIndex = 0; keyIndex < keyFrames.size() - 1; ++keyIndex)
	{
		uint32_t nextKeyIndex = keyIndex + 1;
		if (keyFrames[keyIndex].time <= time && time <= keyFrames[nextKeyIndex].time)
		{
			float t = (time - keyFrames[keyIndex].time) / (keyFrames[nextKeyIndex].time - keyFrames[keyIndex].time); // 補間係数を計算
			return Quat::Slerp(keyFrames[keyIndex].value, keyFrames[nextKeyIndex].value, t); // 球面線形補間
		}
	}

	return (*keyFrames.rbegin()).value; // 最後のキーフレームの値を返す
}
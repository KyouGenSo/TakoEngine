#include "Model.h"
#include "ModelBasic.h"
#include "DX12Basic.h"
#include "TextureManager.h"
#include "SrvManager.h"
#include "Draw2D.h"
#include "Mat4x4Func.h"
#include "QuatFunc.h"
#include <cassert>
#include <fstream>
#include <sstream>

void Model::Initialize(ModelBasic* modelBasic, const std::string& fileName, bool hasAnimation, bool hasSkeleton)
{
	m_modelBasic_ = modelBasic;

	directoryFolderName_ = m_modelBasic_->GetDirectoryFolderName();

	ModelFolderName_ = m_modelBasic_->GetModelFolderName();

	hasAnimation_ = hasAnimation;

	hasSkeleton_ = hasSkeleton;

	// objファイルの読み込み
	LoadModelFile(directoryFolderName_ + "/" + ModelFolderName_, fileName);

	// アニメーションの読み込み
	if (hasAnimation_)
	{
		animationData_ = LoadAnimationFile(directoryFolderName_ + "/" + ModelFolderName_, fileName); 
	}

	// skeletonの生成
	if (hasSkeleton_)
	{
		skeleton_ = CreateSkeleton(modelData_.rootNode);
	}

	// 頂点データの生成
	CreateVertexData();

	// インデックスデータの生成
	CreateIndexData();

	// マテリアルデータの生成
	CreateMaterialData();

	// テクスチャの読み込み
	TextureManager::GetInstance()->LoadTexture(modelData_.material.texturePath);

	// テクスチャインデックスを保存
	modelData_.material.textureIndex = TextureManager::GetInstance()->GetSRVIndex(modelData_.material.texturePath);
}

void Model::Update()
{
	if (hasAnimation_ && !hasSkeleton_)
	{
		UpdateAnimation(1.0f / 60.0f);
	}

	if (hasSkeleton_ && hasAnimation_)
	{
		animationTime_ += 1.0f / 60.0f; // アニメーション時間を更新
		animationTime_ = std::fmod(animationTime_, animationData_.duration); // アニメーション時間がアニメーションの長さを超えたらループ
		ApplyAnimation(animationTime_);
		UpdateSkeleton();
	}
}

void Model::Draw(Matrix4x4 world, Matrix4x4 viewProjection)
{
	// 頂点バッファビューを設定
	m_modelBasic_->GetDX12Basic()->GetCommandList()->IASetVertexBuffers(0, 1, &vertexBufferView_);

	// インデックスバッファビューを設定
	m_modelBasic_->GetDX12Basic()->GetCommandList()->IASetIndexBuffer(&indexBufferView_);

	// マテリアルデータを設定
	m_modelBasic_->GetDX12Basic()->GetCommandList()->SetGraphicsRootConstantBufferView(0, materialResource_->GetGPUVirtualAddress());

	// SRVのDescriptorTableを設定,テクスチャを指定
	SrvManager::GetInstance()->SetRootDescriptorTable(2, modelData_.material.textureIndex);

	// 描画
	m_modelBasic_->GetDX12Basic()->GetCommandList()->DrawIndexedInstanced(UINT(modelData_.indices.size()), 1, 0, 0, 0);

	// skeletonの描画
	if (hasSkeleton_)
	{
		DrawSkeleton(world, viewProjection);
	}
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
		modelData_.vertices.resize(mesh->mNumVertices); // 頂点数だけリサイズ

		for (uint32_t vertexIndex = 0; vertexIndex < mesh->mNumVertices; ++vertexIndex)
		{
			aiVector3D position = mesh->mVertices[vertexIndex];
			aiVector3D texcoord = mesh->mTextureCoords[0][vertexIndex];
			aiVector3D normal = mesh->mNormals[vertexIndex];

			VertexData vertex;
			vertex.position = Vector4(position.x, position.y, position.z, 1.0f);
			vertex.texcoord = Vector2(texcoord.x, texcoord.y);
			vertex.normal = Vector3(normal.x, normal.y, normal.z);

			vertex.position.x *= -1.0f;
			vertex.normal.x *= -1.0f;

			modelData_.vertices[vertexIndex] = vertex;
		}

		for (uint32_t faceIndex = 0; faceIndex < mesh->mNumFaces; ++faceIndex) {
			aiFace& face = mesh->mFaces[faceIndex];
			assert(face.mNumIndices == 3); // 三角形以外はエラー

			for (uint32_t element = 0; element < face.mNumIndices; ++element) {
				uint32_t vertexIndex = face.mIndices[element];
				modelData_.indices.push_back(vertexIndex);
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

void Model::UpdateSkeleton()
{
	for (Joint& joint : skeleton_.joints)
	{
		joint.localMatrix = Mat4x4::MakeAffine(joint.transform.scale, joint.transform.rotate, joint.transform.translate); // ローカル変換行列を生成
		
		if (joint.parentIndex) {
			joint.skeletonSpaceMatrix = joint.localMatrix * skeleton_.joints[*joint.parentIndex].skeletonSpaceMatrix; // 親がいる場合は親のスケルトン空間行列を掛ける
		}
		else
		{
			joint.skeletonSpaceMatrix = joint.localMatrix; // 親がいない場合はローカル変換行列がスケルトン空間行列
		}
	}
}

void Model::ApplyAnimation(float time)
{
	for (Joint& joint : skeleton_.joints) {
		if (auto it = animationData_.nodeAnimations.find(joint.name); it != animationData_.nodeAnimations.end())
		{
			const NodeAnimetion& rootAnimetion = (*it).second; // ルートノードのアニメーションを取得

			// 位置アニメーションの計算
			joint.transform.translate = CalcKeyFrameValue(rootAnimetion.translate.keyFrames, time);
			// 回転アニメーションの計算
			joint.transform.rotate = CalcKeyFrameValue(rootAnimetion.rotate.keyFrames, time);
			// スケールアニメーションの計算
			joint.transform.scale = CalcKeyFrameValue(rootAnimetion.scale.keyFrames, time);
		}
	}
}

void Model::DrawSkeleton(Matrix4x4 world, Matrix4x4 viewProjection)
{
	// Draw each joint as a sphere and draw lines between joints to represent bones
	for (const Joint& joint : skeleton_.joints)
	{
		// Calculate the position of the joint in world space
		Matrix4x4 jointWorldMatrix = joint.skeletonSpaceMatrix * world;
		Vector3 jointPosition = Mat4x4::TransForm(jointWorldMatrix, Vector3(0.0f, 0.0f, 0.0f));

		// Draw the joint as a sphere
		float radius = 0.003f; // Sphere radius
		Draw2D::GetInstance()->DrawSphere(jointPosition, radius, Vector4(1.0f, 1.0f, 1.0f, 1.0f), viewProjection);

		// Draw line to parent joint if it exists
		if (joint.parentIndex)
		{
			const Joint& parentJoint = skeleton_.joints[*joint.parentIndex];

			// Calculate the position of the parent joint in world space
			Matrix4x4 parentWorldMatrix = parentJoint.skeletonSpaceMatrix * world;
			Vector3 parentPosition = Mat4x4::TransForm(parentWorldMatrix, Vector3(0.0f, 0.0f, 0.0f));

			// Draw a line between the current joint and its parent
			Draw2D::GetInstance()->DrawLine(
				jointPosition,
				parentPosition,
				Vector4(1.0f, 1.0f, 1.0f, 1.0f),
				viewProjection
			);
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

void Model::CreateIndexData()
{
	// インデックスリソースを生成
	indexResource_ = m_modelBasic_->GetDX12Basic()->MakeBufferResource(sizeof(uint32_t) * modelData_.indices.size());

	// インデックスバッファビューを作る
	indexBufferView_.BufferLocation = indexResource_->GetGPUVirtualAddress();
	indexBufferView_.SizeInBytes = UINT(sizeof(uint32_t) * modelData_.indices.size());
	indexBufferView_.Format = DXGI_FORMAT_R32_UINT;

	// インデックスリソースをマップ
	uint32_t* indexData = nullptr;
	indexResource_->Map(0, nullptr, reinterpret_cast<void**>(&indexData));
	memcpy(indexData, modelData_.indices.data(), sizeof(uint32_t) * modelData_.indices.size());

	indexResource_->Unmap(0, nullptr);
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

int32_t Model::CreateJoint(const Node& node, const std::optional<int32_t>& parentIndex, std::vector<Joint>& joints)
{
	Joint joint;
	joint.name = node.name;
	joint.localMatrix = node.localMatrix;
	joint.skeletonSpaceMatrix = Mat4x4::MakeIdentity();
	joint.transform = node.transform;
	joint.index = int32_t(joints.size());              // 現在登録されている数をindexとして設定
	joint.parentIndex = parentIndex;
	joints.push_back(joint);                           // skeletonのjoint列に追加

	for (const Node& child : node.children)
	{
		int32_t childIndex = CreateJoint(child, joint.index, joints); // 再帰的に子Jointを生成
		joints[joint.index].childrenIndex.push_back(childIndex);      // 子Jointのindexを追加
	}

	return joint.index;
}

Skeleton Model::CreateSkeleton(const Node& rootNode)
{
	Skeleton skeleton;
	skeleton.root = CreateJoint(rootNode, {}, skeleton.joints); // ルートノードからジョイントを生成

	// 名前とindexのマップを作る
	for (const Joint& joint : skeleton.joints)
	{
		skeleton.jointMap.emplace(joint.name, joint.index);
	}

	UpdateSkeleton(); // スケルトンの更新

	return skeleton;
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
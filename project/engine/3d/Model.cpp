#include "Model.h"
#include "ModelBasic.h"
#include "DX12Basic.h"
#include "TextureManager.h"
#include "SrvManager.h"
#include "Draw2D.h"
#include "Mat4x4Func.h"
#include "QuatFunc.h"
#include "Object3dBasic.h"

#include <cassert>
#include <fstream>
#include <sstream>

///------------------------------------------------///
///                 PUBLIC METHODS                ///
///-----------------------------------------------///

void Model::Initialize(ModelBasic* modelBasic, const std::string& fileName, bool hasAnimation, bool hasSkeleton)
{
  m_modelBasic_ = modelBasic;

  m_dx12_ = m_modelBasic_->GetDX12Basic();

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

  // skeletonの生成とメッシュのスキンニングデータの初期化
  if (hasSkeleton_)
  {
    skeleton_ = CreateSkeleton(rootNode_);
    InitializeMatrixPalette();

    for (auto& mesh : meshes_) {
      // 各メッシュにスキニングデータを設定
      mesh->InitializeSkinning(skinClusterData_, skeleton_.jointMap);
    }
  }
}

void Model::Finalize()
{
  // メッシュの解放
  for (auto& mesh : meshes_)
  {
    delete mesh;
  }
  meshes_.clear();
}

void Model::Update()
{
  if (hasAnimation_ && !hasSkeleton_)
  {
    UpdateAnimation(1.0f / 60.0f);
  }

  if (hasSkeleton_ && hasAnimation_) {
    // スケルトンアニメーションの場合

    // スキニング処理の準備（ボーン行列の更新など）
    PrepareSkinning();

  }
}

void Model::Draw(Matrix4x4 world, Matrix4x4 viewProjection)
{

  // スキニング処理の実行（ComputeShaderによる頂点変形）
  ExecuteSkinning();

  // 全メッシュの描画
  if (meshes_.size() <= 1)
  {
    for (auto& mesh : meshes_) {
      mesh->Draw();
    }
  }
  else
  {
    ProcessNodeHierarchy(rootNode_, Mat4x4::MakeIdentity(), world, viewProjection);
  }

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
  const aiScene* scene = importer.ReadFile(filePath.c_str(), aiProcess_FlipWindingOrder | aiProcess_FlipUVs);
  assert(scene->HasMeshes()); // メッシュがない場合はエラー

  // ルートノードの読み込み
  rootNode_ = ReadNode(scene->mRootNode);

  // メッシュの解析
  for (uint32_t meshIndex = 0; meshIndex < scene->mNumMeshes; meshIndex++)
  {
    aiMesh* mesh = scene->mMeshes[meshIndex];
    assert(mesh->HasTextureCoords(0) && mesh->HasNormals());

    // 頂点の解析
    std::vector<VertexData> vertices(mesh->mNumVertices);
    for (uint32_t vertexIndex = 0; vertexIndex < mesh->mNumVertices; ++vertexIndex) {
      aiVector3D& position = mesh->mVertices[vertexIndex];
      aiVector3D& texcoord = mesh->mTextureCoords[0][vertexIndex];
      aiVector3D& normal = mesh->mNormals[vertexIndex];

      vertices[vertexIndex].position = { -position.x, position.y, position.z, 1.0f };
      vertices[vertexIndex].texcoord = { texcoord.x, texcoord.y };
      vertices[vertexIndex].normal = { -normal.x, normal.y, normal.z };
    }

    // インデックスの解析
    std::vector<uint32_t> indices;
    for (uint32_t faceIndex = 0; faceIndex < mesh->mNumFaces; ++faceIndex) {
      aiFace& face = mesh->mFaces[faceIndex];
      assert(face.mNumIndices == 3);  // 三角形のみ対応

      for (uint32_t element = 0; element < face.mNumIndices; ++element) {
        indices.push_back(face.mIndices[element]);
      }
    }

    // スキンクラスターデータの解析
    for (uint32_t boneIndex = 0; boneIndex < mesh->mNumBones; ++boneIndex)
    {
      aiBone* bone = mesh->mBones[boneIndex];
      std::string jointName = bone->mName.C_Str();
      JointWeightData& jointWeightData = skinClusterData_[jointName];

      aiMatrix4x4 bindPoseMatrixAssimp = bone->mOffsetMatrix.Inverse();
      aiVector3D scale, tanslate;
      aiQuaternion rotate;
      bindPoseMatrixAssimp.Decompose(scale, rotate, tanslate);
      // 左手系のBindPoseMatrixを作る
      Matrix4x4 bindPoseMatrix = Mat4x4::MakeAffine(
        { scale.x, scale.y, scale.z },
        { rotate.x, -rotate.y, -rotate.z, rotate.w },
        { -tanslate.x, tanslate.y, tanslate.z });
      jointWeightData.inverseBindMatrix = Mat4x4::Inverse(bindPoseMatrix);

      for (uint32_t weightIndex = 0; weightIndex < bone->mNumWeights; ++weightIndex)
      {
        jointWeightData.vertexWeights.push_back({
          .weight= bone->mWeights[weightIndex].mWeight ,
          .vertexIndex= bone->mWeights[weightIndex].mVertexId });
      }
    }

    // マテリアルファイルの読み込み
    TextureData textureData;
    if (mesh->mMaterialIndex < scene->mNumMaterials) {
      aiMaterial* material = scene->mMaterials[mesh->mMaterialIndex];
      if (material->GetTextureCount(aiTextureType_DIFFUSE) > 0) {
        aiString texturePath;
        material->GetTexture(aiTextureType_DIFFUSE, 0, &texturePath);
        textureData.texturePath = texturePath.C_Str();

        // テクスチャ管理
        if (textureCache_.find(textureData.texturePath) == textureCache_.end()) {
          TextureManager::GetInstance()->LoadTexture(textureData.texturePath);
          textureData.textureIndex = TextureManager::GetInstance()->GetSRVIndex(textureData.texturePath);
          textureCache_[textureData.texturePath] = textureData;
        } else {
          textureData = textureCache_[textureData.texturePath];
        }
      }
    }

    // メッシュデータの保存
    Mesh* newMesh = new Mesh();
    newMesh->Initialize(m_modelBasic_, vertices, indices, textureData);
    meshes_.push_back(newMesh);
  }
}

void Model::SetShininess(float shininess)
{
  for (auto& mesh : meshes_)
  {
    mesh->SetShininess(shininess);
  }
}

void Model::SetEnableLighting(bool enableLighting)
{
  for (auto& mesh : meshes_)
  {
    mesh->SetEnableLighting(enableLighting);
  }
}

void Model::SetEnableHighlight(bool enableHighlight)
{
  for (auto& mesh : meshes_)
  {
    mesh->SetEnableHighlight(enableHighlight);
  }
}

void Model::SetMaterialColor(const Vector4& color)
{
  for (auto& mesh : meshes_)
  {
    mesh->SetMaterialColor(color);
  }
}

void Model::ProcessNodeHierarchy(const Node& node, const Matrix4x4& parentGlobalMatrix, Matrix4x4 world, Matrix4x4 viewProjection)
{
  // このノードのグローバル行列を計算（親の変換を適用）
  Matrix4x4 globalMatrix = Mat4x4::Multiply(node.localMatrix, parentGlobalMatrix);

  // このノードに関連付けられたメッシュを処理
  for (int meshIndex : node.meshIndices) {
    if (meshIndex < static_cast<int>(meshes_.size())) {
      // メッシュのワールド変換行列を計算
      Matrix4x4 meshWorldMatrix = Mat4x4::Multiply(globalMatrix, world);

      // メッシュのトランスフォーム情報を更新
      meshes_[meshIndex]->UpdateTransformation(meshWorldMatrix, viewProjection);

      // 更新されたトランスフォーム情報で描画
      meshes_[meshIndex]->DrawWithCurrentTransform();
    }
  }

  // 子ノードを再帰的に処理
  for (const Node& child : node.children) {
    ProcessNodeHierarchy(child, globalMatrix, world, viewProjection);
  }
}

///-----------------------------------------------///
///                PRIVATE METHODS                ///
///-----------------------------------------------///

void Model::DrawSkeleton(Matrix4x4 world, Matrix4x4 viewProjection)
{
  // Draw each joint as a sphere and draw lines between joints to represent bones
  for (const Joint& joint : skeleton_.joints)
  {
    // Calculate the position of the joint in world space
    Matrix4x4 jointWorldMatrix = joint.skeletonSpaceMatrix * world;
    Vector3 jointPosition = Mat4x4::TransForm(jointWorldMatrix, Vector3(0.0f, 0.0f, 0.0f));

    // Draw the joint as a sphere
    //float radius = 0.01f; // Sphere radius
    //Draw2D::GetInstance()->DrawSphere(jointPosition, radius, Vector4(1.0f, 1.0f, 1.0f, 1.0f));

    // Draw line to parent joint if it exists
    if (joint.parentIndex)
    {
      const Joint& parentJoint = skeleton_.joints[*joint.parentIndex];

      // Calculate the position of the parent joint in world space
      Matrix4x4 parentWorldMatrix = parentJoint.skeletonSpaceMatrix * world;
      Vector3 parentPosition = Mat4x4::TransForm(parentWorldMatrix, Vector3(0.0f, 0.0f, 0.0f));
      viewProjection;
      // Draw a line between the current joint and its parent
      Draw2D::GetInstance()->DrawLine(
        jointPosition,
        parentPosition,
        Vector4(1.0f, 1.0f, 1.0f, 1.0f)
      );
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
  animation.duration = static_cast<float>(aiAnimation->mDuration / aiAnimation->mTicksPerSecond); // アニメーションの長さを取得,秒に変換

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
      keyFrame.time = static_cast<float>(aiKey.mTime / aiAnimation->mTicksPerSecond); // 時間を秒に変換
      keyFrame.value = Vector3(-aiKey.mValue.x, aiKey.mValue.y, aiKey.mValue.z);
      nodeAnimetion.translate.keyFrames.push_back(keyFrame);
    }

    // 回転アニメーションの解析
    for (uint32_t keyIndex = 0; keyIndex < aiNodeAnim->mNumRotationKeys; ++keyIndex)
    {
      aiQuatKey& aiKey = aiNodeAnim->mRotationKeys[keyIndex];
      KeyFrameQuaternion keyFrame;
      keyFrame.time = static_cast<float>(aiKey.mTime / aiAnimation->mTicksPerSecond); // 時間を秒に変換
      keyFrame.value = Quaternion(aiKey.mValue.x, -aiKey.mValue.y, -aiKey.mValue.z, aiKey.mValue.w); // クォータニオンのy,z成分を反転,右手系から左手系に変換
      nodeAnimetion.rotate.keyFrames.push_back(keyFrame);
    }

    // スケールアニメーションの解析
    for (uint32_t keyIndex = 0; keyIndex < aiNodeAnim->mNumScalingKeys; ++keyIndex)
    {
      aiVectorKey& aiKey = aiNodeAnim->mScalingKeys[keyIndex];
      KeyFrameVector3 keyFrame;
      keyFrame.time = static_cast<float>(aiKey.mTime / aiAnimation->mTicksPerSecond); // 時間を秒に変換
      keyFrame.value = Vector3(aiKey.mValue.x, aiKey.mValue.y, aiKey.mValue.z);
      nodeAnimetion.scale.keyFrames.push_back(keyFrame);
    }
  }

  return animation;
}

void Model::UpdateSkinning()
{
  if (hasAnimation_ && hasSkeleton_) {
    // アニメーション時間の更新
    animationTime_ += 1.0f / 60.0f;
    animationTime_ = std::fmod(animationTime_, animationData_.duration);

    // アニメーションの適用
    UpdateSkeletonAnimation(animationTime_);

    // スケルトン行列の更新
    UpdateSkeleton();

    // パレットの更新（ボーン変換行列）
    for (size_t jointIndex = 0; jointIndex < skeleton_.joints.size(); ++jointIndex) {
      mappedPalette_[jointIndex].skeletonSpaceMat =
        inverseBindMatrices_[jointIndex] * skeleton_.joints[jointIndex].skeletonSpaceMatrix;

      mappedPalette_[jointIndex].skeletonSpaceMatrixInvTransposeMat =
        Mat4x4::Transpose(Mat4x4::Inverse(mappedPalette_[jointIndex].skeletonSpaceMat));
    }
  }
}

void Model::PrepareSkinning()
{
  // スケルトンやアニメーションがない場合は何もしない
  if (!hasSkeleton_ || !hasAnimation_) {
    return;
  }

  // アニメーション時間の更新
  animationTime_ += 1.0f / 60.0f; // 固定フレームレート（必要に応じてdeltaTimeに変更可能）
  animationTime_ = std::fmod(animationTime_, animationData_.duration); // ループ処理

  // アニメーションの適用（ポーズの更新）
  UpdateSkeletonAnimation(animationTime_);

  // スケルトン行列の更新
  UpdateSkeleton();

  // パレット（ボーン変換行列）の更新
  for (size_t jointIndex = 0; jointIndex < skeleton_.joints.size(); ++jointIndex) {
    // InverseBindMatrix（初期ポーズの逆行列）* 現在のスケルトン空間行列
    mappedPalette_[jointIndex].skeletonSpaceMat =
      inverseBindMatrices_[jointIndex] * skeleton_.joints[jointIndex].skeletonSpaceMatrix;

    // 法線変換用の逆転置行列も計算
    mappedPalette_[jointIndex].skeletonSpaceMatrixInvTransposeMat =
      Mat4x4::Transpose(Mat4x4::Inverse(mappedPalette_[jointIndex].skeletonSpaceMat));
  }

  // UAVバリアを設定
  for (auto& mesh : meshes_) {
    if (mesh->HasSkinning()) {
      m_dx12_->SetUAVBarrier(mesh->GetUAVVertexResource());
    }
  }
}

void Model::ExecuteSkinning()
{
  // スケルトンがない場合は何もしない
  if (!hasSkeleton_) {
    return;
  }

  // ComputeShaderの設定
  m_modelBasic_->SetSkinningCSSetting();

  // 共有パレット（ボーン行列）のSRVを設定
  SrvManager::GetInstance()->SetComputeRootDescriptorTable(0, paletteSrvIndex_);

  // 各メッシュごとにスキニング計算を実行
  for (auto& mesh : meshes_) {
    // メッシュがスキニングを使用しない場合はスキップ
    if (!mesh->HasSkinning()) {
      continue;
    }

    // スキニング計算を実行
    mesh->SkinningCompute();
  }

  // 共通レンダリング設定に戻す
  Object3dBasic::GetInstance()->SetCommonRenderSetting();
}

void Model::InitializeMatrixPalette() {
  // スケルトンがない場合は何もしない
  if (!hasSkeleton_) {
    return;
  }

  // 1. インバースバインドマトリクスの準備
  inverseBindMatrices_.resize(skeleton_.joints.size());

  // 各ジョイントに対応するインバースバインドマトリクスを設定
  for (const auto& [jointName, jointWeightData] : skinClusterData_) {
    auto it = skeleton_.jointMap.find(jointName);
    if (it != skeleton_.jointMap.end()) {
      int32_t jointIndex = it->second;
      if (jointIndex < static_cast<int32_t>(inverseBindMatrices_.size())) {
        inverseBindMatrices_[jointIndex] = jointWeightData.inverseBindMatrix;
      }
    }
  }

  // 2. パレットリソースの作成
  DX12Basic* dx12 = m_dx12_;
  UINT paletteSize = static_cast<UINT>(sizeof(WellForGPU) * skeleton_.joints.size());

  // パレットバッファの生成
  paletteResource_ = dx12->MakeBufferResource(paletteSize);

  // パレットバッファをマップ
  WellForGPU* mappedData = nullptr;
  paletteResource_->Map(0, nullptr, reinterpret_cast<void**>(&mappedData));
  mappedPalette_ = std::span<WellForGPU>(mappedData, skeleton_.joints.size());

  // 初期状態の設定（単位行列など）
  for (size_t i = 0; i < skeleton_.joints.size(); ++i) {
    mappedPalette_[i].skeletonSpaceMat = Mat4x4::MakeIdentity();
    mappedPalette_[i].skeletonSpaceMatrixInvTransposeMat = Mat4x4::MakeIdentity();
  }

  // 3. パレットのSRVを作成
  SrvManager* srvManager = SrvManager::GetInstance();
  paletteSrvIndex_ = srvManager->Allocate();
  srvManager->CreateSRVForStructuredBuffer(
    paletteSrvIndex_,
    paletteResource_.Get(),
    static_cast<UINT>(skeleton_.joints.size()),
    sizeof(WellForGPU)
  );
}

int32_t Model::CreateJoint(const Node& node, const std::optional<int32_t>& parentIndex, std::vector<Joint>& joints)
{
  Joint joint;
  joint.name = node.name;
  joint.localMatrix = node.localMatrix;
  joint.skeletonSpaceMatrix = Mat4x4::MakeIdentity();
  joint.transform = node.transform;
  joint.index = static_cast<int32_t>(joints.size());              // 現在登録されている数をindexとして設定
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

Node Model::ReadNode(aiNode* node)
{
  Node result;

  aiVector3D scale, position;
  aiQuaternion rotate;
  node->mTransformation.Decompose(scale, rotate, position);         // スケール,回転,平行移動を取得

  result.transform.scale = { scale.x, scale.y, scale.z };                         // スケールを取得
  result.transform.rotate = { rotate.x, -rotate.y, -rotate.z, rotate.w };         // 回転を取得,右手系から左手系に変換
  result.transform.translate = { -position.x, position.y, position.z };           // 平行移動を取得,x軸を反転

  result.localMatrix = Mat4x4::MakeAffine(
    result.transform.scale, result.transform.rotate, result.transform.translate);   // ローカル変換行列を生成

  result.name = node->mName.C_Str();                                                // ノードの名前を取得

  // メッシュインデックスの読み込み
  result.meshIndices.resize(node->mNumMeshes);
  for (uint32_t i = 0; i < node->mNumMeshes; i++) {
    result.meshIndices[i] = node->mMeshes[i];
  }

  result.children.resize(node->mNumChildren); // 子ノードの数だけリサイズ

  for (uint32_t childIndex = 0; childIndex < node->mNumChildren; ++childIndex)
  {
    result.children[childIndex] = ReadNode(node->mChildren[childIndex]); // 再帰的に子ノードを読み込む
  }

  return result;
}

void Model::UpdateAnimation(float deltaTime)
{
  animationTime_ += deltaTime; // アニメーション時間を更新
  animationTime_ = std::fmod(animationTime_, animationData_.duration); // アニメーション時間がアニメーションの長さを超えたらループ
  NodeAnimetion& nodeAnimetion = animationData_.nodeAnimations[rootNode_.name]; // ルートノードのアニメーションを取得

  // 位置アニメーションの計算
  Vector3 translate = CalcKeyFrameValue(nodeAnimetion.translate.keyFrames, animationTime_);
  // 回転アニメーションの計算
  Quaternion rotate = CalcKeyFrameValue(nodeAnimetion.rotate.keyFrames, animationTime_);
  // スケールアニメーションの計算
  Vector3 scale = CalcKeyFrameValue(nodeAnimetion.scale.keyFrames, animationTime_);

  // ローカル変換行列を生成
  Matrix4x4 localMatrix = Mat4x4::MakeAffine(scale, rotate, translate);
  rootNode_.localMatrix = localMatrix; // ルートノードのローカル変換行列を更新
}

void Model::UpdateSkeleton()
{
  for (Joint& joint : skeleton_.joints)
  {
    joint.localMatrix = Mat4x4::MakeAffine(joint.transform.scale, joint.transform.rotate, joint.transform.translate); // ローカル変換行列を生成

    if (joint.parentIndex) {
      joint.skeletonSpaceMatrix = joint.localMatrix * skeleton_.joints[*joint.parentIndex].skeletonSpaceMatrix; // 親がいる場合は親のスケルトン空間行列を掛ける
    } else
    {
      joint.skeletonSpaceMatrix = joint.localMatrix; // 親がいない場合はローカル変換行列がスケルトン空間行列
    }
  }
}

void Model::UpdateSkinCluster()
{
  for (size_t jointIndex = 0; jointIndex < skeleton_.joints.size(); ++jointIndex)
  {
    assert(jointIndex < inverseBindMatrices_.size());
    mappedPalette_[jointIndex].skeletonSpaceMat = inverseBindMatrices_[jointIndex] * skeleton_.joints[jointIndex].skeletonSpaceMatrix;
    mappedPalette_[jointIndex].skeletonSpaceMatrixInvTransposeMat =
      Mat4x4::Transpose(Mat4x4::Inverse(mappedPalette_[jointIndex].skeletonSpaceMat));
  }
}

void Model::UpdateSkeletonAnimation(float time)
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
    if (keyFrames[keyIndex].time <= time && time <= keyFrames[nextKeyIndex].time)
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
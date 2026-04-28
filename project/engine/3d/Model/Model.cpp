#include "Model.h"
#include "ModelBasic.h"
#include "DX12Basic.h"
#include "TextureManager.h"
#include "SrvManager.h"
#include "Draw2D.h"
#include "Mat4x4Func.h"
#include "QuatFunc.h"
#include "Object3dBasic.h"
#include "ShadowRenderer.h"

#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>

#include <cassert>
#include <fstream>
#include <set>
#include <sstream>

#ifdef _DEBUG
#include "DebugUIManager.h"
#include <imgui.h>
#endif

namespace Tako {

  // 静的メンバー変数の定義
  bool Model::s_showSkeletonDebug = false;

  ///------------------------------------------------///
  ///                 PUBLIC METHODS                ///
  ///-----------------------------------------------///

  void Model::Initialize(ModelBasic* modelBasic, const std::string& fileName)
  {
    m_modelBasic_ = modelBasic;
    m_dx12_ = m_modelBasic_->GetDX12Basic();
    directoryFolderName_ = m_modelBasic_->GetDirectoryFolderName();
    ModelFolderName_ = m_modelBasic_->GetModelFolderName();
    modelFileName_ = fileName;  // ファイル名を保存
    hasAnimation_ = false;  // LoadModelFile で自動設定される
    hasSkeleton_ = false;   // LoadModelFile で自動設定される
    paletteSrvIndex_ = 0;
    expandState_ = 0;
    hoveredJointIndex_ = -1;
    animationSpeed_ = 1.0f;
    isPaused_ = false;

    // obj ファイルの読み込み（この中で hasAnimation_と hasSkeleton_が自動設定される）
    LoadModelFile(directoryFolderName_ + "/" + ModelFolderName_, fileName);

    // アニメーションの読み込み
    if (hasAnimation_) {
      LoadAnimationFile(directoryFolderName_ + "/" + ModelFolderName_, fileName);
    }

    // skeleton の生成とメッシュのスキンニングデータの初期化
    if (hasSkeleton_) {
      skeleton_ = CreateSkeleton(rootNode_);
      InitializeMatrixPalette();

      for (size_t i = 0; i < meshes_.size(); ++i) {
        if (i < meshSkinClusterData_.size() && !meshSkinClusterData_[i].skinClusterData.empty()) {
          // 各メッシュに専用のスキンクラスターデータを設定
          meshes_[i]->InitializeSkinning(meshSkinClusterData_[i].skinClusterData, skeleton_.jointMap);
        }
      }
    }
  }

  void Model::Finalize()
  {
    // スキニング関連リソースの解放
    ReleaseSkinningSRVIndex();

    // メッシュの解放（unique_ptr が自動で delete する）
    meshes_.clear();
  }

  void Model::Update()
  {
    // アニメーションがある場合
    if (hasAnimation_) {
      // 一時停止中でない場合のみアニメーション時間を更新
      if (!isPaused_) {
        // アニメーション時間の更新（再生速度を適用）
        UpdateAnimation(animationSpeed_ / 60.0f);
      }

      // ノード階層のアニメーション更新（スキニングの有無に関わらず実行）
      // 一時停止中でも現在のポーズは表示する必要があるため、この処理は実行する
      if (!currentAnimationName_.empty() && animationTimes_.find(currentAnimationName_) != animationTimes_.end()) {
        UpdateNodeHierarchyAnimation(rootNode_, animationTimes_[currentAnimationName_]);
      }
    }

    // スケルトンアニメーションの場合
    if (hasSkeleton_ && hasAnimation_) {
      // すべてのメッシュのスキニング状態をリセット
      for (auto& mesh : meshes_) {
        mesh->ResetSkinningState();
      }
      // スキニング処理の準備（ボーン行列の更新など）
      PrepareSkinning();
    }
  }

  void Model::Draw(Matrix4x4 world, Matrix4x4 viewProjection)
  {
    // スキニング処理の実行（ComputeShader による頂点変形）
    ExecuteSkinning();

    // スキニングモデルの場合
    if (hasSkeleton_) {
      // スキニングモデルは各メッシュを直接描画（スキニング済みの頂点を使用）
      for (auto& mesh : meshes_) {
        mesh->Draw();
      }
    }
    else if (meshes_.size() <= 1) {
      // 単一メッシュモデルの場合
      for (auto& mesh : meshes_) {
        mesh->Draw();
      }
    }
    else {
      // マルチメッシュモデル（スキニングなし）の場合はノード階層で描画
      // アニメーションがある場合は、更新された rootNode の localMatrix を使用
      Matrix4x4 rootTransform = hasAnimation_ ? rootNode_.localMatrix : Mat4x4::MakeIdentity();
      ProcessNodeHierarchy(rootNode_, rootTransform, world, viewProjection);
    }

    // skeleton の描画
#ifdef _DEBUG
    if (hasSkeleton_ && s_showSkeletonDebug) {
      DrawSkeleton(world);
    }
#endif
  }

  void Model::DrawInstanced(uint32_t instanceCount)
  {
    // インスタンシング描画ではスキニングは未対応
    if (hasSkeleton_) {
#ifdef _DEBUG
      DebugUIManager::GetInstance()->AddLog(
        "Warning: Instanced drawing is not supported for skinned models",
        DebugUIManager::LogType::Warning);
#endif

      return;
    }

#ifdef _DEBUG
    // シャドウパス中の場合はデバッグログ出力
    if (ShadowRenderer::GetInstance()->IsRenderingShadow()) {
      DebugUIManager::GetInstance()->AddLog("Drawing instanced model in shadow pass: " + std::to_string(instanceCount) + " instances", DebugUIManager::LogType::Info);
    }
#endif

    // 各メッシュをインスタンシング描画
    for (auto& mesh : meshes_) {
      mesh->DrawInstanced(instanceCount);
    }
  }

  void Model::LoadModelFile(const std::string& directoryPath, const std::string& fileName)
  {
    Assimp::Importer importer;
    std::string filePath = directoryPath + "/" + fileName;

    const aiScene* scene = importer.ReadFile(filePath.c_str(),
      aiProcess_FlipWindingOrder |
      aiProcess_FlipUVs |
      aiProcess_CalcTangentSpace |
      aiProcess_GenSmoothNormals |
      aiProcess_GenUVCoords |
      aiProcess_JoinIdenticalVertices |
      aiProcess_Triangulate
    );
    assert(scene->HasMeshes());

    // アニメーションの有無を自動判定
    hasAnimation_ = scene->HasAnimations();

    // スケルトン（ボーン）の有無を自動判定
    hasSkeleton_ = false;
    for (uint32_t i = 0; i < scene->mNumMeshes; ++i) {
      if (scene->mMeshes[i]->mNumBones > 0) {
        hasSkeleton_ = true;
        break;
      }
    }

    // ルートノードの読み込み
    rootNode_ = ReadNode(scene->mRootNode);

    // メッシュごとのスキンクラスターデータを準備
    meshSkinClusterData_.resize(scene->mNumMeshes);

    // メッシュの解析
    for (uint32_t meshIndex = 0; meshIndex < scene->mNumMeshes; meshIndex++) {
      aiMesh* mesh = scene->mMeshes[meshIndex];

      bool hasTexCoords = mesh->HasTextureCoords(0);
      bool hasNormal = mesh->HasNormals();

      // 頂点の解析
      std::vector<VertexData> vertices(mesh->mNumVertices);
      for (uint32_t vertexIndex = 0; vertexIndex < mesh->mNumVertices; ++vertexIndex) {
        aiVector3D& position = mesh->mVertices[vertexIndex];
        vertices[vertexIndex].position = { -position.x, position.y, position.z, 1.0f };

        if (hasTexCoords) {
          aiVector3D& texcoord = mesh->mTextureCoords[0][vertexIndex];
          vertices[vertexIndex].texcoord = { texcoord.x, texcoord.y };
        }
        else {
          vertices[vertexIndex].texcoord = { 0.0f, 0.0f };
        }

        if (hasNormal) {
          aiVector3D& normal = mesh->mNormals[vertexIndex];
          vertices[vertexIndex].normal = { -normal.x, normal.y, normal.z };
        }
        else {
          vertices[vertexIndex].normal = { 0.0f, 1.0f, 0.0f };
        }
      }

      // インデックスの解析
      std::vector<uint32_t> indices;
      for (uint32_t faceIndex = 0; faceIndex < mesh->mNumFaces; ++faceIndex) {
        aiFace& face = mesh->mFaces[faceIndex];
        assert(face.mNumIndices == 3);

        for (uint32_t element = 0; element < face.mNumIndices; ++element) {
          indices.push_back(face.mIndices[element]);
        }
      }

      // このメッシュのボーン情報を収集
      for (uint32_t boneIndex = 0; boneIndex < mesh->mNumBones; ++boneIndex) {
        aiBone* bone = mesh->mBones[boneIndex];
        std::string jointName = bone->mName.C_Str();

        // このメッシュ専用のスキンクラスターデータをチェック
        auto& meshSkinData = meshSkinClusterData_[meshIndex].skinClusterData;

        // まだこのジョイントが存在しない場合は逆バインド行列を設定
        if (meshSkinData.find(jointName) == meshSkinData.end()) {
          JointWeightData& jointWeightData = meshSkinData[jointName];

          aiMatrix4x4 bindPoseMatrixAssimp = bone->mOffsetMatrix.Inverse();
          aiVector3D scale, translate;
          aiQuaternion rotate;
          bindPoseMatrixAssimp.Decompose(scale, rotate, translate);

          Matrix4x4 bindPoseMatrix = Mat4x4::MakeAffine(
            { scale.x, scale.y, scale.z },
            { rotate.x, -rotate.y, -rotate.z, rotate.w },
            { -translate.x, translate.y, translate.z });
          jointWeightData.inverseBindMatrix = Mat4x4::Inverse(bindPoseMatrix);
        }

        // 頂点ウェイトデータを追加
        JointWeightData& jointWeightData = meshSkinData[jointName];
        for (uint32_t weightIndex = 0; weightIndex < bone->mNumWeights; ++weightIndex) {
          jointWeightData.vertexWeights.push_back({
            .weight = bone->mWeights[weightIndex].mWeight,
            .vertexIndex = bone->mWeights[weightIndex].mVertexId
            });
        }
      }

      // グローバルなスキンクラスターデータも更新（スケルトン生成用）
      for (const auto& [jointName, jointData] : meshSkinClusterData_[meshIndex].skinClusterData) {
        if (skinClusterData_.find(jointName) == skinClusterData_.end()) {
          skinClusterData_[jointName] = jointData;
        }
      }

      // マテリアルの読み込み
      TextureData textureData;
      // デフォルトのベースカラーを白に設定
      textureData.baseColor = Vector4(1.0f, 1.0f, 1.0f, 1.0f);

      if (mesh->mMaterialIndex < scene->mNumMaterials) {
        aiMaterial* material = scene->mMaterials[mesh->mMaterialIndex];

        // ベースカラーの読み込み
        aiColor4D baseColor;
        if (material->Get(AI_MATKEY_COLOR_DIFFUSE, baseColor) == AI_SUCCESS) {
          textureData.baseColor = Vector4(baseColor.r, baseColor.g, baseColor.b, baseColor.a);
        }
        // PBR マテリアルの場合は BASE_COLOR も試す
        else if (material->Get(AI_MATKEY_BASE_COLOR, baseColor) == AI_SUCCESS) {
          textureData.baseColor = Vector4(baseColor.r, baseColor.g, baseColor.b, baseColor.a);
        }

        // テクスチャの読み込み
        if (material->GetTextureCount(aiTextureType_DIFFUSE) > 0) {
          aiString texturePath;
          material->GetTexture(aiTextureType_DIFFUSE, 0, &texturePath);
          textureData.texturePath = texturePath.C_Str();

          if (textureCache_.find(textureData.texturePath) == textureCache_.end()) {
            TextureManager::GetInstance()->LoadTexture(textureData.texturePath);
            textureData.textureIndex = TextureManager::GetInstance()->GetSRVIndex(textureData.texturePath);
            textureCache_[textureData.texturePath] = textureData;
          }
          else {
            textureData = textureCache_[textureData.texturePath];
          }
        }
        else {
          textureData.texturePath = "";
          textureData.textureIndex = TextureManager::GetInstance()->GetEngineDefaultSRVIndex("white.dds");
        }
      }

      // メッシュデータの保存
      auto newMesh = std::make_unique<Mesh>();
      newMesh->Initialize(m_modelBasic_, vertices, indices, textureData);
      meshes_.push_back(std::move(newMesh));
    }
  }

  std::unique_ptr<Model> Model::Clone() const
  {
    auto newModel = std::make_unique<Model>();
    newModel->m_modelBasic_ = this->m_modelBasic_;
    newModel->m_dx12_ = this->m_dx12_;
    newModel->directoryFolderName_ = this->directoryFolderName_;
    newModel->ModelFolderName_ = this->ModelFolderName_;
    newModel->modelFileName_ = this->modelFileName_;
    newModel->hasAnimation_ = this->hasAnimation_;
    newModel->hasSkeleton_ = this->hasSkeleton_;
    newModel->rootNode_ = this->rootNode_;
    newModel->textureCache_ = this->textureCache_;
    newModel->meshSkinClusterData_ = this->meshSkinClusterData_;
    newModel->expandState_ = this->expandState_;
    newModel->hoveredJointIndex_ = this->hoveredJointIndex_;
    newModel->animationSpeed_ = this->animationSpeed_;
    newModel->isPaused_ = this->isPaused_;

    if (this->hasSkeleton_) {
      newModel->skeleton_ = this->skeleton_;
      newModel->skinClusterData_ = this->skinClusterData_;
      newModel->inverseBindMatrices_ = this->inverseBindMatrices_;
      newModel->paletteSrvIndex_ = this->paletteSrvIndex_;
      newModel->InitializeMatrixPalette();
    }

    if (this->hasAnimation_) {
      newModel->animations_ = this->animations_;
      newModel->currentAnimationName_ = this->currentAnimationName_;
      // 各アニメーションの時間をリセット
      for (const auto& [name, animation] : this->animations_) {
        newModel->animationTimes_[name] = 0.0f;
      }
    }

    // Mesh のクローンを作成
    for (size_t i = 0; i < meshes_.size(); ++i) {
      auto newMesh = meshes_[i]->Clone();
      if (this->hasSkeleton_ && i < this->meshSkinClusterData_.size() && !this->meshSkinClusterData_[i].skinClusterData.empty()) {
        // 各メッシュに専用のスキンクラスターデータを設定
        newMesh->InitializeSkinning(this->meshSkinClusterData_[i].skinClusterData, this->skeleton_.jointMap);
      }
      newModel->meshes_.push_back(std::move(newMesh));
    }

    return newModel;
  }

  void Model::SetShininess(float shininess)
  {
    for (auto& mesh : meshes_) {
      mesh->SetShininess(shininess);
    }
  }

  void Model::SetEnableLighting(bool enableLighting)
  {
    for (auto& mesh : meshes_) {
      mesh->SetEnableLighting(enableLighting);
    }
  }

  void Model::SetEnableHighlight(bool enableHighlight)
  {
    for (auto& mesh : meshes_) {
      mesh->SetEnableHighlight(enableHighlight);
    }
  }

  void Model::SetMaterialColor(const Vector4& color)
  {
    for (auto& mesh : meshes_) {
      mesh->SetMaterialColor(color);
    }
  }

  Vector4 Model::GetMaterialColor() const
  {
    if (!meshes_.empty()) {
      return meshes_[0]->GetMaterialColor();
    }
    return Vector4(1.0f, 1.0f, 1.0f, 1.0f);
  }

  void Model::SetUvTransform(const Transform& uvTransform)
  {
    for (auto& mesh : meshes_) {
      mesh->SetUvTransform(uvTransform);
    }
  }

  void Model::SetEnvironmentTexture(uint32_t textureIndex)
  {
    for (auto& mesh : meshes_) {
      mesh->SetEnvironmentTexture(textureIndex);
    }
  }

  void Model::SetEnableEnvMap(bool enableEnvMap)
  {
    for (auto& mesh : meshes_) {
      mesh->SetEnableEnvMap(enableEnvMap);
    }
  }

  void Model::SetEnvMapCoefficient(float coefficient)
  {
    for (auto& mesh : meshes_) {
      mesh->SetEnvMapCoefficient(coefficient);
    }
  }

  Matrix4x4 Model::GetJointWorldMatrix(const std::string& jointName, const Matrix4x4& worldMatrix) const
  {
    // スケルトンがない場合は単位行列を返す
    if (!hasSkeleton_) {
      return Mat4x4::MakeIdentity();
    }

    // Joint 名からインデックスを検索
    auto it = skeleton_.jointMap.find(jointName);
    if (it == skeleton_.jointMap.end()) {
      // Joint が見つからない場合は単位行列を返す
#ifdef _DEBUG
      DebugUIManager::GetInstance()->AddLog(
        "Warning: Joint '" + jointName + "' not found in skeleton",
        DebugUIManager::LogType::Warning);
#endif

      return Mat4x4::MakeIdentity();
    }

    int32_t jointIndex = it->second;
    if (jointIndex < 0 || jointIndex >= static_cast<int32_t>(skeleton_.joints.size())) {
      return Mat4x4::MakeIdentity();
    }

    // Joint の skeletonSpaceMatrix とワールド行列を掛け合わせて返す
    const Joint& joint = skeleton_.joints[jointIndex];
    return joint.skeletonSpaceMatrix * worldMatrix;
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

  void Model::DrawSkeleton(Matrix4x4 world)
  {
    // Draw each joint as a sphere and draw lines between joints to represent bones
    for (const Joint& joint : skeleton_.joints) {
      // Calculate the position of the joint in world space
      Matrix4x4 jointWorldMatrix = joint.skeletonSpaceMatrix * world;
      Vector3 jointPosition = Mat4x4::Transform(jointWorldMatrix, Vector3(0.0f, 0.0f, 0.0f));

      // Draw line to parent joint if it exists
      if (joint.parentIndex) {
        const Joint& parentJoint = skeleton_.joints[*joint.parentIndex];

        // Calculate the position of the parent joint in world space
        Matrix4x4 parentWorldMatrix = parentJoint.skeletonSpaceMatrix * world;
        Vector3 parentPosition = Mat4x4::Transform(parentWorldMatrix, Vector3(0.0f, 0.0f, 0.0f));

        // ホバー中のジョイントに関連する線は赤色で表示
        Vector4 lineColor = Vector4(1.0f, 1.0f, 1.0f, 1.0f);  // デフォルト: 白
        if (hoveredJointIndex_ == joint.index || hoveredJointIndex_ == *joint.parentIndex) {
          lineColor = Vector4(1.0f, 0.0f, 0.0f, 1.0f);  // 赤色
        }

        // Draw the joint as a AABB
        AABB aabb{
          jointPosition - Vector3(0.01f, 0.01f, 0.01f), // Min corner
          jointPosition + Vector3(0.01f, 0.01f, 0.01f)  // Max corner
        };
        Draw2D::GetInstance()->DrawAABB(aabb, lineColor);

        // Draw a line between the current joint and its parent
        Draw2D::GetInstance()->DrawLine(
          jointPosition,
          parentPosition,
          lineColor
        );
      }
    }
  }

  void Model::DrawImGui()
  {
#ifdef _DEBUG
    // ホバー中のジョイントインデックスをリセット
    hoveredJointIndex_ = -1;

    // ウィンドウタイトルにモデル名を含める
    std::string windowTitle = "[" + modelFileName_ + "] Info";
    ImGui::Begin(windowTitle.c_str());

    // アニメーション制御
    if (hasAnimation_) {
      ImGui::Text("Animation Control");
      ImGui::Separator();

      // 再生/一時停止ボタン
      if (isPaused_) {
        if (ImGui::Button("Resume")) {
          isPaused_ = false;
        }
      }
      else {
        if (ImGui::Button("Pause")) {
          isPaused_ = true;
        }
      }

      // 再生速度スライダー
      ImGui::SliderFloat("Speed", &animationSpeed_, -10.0f, 100.0f, "%.2f");
      ImGui::SameLine();
      if (ImGui::Button("Reset Speed")) {
        animationSpeed_ = 1.0f;
      }

      // アニメーション時間の表示
      if (!currentAnimationName_.empty() && animations_.find(currentAnimationName_) != animations_.end()) {
        float currentTime = animationTimes_[currentAnimationName_];
        float duration = animations_.at(currentAnimationName_).duration;
        ImGui::Text("Time: %.2f / %.2f", currentTime, duration);

        // プログレスバー
        float progress = currentTime / duration;
        ImGui::ProgressBar(progress, ImVec2(-1, 0));
      }

      // 全アニメーション一覧表示
      ImGui::Separator();
      ImGui::Text("Available Animations (%zu):", animations_.size());

      // スクロール可能な子ウィンドウでアニメーションリストを表示
      ImGui::BeginChild("AnimationList", ImVec2(0, 100), true);
      for (const auto& [name, animation] : animations_) {
        bool isCurrentAnimation = (name == currentAnimationName_);

        // 現在のアニメーションは異なる色で表示
        if (isCurrentAnimation) {
          ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.2f, 1.0f, 0.2f, 1.0f)); // 緑色
        }

        // アニメーション名と長さを表示
        if (ImGui::Selectable(name.c_str(), isCurrentAnimation)) {
          SetAnimation(name, 0.3f); // クリックで切り替え
        }
        ImGui::SameLine();
        ImGui::TextDisabled("(%.2fs)", animation.duration);

        if (isCurrentAnimation) {
          ImGui::PopStyleColor();
        }
      }
      ImGui::EndChild();

      ImGui::Separator();
    }

    if (hasSkeleton_) {
      // 3D ビジュアライゼーションのトグル
      ImGui::Checkbox("Show 3D Skeleton", &s_showSkeletonDebug);
      ImGui::Separator();

      ImGui::Text("Total Joints: %zu", skeleton_.joints.size());
      ImGui::Text("Root Joint Index: %d", skeleton_.root);

      // 全展開/全折りたたみボタン
      if (ImGui::Button("Expand All")) {
        expandState_ = 1;  // expand all
      }
      ImGui::SameLine();
      if (ImGui::Button("Collapse All")) {
        expandState_ = 2;  // collapse all
      }
      ImGui::SameLine();
      if (ImGui::Button("Reset")) {
        expandState_ = 0;  // normal
      }

      ImGui::Separator();

      // スクロール可能な子ウィンドウを作成（横スクロールバー付き）
      ImGui::BeginChild("JointHierarchy", ImVec2(0, 0), true, ImGuiWindowFlags_HorizontalScrollbar);

      // ルートジョイントから再帰的に表示
      if (skeleton_.root >= 0 && skeleton_.root < skeleton_.joints.size()) {
        DrawJointHierarchy(skeleton_.root);
      }

      ImGui::EndChild();
    }

    // ノード階層の表示（スケルトンがない場合やデバッグ用）
    if (!hasSkeleton_ || ImGui::CollapsingHeader("Node Hierarchy")) {
      ImGui::Text("Total Meshes: %zu", meshes_.size());
      ImGui::Separator();

      // スクロール可能な子ウィンドウを作成
      ImGui::BeginChild("NodeHierarchy", ImVec2(0, 300), true, ImGuiWindowFlags_HorizontalScrollbar);

      // ルートノードから再帰的に表示
      DrawNodeHierarchyImGui(rootNode_);

      ImGui::EndChild();
    }

    ImGui::End();
#endif
  }

  void Model::DrawJointHierarchy(int32_t jointIndex, int depth)
  {
#ifdef _DEBUG
    if (jointIndex < 0 || jointIndex >= skeleton_.joints.size()) return;

    const Joint& joint = skeleton_.joints[jointIndex];

    // ジョイントタイプを判定
    bool isRoot = (jointIndex == skeleton_.root);
    bool isLeaf = joint.childrenIndex.empty();

    // アイコンと色の設定
    const char* icon = isRoot ? "[ROOT]" : (isLeaf ? "[LEAF]" : "");
    ImVec4 textColor = isRoot ? ImVec4(0.2f, 1.0f, 0.2f, 1.0f) :  // 緑
      isLeaf ? ImVec4(0.7f, 0.7f, 0.7f, 1.0f) :  // 灰色
      ImVec4(1.0f, 1.0f, 1.0f, 1.0f);   // 白

    // 階層に応じたインデントを適用（固定の小さめの値を使用）
    const float indentAmount = 12.0f;
    ImGui::Indent(indentAmount * depth);

    // 展開状態を設定
    if (expandState_ == 1)  // expand all
    {
      ImGui::SetNextItemOpen(true, ImGuiCond_Always);
    }
    else if (expandState_ == 2)  // collapse all
    {
      ImGui::SetNextItemOpen(false, ImGuiCond_Always);
    }

    // ツリーノードの表示
    ImGui::PushStyleColor(ImGuiCol_Text, textColor);
    bool nodeOpen = ImGui::TreeNode((void*)(intptr_t)jointIndex, "%s %s [%d]",
      icon, joint.name.c_str(), joint.index);
    ImGui::PopStyleColor();

    // ツリーノードがホバーされているかチェック
    if (ImGui::IsItemHovered()) {
      hoveredJointIndex_ = jointIndex;
    }

    // ノードが開いている場合、詳細情報を表示
    if (nodeOpen) {
      ImGui::Indent();

      // 親子関係の情報
      ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.8f, 0.8f, 0.8f, 1.0f));
      if (joint.parentIndex.has_value()) {
        const Joint& parentJoint = skeleton_.joints[*joint.parentIndex];
        ImGui::Text("Parent: %s [%d]", parentJoint.name.c_str(), *joint.parentIndex);
      }
      else {
        ImGui::Text("Parent: None (Root)");
      }
      ImGui::Text("Children: %zu", joint.childrenIndex.size());

      // 位置情報をコンパクトに表示
      Vector3 position = {
        joint.skeletonSpaceMatrix.m[3][0],
        joint.skeletonSpaceMatrix.m[3][1],
        joint.skeletonSpaceMatrix.m[3][2]
      };
      ImGui::Text("Pos: (%.2f, %.2f, %.2f)", position.x, position.y, position.z);
      ImGui::PopStyleColor();

      // ホバー時に詳細情報をツールチップで表示
      if (ImGui::IsItemHovered()) {
        ImGui::BeginTooltip();
        ImGui::Text("Joint Details:");
        ImGui::Separator();
        ImGui::Text("Index: %d", joint.index);
        ImGui::Text("Name: %s", joint.name.c_str());
        ImGui::Text("Position: (%.4f, %.4f, %.4f)", position.x, position.y, position.z);
        if (!joint.childrenIndex.empty()) {
          ImGui::Text("Child Indices:");
          for (int32_t childIdx : joint.childrenIndex) {
            ImGui::Text("  - [%d] %s", childIdx, skeleton_.joints[childIdx].name.c_str());
          }
        }
        ImGui::EndTooltip();
      }

      ImGui::Unindent();

      // 子ジョイントを再帰的に表示
      for (int32_t childIndex : joint.childrenIndex) {
        DrawJointHierarchy(childIndex, depth + 1);
      }

      ImGui::TreePop();
    }

    // インデントを元に戻す
    ImGui::Unindent(indentAmount * depth);
#endif
  }

  void Model::DrawNodeHierarchyImGui(const Node& node, int depth)
  {
#ifdef _DEBUG
    // インデント量の設定
    const float indentAmount = 12.0f;
    ImGui::Indent(indentAmount * depth);

    // 現在のアニメーション情報を取得
    bool hasNodeAnimation = false;
    if (hasAnimation_ && !currentAnimationName_.empty() && animations_.find(currentAnimationName_) != animations_.end()) {
      const Animation& currentAnimation = animations_.at(currentAnimationName_);
      hasNodeAnimation = currentAnimation.nodeAnimations.find(node.name) != currentAnimation.nodeAnimations.end();
    }

    // アニメーションがあるノードは緑色、ないノードは白色
    ImVec4 textColor = hasNodeAnimation ? ImVec4(0.2f, 1.0f, 0.2f, 1.0f) : ImVec4(1.0f, 1.0f, 1.0f, 1.0f);

    // メッシュ情報の表示
    std::string meshInfo = "";
    if (!node.meshIndices.empty()) {
      meshInfo = " [Meshes:";
      for (size_t i = 0; i < node.meshIndices.size(); ++i) {
        if (i > 0) meshInfo += ",";
        meshInfo += std::to_string(node.meshIndices[i]);
      }
      meshInfo += "]";
    }

    // ツリーノードの表示
    ImGui::PushStyleColor(ImGuiCol_Text, textColor);
    bool nodeOpen = ImGui::TreeNode(node.name.c_str(), "%s%s%s",
      node.name.c_str(),
      meshInfo.c_str(),
      hasNodeAnimation ? " [ANIM]" : "");
    ImGui::PopStyleColor();

    // ノードが開いている場合、詳細情報を表示
    if (nodeOpen) {
      ImGui::Indent();

      // Transform 情報を表示
      ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.8f, 0.8f, 0.8f, 1.0f));
      ImGui::Text("Transform:");
      ImGui::Text("  Translate: (%.3f, %.3f, %.3f)",
        node.transform.translate.x, node.transform.translate.y, node.transform.translate.z);
      ImGui::Text("  Rotate: (%.3f, %.3f, %.3f, %.3f)",
        node.transform.rotate.x, node.transform.rotate.y, node.transform.rotate.z, node.transform.rotate.w);
      ImGui::Text("  Scale: (%.3f, %.3f, %.3f)",
        node.transform.scale.x, node.transform.scale.y, node.transform.scale.z);

      // LocalMatrix の位置部分を表示（アニメーション適用後の値）
      Vector3 matrixPos = {
        node.localMatrix.m[3][0],
        node.localMatrix.m[3][1],
        node.localMatrix.m[3][2]
      };
      ImGui::Text("LocalMatrix Position: (%.3f, %.3f, %.3f)", matrixPos.x, matrixPos.y, matrixPos.z);

      // メッシュインデックスの詳細
      if (!node.meshIndices.empty()) {
        ImGui::Text("Mesh Indices: %zu", node.meshIndices.size());
        for (int meshIdx : node.meshIndices) {
          ImGui::Text("  - Mesh[%d]", meshIdx);
        }
      }

      // 子ノード数
      ImGui::Text("Children: %zu", node.children.size());
      ImGui::PopStyleColor();

      // ホバー時に詳細情報をツールチップで表示
      if (ImGui::IsItemHovered()) {
        ImGui::BeginTooltip();
        ImGui::Text("Node Details:");
        ImGui::Separator();
        ImGui::Text("Name: %s", node.name.c_str());
        ImGui::Text("Has Animation: %s", hasNodeAnimation ? "Yes" : "No");
        ImGui::Text("Mesh Count: %zu", node.meshIndices.size());
        ImGui::Text("Children Count: %zu", node.children.size());
        ImGui::EndTooltip();
      }

      ImGui::Unindent();

      // 子ノードを再帰的に表示
      for (const Node& child : node.children) {
        DrawNodeHierarchyImGui(child, depth + 1);
      }

      ImGui::TreePop();
    }

    // インデントを元に戻す
    ImGui::Unindent(indentAmount * depth);
#endif
  }

  void Model::LoadAnimationFile(const std::string& directoryPath, const std::string& fileName)
  {
    Assimp::Importer importer;
    std::string filePath = directoryPath + "/" + fileName;
    const aiScene* scene = importer.ReadFile(filePath.c_str(), 0);

    // アニメーションがない場合は早期リターン
    if (!scene->HasAnimations()) {
      hasAnimation_ = false;
      return;
    }

    // すべてのアニメーションを読み込む
    for (uint32_t animIndex = 0; animIndex < scene->mNumAnimations; ++animIndex) {
      Animation animation;
      aiAnimation* aiAnimation = scene->mAnimations[animIndex];
      animation.duration = static_cast<float>(aiAnimation->mDuration / aiAnimation->mTicksPerSecond); // アニメーションの長さを取得,秒に変換

      // アニメーション名を取得（空の場合はデフォルト名を付ける）
      std::string animName = aiAnimation->mName.C_Str();
      if (animName.empty()) {
        animName = "Animation_" + std::to_string(animIndex);
      }

      // ノードアニメーションの解析
      for (uint32_t channelIndex = 0; channelIndex < aiAnimation->mNumChannels; ++channelIndex) {
        aiNodeAnim* aiNodeAnim = aiAnimation->mChannels[channelIndex];
        NodeAnimation& nodeAnimetion = animation.nodeAnimations[aiNodeAnim->mNodeName.C_Str()]; // ノード名をキーにしてノードアニメーションを取得

        // 位置アニメーションの解析
        for (uint32_t keyIndex = 0; keyIndex < aiNodeAnim->mNumPositionKeys; ++keyIndex) {
          aiVectorKey& aiKey = aiNodeAnim->mPositionKeys[keyIndex];
          KeyFrameVector3 keyFrame;
          keyFrame.time = static_cast<float>(aiKey.mTime / aiAnimation->mTicksPerSecond); // 時間を秒に変換
          keyFrame.value = Vector3(-aiKey.mValue.x, aiKey.mValue.y, aiKey.mValue.z);
          nodeAnimetion.translate.keyFrames.push_back(keyFrame);
        }

        // 回転アニメーションの解析
        for (uint32_t keyIndex = 0; keyIndex < aiNodeAnim->mNumRotationKeys; ++keyIndex) {
          aiQuatKey& aiKey = aiNodeAnim->mRotationKeys[keyIndex];
          KeyFrameQuaternion keyFrame;
          keyFrame.time = static_cast<float>(aiKey.mTime / aiAnimation->mTicksPerSecond); // 時間を秒に変換
          keyFrame.value = Quaternion(aiKey.mValue.x, -aiKey.mValue.y, -aiKey.mValue.z, aiKey.mValue.w); // クォータニオンの y,z 成分を反転,右手系から左手系に変換
          nodeAnimetion.rotate.keyFrames.push_back(keyFrame);
        }

        // スケールアニメーションの解析
        for (uint32_t keyIndex = 0; keyIndex < aiNodeAnim->mNumScalingKeys; ++keyIndex) {
          aiVectorKey& aiKey = aiNodeAnim->mScalingKeys[keyIndex];
          KeyFrameVector3 keyFrame;
          keyFrame.time = static_cast<float>(aiKey.mTime / aiAnimation->mTicksPerSecond); // 時間を秒に変換
          keyFrame.value = Vector3(aiKey.mValue.x, aiKey.mValue.y, aiKey.mValue.z);
          nodeAnimetion.scale.keyFrames.push_back(keyFrame);
        }
      }

      // アニメーションをマップに追加
      animations_[animName] = animation;
      animationTimes_[animName] = 0.0f;

      // デフォルトでループ設定を true にする
      animationLoopSettings_[animName] = true;
      animationFinished_[animName] = false;
    }

    // 最初のアニメーションをデフォルトに設定
    if (!animations_.empty()) {
      currentAnimationName_ = animations_.begin()->first;
    }
  }

  void Model::PrepareSkinning()
  {
    // スケルトンやアニメーションがない場合は何もしない
    if (!hasSkeleton_) {
      return;
    }

    // アニメーションがある場合のみ時間を更新
    if (hasAnimation_ && !currentAnimationName_.empty() && animationTimes_.find(currentAnimationName_) != animationTimes_.end()) {
      // アニメーションの適用（ポーズの更新）
      UpdateSkeletonAnimation(animationTimes_[currentAnimationName_]);
    }

    // スケルトン行列の更新
    UpdateSkeleton();

    // パレット（ボーン変換行列）の更新
    for (size_t jointIndex = 0; jointIndex < skeleton_.joints.size(); ++jointIndex) {
      // InverseBindMatrix（初期ポーズの逆行列）* 現在のスケルトン空間行列
      mappedPalette_[jointIndex].skeletonSpaceMat =
        inverseBindMatrices_[jointIndex] * skeleton_.joints[jointIndex].skeletonSpaceMatrix;

      // 法線変換用の逆転置行列も計算
      Matrix4x4 normalMatrix = Mat4x4::Transpose(Mat4x4::Inverse(mappedPalette_[jointIndex].skeletonSpaceMat));
      mappedPalette_[jointIndex].skeletonSpaceMatrixInvTransposeMat = normalMatrix;
    }

    // UAV バリアを設定
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

    // ComputeShader の設定
    m_modelBasic_->SetSkinningCSSetting();

    // 共有パレット（ボーン行列）の SRV を設定
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

    // シャドウマップレンダリング中は設定を維持
    if (!ShadowRenderer::GetInstance()->IsRenderingShadow()) {
      // 通常レンダリング時は共通レンダリング設定に戻す
      Object3dBasic::GetInstance()->SetCommonRenderSetting();
    }
    else {
      // シャドウマップレンダリング中はシャドウレンダラーの設定を使用
      ShadowRenderer::GetInstance()->SetRenderState();
    }
  }

  void Model::InitializeMatrixPalette() {
    // スケルトンがない場合は何もしない
    if (!hasSkeleton_) {
      return;
    }

    // 1. インバースバインドマトリクスの準備
    inverseBindMatrices_.resize(skeleton_.joints.size());

    // すべての行列を単位行列で初期化
    for (size_t i = 0; i < inverseBindMatrices_.size(); ++i) {
      inverseBindMatrices_[i] = Mat4x4::MakeIdentity();
    }

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

    // 初期状態の設定（単位行列）
    for (size_t i = 0; i < skeleton_.joints.size(); ++i) {
      mappedPalette_[i].skeletonSpaceMat = Mat4x4::MakeIdentity();
      mappedPalette_[i].skeletonSpaceMatrixInvTransposeMat = Mat4x4::MakeIdentity();
    }

    // 3. パレットの SRV を作成
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
    joint.index = static_cast<int32_t>(joints.size());              // 現在登録されている数を index として設定
    joint.parentIndex = parentIndex;
    joints.push_back(joint);                           // skeleton の joint 列に追加

    for (const Node& child : node.children) {
      int32_t childIndex = CreateJoint(child, joint.index, joints); // 再帰的に子 Joint を生成
      joints[joint.index].childrenIndex.push_back(childIndex);      // 子 Joint の index を追加
    }

    return joint.index;
  }

  Skeleton Model::CreateSkeleton(const Node& rootNode)
  {
    Skeleton skeleton;
    skeleton.root = CreateJoint(rootNode, {}, skeleton.joints); // ルートノードからジョイントを生成

    // 名前と index のマップを作る
    for (const Joint& joint : skeleton.joints) {
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
    result.transform.translate = { -position.x, position.y, position.z };           // 平行移動を取得,x 軸を反転

    result.localMatrix = Mat4x4::MakeAffine(
      result.transform.scale, result.transform.rotate, result.transform.translate);   // ローカル変換行列を生成

    result.name = node->mName.C_Str();                                                // ノードの名前を取得

    // メッシュインデックスの読み込み
    result.meshIndices.resize(node->mNumMeshes);
    for (uint32_t i = 0; i < node->mNumMeshes; i++) {
      result.meshIndices[i] = node->mMeshes[i];
    }

    result.children.resize(node->mNumChildren); // 子ノードの数だけリサイズ

    for (uint32_t childIndex = 0; childIndex < node->mNumChildren; ++childIndex) {
      result.children[childIndex] = ReadNode(node->mChildren[childIndex]); // 再帰的に子ノードを読み込む
    }

    return result;
  }

  void Model::UpdateAnimation(float deltaTime)
  {
    if (!hasAnimation_ || animations_.empty() || currentAnimationName_.empty()) {
      return;
    }

    // 遷移中の処理
    if (isTransitioning_) {
      transitionTime_ += deltaTime;

      // 遷移完了チェック
      if (transitionTime_ >= transitionDuration_) {
        isTransitioning_ = false;
        transitionTime_ = 0.0f;
        transitionDuration_ = 0.0f;
      }
    }

    // 現在のアニメーションの時間を更新
    float& animTime = animationTimes_[currentAnimationName_];
    animTime += deltaTime;

    // ループ設定を確認
    bool shouldLoop = true;  // デフォルトはループする
    auto loopIt = animationLoopSettings_.find(currentAnimationName_);
    if (loopIt != animationLoopSettings_.end()) {
      shouldLoop = loopIt->second;
    }

    // ループ処理または終了処理
    float duration = animations_[currentAnimationName_].duration;
    if (shouldLoop) {
      // ループする場合は時間をリセット
      animTime = std::fmod(animTime, duration);
      animationFinished_[currentAnimationName_] = false;
    }
    else {
      // ループしない場合は最大値でクランプ
      if (animTime >= duration) {
        animTime = duration;
        animationFinished_[currentAnimationName_] = true;
      }
      else {
        animationFinished_[currentAnimationName_] = false;
      }
    }
  }

  void Model::UpdateNodeHierarchyAnimation(Node& node, float time)
  {
    if (!hasAnimation_ || animations_.empty() || currentAnimationName_.empty()) {
      return;
    }

    // 現在のアニメーションを取得
    const Animation& currentAnimation = animations_[currentAnimationName_];

    // このノードのアニメーションを検索
    if (auto it = currentAnimation.nodeAnimations.find(node.name); it != currentAnimation.nodeAnimations.end()) {
      const NodeAnimation& nodeAnimation = it->second;

      // 位置アニメーションの計算
      Vector3 translate = CalcKeyFrameValue(nodeAnimation.translate.keyFrames, time);
      // 回転アニメーションの計算
      Quaternion rotate = CalcKeyFrameValue(nodeAnimation.rotate.keyFrames, time);
      // スケールアニメーションの計算
      Vector3 scale = CalcKeyFrameValue(nodeAnimation.scale.keyFrames, time);

      // 遷移中の補間処理
      if (isTransitioning_ && previousPose_.nodeTransforms.find(node.name) != previousPose_.nodeTransforms.end()) {
        float t = transitionTime_ / transitionDuration_;
        t = std::min(t, 1.0f);  // 0〜1にクランプ

        const QuatTransform& prevTransform = previousPose_.nodeTransforms[node.name];

        // 前のポーズから現在のアニメーションへ補間
        translate = Vec3::Lerp(prevTransform.translate, translate, t);
        rotate = Quat::Slerp(prevTransform.rotate, rotate, t);
        scale = Vec3::Lerp(prevTransform.scale, scale, t);
      }

      // ローカル変換行列を更新
      node.transform.translate = translate;
      node.transform.rotate = rotate;
      node.transform.scale = scale;
      node.localMatrix = Mat4x4::MakeAffine(scale, rotate, translate);
    }

    // 子ノードも再帰的に更新
    for (Node& child : node.children) {
      UpdateNodeHierarchyAnimation(child, time);
    }
  }

  void Model::UpdateSkeleton()
  {
    for (Joint& joint : skeleton_.joints) {
      joint.localMatrix = Mat4x4::MakeAffine(joint.transform.scale, joint.transform.rotate, joint.transform.translate); // ローカル変換行列を生成

      if (joint.parentIndex) {
        joint.skeletonSpaceMatrix = joint.localMatrix * skeleton_.joints[*joint.parentIndex].skeletonSpaceMatrix; // 親がいる場合は親のスケルトン空間行列を掛ける
      }
      else {
        joint.skeletonSpaceMatrix = joint.localMatrix; // 親がいない場合はローカル変換行列がスケルトン空間行列
      }
    }
  }

  void Model::UpdateSkeletonAnimation(float time)
  {
    if (!hasAnimation_ || animations_.empty() || currentAnimationName_.empty()) {
      return;
    }

    // 現在のアニメーションを取得
    const Animation& currentAnimation = animations_[currentAnimationName_];

    for (Joint& joint : skeleton_.joints) {
      if (auto it = currentAnimation.nodeAnimations.find(joint.name); it != currentAnimation.nodeAnimations.end()) {
        const NodeAnimation& rootAnimetion = (*it).second; // ルートノードのアニメーションを取得

        // 位置アニメーションの計算
        Vector3 translate = CalcKeyFrameValue(rootAnimetion.translate.keyFrames, time);
        // 回転アニメーションの計算
        Quaternion rotate = CalcKeyFrameValue(rootAnimetion.rotate.keyFrames, time);
        // スケールアニメーションの計算
        Vector3 scale = CalcKeyFrameValue(rootAnimetion.scale.keyFrames, time);

        // 遷移中の補間処理
        if (isTransitioning_ && previousPose_.jointTransforms.find(joint.name) != previousPose_.jointTransforms.end()) {
          float t = transitionTime_ / transitionDuration_;
          t = std::min(t, 1.0f);  // 0〜1にクランプ

          const QuatTransform& prevTransform = previousPose_.jointTransforms[joint.name];

          // 前のポーズから現在のアニメーションへ補間
          translate = Vec3::Lerp(prevTransform.translate, translate, t);
          rotate = Quat::Slerp(prevTransform.rotate, rotate, t);
          scale = Vec3::Lerp(prevTransform.scale, scale, t);
        }

        // ジョイントのトランスフォームを更新
        joint.transform.translate = translate;
        joint.transform.rotate = rotate;
        joint.transform.scale = scale;
      }
    }
  }

  void Model::ReleaseSkinningSRVIndex()
  {
    // パレット SRV の解放
    if (paletteSrvIndex_ != 0) {
      // アロケートされているか確認してから解放
      if (SrvManager::GetInstance()->IsAllocated(paletteSrvIndex_)) {
        SrvManager::GetInstance()->Free(paletteSrvIndex_);
      }
      paletteSrvIndex_ = 0;
    }
  }

  Vector3 Model::CalcKeyFrameValue(const std::vector<KeyFrameVector3>& keyFrames, float time)
  {
    assert(!keyFrames.empty()); // キーフレームがない場合はエラー
    if (keyFrames.size() == 1 || time <= keyFrames[0].time) {
      return keyFrames[0].value; // 最初のキーフレームの値を返す
    }

    for (uint32_t keyIndex = 0; keyIndex < keyFrames.size() - 1; ++keyIndex) {
      uint32_t nextKeyIndex = keyIndex + 1;
      if (keyFrames[keyIndex].time <= time && time <= keyFrames[nextKeyIndex].time) {
        float t = (time - keyFrames[keyIndex].time) / (keyFrames[nextKeyIndex].time - keyFrames[keyIndex].time); // 補間係数を計算
        return Vec3::Lerp(keyFrames[keyIndex].value, keyFrames[nextKeyIndex].value, t); // 線形補間
      }
    }

    return (*keyFrames.rbegin()).value; // 最後のキーフレームの値を返す
  }

  Quaternion Model::CalcKeyFrameValue(const std::vector<KeyFrameQuaternion>& keyFrames, float time)
  {
    assert(!keyFrames.empty()); // キーフレームがない場合はエラー
    if (keyFrames.size() == 1 || time <= keyFrames[0].time) {
      return keyFrames[0].value; // 最初のキーフレームの値を返す
    }

    for (uint32_t keyIndex = 0; keyIndex < keyFrames.size() - 1; ++keyIndex) {
      uint32_t nextKeyIndex = keyIndex + 1;
      if (keyFrames[keyIndex].time <= time && time <= keyFrames[nextKeyIndex].time) {
        float t = (time - keyFrames[keyIndex].time) / (keyFrames[nextKeyIndex].time - keyFrames[keyIndex].time); // 補間係数を計算
        return Quat::Slerp(keyFrames[keyIndex].value, keyFrames[nextKeyIndex].value, t); // 球面線形補間
      }
    }

    return (*keyFrames.rbegin()).value; // 最後のキーフレームの値を返す
  }

  // アニメーション制御メソッドの実装
  void Model::SetAnimation(const std::string& animationName)
  {
    if (animations_.find(animationName) != animations_.end()) {
      currentAnimationName_ = animationName;
      animationTimes_[animationName] = 0.0f;  // アニメーション時間をリセット
      animationFinished_[animationName] = false;  // 終了フラグをリセット
      isTransitioning_ = false;  // 即座に切り替えるため遷移フラグをオフ
    }
    else {
#ifdef  _DEBUG
      DebugUIManager::GetInstance()->AddLog(
        "Warning: Animation '" + animationName + "' not found in model '" + modelFileName_ + "'",
        DebugUIManager::LogType::Warning);
#endif
    }
  }

  void Model::SetAnimation(const std::string& animationName, float transitionDuration)
  {
    if (animations_.find(animationName) != animations_.end()) {
      // 同じアニメーションへの遷移は無視
      if (currentAnimationName_ == animationName) {
        return;
      }

      // 現在のポーズを保存
      SaveCurrentPose();

      // 遷移設定
      currentAnimationName_ = animationName;
      animationTimes_[animationName] = 0.0f;  // アニメーション時間をリセット
      animationFinished_[animationName] = false;  // 終了フラグをリセット
      transitionDuration_ = transitionDuration;
      transitionTime_ = 0.0f;
      isTransitioning_ = true;
    }
    else {
#ifdef  _DEBUG
      DebugUIManager::GetInstance()->AddLog(
        "Warning: Animation '" + animationName + "' not found in model '" + modelFileName_ + "'",
        DebugUIManager::LogType::Warning);
#endif
    }
  }

  std::vector<std::string> Model::GetAnimationNames() const
  {
    std::vector<std::string> names;
    names.reserve(animations_.size());

    for (const auto& [name, animation] : animations_) {
      names.push_back(name);
    }

    return names;
  }

  void Model::SetAnimationLoop(const std::string& animationName, bool loop)
  {
    // アニメーションが存在する場合のみ設定
    if (animations_.find(animationName) != animations_.end()) {
      animationLoopSettings_[animationName] = loop;

      // ループしない設定の場合、終了フラグを初期化
      if (!loop) {
        animationFinished_[animationName] = false;
      }
    }
  }

  bool Model::IsAnimationLooping(const std::string& animationName) const
  {
    // 設定がない場合はデフォルトでループする
    auto it = animationLoopSettings_.find(animationName);
    if (it != animationLoopSettings_.end()) {
      return it->second;
    }
    return true;  // デフォルトはループ
  }

  bool Model::IsAnimationFinished(const std::string& animationName) const
  {
    // 終了フラグを確認
    auto it = animationFinished_.find(animationName);
    if (it != animationFinished_.end()) {
      return it->second;
    }
    return false;  // デフォルトは未終了
  }

  void Model::SaveCurrentPose()
  {
    // 前回のポーズをクリア
    previousPose_.nodeTransforms.clear();
    previousPose_.jointTransforms.clear();

    // ノードベースアニメーションの場合
    SaveNodePose(rootNode_);

    // スケルトンアニメーションの場合
    if (hasSkeleton_) {
      for (const auto& joint : skeleton_.joints) {
        previousPose_.jointTransforms[joint.name] = joint.transform;
      }
    }
  }

  void Model::SaveNodePose(const Node& node)
  {
    // 現在のノードのトランスフォームを保存
    previousPose_.nodeTransforms[node.name] = node.transform;

    // 子ノードも再帰的に保存
    for (const auto& child : node.children) {
      SaveNodePose(child);
    }
  }

} // namespace Tako
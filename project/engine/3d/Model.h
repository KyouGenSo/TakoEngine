#pragma once
#include <d3d12.h>
#include <unordered_map>
#include <map>
#include<wrl.h>
#include "ModelStruct.h"

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

#include "Mesh.h"

class ModelBasic;
class DX12Basic;

class Model
{
public: // メンバー関数
  /// <summary>
  /// 初期化
  /// </summary>
  void Initialize(ModelBasic* modelBasic, const std::string& fileName, bool hasAnimation, bool hasSkeleton);

  /// <summary>
  /// 終了処理
  /// </summary>
  void Finalize();

  /// <summary>
  /// 更新
  /// </summary>
  void Update();

  /// <summary>
  /// 描画
  /// </summary>
  void Draw(Matrix4x4 world, Matrix4x4 viewProjection);

  /// <summary>
  /// objファイルの読み込む
  ///	</summary>
  void LoadModelFile(const std::string& directoryPath, const std::string& fileName);

  /// <summary>
  /// デバッグUIを表示
  /// </summary>
  void DrawImGui();

  /// <summary>
  /// クローン
  ///	</summary>
  Model* Clone() const;

  // -----------------------------------Getters-----------------------------------//
  // nodeのlocalMatrixを取得
  const Matrix4x4& GetLocalMatrix() const { return rootNode_.localMatrix; }
  // skeletonを取得
  const Skeleton& GetSkeleton() const { return skeleton_; }
  // アニメーションの有無を取得
  bool HasAnimation() const { return hasAnimation_; }
  // Skeletonの有無を取得
  bool HasSkeleton() const { return hasSkeleton_; }
  
  /// <summary>
  /// 指定したJointのワールド座標変換行列を取得
  /// </summary>
  /// <param name="jointName">Joint名</param>
  /// <param name="worldMatrix">モデルのワールド行列</param>
  /// <returns>Jointのワールド座標変換行列</returns>
  Matrix4x4 GetJointWorldMatrix(const std::string& jointName, const Matrix4x4& worldMatrix) const;

  // -----------------------------------Setters-----------------------------------//
  void SetShininess(float shininess);
  void SetEnableLighting(bool enableLighting);
  void SetEnableHighlight(bool enableHighlight);
  void SetMaterialColor(const Vector4& color);
  void SetUvTransform(const Transform& uvTransform);
  void SetEnvironmentTexture(uint32_t textureIndex);
  void SetEnableEnvMap(bool enableEnvMap);
  void SetEnvMapCoefficient(float coefficient);
  // デバッグ表示の有効/無効を設定
  static void SetShowSkeletonDebug(bool show) { s_showSkeletonDebug = show; }
  static bool GetShowSkeletonDebug() { return s_showSkeletonDebug; }

  // -----------------------------------Animation Control-----------------------------------//
  /// <summary>
  /// アニメーションを切り替える
  /// </summary>
  /// <param name="animationName">アニメーション名</param>
  void SetAnimation(const std::string& animationName);

  /// <summary>
  /// アニメーションを補間付きで切り替える
  /// </summary>
  /// <param name="animationName">アニメーション名</param>
  /// <param name="transitionDuration">遷移時間（秒）</param>
  void SetAnimation(const std::string& animationName, float transitionDuration);

  /// <summary>
  /// 登録されているアニメーション名のリストを取得
  /// </summary>
  /// <returns>アニメーション名のリスト</returns>
  std::vector<std::string> GetAnimationNames() const;

  /// <summary>
  /// 現在のアニメーション名を取得
  /// </summary>
  /// <returns>現在のアニメーション名</returns>
  const std::string& GetCurrentAnimationName() const { return currentAnimationName_; }

  /// <summary>
  /// アニメーション再生速度を設定
  /// </summary>
  /// <param name="speed">再生速度（1.0fが通常速度、負の値で逆再生）</param>
  void SetAnimationSpeed(float speed) { animationSpeed_ = speed; }

  /// <summary>
  /// アニメーション再生速度を取得
  /// </summary>
  /// <returns>現在の再生速度</returns>
  float GetAnimationSpeed() const { return animationSpeed_; }

  /// <summary>
  /// アニメーションを一時停止
  /// </summary>
  void PauseAnimation() { isPaused_ = true; }

  /// <summary>
  /// アニメーションを再生再開
  /// </summary>
  void ResumeAnimation() { isPaused_ = false; }

  /// <summary>
  /// アニメーションが一時停止中かを取得
  /// </summary>
  /// <returns>一時停止中ならtrue</returns>
  bool IsAnimationPaused() const { return isPaused_; }

private: // プライベートメンバー関数
  /// <summary>
  /// ノード階層で座標変換行列を処理して描画
  /// </summary>
  void ProcessNodeHierarchy(const Node& node, const Matrix4x4& parentGlobalMatrix, Matrix4x4 world, Matrix4x4 viewProjection);

  /// <summary>
  /// Skeletonのデバグ用描画
  /// </summary>
  void DrawSkeleton(Matrix4x4 world);

  /// <summary>
  /// アニメーションの読み込み
  /// </summary>
  void LoadAnimationFile(const std::string& directoryPath, const std::string& fileName);

  /// <summary>
  /// スキニング処理関連
  /// </summary>
  void InitializeMatrixPalette();
  void PrepareSkinning();
  void ExecuteSkinning();

  /// <summary>
  /// ノード読み込み
  /// <summary>
  Node ReadNode(aiNode* node);

  /// <summary>
  /// Jointの生成
  /// </summary>
  int32_t CreateJoint(const Node& node, const std::optional<int32_t>& parentIndex, std::vector<Joint>& joints);

  /// <summary>
  /// Skeletonの生成
  /// </summary>
  Skeleton CreateSkeleton(const Node& rootNode);

  /// <summary>
  /// キーフレームの値を計算
  /// <summary>
  Vector3 CalcKeyFrameValue(const std::vector<KeyFrameVector3>& keyFrames, float time);
  Quaternion CalcKeyFrameValue(const std::vector<KeyFrameQuaternion>& keyFrames, float time);

  /// <summary>
  /// アニメーションの更新
  /// </summary>
  void UpdateAnimation(float deltaTime);

  /// <summary>
  /// ノード階層のアニメーション更新
  /// </summary>
  void UpdateNodeHierarchyAnimation(Node& node, float time);

  /// <summary>
  /// skeletonの更新
  /// </summary>
  void UpdateSkeleton();

  /// <summary>
  /// アニメーションを適用
  /// </summary>
  void UpdateSkeletonAnimation(float time);

  /// <summary>
  /// スキニング関連リソースの解放
  /// </summary>
  void ReleaseSkinningSRVIndex();

  /// <summary>
  /// ジョイント階層を再帰的に表示（ImGui用）
  /// </summary>
  void DrawJointHierarchy(int32_t jointIndex, int depth = 0);

  /// <summary>
  /// 現在のポーズを保存
  /// </summary>
  void SaveCurrentPose();

  /// <summary>
  /// ノードのポーズを再帰的に保存
  /// </summary>
  void SaveNodePose(const Node& node);

private: // メンバ変数

  // アニメーション遷移関連構造体
  struct AnimationTransitionState {
    std::map<std::string, QuatTransform> nodeTransforms;  // ノード用
    std::map<std::string, QuatTransform> jointTransforms; // ジョイント用
  };

  ModelBasic* m_modelBasic_;
  DX12Basic* m_dx12_;

  std::string directoryFolderName_;
  std::string ModelFolderName_;
  std::string modelFileName_;  // モデルファイル名を保存

  // モデルデータ
  std::vector<Mesh*> meshes_;

  // ノードデータ
  Node rootNode_;

  // テクスチャキャッシュ
  std::unordered_map<std::string, TextureData> textureCache_;

  // スケルトン・アニメーション関連
  std::map<std::string, Animation> animations_;  // 複数アニメーション対応
  std::string currentAnimationName_;              // 現在のアニメーション名
  std::map<std::string, float> animationTimes_;  // 各アニメーションの再生時間
  Skeleton skeleton_;
  bool hasAnimation_ = false;
  bool hasSkeleton_ = false;

  // スキニング関連
  std::vector<Matrix4x4> inverseBindMatrices_;
  Microsoft::WRL::ComPtr<ID3D12Resource> paletteResource_;
  std::span<WellForGPU> mappedPalette_;
  uint32_t paletteSrvIndex_ = 0;
  std::pair<D3D12_CPU_DESCRIPTOR_HANDLE, D3D12_GPU_DESCRIPTOR_HANDLE> paletteSrvHandle_;
  std::map<std::string, JointWeightData> skinClusterData_;
  std::vector<MeshSkinClusterData> meshSkinClusterData_;

  // デバッグ表示用
  static bool s_showSkeletonDebug;  // 全体的なスケルトン表示ON/OFF
  int expandState_ = 0;  // 0: normal, 1: expand all, 2: collapse all
  int32_t hoveredJointIndex_ = -1;  // ホバー中のジョイントインデックス（-1: なし）

  // アニメーション遷移関連
  AnimationTransitionState previousPose_;     // 遷移前のポーズ
  float transitionDuration_ = 0.0f;          // 遷移時間（秒）
  float transitionTime_ = 0.0f;              // 現在の遷移経過時間
  bool isTransitioning_ = false;             // 遷移中フラグ

  // アニメーション再生制御
  float animationSpeed_ = 1.0f;             // アニメーション再生速度（1.0fが通常速度）
  bool isPaused_ = false;                   // アニメーション一時停止フラグ
};
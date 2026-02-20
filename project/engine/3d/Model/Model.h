#pragma once
#include <d3d12.h>
#include <unordered_map>
#include <map>
#include<wrl.h>
#include "ModelStruct.h"

#include <assimp/scene.h>

#include "Mesh.h"

namespace Tako {

  class ModelBasic;
  class DX12Basic;

  /// <summary>
  /// 3D モデル管理クラス
  /// アニメーション、スキニング、マテリアル対応
  /// </summary>
  class Model
  {
  public: // メンバー関数
    /// <summary>
    /// 初期化
    /// </summary>
    /// <param name="modelBasic">モデル基本システムへのポインタ</param>
    /// <param name="fileName">読み込むモデルファイル名</param>
    void Initialize(ModelBasic* modelBasic, const std::string& fileName);

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
    /// <param name="world">ワールド変換行列</param>
    /// <param name="viewProjection">ビュープロジェクション行列</param>
    void Draw(Matrix4x4 world, Matrix4x4 viewProjection);

    /// <summary>
    /// インスタンシング描画
    /// </summary>
    /// <param name="instanceCount">描画するインスタンス数</param>
    void DrawInstanced(uint32_t instanceCount);

    /// <summary>
    /// obj ファイルの読み込む
    ///	</summary>
    /// <param name="directoryPath">モデルファイルのディレクトリパス</param>
    /// <param name="fileName">モデルファイル名</param>
    void LoadModelFile(const std::string& directoryPath, const std::string& fileName);

    /// <summary>
    /// デバッグ UI を表示
    /// </summary>
    void DrawImGui();

    /// <summary>
    /// クローン
    ///	</summary>
    Model* Clone() const;

    // -----------------------------------Getters-----------------------------------//
    /// <summary>
    /// ルートノードのローカル行列を取得
    /// </summary>
    /// <returns>ローカル行列</returns>
    const Matrix4x4& GetLocalMatrix() const { return rootNode_.localMatrix; }

    /// <summary>
    /// スケルトンを取得
    /// </summary>
    /// <returns>スケルトン情報</returns>
    const Skeleton& GetSkeleton() const { return skeleton_; }

    /// <summary>
    /// アニメーションの有無を取得
    /// </summary>
    /// <returns>アニメーションが存在する場合 true</returns>
    bool HasAnimation() const { return hasAnimation_; }

    /// <summary>
    /// スケルトンの有無を取得
    /// </summary>
    /// <returns>スケルトンが存在する場合 true</returns>
    bool HasSkeleton() const { return hasSkeleton_; }

    /// <summary>
    /// 指定した Joint のワールド座標変換行列を取得
    /// </summary>
    /// <param name="jointName">Joint 名</param>
    /// <param name="worldMatrix">モデルのワールド行列</param>
    /// <returns>Joint のワールド座標変換行列</returns>
    Matrix4x4 GetJointWorldMatrix(const std::string& jointName, const Matrix4x4& worldMatrix) const;

    // -----------------------------------Setters-----------------------------------//
    /// <summary>
    /// 光沢度を設定
    /// </summary>
    /// <param name="shininess">光沢度</param>
    void SetShininess(float shininess);

    /// <summary>
    /// ライティングの有効/無効を設定
    /// </summary>
    /// <param name="enableLighting">ライティングを有効にするか</param>
    void SetEnableLighting(bool enableLighting);

    /// <summary>
    /// ハイライトの有効/無効を設定
    /// </summary>
    /// <param name="enableHighlight">ハイライトを有効にするか</param>
    void SetEnableHighlight(bool enableHighlight);

    /// <summary>
    /// マテリアルカラーを設定
    /// </summary>
    /// <param name="color">マテリアルカラー（RGBA）</param>
    void SetMaterialColor(const Vector4& color);

    /// <summary>
    /// マテリアルカラーを取得
    /// </summary>
    /// <returns>マテリアルカラー（RGBA）</returns>
    Vector4 GetMaterialColor() const;

    /// <summary>
    /// UV トランスフォームを設定
    /// </summary>
    /// <param name="uvTransform">UV トランスフォーム情報</param>
    void SetUvTransform(const Transform& uvTransform);

    /// <summary>
    /// 環境マップテクスチャを設定
    /// </summary>
    /// <param name="textureIndex">テクスチャインデックス</param>
    void SetEnvironmentTexture(uint32_t textureIndex);

    /// <summary>
    /// 環境マップの有効/無効を設定
    /// </summary>
    /// <param name="enableEnvMap">環境マップを有効にするか</param>
    void SetEnableEnvMap(bool enableEnvMap);

    /// <summary>
    /// 環境マップの係数を設定
    /// </summary>
    /// <param name="coefficient">環境マップ係数</param>
    void SetEnvMapCoefficient(float coefficient);

    /// <summary>
    /// スケルトンデバッグ表示の有効/無効を設定（静的関数）
    /// </summary>
    /// <param name="show">デバッグ表示を有効にするか</param>
    static void SetShowSkeletonDebug(bool show) { s_showSkeletonDebug = show; }

    /// <summary>
    /// スケルトンデバッグ表示の状態を取得（静的関数）
    /// </summary>
    /// <returns>デバッグ表示が有効な場合 true</returns>
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
    /// <param name="speed">再生速度（1.0f が通常速度、負の値で逆再生）</param>
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
    /// <returns>一時停止中なら true</returns>
    bool IsAnimationPaused() const { return isPaused_; }

    /// <summary>
    /// アニメーションのループ設定を変更
    /// </summary>
    /// <param name="animationName">アニメーション名</param>
    /// <param name="loop">ループするかどうか</param>
    void SetAnimationLoop(const std::string& animationName, bool loop);

    /// <summary>
    /// アニメーションがループ設定されているかを取得
    /// </summary>
    /// <param name="animationName">アニメーション名</param>
    /// <returns>ループ設定されていれば true</returns>
    bool IsAnimationLooping(const std::string& animationName) const;

    /// <summary>
    /// アニメーションが終了したかを取得（ループしない場合のみ有効）
    /// </summary>
    /// <param name="animationName">アニメーション名</param>
    /// <returns>アニメーションが終了していれば true</returns>
    bool IsAnimationFinished(const std::string& animationName) const;

  private: // プライベートメンバー関数
    /// <summary>
    /// ノード階層で座標変換行列を処理して描画
    /// </summary>
    /// <param name="node">処理対象のノード</param>
    /// <param name="parentGlobalMatrix">親ノードのグローバル変換行列</param>
    /// <param name="world">ワールド変換行列</param>
    /// <param name="viewProjection">ビュープロジェクション行列</param>
    void ProcessNodeHierarchy(const Node& node, const Matrix4x4& parentGlobalMatrix, Matrix4x4 world, Matrix4x4 viewProjection);

    /// <summary>
    /// Skeleton のデバグ用描画
    /// </summary>
    /// <param name="world">ワールド変換行列</param>
    void DrawSkeleton(Matrix4x4 world);

    /// <summary>
    /// アニメーションの読み込み
    /// </summary>
    /// <param name="directoryPath">アニメーションファイルのディレクトリパス</param>
    /// <param name="fileName">アニメーションファイル名</param>
    void LoadAnimationFile(const std::string& directoryPath, const std::string& fileName);

    /// <summary>
    /// スキニング処理関連
    /// </summary>
    void InitializeMatrixPalette();
    void PrepareSkinning();
    void ExecuteSkinning();

    /// <summary>
    /// ノード読み込み
    /// </summary>
    /// <param name="node">Assimp ノードポインタ</param>
    /// <returns>読み込まれたノード</returns>
    Node ReadNode(aiNode* node);

    /// <summary>
    /// Joint の生成
    /// </summary>
    /// <param name="node">ノード情報</param>
    /// <param name="parentIndex">親ジョイントのインデックス（ルートの場合は nullopt）</param>
    /// <param name="joints">ジョイント配列（出力先）</param>
    /// <returns>生成されたジョイントのインデックス</returns>
    int32_t CreateJoint(const Node& node, const std::optional<int32_t>& parentIndex, std::vector<Joint>& joints);

    /// <summary>
    /// Skeleton の生成
    /// </summary>
    /// <param name="rootNode">ルートノード</param>
    /// <returns>生成されたスケルトン</returns>
    Skeleton CreateSkeleton(const Node& rootNode);

    /// <summary>
    /// キーフレームの値を計算（Vector3版）
    /// </summary>
    /// <param name="keyFrames">キーフレーム配列</param>
    /// <param name="time">現在の時間</param>
    /// <returns>補間された Vector3値</returns>
    Vector3 CalcKeyFrameValue(const std::vector<KeyFrameVector3>& keyFrames, float time);

    /// <summary>
    /// キーフレームの値を計算（Quaternion 版）
    /// </summary>
    /// <param name="keyFrames">キーフレーム配列</param>
    /// <param name="time">現在の時間</param>
    /// <returns>補間された Quaternion 値</returns>
    Quaternion CalcKeyFrameValue(const std::vector<KeyFrameQuaternion>& keyFrames, float time);

    /// <summary>
    /// アニメーションの更新
    /// </summary>
    /// <param name="deltaTime">前フレームからの経過時間（秒）</param>
    void UpdateAnimation(float deltaTime);

    /// <summary>
    /// ノード階層のアニメーション更新
    /// </summary>
    /// <param name="node">更新対象のノード</param>
    /// <param name="time">アニメーション時間</param>
    void UpdateNodeHierarchyAnimation(Node& node, float time);

    /// <summary>
    /// skeleton の更新
    /// </summary>
    void UpdateSkeleton();

    /// <summary>
    /// アニメーションを適用
    /// </summary>
    /// <param name="time">アニメーション時間</param>
    void UpdateSkeletonAnimation(float time);

    /// <summary>
    /// スキニング関連リソースの解放
    /// </summary>
    void ReleaseSkinningSRVIndex();

    /// <summary>
    /// ジョイント階層を再帰的に表示（ImGui 用）
    /// </summary>
    /// <param name="jointIndex">表示するジョイントのインデックス</param>
    /// <param name="depth">階層の深さ（インデント用）</param>
    void DrawJointHierarchy(int32_t jointIndex, int depth = 0);

    /// <summary>
    /// ノード階層を再帰的に表示（ImGui 用）
    /// </summary>
    /// <param name="node">表示するノード</param>
    /// <param name="depth">階層の深さ（インデント用）</param>
    void DrawNodeHierarchyImGui(const Node& node, int depth = 0);

    /// <summary>
    /// 現在のポーズを保存
    /// </summary>
    void SaveCurrentPose();

    /// <summary>
    /// ノードのポーズを再帰的に保存
    /// </summary>
    /// <param name="node">保存対象のノード</param>
    void SaveNodePose(const Node& node);

  private: // メンバ変数

    ModelBasic* m_modelBasic_;  ///< モデル基本システムへのポインタ
    DX12Basic* m_dx12_;  ///< DirectX12基盤システムへのポインタ

    std::string directoryFolderName_;  ///< モデルファイルのディレクトリパス
    std::string ModelFolderName_;  ///< モデルフォルダ名
    std::string modelFileName_;  ///< モデルファイル名

    // モデルデータ
    std::vector<Mesh*> meshes_;  ///< メッシュデータ配列

    // ノードデータ
    Node rootNode_;  ///< ルートノード（階層構造の起点）

    // テクスチャキャッシュ
    std::unordered_map<std::string, TextureData> textureCache_;  ///< テクスチャデータのキャッシュ

    // スケルトン・アニメーション関連
    std::map<std::string, Animation> animations_;  ///< 複数アニメーション対応のアニメーションマップ
    std::string currentAnimationName_;  ///< 現在再生中のアニメーション名
    std::map<std::string, float> animationTimes_;  ///< 各アニメーションの再生時間
    Skeleton skeleton_;  ///< スケルトン情報
    bool hasAnimation_ = false;  ///< アニメーション有無フラグ
    bool hasSkeleton_ = false;  ///< スケルトン有無フラグ

    // スキニング関連
    std::vector<Matrix4x4> inverseBindMatrices_;  ///< 逆バインド行列配列
    Microsoft::WRL::ComPtr<ID3D12Resource> paletteResource_;  ///< スキニング用パレットリソース
    std::span<WellForGPU> mappedPalette_;  ///< マップされたパレットメモリ
    uint32_t paletteSrvIndex_ = 0;  ///< パレット SRV インデックス
    std::pair<D3D12_CPU_DESCRIPTOR_HANDLE, D3D12_GPU_DESCRIPTOR_HANDLE> paletteSrvHandle_;  ///< パレット SRV ハンドルペア
    std::map<std::string, JointWeightData> skinClusterData_;  ///< スキンクラスターデータマップ
    std::vector<MeshSkinClusterData> meshSkinClusterData_;  ///< メッシュごとのスキンクラスターデータ

    // デバッグ表示用
    static bool s_showSkeletonDebug;  ///< 全体的なスケルトン表示 ON/OFF
    int expandState_ = 0;  ///< ImGUI 展開状態（0:通常, 1:全展開, 2:全折畳）
    int32_t hoveredJointIndex_ = -1;  ///< ホバー中のジョイントインデックス（-1:なし）

    // アニメーション遷移関連
    AnimationTransitionState previousPose_;  ///< 遷移前のポーズ
    float transitionDuration_ = 0.0f;  ///< 遷移時間（秒）
    float transitionTime_ = 0.0f;  ///< 現在の遷移経過時間
    bool isTransitioning_ = false;  ///< 遷移中フラグ

    // アニメーション再生制御
    float animationSpeed_ = 1.0f;  ///< アニメーション再生速度（1.0f が通常速度）
    bool isPaused_ = false;  ///< アニメーション一時停止フラグ

    // アニメーションループ制御
    std::map<std::string, bool> animationLoopSettings_;  ///< 各アニメーションのループ設定
    std::map<std::string, bool> animationFinished_;  ///< 各アニメーションの終了フラグ
  };

} // namespace Tako
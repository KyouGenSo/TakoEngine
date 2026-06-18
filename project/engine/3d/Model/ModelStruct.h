#pragma once
#include <string>
#include <vector>
#include <map>
#include <optional>
#include <span>
#include <array>

#include <d3d12.h>
#include <wrl.h>

#include "vector2.h"
#include "vector3.h"
#include "vector4.h"
#include "Matrix4x4.h"
#include "Quaternion.h"
#include "Transform.h"

namespace Tako {

  /// <summary>
  /// シーングラフノード構造体
  /// 3D モデルの階層構造を表現（親子関係によるトランスフォームの継承）
  /// </summary>
  struct Node
  {
    QuatTransform     transform;    ///< このノードのローカル座標変換（位置・回転・スケール）
    Matrix4x4         localMatrix;  ///< ローカル変換行列（transform から計算）
    std::string       name;         ///< ノード名（ボーン検索などに使用）
    std::vector<Node> children;     ///< 子ノードのリスト
    std::vector<int>  meshIndices;  ///< このノードに関連付けられたメッシュインデックス
  };

  /// <summary>
  /// 頂点データ構造体
  /// 3D モデルの各頂点が持つ属性情報
  /// </summary>
  struct VertexData
  {
    Vector4 position;  ///< 頂点位置（w=1.0で同次座標）
    Vector2 texcoord;  ///< テクスチャ座標（UV 座標）
    Vector3 normal;    ///< 法線ベクトル（ライティング計算用）
  };

  /// <summary>
  /// テクスチャデータ構造体
  /// マテリアルに関連付けられたテクスチャ情報
  /// </summary>
  struct TextureData {
    std::string texturePath;   ///< テクスチャファイルのパス
    uint32_t    textureIndex;  ///< TextureManager でのインデックス
    Vector4     baseColor;     ///< 基本色（テクスチャと乗算される）
  };

  /// <summary>
  /// 頂点ウェイト情報構造体
  /// スキニング用の各頂点への影響度
  /// </summary>
  struct VertexWeightData
  {
    float    weight;       ///< ウェイト値（影響度：0.0〜1.0）
    uint32_t vertexIndex;  ///< 影響を受ける頂点のインデックス
  };

  /// <summary>
  /// ジョイント（ボーン）ウェイトデータ構造体
  /// 1つのジョイントが影響する全頂点のウェイト情報
  /// </summary>
  struct JointWeightData
  {
    Matrix4x4                     inverseBindMatrix;  ///< バインドポーズ時の逆行列（スキニング計算用）
    std::vector<VertexWeightData> vertexWeights;      ///< このジョイントが影響する頂点とウェイトのリスト
  };

  /// <summary>
  /// モデルデータ構造体（スキニングなし）
  /// 静的な3D モデルのメッシュデータ
  /// </summary>
  struct ModelData
  {
    std::vector<VertexData> vertices;     ///< 頂点データ配列
    TextureData             textureData;  ///< テクスチャ情報
  };

  /// <summary>
  /// スキンニング対応モデルデータ構造体
  /// スケルタルアニメーションに必要な全情報を含む
  /// </summary>
  struct SkinnigModelData {
    std::map<std::string, JointWeightData> skinClusterData;  ///< ジョイント名からウェイトデータへのマップ
    std::vector<VertexData>                vertices;         ///< 頂点データ配列
    std::vector<uint32_t>                  indices;          ///< 頂点インデックス配列（描画順序）
    TextureData                            textureData;      ///< テクスチャ情報
    Node                                   rootNode;         ///< シーングラフのルートノード
  };

  /// <summary>
  /// マテリアル構造体（GPU 用）
  /// シェーダーに送信される材質パラメータ
  /// </summary>
  struct Material
  {
    Vector4   color;              ///< マテリアルカラー（RGBA）
    int32_t   enableLighting;     ///< ライティング有効フラグ（0=無効, 1=有効）
    float     padding1[3];        ///< 16バイトアライメント用パディング
    Matrix4x4 uvTransform;        ///< UV 座標変換行列（テクスチャアニメーション用）
    float     shininess;          ///< 光沢度（スペキュラ反射の鋭さ）
    float     envMapCoefficient;  ///< 環境マップの影響度（0.0〜1.0）
    int32_t   enableHighlight;    ///< ハイライト有効フラグ
    int32_t   enableEnvMap;       ///< 環境マップ有効フラグ
  };

  /// <summary>
  /// キーフレーム構造体（テンプレート）
  /// アニメーションの特定時刻での値を保持
  /// </summary>
  template<typename tValue>
  struct KeyFrame
  {
    float  time;   ///< キーフレームの時刻（秒）
    tValue value;  ///< その時刻での値（Vector3, Quaternion など）
  };

  using KeyFrameVector3    = KeyFrame<Vector3>;     ///< 位置・スケール用キーフレーム
  using KeyFrameQuaternion = KeyFrame<Quaternion>;  ///< 回転用キーフレーム

  /// <summary>
  /// アニメーションカーブ構造体（テンプレート）
  /// 時系列キーフレームの集合（補間により中間値を計算）
  /// </summary>
  template<typename tValue>
  struct AnimationCurve
  {
    std::vector<KeyFrame<tValue>> keyFrames;  ///< 時系列順のキーフレーム配列
  };

  /// <summary>
  /// ノードアニメーション構造体
  /// 1つのノード/ボーンの移動・回転・拡大縮小アニメーション
  /// </summary>
  struct NodeAnimation
  {
    AnimationCurve<Vector3>    translate;  ///< 位置アニメーションカーブ
    AnimationCurve<Quaternion> rotate;     ///< 回転アニメーションカーブ
    AnimationCurve<Vector3>    scale;      ///< スケールアニメーションカーブ
  };

  /// <summary>
  /// アニメーション構造体
  /// 全ノード/ボーンのアニメーションデータを含むクリップ
  /// </summary>
  struct Animation
  {
    float                                duration;        ///< アニメーションの総再生時間（秒）
    std::map<std::string, NodeAnimation> nodeAnimations;  ///< ノード名から各ノードのアニメーションへのマップ
  };

  /// <summary>
  /// アニメーション遷移状態構造体
  /// 異なるアニメーション間をブレンドする際の中間トランスフォーム
  /// </summary>
  struct AnimationTransitionState {
    std::map<std::string, QuatTransform> nodeTransforms;   ///< ノード用のブレンド済みトランスフォーム
    std::map<std::string, QuatTransform> jointTransforms;  ///< ジョイント用のブレンド済みトランスフォーム
  };

  /// <summary>
  /// ジョイント（ボーン）構造体
  /// スケルトンを構成する1つの骨の情報
  /// </summary>
  struct Joint
  {
    QuatTransform          transform;            ///< ローカル座標でのトランスフォーム
    Matrix4x4              localMatrix;          ///< ローカル変換行列
    Matrix4x4              skeletonSpaceMatrix;  ///< スケルトン空間での変換行列（親からの累積）
    std::string            name;                 ///< ジョイント名（ウェイトデータとの関連付けに使用）
    std::vector<int32_t>   childrenIndex;        ///< 子ジョイントのインデックスリスト
    int32_t                index;                ///< このジョイント自身のインデックス
    std::optional<int32_t> parentIndex;          ///< 親ジョイントのインデックス
  };

  /// <summary>
  /// スケルトン構造体
  /// 全ジョイントを管理する骨格データ
  /// </summary>
  struct Skeleton
  {
    int32_t                        root;      ///< ルートジョイントのインデックス
    std::map<std::string, int32_t> jointMap;  ///< ジョイント名からインデックスへのマップ（高速検索用）
    std::vector<Joint>             joints;    ///< 全ジョイントの配列
  };

  const uint32_t MAX_INFLUENCE = 4;  ///< 1頂点に影響できる最大ジョイント数

  /// <summary>
  /// 頂点影響情報構造体（GPU 用）
  /// 各頂点がどのジョイントからどれだけ影響を受けるか
  /// </summary>
  struct VertexInfluence
  {
    std::array<float, MAX_INFLUENCE>    weights;       ///< 各ジョイントからの影響度（合計1.0）
    std::array<uint32_t, MAX_INFLUENCE> jointIndices;  ///< 影響するジョイントのインデックス
  };

  /// <summary>
  /// GPU 用スキニング行列構造体
  /// Compute Shader でのスキニング計算に使用
  /// </summary>
  struct WellForGPU
  {
    Matrix4x4 skeletonSpaceMat;                    ///< 頂点位置変換用スケルトン空間行列
    Matrix4x4 skeletonSpaceMatrixInvTransposeMat;  ///< 法線変換用逆転置行列
  };

  /// <summary>
  /// スキニング情報構造体
  /// Compute Shader 用のメタデータ
  /// </summary>
  struct SkinningInfo
  {
    uint32_t numVertices;  ///< スキニング対象の頂点数
  };

  /// <summary>
  /// スキンクラスター構造体
  /// GPU スキニングに必要な全リソースを管理
  /// </summary>
  struct SkinCluster
  {
    std::vector<Matrix4x4>                                              inverseBindMatrices;  ///< 各ジョイントの逆バインドポーズ行列
    Microsoft::WRL::ComPtr<ID3D12Resource>                              influenceResource;    ///< 頂点影響情報リソース（GPU 側）
    std::span<VertexInfluence>                                          mappedInfluences;     ///< 頂点影響情報の書き込み先ポインタ
    uint32_t                                                            influenceSrvIndex;    ///< 頂点影響情報の SRV インデックス
    Microsoft::WRL::ComPtr<ID3D12Resource>                              paletteResource;      ///< スキニング行列パレットリソース
    std::span<WellForGPU>                                               mappedPalette;        ///< パレットの書き込み先ポインタ
    uint32_t                                                            paletteSrvIndex;      ///< パレットの SRV インデックス
    std::pair<D3D12_CPU_DESCRIPTOR_HANDLE, D3D12_GPU_DESCRIPTOR_HANDLE> paletteSrvHandle;     ///< パレットのディスクリプタハンドル
  };

  /// <summary>
  /// メッシュスキンクラスターデータ構造体
  /// メッシュ単位のスキンクラスター情報
  /// </summary>
  struct MeshSkinClusterData {
    std::map<std::string, JointWeightData> skinClusterData;  ///< ジョイント名からウェイトデータへのマップ
  };

  /// <summary>
  /// インスタンシング描画用データ構造体
  /// 1つのインスタンスのトランスフォームとカラー情報
  /// </summary>
  struct InstanceData {
    Matrix4x4 world;              ///< ワールド変換行列
    Matrix4x4 worldInvTranspose;  ///< ワールド逆転置行列（法線変換用）
    Vector4   color;              ///< インスタンス固有のカラー
    float     padding[12];        ///< 16バイトアラインメント用パディング（定数バッファは256バイト境界）
  };

} // namespace Tako

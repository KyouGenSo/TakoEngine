#pragma once
#include <d3d12.h>
#include <wrl.h>
#include <memory>
#include <string>
#include <vector>
#include "vector2.h"
#include "vector3.h"
#include "vector4.h"
#include "Mat4x4Func.h"
#include "Transform.h"
#include "AnimationPlayer.h"
#include "DrawList.h"

namespace Tako {

class Model;
class Material;

/// <summary>
/// 3D オブジェクト基底クラス
/// Transform 管理とモデル描画機能
/// </summary>
class Object3d {

public: // 構造体
  // 座標変換行列データ（VP は Renderer が渡す視点 CB 側で掛ける）
  struct TransformationMatrix
  {
    Matrix4x4 world;
    Matrix4x4 worldInvTranspose;
  };

public: // メンバー関数

  /// <summary>
  ///　コンストラクタ
  /// </summary>
  Object3d();

  /// <summary>
  /// デストラクタ
  /// </summary>
  ~Object3d();

  /// <summary>
  /// 初期化
  /// </summary>
  void Initialize();

  /// <summary>
  /// 更新
  /// </summary>
  void Update();

  /// <summary>
  /// 描画リストへ登録する
  /// </summary>
  void Draw();

  /// <summary>
  /// デバッグ UI を表示
  /// </summary>
  void DrawImGui();

  /// <summary>
  /// 親 Object3d の特定の Joint にアタッチ
  /// </summary>
  /// <param name="parent">親となる Object3d</param>
  /// <param name="jointName">アタッチする Joint 名</param>
  /// <param name="offset">Joint からのオフセット</param>
  void AttachToJoint(Object3d* parent, const std::string& jointName, const Vector3& offset = Vector3(0.0f, 0.0f, 0.0f));

  /// <summary>
  /// 親 Joint からデタッチ
  /// </summary>
  void DetachFromJoint();

  //=======================================================
  //Setter
  //=======================================================
  /// <summary>
  /// モデルを設定
  /// </summary>
  /// <param name="fileName">モデルファイル名</param>
  void SetModel(const std::string& fileName);

  /// <summary>
  /// コード生成 Model を直接セット（PrimitiveBuilder 等）
  /// </summary>
  /// <param name="model">所有権を渡す Model（unique_ptr）</param>
  void SetModel(std::unique_ptr<Model> model);

  void SetTransform(const Transform& transform) { transform_ = transform; }
  void SetScale(const Vector3& scale) { transform_.scale = scale; }
  void SetRotate(const Vector3& rotate) { transform_.rotate = rotate; }
  void SetTranslate(const Vector3& translate) { transform_.translate = translate; }

  /// <summary>
  /// マテリアルカラーを設定
  /// </summary>
  /// <param name="color">マテリアルカラー（RGBA）</param>
  void SetMaterialColor(const Vector4& color);

  /// <summary>
  /// 半透明描画モードを設定 (両面描画 + 深度書き込み無効で、半透明キューの奥から順に描く)
  /// </summary>
  /// <param name="isTransparent">半透明描画モードにする場合 true</param>
  void SetTransparent(bool isTransparent) { isTransparent_ = isTransparent; }

  void SetCastShadow(bool castShadow) { castShadow_ = castShadow; }

  /// <summary>
  /// 同じ描画キューの中での描画順（小さいほど先）
  /// </summary>
  void SetSortingOrder(int32_t sortingOrder) { sortingOrder_ = sortingOrder; }

  /// <summary>
  /// 同じ maskId のポストエフェクトをこのオブジェクトの画素だけに掛ける（0 で解除、1〜255）
  /// </summary>
  void SetMaskId(uint32_t maskId) { maskId_ = maskId; }

  /// <summary>
  /// メッシュの描画に使うマテリアルを差し替える（nullptr で既定に戻す）
  /// </summary>
  /// <param name="meshIndex">モデル内のメッシュ番号</param>
  /// <param name="material">差し替えるマテリアル（複数の Object3d で共有してよい）</param>
  void SetMaterial(size_t meshIndex, std::shared_ptr<Material> material);

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
  /// 環境マップの係数を設定
  /// </summary>
  /// <param name="coefficient">環境マップ係数</param>
  void SetEnvMapCoefficient(float coefficient);

  /// <summary>
  /// 通常テクスチャを差し替える
  /// </summary>
  /// <param name="fileName">"resources/Texture/" 相対名、または "EngineResources/" プレフィックス付きパス</param>
  void SetTexture(const std::string& fileName);

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
  /// 環境マップの有効/無効を設定
  /// </summary>
  /// <param name="enableEnvMap">環境マップを有効にするか</param>
  void SetEnableEnvMap(bool enableEnvMap);

  /// <summary>
  /// メッシュ名を指定して表示・非表示を設定
  /// </summary>
  /// <param name="name">メッシュ名</param>
  /// <param name="visible">表示する場合 true</param>
  void SetMeshVisible(const std::string& name, bool visible);

  void SetAttachmentTranslate(const Vector3& offset) { attachmentOffset_.translate = offset; }
  void SetAttachmentRotate(const Vector3& offset) { attachmentOffset_.rotate = offset; }
  void SetAttachmentScale(const Vector3& offset) { attachmentOffset_.scale = offset; }
  void SetAttachmentTransform(const Transform& transform) { attachmentOffset_ = transform; }

  //=======================================================
  //Getter
  //=======================================================
  const Vector3& GetScale() const { return transform_.scale; }
  const Vector3& GetRotate() const { return transform_.rotate; }
  const Vector3& GetTranslate() const { return transform_.translate; }
  const Transform& GetTransform() const { return transform_; }

  Model* GetModel() const
  {
    return model_.get();
  }

  /// <summary>
  /// SetModel に渡したファイル名（Model を直接渡した場合は空）
  /// </summary>
  const std::string& GetModelName() const { return modelName_; }

#ifdef _DEBUG
  /// <summary>
  /// 生存中の Object3d（エディタでマテリアルを割り当てる先の一覧）
  /// </summary>
  static const std::vector<Object3d*>& GetLiveObjects();
#endif

  /// <summary>
  /// マテリアルカラーを取得
  /// </summary>
  /// <returns>マテリアルカラー（RGBA）</returns>
  Vector4 GetMaterialColor() const;

  bool IsTransparent() const { return isTransparent_; }
  bool CastsShadow() const { return castShadow_; }
  int32_t GetSortingOrder() const { return sortingOrder_; }

  /// <summary>
  /// メッシュ名を指定して表示状態を取得
  /// </summary>
  /// <param name="name">メッシュ名</param>
  /// <returns>表示中なら true</returns>
  bool IsMeshVisible(const std::string& name) const;

  /// <summary>
  /// モデルが持つ全メッシュ名のリストを取得
  /// </summary>
  /// <returns>メッシュ名のリスト（モデル未設定なら空）</returns>
  std::vector<std::string> GetMeshNames() const;

  /// <summary>
  /// 親 Joint にアタッチ済みか取得（親と Joint 名の両方が有効なら true）
  /// </summary>
  bool IsAttached() const { return parentObject_ != nullptr && !parentJointName_.empty(); }

  /// <summary>
  /// ワールド行列を取得
  /// </summary>
  Matrix4x4 GetWorldMatrix() const;

  /// <summary>
  /// トランスフォームと色に上乗せするアニメーション。target が空のトラックだけが効く
  /// </summary>
  AnimationPlayer& GetAnimator() { return animator_; }

private: // プライベートメンバー関数
  /// <summary>
  /// 座標変換行列データの生成
  /// </summary>
  void CreateTransformationMatrixData();

  /// <summary>
  /// 上乗せの色が変わったときだけ、元の色に掛けてモデルへ書き込む
  /// </summary>
  void ApplyAnimationColor(const Vector4& color);

private: // メンバー変数
  std::unique_ptr<Model> model_;
  std::string            modelName_;
  Transform              transform_;

  //バッファリソース
  Microsoft::WRL::ComPtr<ID3D12Resource> transformationMatResource_;

  Matrix4x4 finalWorldMatrix_;
  Matrix4x4 worldMatrix_;

  //バッファリソース内のデータを指すポインタ
  TransformationMatrix* transformationMatData_ = nullptr;

  //Joint アタッチメント用変数
  Object3d*   parentObject_       = nullptr;
  std::string parentJointName_;
  Transform   attachmentOffset_{};

  bool     isTransparent_ = false;  ///< 半透明描画モードフラグ
  bool     castShadow_    = true;
  int32_t  sortingOrder_  = 0;
  uint32_t maskId_        = 0;

  //アニメーションの上乗せ
  AnimationPlayer animator_;
  Vector4         animColor_          = { 1.0f, 1.0f, 1.0f, 1.0f };  ///< モデルへ書き込み済みの上乗せの色
  Vector4         baseColor_          = { 1.0f, 1.0f, 1.0f, 1.0f };  ///< 上乗せ前の色。animColor_ が白でない間だけ有効
  bool            isAnimationVisible_ = true;
};

} // namespace Tako

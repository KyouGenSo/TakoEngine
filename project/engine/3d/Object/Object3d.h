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

namespace Tako {

class Model;
class Camera;

/// <summary>
/// 3D オブジェクト基底クラス
/// Transform 管理とモデル描画機能
/// </summary>
class Object3d {

public: // 構造体
  // 座標変換行列データ
  struct TransformationMatrix
  {
    Matrix4x4 WVP;
    Matrix4x4 world;
    Matrix4x4 worldInvTranspose;
  };

  // Shader 用のカメラ
  struct CameraForGPU
  {
    Vector3 worldPos;
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
  /// 描画
  /// </summary>
  void Draw();

  /// <summary>
  /// デバッグ UI を表示
  /// </summary>
  void DrawImGui();

  //-----------------------------------------Getter-----------------------------------------//
  /// <summary>
  /// スケールを取得
  /// </summary>
  /// <returns>スケール値</returns>
  const Vector3& GetScale() const { return transform_.scale; }

  /// <summary>
  /// 回転を取得
  /// </summary>
  /// <returns>回転値（ラジアン）</returns>
  const Vector3& GetRotate() const { return transform_.rotate; }

  /// <summary>
  /// 座標を取得
  /// </summary>
  /// <returns>座標値</returns>
  const Vector3& GetTranslate() const { return transform_.translate; }

  /// <summary>
  /// トランスフォームを取得
  /// </summary>
  /// <returns>トランスフォーム情報</returns>
  const Transform& GetTransform() const { return transform_; }

  /// <summary>
  /// モデルのポインタを取得
  /// </summary>
  /// <returns>モデルポインタ</returns>
  Model* GetModel() const
  {
    return m_model_.get();
  }

  /// <summary>
  /// マテリアルカラーを取得
  /// </summary>
  /// <returns>マテリアルカラー（RGBA）</returns>
  Vector4 GetMaterialColor() const;

  //-----------------------------------------Setter-----------------------------------------//
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

  /// <summary>
  /// カメラを設定
  /// </summary>
  /// <param name="camera">カメラポインタのポインタ</param>
  void SetCamera(Camera** camera) { m_camera_ = camera; }

  /// <summary>
  /// トランスフォームを設定
  /// </summary>
  /// <param name="transform">トランスフォーム情報</param>
  void SetTransform(const Transform& transform) { transform_ = transform; }

  /// <summary>
  /// スケールを設定
  /// </summary>
  /// <param name="scale">スケール値</param>
  void SetScale(const Vector3& scale) { transform_.scale = scale; }

  /// <summary>
  /// 回転を設定
  /// </summary>
  /// <param name="rotate">回転値（ラジアン）</param>
  void SetRotate(const Vector3& rotate) { transform_.rotate = rotate; }

  /// <summary>
  /// 座標を設定
  /// </summary>
  /// <param name="translate">座標値</param>
  void SetTranslate(const Vector3& translate) { transform_.translate = translate; }

  /// <summary>
  /// マテリアルカラーを設定
  /// </summary>
  /// <param name="color">マテリアルカラー（RGBA）</param>
  void SetMaterialColor(const Vector4& color);

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

  // ライトの設定
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

  // Joint アタッチメント機能
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

  /// <summary>
  /// 親 Joint のアタッチメント状態を取得
  /// </summary>
  bool IsAttached() const { return parentObject_ != nullptr && !parentJointName_.empty(); }

  /// <summary>
  /// アタッチメントのオフセットを設定
  /// </summary>
  void SetAttachmentTranslate(const Vector3& offset) { attachmentOffset_.translate = offset; }
  void SetAttachmentRotate(const Vector3& offset) { attachmentOffset_.rotate = offset; }
  void SetAttachmentScale(const Vector3& offset) { attachmentOffset_.scale = offset; }
  void SetAttachmentTransform(const Transform& transform) { attachmentOffset_ = transform; }

  /// <summary>
  /// ワールド行列を取得
  /// </summary>
  Matrix4x4 GetWorldMatrix() const;

private: // プライベートメンバー関数
  /// <summary>
  /// 座標変換行列データの生成
  /// </summary>
  void CreateTransformationMatrixData();

  /// <summary>
  /// シェーダー用カメラデータの生成
  /// </summary>
  void CreateCameraForGPUData();

private: // メンバー変数
  // カメラのクラスポインター
  Camera** m_camera_ = nullptr;

  // モデルクラス
  std::unique_ptr<Model> m_model_;

  // トランスフォーム
  Transform transform_;

  // バッファリソース
  Microsoft::WRL::ComPtr<ID3D12Resource> transformationMatResource_;
  Microsoft::WRL::ComPtr<ID3D12Resource> cameraForGPUResource_;

  Matrix4x4 finalWorldMatrix_;
  Matrix4x4 worldMatrix_;

  // バッファリソース内のデータを指すポインタ
  TransformationMatrix* transformationMatData_ = nullptr;
  CameraForGPU* cameraForGPUData_ = nullptr;

  // Joint アタッチメント用変数
  Object3d* parentObject_ = nullptr; // 親となる Object3d
  std::string parentJointName_;      // アタッチする Joint 名
  Transform attachmentOffset_{};     // Joint からのオフセット
};

} // namespace Tako

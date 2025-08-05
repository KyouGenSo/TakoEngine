#pragma once
#include <d3d12.h>
#include<wrl.h>
#include <string>
#include <vector>
#include "vector2.h"
#include "vector3.h"
#include "vector4.h"
#include "Mat4x4Func.h"
#include "Transform.h"

class Model;

class Camera;

class Object3d {

public: // 構造体
	// 座標変換行列データ
	struct TransformationMatrix
	{
		Matrix4x4 WVP;
		Matrix4x4 world;
		Matrix4x4 worldInvTranspose;
	};

	// Shader用のカメラ
	struct CameraForGPU
	{
		Vector3 worldPos;
	};


public: // メンバー関数

  ~Object3d();

	///<summary>
	/// 初期化
	/// </summary>
	void Initialize();

	///<summary>
	/// 更新
	/// </summary>
	void Update();

	///<summary>
	/// 描画
	/// </summary>
	void Draw();

  /// <summary>
  /// デバッグUIを表示
  /// </summary>
  void DrawImGui();

	//-----------------------------------------Getter-----------------------------------------//
	const Vector3& GetScale() const { return transform_.scale; }
	const Vector3& GetRotate() const { return transform_.rotate; }
	const Vector3& GetTranslate() const { return transform_.translate; }
  const Transform& GetTransform() const { return transform_; }
  // モデルのポインタを取得
  Model* GetModel() const
	{
      return m_model_;
	}

	//-----------------------------------------Setter-----------------------------------------//
	void SetModel(const std::string& fileName);
	void SetCamera(Camera** camera) { m_camera_ = camera; }
  void SetTransform(const Transform& transform) { transform_ = transform; }
	void SetScale(const Vector3& scale) { transform_.scale = scale; }
	void SetRotate(const Vector3& rotate) { transform_.rotate = rotate; }
	void SetTranslate(const Vector3& translate) { transform_.translate = translate; }
  void SetMaterialColor(const Vector4& color);
  void SetUvTransform(const Transform& uvTransform);
  void SetEnvironmentTexture(uint32_t textureIndex);
  void SetEnvMapCoefficient(float coefficient);

	// ライトの設定
	void SetShininess(float shininess);
	void SetEnableLighting(bool enableLighting);
	void SetEnableHighlight(bool enableHighlight);
  void SetEnableEnvMap(bool enableEnvMap);

	// Jointアタッチメント機能
	/// <summary>
	/// 親Object3dの特定のJointにアタッチ
	/// </summary>
	/// <param name="parent">親となるObject3d</param>
	/// <param name="jointName">アタッチするJoint名</param>
	/// <param name="offset">Jointからのオフセット</param>
	void AttachToJoint(Object3d* parent, const std::string& jointName, const Vector3& offset = Vector3(0.0f, 0.0f, 0.0f));

	/// <summary>
	/// 親Jointからデタッチ
	/// </summary>
	void DetachFromJoint();

	/// <summary>
	/// 親Jointのアタッチメント状態を取得
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
	///<summary>
	///　座標変換行列データの生成
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
	Model* m_model_ = nullptr;

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

	// Jointアタッチメント用変数
	Object3d* parentObject_ = nullptr; // 親となるObject3d
	std::string parentJointName_;      // アタッチするJoint名
  Transform attachmentOffset_{};     // Jointからのオフセット
};

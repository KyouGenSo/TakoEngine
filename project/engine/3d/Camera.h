#pragma once
#include "Mat4x4Func.h"
#include "Transform.h"


class Camera
{
public: // メンバー関数

	/// <summary>
	/// コンストラクタ
	/// </summary>
	Camera();

	/// <summary>
	/// 更新
	/// </summary>
	void Update();

  /// <summary>
  /// プロジェクション行列の更新
  /// </summary>
  void UpdateProjectionMatrix();

	//-----------------------------------------Getter-----------------------------------------//
	[[nodiscard]] const Matrix4x4& GetWorldMatrix() const { return worldMatrix_; }
	[[nodiscard]] const Matrix4x4& GetViewMatrix() const { return viewMatrix_; }
	[[nodiscard]] const Matrix4x4& GetProjectionMatrix() const { return projectionMatrix_; }
	[[nodiscard]] const Matrix4x4& GetViewProjectionMatrix() const { return viewProjectionMatrix_; }
	[[nodiscard]] const Vector3& GetRotate() const { return transform_.rotate; }
  [[nodiscard]] float GetRotateX() const { return transform_.rotate.x; }
  [[nodiscard]] float GetRotateY() const { return transform_.rotate.y; }
  [[nodiscard]] float GetRotateZ() const { return transform_.rotate.z; }
	[[nodiscard]] const Vector3& GetTranslate() const { return transform_.translate; }
  [[nodiscard]] const Transform& GetTransform() const { return transform_; }
	[[nodiscard]] float GetFovY() const { return fovY_; }
	[[nodiscard]] float GetAspect() const { return aspect_; }
	[[nodiscard]] float GetNearClip() const { return nearZ_; }
	[[nodiscard]] float GetFarClip() const { return farZ_; }


	//-----------------------------------------Setter-----------------------------------------//
	void SetRotate(const Vector3& rotate) { transform_.rotate = rotate; }
	void SetTranslate(const Vector3& translate) { transform_.translate = translate; }
	void SetFovY(float fovY) { fovY_ = fovY; }
	void SetAspect(float aspect) { aspect_ = aspect; }
	void SetNearClip(float nearZ) { nearZ_ = nearZ; }
	void SetFarClip(float farZ) { farZ_ = farZ; }
	void SetViewProjectionMatrix(const Matrix4x4& viewProjectionMatrix) { viewProjectionMatrix_ = viewProjectionMatrix; }

private: // メンバー変数

	// トランスフォーム
	Transform transform_;

	// ワールド行列
	Matrix4x4 worldMatrix_;

	// ビュー行列
	Matrix4x4 viewMatrix_;

	// プロジェクション行列
	Matrix4x4 projectionMatrix_;
	float fovY_;
	float aspect_;
	float nearZ_;
	float farZ_;

	// ビュープロジェクション行列
	Matrix4x4 viewProjectionMatrix_;
};
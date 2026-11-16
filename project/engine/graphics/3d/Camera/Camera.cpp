#include"Camera.h"
#include "WinApp.h"
#include <algorithm>
#include <cmath>

#include "Vector4.h"

namespace Tako {

  Camera::Camera() :
    transform_({ Vector3(1.0f, 1.0f, 1.0f), Vector3(0.0f, 0.0f, 0.0f), Vector3(0.0f, 0.0f, 0.0f) }),
    fovY_(kDefaultFovY),
    aspect_(static_cast<float>(WinApp::clientWidth) / static_cast<float>(WinApp::clientHeight)),
    nearZ_(kDefaultNearClip),
    farZ_(kDefaultFarClip),
    worldMatrix_(Mat4x4::MakeAffine(transform_.scale, transform_.rotate, transform_.translate)),
    viewMatrix_(Mat4x4::Inverse(worldMatrix_)),
    projectionMatrix_(Mat4x4::MakePerspective(fovY_, aspect_, nearZ_, farZ_)),
    viewProjectionMatrix_(Mat4x4::Multiply(viewMatrix_, projectionMatrix_))
  {
  }


  void Camera::Update()
  {
    worldMatrix_ = Mat4x4::MakeAffine(transform_.scale, transform_.rotate, transform_.translate);

    viewMatrix_ = Mat4x4::Inverse(worldMatrix_);

    projectionMatrix_ = Mat4x4::MakePerspective(fovY_, aspect_, nearZ_, farZ_);

    // viewProjectionMatrix_ は Renderer::Update で合成するためここでは更新しない
    //viewProjectionMatrix_ = Mat4x4::Multiply(viewMatrix_, projectionMatrix_);
  }

  void Camera::UpdateProjectionMatrix()
  {
    float aspectRatio = static_cast<float>(WinApp::clientWidth) / static_cast<float>(WinApp::clientHeight);
    projectionMatrix_ = Mat4x4::MakePerspective(fovY_, aspectRatio, nearZ_, farZ_);
  }

  std::array<Vector3, 8> Camera::GetFrustumCorners() const
  {
    return GetFrustumCorners(nearZ_, farZ_);
  }

  std::array<Vector3, 8> Camera::GetFrustumCornersWithCustomFar(float customFar) const
  {
    // カスタム far クリップを使用（元の farZ_を超えないように制限）
    float limitedFar = min(customFar, farZ_);
    limitedFar = max(limitedFar, nearZ_);  // near より大きいことを保証

    return GetFrustumCorners(nearZ_, limitedFar);
  }

  std::array<Vector3, 8> Camera::GetFrustumCorners(float nearZ, float farZ) const
  {
    std::array<Vector3, 8> corners;

    // 手前の面と奥の面のサイズを計算
    float tanHalfFovY = std::tanf(fovY_ * 0.5f);
    float nearHeight = tanHalfFovY * nearZ * 2.0f;
    float nearWidth = nearHeight * aspect_;
    float farHeight = tanHalfFovY * farZ * 2.0f;
    float farWidth = farHeight * aspect_;

    // カメラのローカル座標系でのコーナー位置を計算
    // 手前の面の4つの頂点（カメラ空間）
    corners[0] = Vector3(-nearWidth * 0.5f, -nearHeight * 0.5f, nearZ); // 左下
    corners[1] = Vector3(nearWidth * 0.5f, -nearHeight * 0.5f, nearZ); // 右下
    corners[2] = Vector3(nearWidth * 0.5f, nearHeight * 0.5f, nearZ); // 右上
    corners[3] = Vector3(-nearWidth * 0.5f, nearHeight * 0.5f, nearZ); // 左上

    // 奥の面の4つの頂点（カメラ空間）
    corners[4] = Vector3(-farWidth * 0.5f, -farHeight * 0.5f, farZ); // 左下
    corners[5] = Vector3(farWidth * 0.5f, -farHeight * 0.5f, farZ); // 右下
    corners[6] = Vector3(farWidth * 0.5f, farHeight * 0.5f, farZ); // 右上
    corners[7] = Vector3(-farWidth * 0.5f, farHeight * 0.5f, farZ); // 左上

    // カメラ空間からワールド空間に変換
    for (int i = 0; i < 8; ++i) {
      corners[i] = Mat4x4::Transform(worldMatrix_, corners[i]);
    }

    return corners;
  }

  std::pair<Vector3, Vector3> Camera::GetFrustumBoundingBox(const Matrix4x4* viewMatrix) const
  {
    // 視錐台の8つの頂点を取得
    std::array<Vector3, 8> corners = GetFrustumCorners();

    // 指定されたビュー行列で変換（指定がある場合）
    if (viewMatrix) {
      for (int i = 0; i < 8; ++i) {
        corners[i] = Mat4x4::Transform(*viewMatrix, corners[i]);
      }
    }

    // 境界ボックスの最小・最大値を計算
    Vector3 minBounds = corners[0];
    Vector3 maxBounds = corners[0];

    for (int i = 1; i < 8; ++i) {
      minBounds.x = min(minBounds.x, corners[i].x);
      minBounds.y = min(minBounds.y, corners[i].y);
      minBounds.z = min(minBounds.z, corners[i].z);

      maxBounds.x = max(maxBounds.x, corners[i].x);
      maxBounds.y = max(maxBounds.y, corners[i].y);
      maxBounds.z = max(maxBounds.z, corners[i].z);
    }

    return std::make_pair(minBounds, maxBounds);
  }

  std::pair<Vector3, Vector3> Camera::GetFrustumBoundingBoxWithCustomFar(float customFar, const Matrix4x4* viewMatrix) const
  {
    // カスタム far クリップで視錐台の8つの頂点を取得
    std::array<Vector3, 8> corners = GetFrustumCornersWithCustomFar(customFar);

    // 指定されたビュー行列で変換（指定がある場合）
    if (viewMatrix) {
      for (int i = 0; i < 8; ++i) {
        corners[i] = Mat4x4::Transform(*viewMatrix, corners[i]);
      }
    }

    // 境界ボックスの最小・最大値を計算
    Vector3 minBounds = corners[0];
    Vector3 maxBounds = corners[0];

    for (int i = 1; i < 8; ++i) {
      minBounds.x = min(minBounds.x, corners[i].x);
      minBounds.y = min(minBounds.y, corners[i].y);
      minBounds.z = min(minBounds.z, corners[i].z);

      maxBounds.x = max(maxBounds.x, corners[i].x);
      maxBounds.y = max(maxBounds.y, corners[i].y);
      maxBounds.z = max(maxBounds.z, corners[i].z);
    }

    return std::make_pair(minBounds, maxBounds);
  }

} // namespace Tako

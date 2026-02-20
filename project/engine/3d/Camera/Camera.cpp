#include"Camera.h"
#include "WinApp.h"
#include <algorithm>
#include <cmath>

#include "Vector4.h"

namespace Tako {

  Camera::Camera() :
    transform_({ Vector3(1.0f, 1.0f, 1.0f), Vector3(0.0f, 0.0f, 0.0f), Vector3(0.0f, 0.0f, 0.0f) }),
    fovY_(0.45f),
    aspect_(static_cast<float>(WinApp::clientWidth) / static_cast<float>(WinApp::clientHeight)),
    nearZ_(0.1f),
    farZ_(1000.0f),
    worldMatrix_(Mat4x4::MakeAffine(transform_.scale, transform_.rotate, transform_.translate)),
    viewMatrix_(Mat4x4::Inverse(worldMatrix_)),
    projectionMatrix_(Mat4x4::MakePerspective(fovY_, aspect_, nearZ_, farZ_)),
    viewProjectionMatrix_(Mat4x4::Multiply(viewMatrix_, projectionMatrix_))
  {
  }


  void Camera::Update()
  {
    // トランスフォームでワールド行列を作る
    worldMatrix_ = Mat4x4::MakeAffine(transform_.scale, transform_.rotate, transform_.translate);

    // ビュー行列を作る
    viewMatrix_ = Mat4x4::Inverse(worldMatrix_);

    // プロジェクション行列を作る
    projectionMatrix_ = Mat4x4::MakePerspective(fovY_, aspect_, nearZ_, farZ_);

    // ビュープロジェクション行列を作る
    //viewProjectionMatrix_ = Mat4x4::Multiply(viewMatrix_, projectionMatrix_);
  }

  void Camera::UpdateProjectionMatrix()
  {
    float aspectRatio = static_cast<float>(WinApp::clientWidth) / static_cast<float>(WinApp::clientHeight);
    projectionMatrix_ = Mat4x4::MakePerspective(fovY_, aspectRatio, nearZ_, farZ_);
  }

  std::array<Vector3, 8> Camera::GetFrustumCorners() const
  {
    std::array<Vector3, 8> corners;

    // 視錐台の near 面と far 面のサイズを計算
    float tanHalfFovY = std::tanf(fovY_ * 0.5f);
    float nearHeight = tanHalfFovY * nearZ_ * 2.0f;
    float nearWidth = nearHeight * aspect_;
    float farHeight = tanHalfFovY * farZ_ * 2.0f;
    float farWidth = farHeight * aspect_;

    // カメラのローカル座標系でのコーナー位置を計算
    // Near 面の4つの頂点（カメラ空間）
    Vector3 nearCenter = Vector3(0, 0, nearZ_);
    corners[0] = Vector3(-nearWidth * 0.5f, -nearHeight * 0.5f, nearZ_); // 左下
    corners[1] = Vector3(nearWidth * 0.5f, -nearHeight * 0.5f, nearZ_); // 右下
    corners[2] = Vector3(nearWidth * 0.5f, nearHeight * 0.5f, nearZ_); // 右上
    corners[3] = Vector3(-nearWidth * 0.5f, nearHeight * 0.5f, nearZ_); // 左上

    // Far 面の4つの頂点（カメラ空間）
    Vector3 farCenter = Vector3(0, 0, farZ_);
    corners[4] = Vector3(-farWidth * 0.5f, -farHeight * 0.5f, farZ_); // 左下
    corners[5] = Vector3(farWidth * 0.5f, -farHeight * 0.5f, farZ_); // 右下
    corners[6] = Vector3(farWidth * 0.5f, farHeight * 0.5f, farZ_); // 右上
    corners[7] = Vector3(-farWidth * 0.5f, farHeight * 0.5f, farZ_); // 左上

    // カメラ空間からワールド空間に変換
    for (int i = 0; i < 8; ++i) {
      Vector3 corner4 = Vector3(corners[i].x, corners[i].y, corners[i].z);
      corner4 = Mat4x4::Transform(worldMatrix_, corner4);
      corners[i] = Vector3(corner4.x, corner4.y, corner4.z);
    }

    return corners;
  }

  std::array<Vector3, 8> Camera::GetFrustumCornersWithCustomFar(float customFar) const
  {
    std::array<Vector3, 8> corners;

    // カスタム far クリップを使用（元の farZ_を超えないように制限）
    float limitedFar = min(customFar, farZ_);
    limitedFar = max(limitedFar, nearZ_);  // near より大きいことを保証

    // 視錐台の near 面と far 面のサイズを計算
    float tanHalfFovY = std::tanf(fovY_ * 0.5f);
    float nearHeight = tanHalfFovY * nearZ_ * 2.0f;
    float nearWidth = nearHeight * aspect_;
    float farHeight = tanHalfFovY * limitedFar * 2.0f;  // カスタム far を使用
    float farWidth = farHeight * aspect_;

    // カメラのローカル座標系でのコーナー位置を計算
    // Near 面の4つの頂点（カメラ空間）
    corners[0] = Vector3(-nearWidth * 0.5f, -nearHeight * 0.5f, nearZ_); // 左下
    corners[1] = Vector3(nearWidth * 0.5f, -nearHeight * 0.5f, nearZ_); // 右下
    corners[2] = Vector3(nearWidth * 0.5f, nearHeight * 0.5f, nearZ_); // 右上
    corners[3] = Vector3(-nearWidth * 0.5f, nearHeight * 0.5f, nearZ_); // 左上

    // Far 面の4つの頂点（カメラ空間）- カスタム far を使用
    corners[4] = Vector3(-farWidth * 0.5f, -farHeight * 0.5f, limitedFar); // 左下
    corners[5] = Vector3(farWidth * 0.5f, -farHeight * 0.5f, limitedFar); // 右下
    corners[6] = Vector3(farWidth * 0.5f, farHeight * 0.5f, limitedFar); // 右上
    corners[7] = Vector3(-farWidth * 0.5f, farHeight * 0.5f, limitedFar); // 左上

    // カメラ空間からワールド空間に変換
    for (int i = 0; i < 8; ++i) {
      Vector3 corner4 = Vector3(corners[i].x, corners[i].y, corners[i].z);
      corner4 = Mat4x4::Transform(worldMatrix_, corner4);
      corners[i] = Vector3(corner4.x, corner4.y, corner4.z);
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
        Vector3 corner4 = Vector3(corners[i].x, corners[i].y, corners[i].z);
        corner4 = Mat4x4::Transform(*viewMatrix, corner4);
        corners[i] = Vector3(corner4.x, corner4.y, corner4.z);
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
        Vector3 corner4 = Vector3(corners[i].x, corners[i].y, corners[i].z);
        corner4 = Mat4x4::Transform(*viewMatrix, corner4);
        corners[i] = Vector3(corner4.x, corner4.y, corner4.z);
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

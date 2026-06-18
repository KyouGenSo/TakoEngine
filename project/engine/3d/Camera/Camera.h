#pragma once
#include "Mat4x4Func.h"
#include "Transform.h"
#include <array>

namespace Tako {

  /// <summary>
  /// カメラシステムクラス
  /// ビュー・プロジェクション行列管理
  /// </summary>
  class Camera
  {
  public: //メンバー関数

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

    //============================================================
    //Setter
    //============================================================
    void SetRotate(const Vector3& rotate) { transform_.rotate = rotate; }
    void SetTranslate(const Vector3& translate) { transform_.translate = translate; }
    void SetFovY(float fovY) { fovY_ = fovY; }
    void SetAspect(float aspect) { aspect_ = aspect; }
    void SetNearClip(float nearZ) { nearZ_ = nearZ; }
    void SetFarClip(float farZ) { farZ_ = farZ; }
    void SetViewProjectionMatrix(const Matrix4x4& viewProjectionMatrix) { viewProjectionMatrix_ = viewProjectionMatrix; }

    //============================================================
    //Getter
    //============================================================
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

    /// <summary>
    /// 視錐台の8つの頂点を取得（ワールド空間）
    /// </summary>
    /// <returns>視錐台の8頂点（0-3:near 面、4-7:far 面）</returns>
    [[nodiscard]] std::array<Vector3, 8> GetFrustumCorners() const;

    /// <summary>
    /// カスタム far クリップで視錐台の8つの頂点を取得（ワールド空間）
    /// </summary>
    /// <param name="customFar">カスタムの far クリップ距離</param>
    /// <returns>視錐台の8頂点（0-3:near 面、4-7:far 面）</returns>
    [[nodiscard]] std::array<Vector3, 8> GetFrustumCornersWithCustomFar(float customFar) const;

    /// <summary>
    /// 視錐台の境界ボックスを取得（min, max）
    /// </summary>
    /// <param name="viewMatrix">変換に使用するビュー行列（省略時はワールド空間）</param>
    /// <returns>first:最小座標、second:最大座標</returns>
    [[nodiscard]] std::pair<Vector3, Vector3> GetFrustumBoundingBox(const Matrix4x4* viewMatrix = nullptr) const;

    /// <summary>
    /// カスタム far クリップで視錐台の境界ボックスを取得（min, max）
    /// </summary>
    /// <param name="customFar">カスタムの far クリップ距離</param>
    /// <param name="viewMatrix">変換に使用するビュー行列（省略時はワールド空間）</param>
    /// <returns>first:最小座標、second:最大座標</returns>
    [[nodiscard]] std::pair<Vector3, Vector3> GetFrustumBoundingBoxWithCustomFar(float customFar, const Matrix4x4* viewMatrix = nullptr) const;

  private: //メンバー変数
    Transform transform_;
    Matrix4x4 worldMatrix_;
    Matrix4x4 viewMatrix_;
    Matrix4x4 projectionMatrix_;
    float     fovY_;
    float     aspect_;
    float     nearZ_;
    float     farZ_;
    Matrix4x4 viewProjectionMatrix_;
  };

} // namespace Tako
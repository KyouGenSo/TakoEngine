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
    /// <summary>
    /// ワールド行列を取得
    /// </summary>
    /// <returns>ワールド行列</returns>
    [[nodiscard]] const Matrix4x4& GetWorldMatrix() const { return worldMatrix_; }

    /// <summary>
    /// ビュー行列を取得
    /// </summary>
    /// <returns>ビュー行列</returns>
    [[nodiscard]] const Matrix4x4& GetViewMatrix() const { return viewMatrix_; }

    /// <summary>
    /// プロジェクション行列を取得
    /// </summary>
    /// <returns>プロジェクション行列</returns>
    [[nodiscard]] const Matrix4x4& GetProjectionMatrix() const { return projectionMatrix_; }

    /// <summary>
    /// ビュープロジェクション行列を取得
    /// </summary>
    /// <returns>ビュープロジェクション行列</returns>
    [[nodiscard]] const Matrix4x4& GetViewProjectionMatrix() const { return viewProjectionMatrix_; }

    /// <summary>
    /// 回転を取得
    /// </summary>
    /// <returns>回転値（ラジアン）</returns>
    [[nodiscard]] const Vector3& GetRotate() const { return transform_.rotate; }

    /// <summary>
    /// X 軸回転を取得
    /// </summary>
    /// <returns>X 軸回転値（ラジアン）</returns>
    [[nodiscard]] float GetRotateX() const { return transform_.rotate.x; }

    /// <summary>
    /// Y 軸回転を取得
    /// </summary>
    /// <returns>Y 軸回転値（ラジアン）</returns>
    [[nodiscard]] float GetRotateY() const { return transform_.rotate.y; }

    /// <summary>
    /// Z 軸回転を取得
    /// </summary>
    /// <returns>Z 軸回転値（ラジアン）</returns>
    [[nodiscard]] float GetRotateZ() const { return transform_.rotate.z; }

    /// <summary>
    /// 座標を取得
    /// </summary>
    /// <returns>座標値</returns>
    [[nodiscard]] const Vector3& GetTranslate() const { return transform_.translate; }

    /// <summary>
    /// トランスフォームを取得
    /// </summary>
    /// <returns>トランスフォーム情報</returns>
    [[nodiscard]] const Transform& GetTransform() const { return transform_; }

    /// <summary>
    /// 垂直視野角を取得
    /// </summary>
    /// <returns>垂直視野角（ラジアン）</returns>
    [[nodiscard]] float GetFovY() const { return fovY_; }

    /// <summary>
    /// アスペクト比を取得
    /// </summary>
    /// <returns>アスペクト比</returns>
    [[nodiscard]] float GetAspect() const { return aspect_; }

    /// <summary>
    /// ニアクリップ距離を取得
    /// </summary>
    /// <returns>ニアクリップ距離</returns>
    [[nodiscard]] float GetNearClip() const { return nearZ_; }

    /// <summary>
    /// ファークリップ距離を取得
    /// </summary>
    /// <returns>ファークリップ距離</returns>
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


    //-----------------------------------------Setter-----------------------------------------//
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
    /// 垂直視野角を設定
    /// </summary>
    /// <param name="fovY">垂直視野角（ラジアン）</param>
    void SetFovY(float fovY) { fovY_ = fovY; }

    /// <summary>
    /// アスペクト比を設定
    /// </summary>
    /// <param name="aspect">アスペクト比</param>
    void SetAspect(float aspect) { aspect_ = aspect; }

    /// <summary>
    /// ニアクリップ距離を設定
    /// </summary>
    /// <param name="nearZ">ニアクリップ距離</param>
    void SetNearClip(float nearZ) { nearZ_ = nearZ; }

    /// <summary>
    /// ファークリップ距離を設定
    /// </summary>
    /// <param name="farZ">ファークリップ距離</param>
    void SetFarClip(float farZ) { farZ_ = farZ; }

    /// <summary>
    /// ビュープロジェクション行列を設定
    /// </summary>
    /// <param name="viewProjectionMatrix">ビュープロジェクション行列</param>
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

} // namespace Tako
#pragma once
#include <memory>
#include "Mat4x4Func.h"
#include "Transform.h"

namespace Tako {

  /// <summary>
  /// デバッグ用の自由カメラクラス。キーボード・マウス操作による3D 空間の自由移動が可能
  /// </summary>
  class DebugCamera
  {
  private: // シングルトン設定

    ///< インスタンス
    static std::unique_ptr<DebugCamera> instance_;

    DebugCamera() = default;
    ~DebugCamera() = default;
    DebugCamera(DebugCamera&) = delete;
    DebugCamera& operator=(DebugCamera&) = delete;

    friend struct std::default_delete<DebugCamera>;

  public: // メンバー関数

    /// <summary>
    /// インスタンスの取得
    /// </summary>
    static DebugCamera* GetInstance();

    /// <summary>
    /// 初期化
    /// </summary>
    void Initialize();

    /// <summary>
    /// 終了処理
    /// </summary>
    void Finalize();

    /// <summary>
    /// カメラの更新
    /// </summary>
    void Update();

    /// <summary>
    /// 3D カメラの移動操作
    /// </summary>
    void Move();

    //-----------------------------------------Getter-----------------------------------------//
    /// <summary>
    /// ビュー行列の取得
    /// </summary>
    /// <returns>現在のビュー行列</returns>
    Matrix4x4 GetViewMat() const { return viewMat_; }

    /// <summary>
    /// 射影行列の取得
    /// </summary>
    /// <returns>現在の射影行列</returns>
    Matrix4x4 GetProjectionMat() const { return projectionMat_; }

    /// <summary>
    /// ビュー射影行列の取得
    /// </summary>
    /// <returns>現在のビュー射影行列</returns>
    Matrix4x4 GetViewProjectionMat() const { return viewProjectionMat_; }

    /// <summary>
    /// 回転の取得
    /// </summary>
    /// <returns>カメラの回転ベクトル</returns>
    Vector3 GetRotate() const { return transform_.rotate; }

    /// <summary>
    /// 位置の取得
    /// </summary>
    /// <returns>カメラの位置ベクトル</returns>
    Vector3 GetTranslate() const { return transform_.translate; }

    //-----------------------------------------Setter-----------------------------------------//
    /// <summary>
    /// ビュー行列の設定
    /// </summary>
    /// <param name="viewMatrix">設定するビュー行列</param>
    void SetViewMat(const Matrix4x4& viewMatrix) { viewMat_ = viewMatrix; }

    /// <summary>
    /// 射影行列の設定
    /// </summary>
    /// <param name="projectionMatrix">設定する射影行列</param>
    void SetProjectionMat(const Matrix4x4& projectionMatrix) { projectionMat_ = projectionMatrix; }

    /// <summary>
    /// ビュー射影行列の設定
    /// </summary>
    /// <param name="viewProjectionMatrix">設定するビュー射影行列</param>
    void SetViewProjectionMat(const Matrix4x4& viewProjectionMatrix) { viewProjectionMat_ = viewProjectionMatrix; }

    /// <summary>
    /// 回転の設定
    /// </summary>
    /// <param name="rotate">設定する回転ベクトル</param>
    void SetRotate(const Vector3& rotate) { transform_.rotate = rotate; }

    /// <summary>
    /// 位置の設定
    /// </summary>
    /// <param name="translate">設定する位置ベクトル</param>
    void SetTranslate(const Vector3& translate) { transform_.translate = translate; }

    /// <summary>
    /// 垂直視野角の設定
    /// </summary>
    /// <param name="fovY">設定する垂直視野角（ラジアン）</param>
    void SetFovY(float fovY) { fovY_ = fovY; }

    /// <summary>
    /// アスペクト比の設定
    /// </summary>
    /// <param name="aspect">設定するアスペクト比</param>
    void SetAspect(float aspect) { aspect_ = aspect; }

    /// <summary>
    /// ニアクリップ距離の設定
    /// </summary>
    /// <param name="nearZ">設定するニアクリップ距離</param>
    void SetNearClip(float nearZ) { nearZ_ = nearZ; }

    /// <summary>
    /// ファークリップ距離の設定
    /// </summary>
    /// <param name="farZ">設定するファークリップ距離</param>
    void SetFarClip(float farZ) { farZ_ = farZ; }

  private: // メンバー変数

    ///< トランスフォーム
    Transform transform_;

    ///< ワールド行列
    Matrix4x4 worldMat_;

    ///< 回転行列
    Matrix4x4 rotMat_;

    ///< ビュー行列
    Matrix4x4 viewMat_;

    ///< プロジェクション行列
    Matrix4x4 projectionMat_;
    float fovY_;       ///< 垂直視野角
    float aspect_;     ///< アスペクト比
    float nearZ_;      ///< ニアクリップ距離
    float farZ_;       ///< ファークリップ距離

    ///< ビュープロジェクション行列
    Matrix4x4 viewProjectionMat_;

    ///< カメラの移動速度（3D）
    float moveSpeed3D_ = 0.35f;

    ///< カメラの回転速度
    float rotateSpeed_ = 0.02f;
  };

} // namespace Tako
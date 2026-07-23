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

    static std::unique_ptr<DebugCamera> instance_;  ///< インスタンス

    struct Token {};  ///< 外部からの直接生成を防ぐ生成キー
    ~DebugCamera() = default;
    DebugCamera(DebugCamera&) = delete;
    DebugCamera& operator=(DebugCamera&) = delete;

    friend struct std::default_delete<DebugCamera>;

  public:
    explicit DebugCamera(Token) {}

  public: //メンバー関数

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

    //============================================================
    //Setter
    //============================================================
    void SetViewMat(const Matrix4x4& viewMatrix) { viewMat_ = viewMatrix; }
    void SetProjectionMat(const Matrix4x4& projectionMatrix) { projectionMat_ = projectionMatrix; }
    void SetViewProjectionMat(const Matrix4x4& viewProjectionMatrix) { viewProjectionMat_ = viewProjectionMatrix; }
    void SetRotate(const Vector3& rotate) { transform_.rotate = rotate; }
    void SetTranslate(const Vector3& translate) { transform_.translate = translate; }
    void SetFovY(float fovY) { fovY_ = fovY; }
    void SetAspect(float aspect) { aspect_ = aspect; }
    void SetNearClip(float nearZ) { nearZ_ = nearZ; }
    void SetFarClip(float farZ) { farZ_ = farZ; }

    //============================================================
    //Getter
    //============================================================
    Matrix4x4 GetViewMat() const { return viewMat_; }
    Matrix4x4 GetProjectionMat() const { return projectionMat_; }
    Matrix4x4 GetViewProjectionMat() const { return viewProjectionMat_; }
    Vector3 GetRotate() const { return transform_.rotate; }
    Vector3 GetTranslate() const { return transform_.translate; }

  private: //メンバー変数

    Transform transform_;  ///< トランスフォーム

    Matrix4x4 worldMat_;  ///< ワールド行列

    Matrix4x4 rotMat_;  ///< 回転行列

    Matrix4x4 viewMat_;  ///< ビュー行列

    //プロジェクション行列
    Matrix4x4 projectionMat_;
    float     fovY_;           ///< 垂直視野角
    float     aspect_;         ///< アスペクト比
    float     nearZ_;          ///< ニアクリップ距離
    float     farZ_;           ///< ファークリップ距離

    Matrix4x4 viewProjectionMat_;  ///< ビュープロジェクション行列

    float moveSpeed3D_ = 0.35f;  ///< カメラの移動速度（3D）

    float rotateSpeed_ = 0.02f;  ///< カメラの回転速度
  };

} // namespace Tako
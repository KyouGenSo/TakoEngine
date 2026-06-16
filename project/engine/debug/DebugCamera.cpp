#include "DebugCamera.h"
#include "WinApp.h"
#include"Input.h"
#include "Vector3.h"

namespace Tako {

  std::unique_ptr<DebugCamera> DebugCamera::instance_ = nullptr;

  DebugCamera* DebugCamera::GetInstance()
  {
    if (!instance_) {
      instance_ = std::unique_ptr<DebugCamera>(new DebugCamera());
    }
    return instance_.get();
  }

  void DebugCamera::Initialize()
  {
    transform_ = { Vector3(1.0f, 1.0f, 1.0f), Vector3(0.0f, 0.0f, 0.0f), Vector3(0.0f, 0.0f, -30.0f) };
    fovY_ = 0.45f;
    aspect_ = static_cast<float>(WinApp::clientWidth) / static_cast<float>(WinApp::clientHeight);
    nearZ_ = 0.1f;
    farZ_ = 1000.0f;
    worldMat_ = Mat4x4::MakeAffine(transform_.scale, transform_.rotate, transform_.translate);
    viewMat_ = Mat4x4::Inverse(worldMat_);
    projectionMat_ = Mat4x4::MakePerspective(fovY_, aspect_, nearZ_, farZ_);
    viewProjectionMat_ = Mat4x4::Multiply(viewMat_, projectionMat_);

    rotMat_ = Mat4x4::MakeRotateXYZ(transform_.rotate);
  }

  void DebugCamera::Finalize()
  {
    instance_.reset();
  }

  void DebugCamera::Update()
  {
    Move();

    // 更新された角度を元に回転行列を再生成する
    rotMat_ = Mat4x4::MakeRotateXYZ(transform_.rotate);

    Matrix4x4 transMat = Mat4x4::MakeTranslate(transform_.translate);

    //  rotMat と transMat でワールド行列を作る
    worldMat_ = Mat4x4::Multiply(rotMat_, transMat);

    // ビュー行列を作る
    viewMat_ = Mat4x4::Inverse(worldMat_);

    // プロジェクション行列を作る
    projectionMat_ = Mat4x4::MakePerspective(fovY_, aspect_, nearZ_, farZ_);

    // ビュープロジェクション行列を作る
    viewProjectionMat_ = Mat4x4::Multiply(viewMat_, projectionMat_);
  }

  void DebugCamera::Move()
  {
    // カメラの移動 (ローカル移動量を集計し、回転を一度だけ適用)
    Input* input = Input::GetInstance();
    Vector3 move = { 0.0f, 0.0f, 0.0f };
    if (input->PushKey(DIK_W))      move.z += moveSpeed3D_;
    if (input->PushKey(DIK_S))      move.z -= moveSpeed3D_;
    if (input->PushKey(DIK_A))      move.x -= moveSpeed3D_;
    if (input->PushKey(DIK_D))      move.x += moveSpeed3D_;
    if (input->PushKey(DIK_LSHIFT)) move.y -= moveSpeed3D_;
    if (input->PushKey(DIK_SPACE))  move.y += moveSpeed3D_;
    transform_.translate += Mat4x4::Transform(Mat4x4::MakeRotateXYZ(transform_.rotate), move);

    // カメラの回転
    if (Input::GetInstance()->PushKey(DIK_UP)) {
      transform_.rotate.x -= rotateSpeed_;
    }

    if (Input::GetInstance()->PushKey(DIK_DOWN)) {
      transform_.rotate.x += rotateSpeed_;
    }

    if (Input::GetInstance()->PushKey(DIK_LEFT)) {
      transform_.rotate.y -= rotateSpeed_;
    }

    if (Input::GetInstance()->PushKey(DIK_RIGHT)) {
      transform_.rotate.y += rotateSpeed_;
    }

  }

} // namespace Tako

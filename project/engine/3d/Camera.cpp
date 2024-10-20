#include"Camera.h"
#include "WinApp.h"

Camera::Camera() : 
	transform_({ Vector3(1.0f, 1.0f, 1.0f), Vector3(0.0f, 0.0f, 0.0f), Vector3(0.0f, 0.0f, 0.0f) }),
	fovY_(0.45f), 
	aspect_(float(WinApp::kClientWidth) / float(WinApp::kClientHeight)),
	nearZ_(0.1f), 
	farZ_(100.0f), 
	worldMatrix_(Mat4x4::MakeAffine(transform_.scale, transform_.rotate, transform_.translate)),
	viewMatrix_(Mat4x4::Inverse(worldMatrix_)),
	projectionMatrix_(Mat4x4::MakePerspective(fovY_, aspect_, nearZ_, farZ_)),
	viewProjectionMatrix_(Mat4x4::Multiply(viewMatrix_, projectionMatrix_))
{}


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

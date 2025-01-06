#include "collider.h"

void Collider::Initialize() {
	transform_.scale = Vector3(radius_, radius_, radius_);
}

void Collider::UpdateTransform() {
	// 中心座標を取得
	transform_.translate = GetCenter();

	transform_.scale = Vector3(radius_, radius_, radius_);
}

void Collider::Draw(Object3d* object3d) {
	object3d->SetTransform(transform_);
	object3d->Update();
	object3d->Draw();
}
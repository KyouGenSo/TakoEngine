#include "BaseCharacter.h"

void BaseCharacter::Initialize(const std::vector<Object3d*> models) {
	Collider::Initialize();

	m_models_ = models;

	transform_.scale = Vector3(1.0f, 1.0f, 1.0f);
	transform_.rotate = Vector3(0.0f, 0.0f, 0.0f);
	transform_.translate = Vector3(0.0f, 0.0f, 0.0f);
}

void BaseCharacter::Update() {
	for (auto& model : m_models_) {
		model->SetTransform(transform_);
		model->Update();
	}
}

void BaseCharacter::Draw() {
	for (auto& model : m_models_) {
		model->Draw();
	}
}

Vector3 BaseCharacter::GetCenter() const { 
	Vector3 worldPos;

	worldPos.x = transform_.translate.x;
	worldPos.y = transform_.translate.y;
	worldPos.z = transform_.translate.z;

	return worldPos;
}

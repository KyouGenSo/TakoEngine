#include"Skydome.h"

Skydome::Skydome() {}

Skydome::~Skydome() { }

void Skydome::Initialize(Object3d* model) {
	assert(model);
	model_ = model;

	transform_.translate = { 0.0f, 0.0f, 0.0f };
	transform_.scale = { 500.0f, 500.0f, 500.0f };
	transform_.rotate = { 0.0f, 0.0f, 0.0f };
}

void Skydome::Update() {
	model_->SetTransform(transform_);
	model_->Update();
}

void Skydome::Draw() {
	model_->Draw();
}
#include "ground.h"

Ground::Ground() {}

Ground::~Ground() {


}

void Ground::Initialize(Object3d* model) {
	assert(model);
	model_ = model;

	transform_.translate = { 0.0f, 0.0f, 0.0f };
	transform_.scale = { 1.0f, 1.0f, 1.0f };
	transform_.rotate = { 0.0f, 0.0f, 0.0f };
}

void Ground::Update() {
	model_->SetTransform(transform_);
	model_->Update();
}

void Ground::Draw() {
	model_->Draw();
}
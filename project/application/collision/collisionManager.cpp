#include "collisionManager.h"
#include "ImGuiManager.h"
#include "ModelManager.h"

void CollisionManager::Initialize() {
	// モデルの読み込み
	ModelManager::GetInstance()->LoadModel("collisionBall.obj");
	//collBallModel_ = std::make_unique<Object3d>();
	//collBallModel_->Initialize();
	//collBallModel_->SetModel("collisionBall.obj");
	//collBallModel_->SetAlpha(0.7f);
}

void CollisionManager::UpdateWorldTransform() {
	if (!collBallVisibility_) {
		return;
	}

	for (Collider* collider : colliders_) {
		collider->UpdateTransform();
	}

	for (Object3d* object3d : collBallModels_) {
		object3d->Update();
	}
}

void CollisionManager::Draw() {
	if (!collBallVisibility_) {
		return;
	}

	for (Collider* collider : colliders_) {
		Object3d* object3d = new Object3d();
		object3d->Initialize();
		object3d->SetModel("collisionBall.obj");
		object3d->SetAlpha(0.5f);
		collBallModels_.push_back(object3d);

		collider->Draw(object3d);
	}
}

void CollisionManager::Reset() { colliders_.clear(); }

void CollisionManager::CheckCollisionPair(Collider* colliderA, Collider* colliderB) {
	// 衝突判定
	Vector3 posA = colliderA->GetCenter();
	Vector3 posB = colliderB->GetCenter();
	Vector3 diff = posB - posA;
	float distance = diff.length();

	// 衝突しているかどうか
	if (distance < colliderA->GetRadius() + colliderB->GetRadius()) {
		colliderA->OnCollision(colliderB);
		colliderB->OnCollision(colliderA);
	}
}

void CollisionManager::CheckAllCollisions() {
	// すべてのコライダーのペアをチェック
	std::list<Collider*>::iterator itA = colliders_.begin();
	for (; itA != colliders_.end(); ++itA) {
		Collider* colliderA = *itA;

		std::list<Collider*>::iterator itB = itA;
		itB++;

		for (; itB != colliders_.end(); ++itB) {
			Collider* colliderB = *itB;

			CheckCollisionPair(colliderA, colliderB);
		}
	}
}

void CollisionManager::ImGuiDraw() {
#ifdef _DEBUG
	ImGui::Checkbox("Collision Ball Visibility", &collBallVisibility_);
#endif _DEBUG
}
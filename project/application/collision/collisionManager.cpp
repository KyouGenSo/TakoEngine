#include "collisionManager.h"
#include "ImGuiManager.h"
#include "ModelManager.h"

void CollisionManager::Initialize() {
	// モデルの読み込み
	//ModelManager::GetInstance()->LoadModel("collisionBall");
	//collBallModel_.get()->SetModel("collisionBall");
	//collBallModel_.get()->SetAlpha(0.7f);
}

void CollisionManager::UpdateWorldTransform() {
	if (!collBallVisibility_) {
		return;
	}

	for (Collider* collider : colliders_) {
		collider->UpdateTransform();
	}
}

void CollisionManager::Draw() {
	if (!collBallVisibility_) {
		return;
	}

	/*for (Collider* collider : colliders_) {
		collider->Draw(collBallModel_.get());
	}*/
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
#include "CollisionManager.h"
#include "AABBCollider.h"
#include "SphereCollider.h"
#include "OBBCollider.h"
#include "Draw2D.h"
#include <algorithm>
#include <cmath>
#include <unordered_map>

#ifdef _DEBUG
#include "ImGui.h"
#endif

CollisionManager* CollisionManager::instance_ = nullptr;

CollisionManager* CollisionManager::GetInstance() {
	if (!instance_) {
		instance_ = new CollisionManager();
	}
	return instance_;
}

void CollisionManager::Destroy() {
	delete instance_;
	instance_ = nullptr;
}

void CollisionManager::Initialize() {
	Reset();
}

void CollisionManager::Reset() {
	colliders_.clear();
	previousCollisions_.clear();
	currentCollisions_.clear();
}

void CollisionManager::CheckAllCollisions() {
  previousCollisions_ = currentCollisions_;
  currentCollisions_.clear();

  auto itA = colliders_.begin();
  for (; itA != colliders_.end(); ++itA) {
    Collider* colliderA = *itA;

    if (!colliderA || !colliderA->IsActive()) {
      continue;
    }

    auto itB = itA;
    ++itB;

    for (; itB != colliders_.end(); ++itB) {
      Collider* colliderB = *itB;

      if (!colliderB || !colliderB->IsActive()) {
        continue;
      }

      if (!CanCollide(colliderA->GetTypeID(), colliderB->GetTypeID())) {
        continue;
      }

      CheckCollisionPair(colliderA, colliderB);
    }
  }

  for (const auto& pair : currentCollisions_) {
    if (previousCollisions_.find(pair) == previousCollisions_.end()) {
      pair.first->OnCollisionEnter(pair.second);
      pair.second->OnCollisionEnter(pair.first);
    } else {
      pair.first->OnCollisionStay(pair.second);
      pair.second->OnCollisionStay(pair.first);
    }
  }

  for (const auto& pair : previousCollisions_) {
    if (currentCollisions_.find(pair) == currentCollisions_.end()) {
      pair.first->OnCollisionExit(pair.second);
      pair.second->OnCollisionExit(pair.first);
    }
  }
}

void CollisionManager::AddCollider(Collider* collider) {
	if (collider) {
		colliders_.push_back(collider);
	}
}

void CollisionManager::RemoveCollider(Collider* collider) {
	colliders_.remove(collider);
}

void CollisionManager::SetCollisionMask(uint32_t typeA, uint32_t typeB, bool canCollide) {
	if (canCollide) {
		collisionMask_[typeA].insert(typeB);
		collisionMask_[typeB].insert(typeA);
	} else {
		collisionMask_[typeA].erase(typeB);
		collisionMask_[typeB].erase(typeA);
	}
}

void CollisionManager::DrawColliders() {
  if (!debugDrawEnabled_) return;

  Draw2D* draw2D = Draw2D::GetInstance();
  if (!draw2D) return;

  // 色の定義（TypeIDに基づいたハッシュ色生成）
  auto GetColorByType = [](uint32_t typeID) -> Vector4 {
    // TypeIDを元に色相を計算（黄金比を使用して均等に分散）
    float hue = std::fmod(typeID * 0.618033988749895f, 1.0f) * 360.0f;

    // HSVからRGBへ変換（簡略版）
    float c = 0.7f;  // 彩度
    float x = c * (1.0f - std::abs(std::fmod(hue / 60.0f, 2.0f) - 1.0f));
    float m = 0.3f;  // 明度調整

    float r = 0.0f, g = 0.0f, b = 0.0f;
    if (hue < 60.0f) { r = c; g = x; b = 0.0f; } else if (hue < 120.0f) { r = x; g = c; b = 0.0f; } else if (hue < 180.0f) { r = 0.0f; g = c; b = x; } else if (hue < 240.0f) { r = 0.0f; g = x; b = c; } else if (hue < 300.0f) { r = x; g = 0.0f; b = c; } else { r = c; g = 0.0f; b = x; }

    return Vector4(r + m, g + m, b + m, 0.5f);
    };

  // すべてのコライダーを描画
  int drawCount = 0;
  for (Collider* collider : colliders_) {
    if (!collider || !collider->IsActive()) continue;

    Vector4 color = GetColorByType(collider->GetTypeID());

    // AABBColliderの場合
    if (AABBCollider* aabb = dynamic_cast<AABBCollider*>(collider)) {
      AABB box = aabb->GetAABB();
      draw2D->DrawAABB(box, color);
      drawCount++;
    }
    // SphereColliderの場合
    else if (SphereCollider* sphere = dynamic_cast<SphereCollider*>(collider)) {
      Vector3 center = sphere->GetCenter();
      float radius = sphere->GetRadius();
      draw2D->DrawSphere(center, radius, color);
      drawCount++;
    }
    // OBBColliderの場合
    else if (OBBCollider* obb = dynamic_cast<OBBCollider*>(collider)) {
      OBB obbData = obb->GetOBB();
      draw2D->DrawOBB(obbData, color);
      drawCount++;
    }
  }
}

void CollisionManager::DrawImGui() {
#ifdef _DEBUG
  ImGui::Begin("CollisionManager Debug");

  // 基本情報
  ImGui::Text("=== Collision System Status ===");
  ImGui::Text("Total Colliders: %zu", colliders_.size());

  // アクティブなコライダー数をカウント
  int activeCount = 0;
  std::unordered_map<uint32_t, int> typeCountMap;

  for (Collider* collider : colliders_) {
    if (collider && collider->IsActive()) {
      activeCount++;
      uint32_t typeID = collider->GetTypeID();
      typeCountMap[typeID]++;
    }
  }

  ImGui::Text("Active Colliders: %d", activeCount);
  for (const auto& [typeID, count] : typeCountMap) {
    ImGui::Text("  - Type %u: %d", typeID, count);
  }

  // 衝突マスク情報
  ImGui::Separator();
  ImGui::Text("=== Collision Masks ===");
  for (const auto& [typeA, typeBSet] : collisionMask_) {
    ImGui::Text("Type %u can collide with:", typeA);
    for (uint32_t typeB : typeBSet) {
      ImGui::Text("  - Type %u", typeB);
    }
  }

  // 現在の衝突情報
  ImGui::Separator();
  ImGui::Text("=== Current Collisions ===");
  ImGui::Text("Active Collision Pairs: %zu", currentCollisions_.size());

  // デバッグ描画設定
  ImGui::Separator();
  ImGui::Text("=== Debug Draw Settings ===");
  ImGui::Checkbox("Enable Debug Draw", &debugDrawEnabled_);

  // 各コライダーの詳細情報
  if (ImGui::CollapsingHeader("Collider Details")) {
    int index = 0;
    for (Collider* collider : colliders_) {
      if (!collider) continue;

      ImGui::PushID(index++);
      uint32_t typeID = collider->GetTypeID();

      ImGui::Text("Collider %d: TypeID=%u, Active=%s",
        index - 1, typeID,
        collider->IsActive() ? "Yes" : "No");

      // AABBColliderの場合
      if (AABBCollider* aabb = dynamic_cast<AABBCollider*>(collider)) {
        AABB box = aabb->GetAABB();
        ImGui::Text("  AABB: min(%.1f,%.1f,%.1f) max(%.1f,%.1f,%.1f)",
          box.min.x, box.min.y, box.min.z,
          box.max.x, box.max.y, box.max.z);
      }
      // SphereColliderの場合
      else if (SphereCollider* sphere = dynamic_cast<SphereCollider*>(collider)) {
        Vector3 center = sphere->GetCenter();
        ImGui::Text("  Sphere: center(%.1f,%.1f,%.1f) radius=%.1f",
          center.x, center.y, center.z, sphere->GetRadius());
      }
      // OBBColliderの場合
      else if (OBBCollider* obb = dynamic_cast<OBBCollider*>(collider)) {
        OBB obbData = obb->GetOBB();
        ImGui::Text("  OBB: center(%.1f,%.1f,%.1f) halfExtents(%.1f,%.1f,%.1f)",
          obbData.center.x, obbData.center.y, obbData.center.z,
          obbData.halfExtents.x, obbData.halfExtents.y, obbData.halfExtents.z);
      }

      ImGui::PopID();
    }
  }

  ImGui::End();
#endif
}

void CollisionManager::CheckCollisionPair(Collider* colliderA, Collider* colliderB) {
	bool isColliding = false;
	
	AABBCollider* aabbA = dynamic_cast<AABBCollider*>(colliderA);
	AABBCollider* aabbB = dynamic_cast<AABBCollider*>(colliderB);
	SphereCollider* sphereA = dynamic_cast<SphereCollider*>(colliderA);
	SphereCollider* sphereB = dynamic_cast<SphereCollider*>(colliderB);
	OBBCollider* obbA = dynamic_cast<OBBCollider*>(colliderA);
	OBBCollider* obbB = dynamic_cast<OBBCollider*>(colliderB);
	
	if (obbA && obbB) {
		isColliding = CheckOBBvsOBB(obbA, obbB);
	} else if (obbA && aabbB) {
		isColliding = CheckOBBvsAABB(obbA, aabbB);
	} else if (aabbA && obbB) {
		isColliding = CheckOBBvsAABB(obbB, aabbA);
	} else if (obbA && sphereB) {
		isColliding = CheckOBBvsSphere(obbA, sphereB);
	} else if (sphereA && obbB) {
		isColliding = CheckOBBvsSphere(obbB, sphereA);
	} else if (aabbA && aabbB) {
		isColliding = CheckAABBvsAABB(aabbA, aabbB);
	} else if (sphereA && sphereB) {
		isColliding = CheckSphereVsSphere(sphereA, sphereB);
	} else if (aabbA && sphereB) {
		isColliding = CheckAABBvsSphere(aabbA, sphereB);
	} else if (sphereA && aabbB) {
		isColliding = CheckAABBvsSphere(aabbB, sphereA);
	}
	
	if (isColliding) {
		currentCollisions_.insert(MakeOrderedPair(colliderA, colliderB));
		
		colliderA->OnCollision(colliderB);
		colliderB->OnCollision(colliderA);
	}
}

bool CollisionManager::CheckAABBvsAABB(AABBCollider* a, AABBCollider* b) {
	AABB aabbA = a->GetAABB();
	AABB aabbB = b->GetAABB();
	
	if (aabbA.min.x > aabbB.max.x || aabbB.min.x > aabbA.max.x) return false;
	if (aabbA.min.y > aabbB.max.y || aabbB.min.y > aabbA.max.y) return false;
	if (aabbA.min.z > aabbB.max.z || aabbB.min.z > aabbA.max.z) return false;
	
	return true;
}

bool CollisionManager::CheckSphereVsSphere(SphereCollider* a, SphereCollider* b) {
	Vector3 posA = a->GetCenter();
	Vector3 posB = b->GetCenter();
	Vector3 diff = posB - posA;
	float distance = diff.Length();
	
	return distance < (a->GetRadius() + b->GetRadius());
}

bool CollisionManager::CheckAABBvsSphere(AABBCollider* aabb, SphereCollider* sphere) {
	AABB box = aabb->GetAABB();
	Vector3 sphereCenter = sphere->GetCenter();
	float radius = sphere->GetRadius();
	
	float closestX = max(box.min.x, min(sphereCenter.x, box.max.x));
	float closestY = max(box.min.y, min(sphereCenter.y, box.max.y));
	float closestZ = max(box.min.z, min(sphereCenter.z, box.max.z));
	
	Vector3 closestPoint = { closestX, closestY, closestZ };
	Vector3 diff = sphereCenter - closestPoint;
	float distanceSquared = diff.x * diff.x + diff.y * diff.y + diff.z * diff.z;
	
	return distanceSquared < (radius * radius);
}

bool CollisionManager::CheckOBBvsOBB(OBBCollider* a, OBBCollider* b) {
	OBB obbA = a->GetOBB();
	OBB obbB = b->GetOBB();
	
	// 分離軸定理（SAT）を使用
	// 15個の軸をチェック（各OBBの3軸 + 9個のクロス積軸）
	
	// OBBの軸を取得
	Vector3 axesA[3] = { obbA.GetAxis(0), obbA.GetAxis(1), obbA.GetAxis(2) };
	Vector3 axesB[3] = { obbB.GetAxis(0), obbB.GetAxis(1), obbB.GetAxis(2) };
	
	// 中心間のベクトル
	Vector3 t = obbB.center - obbA.center;
	
	// 各OBBの3軸をチェック
	for (int i = 0; i < 3; ++i) {
		// A's axes
		float rA = obbA.halfExtents.x * std::abs(axesA[0].Dot(axesA[i])) +
		          obbA.halfExtents.y * std::abs(axesA[1].Dot(axesA[i])) +
		          obbA.halfExtents.z * std::abs(axesA[2].Dot(axesA[i]));
		float rB = obbB.halfExtents.x * std::abs(axesB[0].Dot(axesA[i])) +
		          obbB.halfExtents.y * std::abs(axesB[1].Dot(axesA[i])) +
		          obbB.halfExtents.z * std::abs(axesB[2].Dot(axesA[i]));
		if (std::abs(t.Dot(axesA[i])) > rA + rB) return false;
		
		// B's axes
		rA = obbA.halfExtents.x * std::abs(axesA[0].Dot(axesB[i])) +
		     obbA.halfExtents.y * std::abs(axesA[1].Dot(axesB[i])) +
		     obbA.halfExtents.z * std::abs(axesA[2].Dot(axesB[i]));
		rB = obbB.halfExtents.x * std::abs(axesB[0].Dot(axesB[i])) +
		     obbB.halfExtents.y * std::abs(axesB[1].Dot(axesB[i])) +
		     obbB.halfExtents.z * std::abs(axesB[2].Dot(axesB[i]));
		if (std::abs(t.Dot(axesB[i])) > rA + rB) return false;
	}
	
	// クロス積軸のチェック（9軸）
	for (int i = 0; i < 3; ++i) {
		for (int j = 0; j < 3; ++j) {
			// 軸のクロス積を計算
			Vector3 axis = axesA[i].Cross(axesB[j]);
			float axisLength = axis.Length();
			
			// 平行軸の場合（クロス積がゼロベクトル）はスキップ
			if (axisLength < 0.0001f) continue;
			
			// 軸を正規化
			axis = axis / axisLength;
			
			// 各OBBの頂点を軸に投影して半径を計算
			float rA = obbA.halfExtents.x * std::abs(axesA[0].Dot(axis)) +
			          obbA.halfExtents.y * std::abs(axesA[1].Dot(axis)) +
			          obbA.halfExtents.z * std::abs(axesA[2].Dot(axis));
			float rB = obbB.halfExtents.x * std::abs(axesB[0].Dot(axis)) +
			          obbB.halfExtents.y * std::abs(axesB[1].Dot(axis)) +
			          obbB.halfExtents.z * std::abs(axesB[2].Dot(axis));
			
			// 分離軸定理の判定
			if (std::abs(t.Dot(axis)) > rA + rB) return false;
		}
	}
	
	return true;
}

bool CollisionManager::CheckOBBvsAABB(OBBCollider* obb, AABBCollider* aabb) {
	OBB obbData = obb->GetOBB();
	AABB aabbData = aabb->GetAABB();
	
	// AABBをOBBとして扱う（回転なし）
	OBB aabbAsOBB;
	aabbAsOBB.center = (aabbData.min + aabbData.max) * 0.5f;
	aabbAsOBB.halfExtents = (aabbData.max - aabbData.min) * 0.5f;
	aabbAsOBB.orientation = Mat4x4::MakeIdentity();
	
	// OBB同士の判定として処理（簡略版）
	Vector3 axes[3] = { obbData.GetAxis(0), obbData.GetAxis(1), obbData.GetAxis(2) };
	Vector3 t = aabbAsOBB.center - obbData.center;
	
	// OBBの3軸でチェック
	for (int i = 0; i < 3; ++i) {
		float rOBB = obbData.halfExtents.x * std::abs(axes[0].Dot(axes[i])) +
		            obbData.halfExtents.y * std::abs(axes[1].Dot(axes[i])) +
		            obbData.halfExtents.z * std::abs(axes[2].Dot(axes[i]));
		float rAABB = aabbAsOBB.halfExtents.x * std::abs(axes[i].x) +
		             aabbAsOBB.halfExtents.y * std::abs(axes[i].y) +
		             aabbAsOBB.halfExtents.z * std::abs(axes[i].z);
		if (std::abs(t.Dot(axes[i])) > rOBB + rAABB) return false;
	}
	
	// AABBの3軸（ワールド軸）でチェック
	Vector3 worldAxes[3] = {
		Vector3(1.0f, 0.0f, 0.0f),
		Vector3(0.0f, 1.0f, 0.0f),
		Vector3(0.0f, 0.0f, 1.0f)
	};
	
	for (int i = 0; i < 3; ++i) {
		float rOBB = obbData.halfExtents.x * std::abs(axes[0].Dot(worldAxes[i])) +
		            obbData.halfExtents.y * std::abs(axes[1].Dot(worldAxes[i])) +
		            obbData.halfExtents.z * std::abs(axes[2].Dot(worldAxes[i]));
		float rAABB = aabbAsOBB.halfExtents.x * std::abs(worldAxes[i].x) +
		             aabbAsOBB.halfExtents.y * std::abs(worldAxes[i].y) +
		             aabbAsOBB.halfExtents.z * std::abs(worldAxes[i].z);
		if (std::abs(t.Dot(worldAxes[i])) > rOBB + rAABB) return false;
	}
	
	// クロス積軸のチェック（OBBの軸×ワールド軸の9軸）
	for (int i = 0; i < 3; ++i) {
		for (int j = 0; j < 3; ++j) {
			// OBBの軸とワールド軸のクロス積
			Vector3 axis = axes[i].Cross(worldAxes[j]);
			float axisLength = axis.Length();
			
			// 平行軸の場合（クロス積がゼロベクトル）はスキップ
			if (axisLength < 0.0001f) continue;
			
			// 軸を正規化
			axis = axis / axisLength;
			
			// OBBの投影半径を計算
			float rOBB = obbData.halfExtents.x * std::abs(axes[0].Dot(axis)) +
			            obbData.halfExtents.y * std::abs(axes[1].Dot(axis)) +
			            obbData.halfExtents.z * std::abs(axes[2].Dot(axis));
			
			// AABBの投影半径を計算
			float rAABB = aabbAsOBB.halfExtents.x * std::abs(axis.x) +
			             aabbAsOBB.halfExtents.y * std::abs(axis.y) +
			             aabbAsOBB.halfExtents.z * std::abs(axis.z);
			
			// 分離軸定理の判定
			if (std::abs(t.Dot(axis)) > rOBB + rAABB) return false;
		}
	}
	
	return true;
}

bool CollisionManager::CheckOBBvsSphere(OBBCollider* obb, SphereCollider* sphere) {
	OBB obbData = obb->GetOBB();
	Vector3 sphereCenter = sphere->GetCenter();
	float radius = sphere->GetRadius();
	
	// OBBのローカル座標系での球の中心を計算
	Vector3 localSphereCenter = sphereCenter - obbData.center;
	
	// 最近点を計算
	Vector3 closestPoint = obbData.center;
	Vector3 axes[3] = { obbData.GetAxis(0), obbData.GetAxis(1), obbData.GetAxis(2) };
	
	// halfExtentsを配列として扱う
	float halfExtents[3] = { obbData.halfExtents.x, obbData.halfExtents.y, obbData.halfExtents.z };
	
	for (int i = 0; i < 3; ++i) {
		float distance = localSphereCenter.Dot(axes[i]);
		
		// 軸に沿った範囲内にクランプ
		distance = min(distance, halfExtents[i]);
		distance = max(distance, -halfExtents[i]);
		
		closestPoint += axes[i] * distance;
	}
	
	// 最近点と球の中心の距離を計算
	Vector3 diff = sphereCenter - closestPoint;
	float distanceSquared = diff.LengthSquared();
	
	return distanceSquared < (radius * radius);
}

bool CollisionManager::CanCollide(uint32_t typeA, uint32_t typeB) {
  auto it = collisionMask_.find(typeA);
  if (it != collisionMask_.end()) {
    return it->second.find(typeB) != it->second.end();
  }
  return false;
}

CollisionManager::CollisionPair CollisionManager::MakeOrderedPair(Collider* a, Collider* b) {
  if (a < b) {
    return std::make_pair(a, b);
  }
  return std::make_pair(b, a);
}
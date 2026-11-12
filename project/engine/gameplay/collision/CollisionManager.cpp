#include "CollisionManager.h"
#include "AABBCollider.h"
#include "SphereCollider.h"
#include "OBBCollider.h"
#include "LineRenderer.h"
#include "ColorFunc.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <unordered_map>

#ifdef _DEBUG
#include "ImGuiManager.h"
#endif

namespace Tako {

  namespace {
    // 平行な辺の外積がほぼ 0 になる軸で、誤差による誤った分離判定を防ぐ
    constexpr float kParallelEpsilon = 1e-6f;
  }

  std::unique_ptr<CollisionManager> CollisionManager::instance_ = nullptr;

  CollisionManager* CollisionManager::GetInstance() {
    if (!instance_) {
      instance_ = std::make_unique<CollisionManager>(Token{});
    }
    return instance_.get();
  }

  void CollisionManager::Destroy() {
    instance_.reset();
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
    const auto start = std::chrono::steady_clock::now();

    std::swap(previousCollisions_, currentCollisions_);
    currentCollisions_.clear();

    for (std::vector<Proxy>& proxies : layerProxies_) {
      proxies.clear();
    }
    for (Collider* collider : colliders_) {
      if (collider && collider->IsActive() && collider->GetTypeID() < kMaxLayers) {
        layerProxies_[collider->GetTypeID()].push_back(MakeProxy(collider));
      }
    }

    // 当たる層の組だけを回す。同じ層同士は i < j の組だけ
    testedPairCount_ = 0;
    for (uint32_t typeA = 0; typeA < kMaxLayers; ++typeA) {
      const std::vector<Proxy>& proxiesA = layerProxies_[typeA];
      if (proxiesA.empty()) {
        continue;
      }
      for (uint32_t typeB = typeA; typeB < kMaxLayers; ++typeB) {
        if (!CanCollide(typeA, typeB)) {
          continue;
        }
        const std::vector<Proxy>& proxiesB = layerProxies_[typeB];
        for (size_t i = 0; i < proxiesA.size(); ++i) {
          for (size_t j = (typeA == typeB) ? i + 1 : 0; j < proxiesB.size(); ++j) {
            ++testedPairCount_;
            if (Intersects(proxiesA[i], proxiesB[j])) {
              currentCollisions_.insert(MakeOrderedPair(proxiesA[i].collider, proxiesB[j].collider));
            }
          }
        }
      }
    }

    lastCheckMs_ = std::chrono::duration<float, std::milli>(std::chrono::steady_clock::now() - start).count();

    for (const auto& pair : currentCollisions_) {
      if (previousCollisions_.find(pair) == previousCollisions_.end()) {
        pair.first->OnCollisionEnter(pair.second);
        pair.second->OnCollisionEnter(pair.first);
      }
      else {
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
    std::erase(colliders_, collider);

    // Enter/Exit 判定の整合性のため、衝突履歴からも該当ペアを除去
    auto currentIt = currentCollisions_.begin();
    while (currentIt != currentCollisions_.end()) {
      if (currentIt->first == collider || currentIt->second == collider) {
        currentIt = currentCollisions_.erase(currentIt);
      }
      else {
        ++currentIt;
      }
    }

    auto prevIt = previousCollisions_.begin();
    while (prevIt != previousCollisions_.end()) {
      if (prevIt->first == collider || prevIt->second == collider) {
        prevIt = previousCollisions_.erase(prevIt);
      }
      else {
        ++prevIt;
      }
    }
  }

  void CollisionManager::SetCollisionMask(uint32_t typeA, uint32_t typeB, bool canCollide) {
    if (typeA >= kMaxLayers || typeB >= kMaxLayers) {
      return;
    }
    const uint32_t bitA = 1u << typeA;
    const uint32_t bitB = 1u << typeB;
    if (canCollide) {
      collisionMask_[typeA] |= bitB;
      collisionMask_[typeB] |= bitA;
    }
    else {
      collisionMask_[typeA] &= ~bitB;
      collisionMask_[typeB] &= ~bitA;
    }
  }

  void CollisionManager::DrawColliders() {
    if (!debugDrawEnabled_) return;

    LineRenderer* lineRenderer = LineRenderer::GetInstance();
    if (!lineRenderer) return;

    auto GetColorByType = [](uint32_t typeID) -> Vector4 {
      // 黄金比でTypeIDを色相に分散させ、型ごとに色を散らす
      return Color::FromHSV(std::fmod(typeID * 0.618033988749895f, 1.0f), 0.7f, 1.0f, 0.5f);
      };

    for (Collider* collider : colliders_) {
      if (!collider || !collider->IsActive()) continue;

      Vector4 color = GetColorByType(collider->GetTypeID());

      switch (collider->GetShape()) {
      case ColliderShape::AABB:
        lineRenderer->DrawAABB(static_cast<AABBCollider*>(collider)->GetAABB(), color);
        break;
      case ColliderShape::Sphere: {
        SphereCollider* sphere = static_cast<SphereCollider*>(collider);
        lineRenderer->DrawSphere(sphere->GetCenter(), sphere->GetRadius(), color);
        break;
      }
      case ColliderShape::OBB:
        lineRenderer->DrawOBB(static_cast<OBBCollider*>(collider)->GetOBB(), color);
        break;
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
      ImGui::Text("  - %s: %d", GetLayerLabel(typeID).c_str(), count);
    }

    // 衝突マスク情報
    ImGui::Separator();
    ImGui::Text("=== Collision Masks ===");
    for (uint32_t typeA = 0; typeA < kMaxLayers; ++typeA) {
      if (collisionMask_[typeA] == 0) {
        continue;
      }
      ImGui::Text("%s can collide with:", GetLayerLabel(typeA).c_str());
      for (uint32_t typeB = 0; typeB < kMaxLayers; ++typeB) {
        if (CanCollide(typeA, typeB)) {
          ImGui::Text("  - %s", GetLayerLabel(typeB).c_str());
        }
      }
    }

    // 現在の衝突情報
    ImGui::Separator();
    ImGui::Text("=== Current Collisions ===");
    ImGui::Text("Active Collision Pairs: %zu", currentCollisions_.size());
    ImGui::Text("Tested Pairs: %zu (%.3f ms)", testedPairCount_, lastCheckMs_);

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

        ImGui::Text("Collider %d: %s, Active=%s",
          index - 1, GetLayerLabel(typeID).c_str(),
          collider->IsActive() ? "Yes" : "No");

        switch (collider->GetShape()) {
        case ColliderShape::AABB: {
          AABB box = static_cast<AABBCollider*>(collider)->GetAABB();
          ImGui::Text("  AABB: min(%.1f,%.1f,%.1f) max(%.1f,%.1f,%.1f)",
            box.min.x, box.min.y, box.min.z,
            box.max.x, box.max.y, box.max.z);
          break;
        }
        case ColliderShape::Sphere: {
          SphereCollider* sphere = static_cast<SphereCollider*>(collider);
          Vector3 center = sphere->GetCenter();
          ImGui::Text("  Sphere: center(%.1f,%.1f,%.1f) radius=%.1f",
            center.x, center.y, center.z, sphere->GetRadius());
          break;
        }
        case ColliderShape::OBB: {
          OBB obbData = static_cast<OBBCollider*>(collider)->GetOBB();
          ImGui::Text("  OBB: center(%.1f,%.1f,%.1f) halfExtents(%.1f,%.1f,%.1f)",
            obbData.center.x, obbData.center.y, obbData.center.z,
            obbData.halfExtents.x, obbData.halfExtents.y, obbData.halfExtents.z);
          break;
        }
        }

        ImGui::PopID();
      }
    }

    ImGui::End();
#endif
  }

  CollisionManager::Proxy CollisionManager::MakeProxy(Collider* collider) {
    Proxy proxy{};
    proxy.collider = collider;
    proxy.shape    = collider->GetShape();

    switch (proxy.shape) {
    case ColliderShape::AABB: {
      AABBCollider* aabb = static_cast<AABBCollider*>(collider);
      const Vector3 half = aabb->GetSize() * 0.5f;
      proxy.center  = aabb->GetCenter();
      proxy.half[0] = half.x;
      proxy.half[1] = half.y;
      proxy.half[2] = half.z;
      proxy.axis[0] = { 1.0f, 0.0f, 0.0f };
      proxy.axis[1] = { 0.0f, 1.0f, 0.0f };
      proxy.axis[2] = { 0.0f, 0.0f, 1.0f };
      break;
    }
    case ColliderShape::Sphere: {
      SphereCollider* sphere = static_cast<SphereCollider*>(collider);
      proxy.center = sphere->GetCenter();
      proxy.radius = sphere->GetRadius();
      break;
    }
    case ColliderShape::OBB: {
      const OBB obb = static_cast<OBBCollider*>(collider)->GetOBB();
      proxy.center  = obb.center;
      proxy.half[0] = obb.halfExtents.x;
      proxy.half[1] = obb.halfExtents.y;
      proxy.half[2] = obb.halfExtents.z;
      for (int i = 0; i < 3; ++i) {
        proxy.axis[i] = obb.GetAxis(i);
      }
      break;
    }
    }
    return proxy;
  }

  bool CollisionManager::Intersects(const Proxy& a, const Proxy& b) {
    const bool sphereA = a.shape == ColliderShape::Sphere;
    const bool sphereB = b.shape == ColliderShape::Sphere;
    if (sphereA && sphereB) return SphereVsSphere(a, b);
    if (sphereA) return BoxVsSphere(b, a);
    if (sphereB) return BoxVsSphere(a, b);
    return BoxVsBox(a, b);
  }

  bool CollisionManager::SphereVsSphere(const Proxy& a, const Proxy& b) {
    const Vector3 diff      = b.center - a.center;
    const float   radiusSum = a.radius + b.radius;
    return diff.LengthSquared() < radiusSum * radiusSum;
  }

  bool CollisionManager::BoxVsSphere(const Proxy& box, const Proxy& sphere) {
    // 箱の軸ごとに球の中心をクランプして最近点を求める（AABB は軸がワールド軸なだけで同じ式）
    const Vector3 local   = sphere.center - box.center;
    Vector3       closest = box.center;
    for (int i = 0; i < 3; ++i) {
      const float distance = std::clamp(local.Dot(box.axis[i]), -box.half[i], box.half[i]);
      closest += box.axis[i] * distance;
    }

    const Vector3 diff = sphere.center - closest;
    return diff.LengthSquared() < sphere.radius * sphere.radius;
  }

  bool CollisionManager::BoxVsBox(const Proxy& a, const Proxy& b) {
    const Vector3 d = b.center - a.center;

    if (a.shape == ColliderShape::AABB && b.shape == ColliderShape::AABB) {
      return std::abs(d.x) <= a.half[0] + b.half[0]
          && std::abs(d.y) <= a.half[1] + b.half[1]
          && std::abs(d.z) <= a.half[2] + b.half[2];
    }

    // 分離軸定理（Ericsson『Real-Time Collision Detection』4.4.1）。
    // B の軸を A の座標系で表した R を先に作り、15 軸とも正規化せずに投影半径を比べる
    float R[3][3];
    float absR[3][3];
    float t[3];
    for (int i = 0; i < 3; ++i) {
      t[i] = d.Dot(a.axis[i]);
      for (int j = 0; j < 3; ++j) {
        R[i][j]    = a.axis[i].Dot(b.axis[j]);
        absR[i][j] = std::abs(R[i][j]) + kParallelEpsilon;
      }
    }

    // A の 3 軸
    for (int i = 0; i < 3; ++i) {
      const float rb = b.half[0] * absR[i][0] + b.half[1] * absR[i][1] + b.half[2] * absR[i][2];
      if (std::abs(t[i]) > a.half[i] + rb) return false;
    }

    // B の 3 軸
    for (int j = 0; j < 3; ++j) {
      const float ra = a.half[0] * absR[0][j] + a.half[1] * absR[1][j] + a.half[2] * absR[2][j];
      if (std::abs(t[0] * R[0][j] + t[1] * R[1][j] + t[2] * R[2][j]) > ra + b.half[j]) return false;
    }

    // A の軸 i と B の軸 j の外積 9 軸
    for (int i = 0; i < 3; ++i) {
      const int i1 = (i + 1) % 3;
      const int i2 = (i + 2) % 3;
      for (int j = 0; j < 3; ++j) {
        const int   j1 = (j + 1) % 3;
        const int   j2 = (j + 2) % 3;
        const float ra = a.half[i1] * absR[i2][j] + a.half[i2] * absR[i1][j];
        const float rb = b.half[j1] * absR[i][j2] + b.half[j2] * absR[i][j1];
        if (std::abs(t[i2] * R[i1][j] - t[i1] * R[i2][j]) > ra + rb) return false;
      }
    }

    return true;
  }

  void CollisionManager::SetLayerNames(std::vector<std::string> names) {
    if (names.size() > kMaxLayers) {
      names.resize(kMaxLayers);
    }
    layerNames_ = std::move(names);
  }

  void CollisionManager::RemoveLayer(uint32_t index) {
    if (index >= layerNames_.size()) {
      return;
    }
    layerNames_.erase(layerNames_.begin() + index);
    RemapCollisionMasks([index](uint32_t type) -> std::optional<uint32_t> {
      if (type == index) {
        return std::nullopt;
      }
      return type > index ? type - 1 : type;
    });
  }

  void CollisionManager::SwapLayers(uint32_t typeA, uint32_t typeB) {
    if (typeA >= layerNames_.size() || typeB >= layerNames_.size()) {
      return;
    }
    std::swap(layerNames_[typeA], layerNames_[typeB]);
    RemapCollisionMasks([typeA, typeB](uint32_t type) -> std::optional<uint32_t> {
      return type == typeA ? typeB : type == typeB ? typeA : type;
    });
  }

  void CollisionManager::RemapCollisionMasks(const std::function<std::optional<uint32_t>(uint32_t)>& remap) {
    std::array<uint32_t, kMaxLayers> remapped{};
    for (uint32_t typeA = 0; typeA < kMaxLayers; ++typeA) {
      if (collisionMask_[typeA] == 0) {
        continue;
      }
      const std::optional<uint32_t> newA = remap(typeA);
      if (!newA || *newA >= kMaxLayers) {
        continue;
      }
      for (uint32_t typeB = 0; typeB < kMaxLayers; ++typeB) {
        if (!CanCollide(typeA, typeB)) {
          continue;
        }
        const std::optional<uint32_t> newB = remap(typeB);
        if (newB && *newB < kMaxLayers) {
          remapped[*newA] |= 1u << *newB;
        }
      }
    }
    collisionMask_ = remapped;
  }

  std::string CollisionManager::GetLayerLabel(uint32_t typeID) const {
    return typeID < layerNames_.size() ? layerNames_[typeID] : "Type " + std::to_string(typeID);
  }

  bool CollisionManager::CanCollide(uint32_t typeA, uint32_t typeB) const {
    return typeA < kMaxLayers && typeB < kMaxLayers && ((collisionMask_[typeA] >> typeB) & 1u) != 0;
  }

  CollisionManager::CollisionPair CollisionManager::MakeOrderedPair(Collider* a, Collider* b) {
    if (a < b) {
      return std::make_pair(a, b);
    }
    return std::make_pair(b, a);
  }

} // namespace Tako

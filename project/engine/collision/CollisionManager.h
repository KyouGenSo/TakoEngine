#pragma once
#include <list>
#include <unordered_map>
#include <unordered_set>
#include <memory>
#include <utility>
#include "Collider.h"

namespace Tako {

  class AABBCollider;
  class SphereCollider;
  class OBBCollider;

  /// <summary>
  /// 衝突判定を一元管理するシングルトンマネージャー。コライダーの登録、衝突検出、衝突マスク管理を行う
  /// </summary>
  class CollisionManager {
  private: //構造体
    using CollisionPair = std::pair<Collider*, Collider*>;
    struct PairHash {
      size_t operator()(const CollisionPair& p) const {
        auto h1 = std::hash<void*>{}(p.first);
        auto h2 = std::hash<void*>{}(p.second);
        return h1 ^ (h2 << 1);
      }
    };

  public: //メンバー関数
    CollisionManager(const CollisionManager&) = delete;
    CollisionManager& operator=(const CollisionManager&) = delete;

    /// <summary>
    /// シングルトンインスタンスを取得
    /// </summary>
    /// <returns>CollisionManager のインスタンス</returns>
    static CollisionManager* GetInstance();

    /// <summary>
    /// シングルトンインスタンスを破棄
    /// </summary>
    static void Destroy();

    /// <summary>
    /// 初期化処理
    /// </summary>
    void Initialize();

    /// <summary>
    /// 全コライダーをクリアしリセット
    /// </summary>
    void Reset();

    /// <summary>
    /// 登録された全コライダーの衝突判定を実行
    /// </summary>
    void CheckAllCollisions();

    /// <summary>
    /// コライダーを登録
    /// </summary>
    /// <param name="collider">登録するコライダー</param>
    void AddCollider(Collider* collider);

    /// <summary>
    /// コライダーの登録を解除
    /// </summary>
    /// <param name="collider">解除するコライダー</param>
    void RemoveCollider(Collider* collider);

    /// <summary>
    /// 全コライダーのデバッグ描画
    /// </summary>
    void DrawColliders();

    /// <summary>
    /// ImGui デバッグウィンドウの描画
    /// </summary>
    void DrawImGui();

    //============================================================
    //Setter
    //============================================================
    /// <summary>
    /// 指定した型同士の衝突判定を有効/無効化
    /// </summary>
    /// <param name="typeA">型 A</param>
    /// <param name="typeB">型 B</param>
    /// <param name="canCollide">衝突判定を行う場合 true</param>
    void SetCollisionMask(uint32_t typeA, uint32_t typeB, bool canCollide);

    void SetDebugDrawEnabled(bool enabled) { debugDrawEnabled_ = enabled; }

    //============================================================
    //Getter
    //============================================================
    bool IsDebugDrawEnabled() const { return debugDrawEnabled_; }
    size_t GetColliderCount() const { return colliders_.size(); }
    const std::list<Collider*>& GetColliders() const { return colliders_; }
    const std::unordered_map<uint32_t, std::unordered_set<uint32_t>>& GetCollisionMasks() const { return collisionMask_; }

  private: //非公開関数
    CollisionManager() = default;
    ~CollisionManager() = default;

    friend struct std::default_delete<CollisionManager>;

    // 形状を判別して対応する判定関数へ振り分け、衝突時のみ currentCollisions_ にペアを登録する
    void CheckCollisionPair(Collider* colliderA, Collider* colliderB);

    // 以下は交差有無のみを返す（接触点・貫通深度は算出しない）。true=交差
    bool CheckAABBvsAABB(AABBCollider* a, AABBCollider* b);
    bool CheckSphereVsSphere(SphereCollider* a, SphereCollider* b);
    bool CheckAABBvsSphere(AABBCollider* aabb, SphereCollider* sphere);
    bool CheckOBBvsOBB(OBBCollider* a, OBBCollider* b);
    bool CheckOBBvsAABB(OBBCollider* obb, AABBCollider* aabb);
    bool CheckOBBvsSphere(OBBCollider* obb, SphereCollider* sphere);

    bool CanCollide(uint32_t typeA, uint32_t typeB);
    CollisionPair MakeOrderedPair(Collider* a, Collider* b);

  private: //メンバー変数
    //登録コライダーと衝突マスク
    std::list<Collider*>                                       colliders_;
    std::unordered_map<uint32_t, std::unordered_set<uint32_t>> collisionMask_;

    //衝突ペアの前フレーム/現フレーム追跡
    std::unordered_set<CollisionPair, PairHash> previousCollisions_;
    std::unordered_set<CollisionPair, PairHash> currentCollisions_;

    static std::unique_ptr<CollisionManager> instance_;  ///< シングルトン

    bool debugDrawEnabled_ = false;  ///< デバッグ
  };

} // namespace Tako
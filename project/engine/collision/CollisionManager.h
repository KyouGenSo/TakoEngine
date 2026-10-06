#pragma once
#include <functional>
#include <list>
#include <unordered_map>
#include <unordered_set>
#include <memory>
#include <optional>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>
#include "Collider.h"

namespace Tako {

  class AABBCollider;
  class SphereCollider;
  class OBBCollider;

  /// <summary>
  /// 衝突判定を一元管理するシングルトンマネージャー。コライダーの登録、衝突検出、衝突マスク管理を行う
  /// </summary>
  class CollisionManager {
  public: //定数
    static constexpr size_t kMaxLayers = 32;  ///< 層名の上限（Unity / Godot と同じ）

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

    /// <summary>
    /// 全ての型の組み合わせを衝突しない状態に戻す
    /// </summary>
    void ClearCollisionMasks() { collisionMask_.clear(); }

    /// <summary>
    /// 層を削除し、後ろの層の名前と衝突マスクを 1 つ前へ詰める（コライダーに設定済みの型 ID は変わらない）
    /// </summary>
    void RemoveLayer(uint32_t index);

    /// <summary>
    /// 2 つの層の名前と衝突マスクを入れ替える（コライダーに設定済みの型 ID は変わらない）
    /// </summary>
    void SwapLayers(uint32_t typeA, uint32_t typeB);

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

    /// <summary>
    /// 層の enum をキャストせずに渡せる版
    /// </summary>
    template<class E> requires std::is_enum_v<E>
    void SetCollisionMask(E typeA, E typeB, bool canCollide) {
      SetCollisionMask(static_cast<uint32_t>(typeA), static_cast<uint32_t>(typeB), canCollide);
    }

    void SetDebugDrawEnabled(bool enabled) { debugDrawEnabled_ = enabled; }

    /// <summary>
    /// 型 ID ごとの表示名（添字 = 型 ID、kMaxLayers を超えた分は切り捨てる）
    /// </summary>
    void SetLayerNames(std::vector<std::string> names);

    //============================================================
    //Getter
    //============================================================
    bool IsDebugDrawEnabled() const { return debugDrawEnabled_; }
    size_t GetColliderCount() const { return colliders_.size(); }
    const std::list<Collider*>& GetColliders() const { return colliders_; }
    const std::unordered_map<uint32_t, std::unordered_set<uint32_t>>& GetCollisionMasks() const { return collisionMask_; }
    std::vector<std::string>& GetLayerNames() { return layerNames_; }

    /// <summary>
    /// 表示用の型名（層名があれば層名、無ければ "Type N"）
    /// </summary>
    std::string GetLayerLabel(uint32_t typeID) const;

    /// <summary>
    /// 型 A と型 B が衝突判定の対象か
    /// </summary>
    bool CanCollide(uint32_t typeA, uint32_t typeB) const;

  private: //非公開関数
    struct Token {};  ///< 外部からの直接生成を防ぐ生成キー
    ~CollisionManager() = default;

    friend struct std::default_delete<CollisionManager>;

  public:
    explicit CollisionManager(Token) {}

  private:
    // 形状を判別して対応する判定関数へ振り分け、衝突時のみ currentCollisions_ にペアを登録する
    void CheckCollisionPair(Collider* colliderA, Collider* colliderB);

    // 以下は交差有無のみを返す（接触点・貫通深度は算出しない）。true=交差
    bool CheckAABBvsAABB(AABBCollider* a, AABBCollider* b);
    bool CheckSphereVsSphere(SphereCollider* a, SphereCollider* b);
    bool CheckAABBvsSphere(AABBCollider* aabb, SphereCollider* sphere);
    bool CheckOBBvsOBB(OBBCollider* a, OBBCollider* b);
    bool CheckOBBvsAABB(OBBCollider* obb, AABBCollider* aabb);
    bool CheckOBBvsSphere(OBBCollider* obb, SphereCollider* sphere);

    CollisionPair MakeOrderedPair(Collider* a, Collider* b);

    // 衝突マスクの型 ID を remap で付け替える。nullopt を返した型 ID は組み合わせごと外す
    void RemapCollisionMasks(const std::function<std::optional<uint32_t>(uint32_t)>& remap);

  private: //メンバー変数
    //登録コライダーと衝突マスク
    std::list<Collider*>                                       colliders_;
    std::unordered_map<uint32_t, std::unordered_set<uint32_t>> collisionMask_;
    std::vector<std::string>                                   layerNames_;     ///< 添字 = 型 ID。意味付けはゲーム側の型 ID 定義に合わせる

    //衝突ペアの前フレーム/現フレーム追跡
    std::unordered_set<CollisionPair, PairHash> previousCollisions_;
    std::unordered_set<CollisionPair, PairHash> currentCollisions_;

    static std::unique_ptr<CollisionManager> instance_;  ///< シングルトン

    bool debugDrawEnabled_ = false;  ///< デバッグ
  };

} // namespace Tako
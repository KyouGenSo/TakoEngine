#pragma once
#include <array>
#include <cstdint>
#include <functional>
#include <unordered_set>
#include <memory>
#include <optional>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>
#include "Collider.h"

namespace Tako {

  /// <summary>
  /// 衝突判定を一元管理するシングルトンマネージャー。コライダーの登録、衝突検出、衝突マスク管理を行う
  /// </summary>
  class CollisionManager {
  public: //定数
    static constexpr size_t kMaxLayers = 32;  ///< 層名の上限（Unity / Godot と同じ）。層ごとの当たり表は uint32_t 1 個のビット

  private: //構造体
    using CollisionPair = std::pair<Collider*, Collider*>;
    struct PairHash {
      size_t operator()(const CollisionPair& p) const {
        auto h1 = std::hash<void*>{}(p.first);
        auto h2 = std::hash<void*>{}(p.second);
        return h1 ^ (h2 << 1);
      }
    };

    /// <summary>
    /// 1 フレーム分のワールド形状。AABB はワールド軸を向いた箱として OBB と同じ形で持つ
    /// </summary>
    struct Proxy {
      Collider*     collider;
      ColliderShape shape;
      Vector3       center;
      float         half[3];   ///< 箱の各軸の半サイズ
      Vector3       axis[3];   ///< 箱の正規化済みの軸（AABB はワールド軸）
      float         radius;    ///< 球の半径
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
    void ClearCollisionMasks() { collisionMask_.fill(0); }

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
    /// 指定した型同士の衝突判定を有効/無効化（kMaxLayers 以上の型 ID は無視する）
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
    const std::vector<Collider*>& GetColliders() const { return colliders_; }
    std::vector<std::string>& GetLayerNames() { return layerNames_; }
    size_t GetTestedPairCount() const { return testedPairCount_; }
    float GetLastCheckMs() const { return lastCheckMs_; }

    /// <summary>
    /// 添字 = 型 A、ビット b = 型 B と衝突する
    /// </summary>
    const std::array<uint32_t, kMaxLayers>& GetCollisionMasks() const { return collisionMask_; }

    /// <summary>
    /// 表示用の型名（層名があれば層名、無ければ "Type N"）
    /// </summary>
    std::string GetLayerLabel(uint32_t typeID) const;

    /// <summary>
    /// 型 A と型 B が衝突判定の対象か（kMaxLayers 以上の型 ID は常に false）
    /// </summary>
    bool CanCollide(uint32_t typeA, uint32_t typeB) const;

  private: //非公開関数
    struct Token {};  ///< 外部からの直接生成を防ぐ生成キー
    ~CollisionManager() = default;

    friend struct std::default_delete<CollisionManager>;

  public:
    explicit CollisionManager(Token) {}

  private:
    // コライダーのワールド形状を 1 回だけ計算して Proxy に詰める
    static Proxy MakeProxy(Collider* collider);

    // 以下は交差有無のみを返す（接触点・貫通深度は算出しない）。true=交差
    static bool Intersects(const Proxy& a, const Proxy& b);
    static bool SphereVsSphere(const Proxy& a, const Proxy& b);
    static bool BoxVsSphere(const Proxy& box, const Proxy& sphere);
    static bool BoxVsBox(const Proxy& a, const Proxy& b);

    CollisionPair MakeOrderedPair(Collider* a, Collider* b);

    // 衝突マスクの型 ID を remap で付け替える。nullopt を返した型 ID は組み合わせごと外す
    void RemapCollisionMasks(const std::function<std::optional<uint32_t>(uint32_t)>& remap);

  private: //メンバー変数
    //登録コライダーと衝突マスク
    std::vector<Collider*>           colliders_;
    std::array<uint32_t, kMaxLayers> collisionMask_{};  ///< 添字 = 型 A、ビット b = 型 B と衝突する（対称に保つ）
    std::vector<std::string>         layerNames_;       ///< 添字 = 型 ID。意味付けはゲーム側の型 ID 定義に合わせる

    //判定の作業領域。毎フレーム clear するだけで容量は残す
    std::array<std::vector<Proxy>, kMaxLayers> layerProxies_;  ///< 添字 = 型 ID。アクティブなコライダーだけ入る

    //衝突ペアの前フレーム/現フレーム追跡
    std::unordered_set<CollisionPair, PairHash> previousCollisions_;
    std::unordered_set<CollisionPair, PairHash> currentCollisions_;

    //計測（直近の CheckAllCollisions 1 回分）
    size_t testedPairCount_ = 0;     ///< 形状の交差判定まで進んだペア数
    float  lastCheckMs_     = 0.0f;  ///< 判定にかかった時間。Enter/Stay/Exit の通知は含まない

    static std::unique_ptr<CollisionManager> instance_;  ///< シングルトン

    bool debugDrawEnabled_ = false;  ///< デバッグ
  };

} // namespace Tako
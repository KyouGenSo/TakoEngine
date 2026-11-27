#pragma once
#include "Component.h"
#include "PhysicsWorld.h"
#include "Vector3.h"

#include <cstdint>
#include <memory>
#include <string>

namespace Tako {

  class RigidbodyComponent;

  /// <summary>
  /// Actor に付く当たりの形。Play 中だけ物理の世界に形を持ち、当たりを同じ Actor の全コンポーネントへ渡す。
  /// 自分か親の Actor に Rigidbody があれば、その剛体の形になる（Trigger は除く）。無ければ、動かない実体か Trigger になる
  /// </summary>
  TCLASS(DisplayName = "Collider")
  class TAKO_API ColliderComponent : public Component {
    GENERATED_BODY()

  public: //定数
    // 層を選んでいないコライダーの層
    static constexpr uint32_t kActorLayer = PhysicsWorld::kDefaultLayer;

  public: //構造体
    TENUM()
    enum class Shape { Sphere, Box, Capsule, Mesh, ConvexHull };

  public: //メンバー関数
    ColliderComponent();
    ~ColliderComponent() override;

    /// <summary>
    /// 形を物理の世界に入れる。他の BeginPlay から触れるよう、BeginPlay より前に行う
    /// </summary>
    void Awake() override;

    void EndPlay() override;

    void OnActiveChanged(bool active) override;

    void PostEditChange() override;

#ifdef TAKO_EDITOR
    /// <summary>
    /// Layer 欄
    /// </summary>
    bool DrawCustomImGui() override;

    /// <summary>
    /// 層を、Engine Settings の Physics で付けた名前から選ぶ欄。層を持つ他のコンポーネントも使う
    /// </summary>
    /// <returns>値が変わったか</returns>
    static bool DrawLayerCombo(uint32_t& layer);

    /// <summary>
    /// 形を線で描く（Sphere・Box・Capsule だけ）
    /// </summary>
    void DrawSelectionGizmo(const Vector3& cameraPosition) override;
#endif

    //=====================================
    //Setter
    //=====================================
    /// <summary>
    /// 当たり判定を一時的に止める・戻す（攻撃の間だけ当てるなど）。止めると相手に Exit が届き、戻して重なっていれば Enter が出直す
    /// </summary>
    void SetActive(bool active);

    /// <summary>
    /// true なら、重なりを知らせるだけで押し合わない
    /// </summary>
    void SetTrigger(bool isTrigger);

    void SetShape(Shape shape);

    /// <summary>
    /// Sphere と Capsule の半径
    /// </summary>
    void SetRadius(float radius);

    /// <summary>
    /// Capsule の全長（両端の半球を含む）
    /// </summary>
    void SetHeight(float height);

    /// <summary>
    /// Box の全長
    /// </summary>
    void SetSize(const Vector3& size);

    /// <summary>
    /// Actor からのずれ
    /// </summary>
    void SetOffset(const Vector3& offset);

    //=====================================
    //Getter
    //=====================================
    bool IsActive() const { return isActive_; }

    bool IsTrigger() const { return isTrigger_; }

    uint32_t GetLayer() const { return layer_; }

  private: //非公開関数
    friend class RigidbodyComponent;

    /// <summary>
    /// 所属先（Rigidbody か、自分のボディ）を決め直して、今の値を入れる
    /// </summary>
    void Refresh();

    void DestroyOwnBody();

    /// <summary>
    /// 自分か親の Actor の、Play 中の Rigidbody（近い方）
    /// </summary>
    RigidbodyComponent* FindRigidbody() const;

    /// <summary>
    /// 当たる状態か（SetActive と、Actor の Active の両方がオン）
    /// </summary>
    bool IsEnabled() const;

    PhysicsWorld::Shape MakeShape();

    PhysicsWorld::BodySettings MakeSettings() const;

    /// <summary>
    /// Mesh と ConvexHull の頂点を、Model の欄のモデル（空なら、同じ Actor の Mesh コンポーネントのモデルと Offset）から作る
    /// </summary>
    std::shared_ptr<const PhysicsWorld::MeshData> BuildMeshData() const;

  private: //メンバー変数
    TPROPERTY(Edit)
    Shape                           shape_       = Shape::Sphere;
    TPROPERTY(Edit, Tooltip = "オンなら、重なりを知らせるだけで押し合わない")
    bool                            isTrigger_   = false;
    TPROPERTY(Edit, Tooltip = "Sphere と Capsule の半径", ClampMin = 0, Speed = 0.05)
    float                           radius_      = 1.0f;
    TPROPERTY(Edit, Tooltip = "Capsule の全長（両端の半球を含む。軸は Y）", ClampMin = 0, Speed = 0.05)
    float                           height_      = 2.0f;
    TPROPERTY(Edit, Tooltip = "Box の全長", ClampMin = 0, Speed = 0.05)
    Vector3 size_{ 1.0f, 1.0f, 1.0f };
    TPROPERTY(Edit, Tooltip = "Mesh と Convex Hull の形にするモデル。空なら、同じ Actor の Mesh コンポーネントのモデル", Asset = "Model")
    std::string                     model_;
    TPROPERTY(Edit, Speed = 0.05)
    Vector3                         offset_{};
    TPROPERTY(Edit, Tooltip = "滑りにくさ（0 = 滑る）。Rigidbody の形のときは、最初の Collider の値が使われる", ClampMin = 0, Speed = 0.01)
    float                           friction_    = 0.5f;
    TPROPERTY(Edit, Tooltip = "跳ね返り（0 = 跳ねない、1 = 同じ速さで跳ね返る）", ClampMin = 0, ClampMax = 1, Speed = 0.01)
    float                           restitution_ = 0.0f;
    TPROPERTY()
    uint32_t                        layer_       = kActorLayer;

    std::shared_ptr<const PhysicsWorld::MeshData> meshData_;  ///< Mesh と ConvexHull の頂点。値を変えたら（PostEditChange）作り直す

    RigidbodyComponent*      rigidbody_     = nullptr;                     ///< 形を預けている Rigidbody。無ければ、body_ を自分で持つ
    PhysicsWorld::BodyHandle body_          = PhysicsWorld::kInvalidBody;
    bool                     isPlaying_     = false;                       ///< Awake から EndPlay までの間
    bool                     isActive_      = true;
    bool                     isBodyTrigger_ = false;                       ///< body_ を作ったときの isTrigger_（変わっていたら、ボディを作り直す）
  };

} // namespace Tako

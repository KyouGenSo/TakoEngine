#pragma once
#include "Component.h"
#include "Transform.h"
#include "Vector3.h"

#include <cstdint>
#include <memory>

namespace Tako {

  class Collider;

  /// <summary>
  /// Actor の Transform に付く当たり判定。Play 中だけ CollisionManager に登録し、当たりを同じ Actor の全コンポーネントへ渡す
  /// </summary>
  TCLASS()
  class ColliderComponent : public Component {
    GENERATED_BODY()

  public: //定数
    // ゲームの層と混ざらないよう、既定ではこの層どうしだけが当たる（ゲームのコライダーは層で GetOwner をキャストするため）
    static constexpr uint32_t kActorLayer = 31;

  public: //構造体
    TENUM()
    enum class Shape { Sphere, Box };

  public: //メンバー関数
    ColliderComponent();
    ~ColliderComponent() override;

    void BeginPlay() override;

    /// <summary>
    /// 親のある Actor では、ワールドの Transform を計算してコライダーに渡す
    /// </summary>
    void LateTick(float deltaTime) override;

    void EndPlay() override;

    void PostEditChange() override;

#ifdef _DEBUG
    /// <summary>
    /// Layer 欄（Engine Settings の Physics で付けた層の名前から選ぶ）
    /// </summary>
    bool DrawCustomImGui() override;
#endif

    /// <summary>
    /// コライダーからの当たりを、同じ Actor の全コンポーネントへ渡す
    /// </summary>
    void Notify(Collider* other, void (Component::* handler)(Actor*));

  private: //非公開関数
    /// <summary>
    /// 形の値をコライダーへ写す
    /// </summary>
    void ApplyShape();

  private: //メンバー変数
    TPROPERTY(Edit)
    Shape shape_ = Shape::Sphere;
    TPROPERTY(Edit, ClampMin = 0, Speed = 0.05)
    float radius_ = 1.0f;
    TPROPERTY(Edit, Tooltip = "Box の全長", ClampMin = 0, Speed = 0.05)
    Vector3 size_{ 1.0f, 1.0f, 1.0f };
    TPROPERTY(Edit, Speed = 0.05)
    Vector3 offset_{};
    TPROPERTY()
    uint32_t layer_ = kActorLayer;

    std::unique_ptr<Collider> collider_;
    Transform                 worldTransform_{};  ///< 親のある Actor のときだけ使う
  };

} // namespace Tako

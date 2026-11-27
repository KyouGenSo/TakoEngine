#pragma once
#include "Component.h"
#include "Matrix4x4.h"
#include "PhysicsWorld.h"
#include "Vector3.h"

#include <cstdint>
#include <vector>

namespace Tako {

  class ColliderComponent;

  /// <summary>
  /// Actor を剛体にする。形は、同じ Actor と、Rigidbody を持たない子孫の Actor の Collider（Trigger を除く）を 1 つにまとめた物。
  /// Dynamic は物理が Actor を動かし、Kinematic は Actor の動きに付いていって Dynamic を押す
  /// </summary>
  TCLASS(DisplayName = "Rigidbody", Order = -10)
  class TAKO_API RigidbodyComponent : public Component {
    GENERATED_BODY()

  public: //構造体
    TENUM()
    enum class BodyType { Dynamic, Kinematic };

  public: //メンバー関数
    RigidbodyComponent();
    ~RigidbodyComponent() override;

    /// <summary>
    /// ボディを作る。Order が Collider より小さいので、先に Awake する（Collider は、Awake でこのボディに形を預ける）
    /// </summary>
    void Awake() override;

    void EndPlay() override;

    void OnActiveChanged(bool active) override;

    void PostEditChange() override;

#ifdef TAKO_EDITOR
    /// <summary>
    /// 動きを止める軸のチェック
    /// </summary>
    bool DrawCustomImGui() override;
#endif

    /// <summary>
    /// 次の 1 ステップの間、力を掛ける（質量が大きいほど効きにくい）。掛け続けるには、FixedTick で毎回呼ぶ。Dynamic だけに効く（下の 2 つと、速度の Setter も同じ）
    /// </summary>
    void AddForce(const Vector3& force);

    /// <summary>
    /// 次の 1 ステップの間、回す力を掛ける（向きが軸、長さが強さ）
    /// </summary>
    void AddTorque(const Vector3& torque);

    /// <summary>
    /// 速度をその場で変える（質量が大きいほど効きにくい）。ジャンプや、弾が当たった衝撃に使う
    /// </summary>
    void AddImpulse(const Vector3& impulse);

    /// <summary>
    /// Collider を集め直して、形を作り直す。Collider が、自分の形や有効・無効が変わったときに呼ぶ
    /// </summary>
    void RebuildShapes();

    //=====================================
    //Setter
    //=====================================
    void SetBodyType(BodyType type);

    /// <summary>
    /// 動きを止める軸（PhysicsWorld::kLock〜 のビット）。全部を止めると、動かないボディになる
    /// </summary>
    void SetLockedAxes(uint32_t lockedAxes);

    void SetLinearVelocity(const Vector3& velocity);

    /// <summary>
    /// 向きが回転の軸、長さが速さ（ラジアン / 秒）
    /// </summary>
    void SetAngularVelocity(const Vector3& velocity);

    //=====================================
    //Getter
    //=====================================
    BodyType GetBodyType() const { return type_; }

    uint32_t GetLockedAxes() const { return lockedAxes_; }

    Vector3 GetLinearVelocity() const;

    Vector3 GetAngularVelocity() const;

    /// <summary>
    /// Awake から EndPlay までの間（Collider は、この間の Rigidbody にだけ形を預ける）
    /// </summary>
    bool IsPlaying() const { return isPlaying_; }

  private: //非公開関数
    /// <summary>
    /// actor と、Rigidbody を持たない子孫の Collider を colliders_ に集め、当たる状態の物の形を shapes に足す
    /// </summary>
    /// <param name="toBody">ワールドから、このボディの Actor のローカルへの行列</param>
    void CollectShapes(Actor& actor, const Matrix4x4& toBody, std::vector<PhysicsWorld::Shape>& shapes);

    PhysicsWorld::BodySettings MakeSettings() const;

  private: //メンバー変数
    TPROPERTY(Edit, Tooltip = "Dynamic は物理が動かす。Kinematic は Actor の動きに付いていき、Dynamic を押す")
    BodyType type_           = BodyType::Dynamic;
    TPROPERTY(Edit, ClampMin = 0.001, Speed = 0.05)
    float    mass_           = 1.0f;
    TPROPERTY(Edit, Tooltip = "動きの減り方（1 秒あたりに失う速さの割合）", ClampMin = 0, Speed = 0.01)
    float    linearDamping_  = 0.05f;
    TPROPERTY(Edit, Tooltip = "回転の減り方", ClampMin = 0, Speed = 0.01)
    float    angularDamping_ = 0.05f;
    TPROPERTY(Edit, Tooltip = "重力の倍率（0 = 重力を受けない）", Speed = 0.05)
    float    gravityScale_   = 1.0f;
    TPROPERTY(Edit, Tooltip = "速く動いても、実体を通り抜けない（計算は重くなる）")
    bool     isContinuous_   = false;
    TPROPERTY()
    uint32_t lockedAxes_     = 0;

    std::vector<ColliderComponent*> colliders_;                               ///< 形を預かっている Collider（当たらない状態の物も含む）
    PhysicsWorld::BodyHandle        body_      = PhysicsWorld::kInvalidBody;
    BodyType                        bodyType_  = BodyType::Dynamic;           ///< body_ を作ったときの type_（変わっていたら、ボディを作り直す）
    bool                            isPlaying_ = false;
  };

} // namespace Tako

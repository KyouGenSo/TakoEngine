#pragma once
#include "ActorRef.h"
#include "Component.h"
#include "PhysicsWorld.h"
#include "Vector3.h"

namespace Tako {

  /// <summary>
  /// 同じ Actor の Rigidbody を、他の Rigidbody かワールドにつなぐ（扉の蝶番、吊るした物、固定）。つないだどちらかの Actor がオフの間は、効かない
  /// </summary>
  TCLASS(DisplayName = "Joint", Requires = "RigidbodyComponent")
  class TAKO_API JointComponent : public Component {
    GENERATED_BODY()

  public: //構造体
    TENUM()
    enum class JointType {
      Fixed,     ///< 相手との位置と向きを、今のまま固める
      Point,     ///< 1 点でつなぐ（向きは自由）
      Hinge,     ///< 1 点でつなぎ、軸の周りだけ回る（蝶番）
      Distance,  ///< 2 点の距離を保つ（棒やロープ）
    };

  public: //メンバー関数
    JointComponent();
    ~JointComponent() override;

    /// <summary>
    /// つなぐ。両方の Rigidbody のボディができた後（全部の Awake の後）に行う
    /// </summary>
    void BeginPlay() override;

    void EndPlay() override;

    void PostEditChange() override;

    /// <summary>
    /// 自分のジョイントが Break Force で外れていたら、つなぎ直さない状態にする（次の Play まで）
    /// </summary>
    void OnJointBreak(float breakForce) override;

    //=====================================
    //Setter（Play 中に呼ぶと、今の位置でつなぎ直す）
    //=====================================
    void SetType(JointType type);

    /// <summary>
    /// つなぐ相手（Rigidbody を持つ Actor）。nullptr なら、ワールドの今の位置に留める
    /// </summary>
    void SetConnectedActor(const Actor* actor);

    /// <summary>
    /// つなぐ点（この Actor から見た位置）。Fixed は使わない
    /// </summary>
    void SetAnchor(const Vector3& anchor);

    /// <summary>
    /// Distance の、相手の側の点（相手の Actor から見た位置。相手が無ければワールドの位置）
    /// </summary>
    void SetConnectedAnchor(const Vector3& anchor);

    /// <summary>
    /// Hinge の軸（この Actor から見た向き）
    /// </summary>
    void SetAxis(const Vector3& axis);

    /// <summary>
    /// Hinge の回る範囲（ラジアン。つないだときの向きが 0 で、下限は -π〜0、上限は 0〜π）
    /// </summary>
    void SetAngleLimits(float minAngle, float maxAngle);

    /// <summary>
    /// Distance の範囲。負の値なら、つないだときの距離
    /// </summary>
    void SetDistanceLimits(float minDistance, float maxDistance);

    /// <summary>
    /// つないだ相手の Rigidbody とも当たるか（既定は、当たらない）
    /// </summary>
    void SetEnableCollision(bool enableCollision);

    /// <summary>
    /// Hinge の回りにくさ（N·m）。モーターが無いときに効く
    /// </summary>
    void SetFriction(float friction);

    /// <summary>
    /// Distance のばねの強さ（N/m。0 なら、範囲の端で固く止まる）と、揺れの収まり方（N·s/m）
    /// </summary>
    void SetSpring(float spring, float damper);

    /// <summary>
    /// つないだ点に、これより強い力（N）が掛かると外れる。0 なら外れない
    /// </summary>
    void SetBreakForce(float breakForce);

    /// <summary>
    /// Hinge のモーター：この Actor を、軸の周りに speed（ラジアン / 秒。向きは SetAngularVelocity と同じ）で回す。maxTorque（N·m）が 0 なら、モーターを止める。
    /// Play 中に呼んでも、つなぎ直さない
    /// </summary>
    void SetMotor(float speed, float maxTorque);

    //=====================================
    //Getter
    //=====================================
    /// <summary>
    /// Break Force より強い力で外れたか（次の Play で、またつながる）
    /// </summary>
    bool IsBroken() const { return isBroken_; }

  private: //非公開関数
    void CreateJoint();

    void DestroyJoint();

  private: //メンバー変数
    TPROPERTY(Edit)
    JointType                       type_              = JointType::Fixed;
    TPROPERTY(Edit, Tooltip = "つなぐ相手。空なら、ワールドの今の位置に留める", ActorWith = "RigidbodyComponent")
    ActorRef                        connectedBody_;
    TPROPERTY(Edit, Tooltip = "つなぐ点（この Actor から見た位置）。Fixed は使わない", Speed = 0.05, ShowIf = type_ != JointType::Fixed)
    Vector3                         anchor_{};
    TPROPERTY(Edit, Tooltip = "Distance の、相手の側の点（相手の Actor から見た位置。相手が無ければワールドの位置）", Speed = 0.05, ShowIf = type_ == JointType::Distance)
    Vector3                         connectedAnchor_{};
    TPROPERTY(Edit, Tooltip = "Hinge の軸（この Actor から見た向き）", Speed = 0.05, ShowIf = type_ == JointType::Hinge)
    Vector3 axis_{ 0.0f, 1.0f, 0.0f };
    TPROPERTY(Edit, Angle, Tooltip = "Hinge の回る範囲の下限（つないだときの向きが 0）", ClampMin = -180, ClampMax = 0, ShowIf = type_ == JointType::Hinge)
    float                           minAngle_          = -3.1415927f;
    TPROPERTY(Edit, Angle, Tooltip = "Hinge の回る範囲の上限（つないだときの向きが 0）", ClampMin = 0, ClampMax = 180, ShowIf = type_ == JointType::Hinge)
    float                           maxAngle_          = 3.1415927f;
    TPROPERTY(Edit, Tooltip = "Distance の一番短い距離。負の値なら、つないだときの距離", Speed = 0.05, ShowIf = type_ == JointType::Distance)
    float                           minDistance_       = -1.0f;
    TPROPERTY(Edit, Tooltip = "Distance の一番長い距離。負の値なら、つないだときの距離", Speed = 0.05, ShowIf = type_ == JointType::Distance)
    float                           maxDistance_       = -1.0f;
    TPROPERTY(Edit, Tooltip = "Distance のばねの強さ（N/m）。0 なら、範囲の端で固く止まる", ClampMin = 0, Speed = 1, ShowIf = type_ == JointType::Distance)
    float                           spring_            = 0.0f;
    TPROPERTY(Edit, Tooltip = "Distance のばねの、揺れの収まり方（N·s/m）", ClampMin = 0, Speed = 0.1, ShowIf = type_ == JointType::Distance)
    float                           damper_            = 0.0f;
    TPROPERTY(Edit, Angle, Tooltip = "Hinge のモーターが目指す、回る速さ（1 秒あたりの角度）。Motor Torque が 0 なら回らない", ShowIf = type_ == JointType::Hinge)
    float                           motorSpeed_        = 0.0f;
    TPROPERTY(Edit, Tooltip = "Hinge のモーターの力の上限（N·m）。0 なら、モーターなし", ClampMin = 0, Speed = 0.5, ShowIf = type_ == JointType::Hinge)
    float                           motorTorque_       = 0.0f;
    TPROPERTY(Edit, Tooltip = "Hinge の回りにくさ（N·m）。大きいほど早く止まる。モーターが無いときに効く", ClampMin = 0, Speed = 0.05, ShowIf = type_ == JointType::Hinge)
    float                           friction_          = 0.0f;
    TPROPERTY(Edit, Tooltip = "つないだ点に、これより強い力（N）が掛かると外れる。0 なら外れない", ClampMin = 0, Speed = 1)
    float                           breakForce_        = 0.0f;
    TPROPERTY(Edit, Tooltip = "オンなら、つないだ相手の Rigidbody とも当たる（オフなら、扉と枠のように重ねてつないでも押し合わない）")
    bool                            enableCollision_   = false;

    PhysicsWorld::JointHandle joint_     = PhysicsWorld::kInvalidJoint;
    bool                      isPlaying_ = false;
    bool                      isBroken_  = false;
  };

} // namespace Tako

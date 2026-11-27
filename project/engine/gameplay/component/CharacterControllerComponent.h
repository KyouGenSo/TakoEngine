#pragma once
#include "Component.h"
#include "PhysicsWorld.h"
#include "Vector3.h"

#include <cstdint>

namespace Tako {

  /// <summary>
  /// 壁と床に沿って動くキャラクター。形は、ワールドの Y 軸に立ったカプセル（Actor が回っても傾かない）。
  /// スクリプトが速度を決め、物理が重力・坂・段差・押し戻しを解いて Actor の位置へ書く。剛体ではないので、Rigidbody と一緒には付けない
  /// </summary>
  TCLASS(DisplayName = "Character Controller")
  class TAKO_API CharacterControllerComponent : public Component {
    GENERATED_BODY()

  public: //メンバー関数
    CharacterControllerComponent();
    ~CharacterControllerComponent() override;

    void Awake() override;

    void EndPlay() override;

    void OnActiveChanged(bool active) override;

    void PostEditChange() override;

#ifdef TAKO_EDITOR
    /// <summary>
    /// Layer 欄
    /// </summary>
    bool DrawCustomImGui() override;
#endif

    //=====================================
    //Setter
    //=====================================
    /// <summary>
    /// 動く速度（1 秒あたり）。重力は物理が毎ステップ足すので、歩くときは GetVelocity の y を残して x と z だけを入れ、跳ぶときは y に上向きの速さを入れる
    /// </summary>
    void SetVelocity(const Vector3& velocity);

    //=====================================
    //Getter
    //=====================================
    /// <summary>
    /// SetVelocity で入れた速度に、重力が足された値。立っている間は下向きの分が、天井に当たると上向きの分が 0 に戻る
    /// </summary>
    Vector3 GetVelocity() const;

    /// <summary>
    /// 立てる面（Max Slope Angle より緩い面）の上に居るか
    /// </summary>
    bool IsGrounded() const;

  private: //非公開関数
    void CreateCharacter();

    void DestroyCharacter();

  private: //メンバー変数
    TPROPERTY(Edit, ClampMin = 0, Speed = 0.05)
    float    radius_        = 0.5f;
    TPROPERTY(Edit, Tooltip = "カプセルの全長（両端の半球を含む）", ClampMin = 0, Speed = 0.05)
    float    height_        = 2.0f;
    TPROPERTY(Edit, Tooltip = "Actor から見た、カプセルの中心", Speed = 0.05)
    Vector3  offset_{};
    TPROPERTY(Edit, Angle, Tooltip = "登れる坂の角度。これより急な面は、壁として滑る", ClampMin = 0, ClampMax = 90)
    float    maxSlopeAngle_ = 0.8726646f;
    TPROPERTY(Edit, Tooltip = "乗り越えられる段差の高さ", ClampMin = 0, Speed = 0.01)
    float    stepHeight_    = 0.4f;
    TPROPERTY(Edit, Tooltip = "重力の倍率（0 = 重力を受けない）", Speed = 0.05)
    float    gravityScale_  = 1.0f;
    TPROPERTY()
    uint32_t layer_         = PhysicsWorld::kDefaultLayer;

    PhysicsWorld::CharacterHandle character_ = PhysicsWorld::kInvalidCharacter;
  };

} // namespace Tako

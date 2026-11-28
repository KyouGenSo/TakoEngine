#include "PhysicsWorld.h"

// Jolt のヘッダは、Windows.h を読むヘッダより先に置く（Windows.h の min / max マクロに当たる）
#include <Jolt/Jolt.h>

#include <Jolt/Core/Factory.h>
#include <Jolt/Core/JobSystemThreadPool.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Body/BodyLock.h>
#include <Jolt/Physics/Character/CharacterVirtual.h>
#include <Jolt/Physics/Collision/CastResult.h>
#include <Jolt/Physics/Collision/CollideShape.h>
#include <Jolt/Physics/Collision/CollisionCollectorImpl.h>
#include <Jolt/Physics/Collision/ContactListener.h>
#include <Jolt/Physics/Collision/EstimateCollisionResponse.h>
#include <Jolt/Physics/Collision/RayCast.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/CapsuleShape.h>
#include <Jolt/Physics/Collision/Shape/ConvexHullShape.h>
#include <Jolt/Physics/Collision/Shape/EmptyShape.h>
#include <Jolt/Physics/Collision/Shape/MeshShape.h>
#include <Jolt/Physics/Collision/Shape/OffsetCenterOfMassShape.h>
#include <Jolt/Physics/Collision/Shape/RotatedTranslatedShape.h>
#include <Jolt/Physics/Collision/Shape/ScaledShape.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>
#include <Jolt/Physics/Collision/Shape/StaticCompoundShape.h>
#include <Jolt/Physics/Collision/ShapeCast.h>
#include <Jolt/Physics/Constraints/DistanceConstraint.h>
#include <Jolt/Physics/Constraints/FixedConstraint.h>
#include <Jolt/Physics/Constraints/HingeConstraint.h>
#include <Jolt/Physics/Constraints/PointConstraint.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/RegisterTypes.h>
#ifdef JPH_DEBUG_RENDERER
#include <Jolt/Renderer/DebugRendererSimple.h>
#endif

#include "Actor.h"
#include "Component.h"
#include "Logger.h"
#include "Mat4x4Func.h"
#include "QuatFunc.h"
#ifdef JPH_DEBUG_RENDERER
#include "LineRenderer.h"
#endif

#include <algorithm>
#include <cfloat>
#include <chrono>
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <iterator>
#include <mutex>
#include <string>
#include <unordered_map>
#include <utility>

namespace Tako {

  namespace {

    using Clock = std::chrono::steady_clock;

    // 世界の上限。越えた分の接触は捨てられる（PhysicsSystem::Update がエラーを返す）。Jolt のサンプルと同じ値
    constexpr JPH::uint kMaxBodies             = 10240;
    constexpr JPH::uint kMaxBodyPairs          = 65536;
    constexpr JPH::uint kMaxContactConstraints = 20480;

    constexpr JPH::uint kTempAllocatorBytes = 10 * 1024 * 1024;  ///< 1 ステップの作業用。足りない分は malloc に回る
    constexpr float     kMinShapeSize       = 0.001f;            ///< 大きさ 0 の形は Jolt が扱えないので、ここまで切り上げる
    constexpr float     kMinMass            = 0.001f;            ///< 質量 0 の Dynamic は Jolt が扱えないので、ここまで切り上げる

    // lockedAxes のビットは、Jolt の「動ける軸」のビットと同じ並び
    static_assert(PhysicsWorld::kLockPositionX == static_cast<uint32_t>(JPH::EAllowedDOFs::TranslationX)
               && PhysicsWorld::kLockRotationZ == static_cast<uint32_t>(JPH::EAllowedDOFs::RotationZ));

    // ObjectLayer = 層の番号（0〜31）＋ 静止ボディの印。静止ボディは、動かない物だけの木（BroadPhaseLayer）に入れる
    constexpr JPH::ObjectLayer kStaticBit = 32;
    constexpr JPH::ObjectLayer kLayerBits = kStaticBit - 1;

    constexpr JPH::BroadPhaseLayer kStaticBroadPhase(0);
    constexpr JPH::BroadPhaseLayer kMovingBroadPhase(1);

    // 当たっている 2 つの通知先。ポインタの小さい方が first
    using OwnerPair = std::pair<Component*, Component*>;

    /// <summary>
    /// 当たっている組 1 つと、最後に調べた接触の値。一覧を並べる・比べるのは、組だけ
    /// </summary>
    struct Contact {
      OwnerPair pair;
      Vector3   point{};             ///< 触れた位置（ワールド。接触点の平均）
      Vector3   normal{};            ///< first の形から second の形へ向く向き
      Vector3   relativeVelocity{};  ///< second の速度 − first の速度（ぶつかる前）
      float     impulse = 0.0f;      ///< ぶつかりを解くのに掛かる力積の見積もり。当たり始めた接触にだけ入る
    };
    using ContactList = std::vector<Contact>;

    enum class ContactEvent : uint8_t { Enter, Stay, Exit };

    // 当たらない 2 つの Actor。ポインタの小さい方が first
    using ActorPair = std::pair<const Actor*, const Actor*>;

    ActorPair MakeActorPair(const Actor* a, const Actor* b) { return std::less<>{}(b, a) ? ActorPair{ b, a } : ActorPair{ a, b }; }

    /// <summary>
    /// 当たらない Actor の組。ステップの間は計算用のスレッドが読むので、書き換えるのはステップの外だけ
    /// </summary>
    struct IgnoredPairs {
      bool Contains(const Actor* a, const Actor* b) const {
        if (byRequest.empty() && byJoint.empty()) {
          return false;
        }
        const ActorPair pair = MakeActorPair(a, b);
        return std::ranges::find(byRequest, pair) != byRequest.end() || std::ranges::find(byJoint, pair) != byJoint.end();
      }

      std::vector<ActorPair> byRequest;  ///< IgnoreCollision の分（同じ組は 1 つ）
      std::vector<ActorPair> byJoint;    ///< Enable Collision がオフのジョイントの分（ジョイントごとに 1 つ。同じ組が 2 つ以上入る事がある）
    };

    JPH::Vec3 ToJolt(const Vector3& v) { return JPH::Vec3(v.x, v.y, v.z); }
    JPH::Quat ToJolt(const Quaternion& q) { return JPH::Quat(q.x, q.y, q.z, q.w); }
    Vector3 ToEngine(JPH::Vec3Arg v) { return { v.GetX(), v.GetY(), v.GetZ() }; }

    /// <summary>
    /// Vector3 の == は誤差を許すので、「前に写した値のまま」かはビットで比べる
    /// </summary>
    template<class T>
    bool HasSameBits(const T& a, const T& b) { return std::memcmp(&a, &b, sizeof(T)) == 0; }

    JPH::ObjectLayer ToObjectLayer(PhysicsWorld::BodyKind kind, uint32_t layer) {
      const uint32_t index = (std::min)(layer, static_cast<uint32_t>(PhysicsWorld::kMaxLayers - 1));
      return static_cast<JPH::ObjectLayer>(kind == PhysicsWorld::BodyKind::Static ? index | kStaticBit : index);
    }

    JPH::EAllowedDOFs ToAllowedDofs(uint32_t lockedAxes) {
      return static_cast<JPH::EAllowedDOFs>(~lockedAxes & static_cast<uint32_t>(JPH::EAllowedDOFs::All));
    }

    /// <summary>
    /// 世界に入れるとき・置き直すときに、起こすか。Static は起きず、Kinematic は動かしたときに MoveKinematic が起こす
    /// </summary>
    JPH::EActivation ActivationOf(PhysicsWorld::BodyKind kind) {
      const bool wakes = kind == PhysicsWorld::BodyKind::Trigger || kind == PhysicsWorld::BodyKind::Dynamic;
      return wakes ? JPH::EActivation::Activate : JPH::EActivation::DontActivate;
    }

    class BroadPhaseLayers final : public JPH::BroadPhaseLayerInterface {
    public: //メンバー関数
      JPH::uint GetNumBroadPhaseLayers() const override { return 2; }

      JPH::BroadPhaseLayer GetBroadPhaseLayer(JPH::ObjectLayer layer) const override {
        return (layer & kStaticBit) ? kStaticBroadPhase : kMovingBroadPhase;
      }
    };

    /// <summary>
    /// 静止ボディどうしは調べない
    /// </summary>
    class ObjectVsBroadPhaseFilter final : public JPH::ObjectVsBroadPhaseLayerFilter {
    public: //メンバー関数
      bool ShouldCollide(JPH::ObjectLayer layer, JPH::BroadPhaseLayer broadPhase) const override {
        return !(layer & kStaticBit) || broadPhase != kStaticBroadPhase;
      }
    };

    /// <summary>
    /// 層の当たり表で決める。ステップの間に計算用のスレッドから呼ばれる（その間、表は書き換えない）
    /// </summary>
    class ObjectLayerFilter final : public JPH::ObjectLayerPairFilter {
    public: //メンバー関数
      explicit ObjectLayerFilter(const PhysicsWorld& world) : world_(world) {}

      bool ShouldCollide(JPH::ObjectLayer a, JPH::ObjectLayer b) const override {
        return !(a & b & kStaticBit) && world_.CanCollide(a & kLayerBits, b & kLayerBits);
      }

    private: //メンバー変数
      const PhysicsWorld& world_;
    };

    /// <summary>
    /// ステップの間に触れていた組を集める。当たらない組の接触は、ここで捨てる。計算用のスレッドから同時に呼ばれ、ボディは読むだけ
    /// </summary>
    class ContactCollector final : public JPH::ContactListener {
    public: //メンバー関数
      explicit ContactCollector(const IgnoredPairs& ignored) : ignored_(ignored) {}

      JPH::ValidateResult OnContactValidate(const JPH::Body& a, const JPH::Body& b, JPH::RVec3Arg, const JPH::CollideShapeResult&) override {
        const bool isIgnored = ignored_.Contains(reinterpret_cast<const Actor*>(a.GetUserData()), reinterpret_cast<const Actor*>(b.GetUserData()));
        return isIgnored ? JPH::ValidateResult::RejectAllContactsForThisBodyPair : JPH::ValidateResult::AcceptAllContactsForThisBodyPair;
      }

      void OnContactAdded(const JPH::Body& a, const JPH::Body& b, const JPH::ContactManifold& manifold, JPH::ContactSettings& settings) override { Collect(a, b, manifold, &settings); }
      void OnContactPersisted(const JPH::Body& a, const JPH::Body& b, const JPH::ContactManifold& manifold, JPH::ContactSettings&) override { Collect(a, b, manifold, nullptr); }

    public: //メンバー変数
      ContactList contacts;  ///< ステップの外で、メインスレッドが読んで空にする

    private: //非公開関数
      /// <param name="added">当たり始めた接触なら、その設定（力積を見積もる）。続いている接触は nullptr</param>
      void Collect(const JPH::Body& a, const JPH::Body& b, const JPH::ContactManifold& manifold, const JPH::ContactSettings* added) {
        auto* ownerA = reinterpret_cast<Component*>(a.GetShape()->GetSubShapeUserData(manifold.mSubShapeID1));
        auto* ownerB = reinterpret_cast<Component*>(b.GetShape()->GetSubShapeUserData(manifold.mSubShapeID2));
        // 通知先の無い形と、同じ Actor の形どうしは知らせない
        if (!ownerA || !ownerB || ownerA->GetOwner() == ownerB->GetOwner()) {
          return;
        }

        Contact   contact;
        JPH::Vec3 sum = JPH::Vec3::sZero();
        for (const JPH::Vec3& point : manifold.mRelativeContactPointsOn1) {
          sum += point;
        }
        const float count = static_cast<float>((std::max)(manifold.mRelativeContactPointsOn1.size(), JPH::ContactPoints::size_type(1)));
        contact.pair             = { ownerA, ownerB };
        contact.point            = ToEngine(manifold.mBaseOffset + sum / count);
        contact.normal           = ToEngine(manifold.mWorldSpaceNormal);
        contact.relativeVelocity = ToEngine(b.GetLinearVelocity() - a.GetLinearVelocity());
        if (added && !added->mIsSensor) {
          JPH::CollisionEstimationResult estimate;
          JPH::EstimateCollisionResponse(a, b, manifold, estimate, *added);
          for (const float impulse : estimate.mContactImpulse) {
            contact.impulse += impulse;
          }
        }
        // 組は、ポインタの小さい方を first にする。入れ替えたら、向きも逆になる
        if (ownerB < ownerA) {
          contact.pair             = { ownerB, ownerA };
          contact.normal           = -contact.normal;
          contact.relativeVelocity = -contact.relativeVelocity;
        }
        const std::lock_guard lock(mutex_);
        contacts.push_back(contact);
      }

    private: //メンバー変数
      std::mutex          mutex_;
      const IgnoredPairs& ignored_;
    };

    /// <summary>
    /// レイを当てる層（ビット n = 層 n）
    /// </summary>
    class LayerMaskFilter final : public JPH::ObjectLayerFilter {
    public: //メンバー関数
      explicit LayerMaskFilter(uint32_t mask) : mask_(mask) {}

      bool ShouldCollide(JPH::ObjectLayer layer) const override { return ((mask_ >> (layer & kLayerBits)) & 1u) != 0; }

    private: //メンバー変数
      uint32_t mask_;
    };

    class TriggerBodyFilter final : public JPH::BodyFilter {
    public: //メンバー関数
      explicit TriggerBodyFilter(bool hitsTriggers) : hitsTriggers_(hitsTriggers) {}

      bool ShouldCollideLocked(const JPH::Body& body) const override { return hitsTriggers_ || !body.IsSensor(); }

    private: //メンバー変数
      bool hitsTriggers_;
    };

    /// <summary>
    /// 動かした形が最初に当たる所を取る。動かす前から重なっている形（進んだ割合が 0）は取らない（Unity と同じ）
    /// </summary>
    class FirstMovingHitCollector final : public JPH::CastShapeCollector {
    public: //メンバー関数
      void AddHit(const JPH::ShapeCastResult& result) override {
        if (result.mFraction > 0.0f && result.mFraction < GetEarlyOutFraction()) {
          hit    = result;
          hasHit = true;
          UpdateEarlyOutFraction(result.mFraction);
        }
      }

    public: //メンバー変数
      JPH::ShapeCastResult hit;
      bool                 hasHit = false;
    };

    /// <summary>
    /// キャラクターは、自分の Actor とその子孫に付いたボディには当たらない（一緒に動くボディに、押し出され続けないように）。当たらない組の相手にも当たらない
    /// </summary>
    class OtherActorFilter final : public JPH::BodyFilter {
    public: //メンバー関数
      OtherActorFilter(const Actor* actor, const IgnoredPairs& ignored) : actor_(actor), ignored_(ignored) {}

      bool ShouldCollideLocked(const JPH::Body& body) const override {
        const auto* bodyActor = reinterpret_cast<const Actor*>(body.GetUserData());
        if (ignored_.Contains(actor_, bodyActor)) {
          return false;
        }
        for (const Actor* owner = bodyActor; owner; owner = owner->GetParent()) {
          if (owner == actor_) {
            return false;
          }
        }
        return true;
      }

    private: //メンバー変数
      const Actor*        actor_;
      const IgnoredPairs& ignored_;
    };

#ifdef JPH_DEBUG_RENDERER
    /// <summary>
    /// Jolt のデバッグ描画を、エンジンの線で描く
    /// </summary>
    class LineDebugRenderer final : public JPH::DebugRendererSimple {
    public: //メンバー関数
      void DrawLine(JPH::RVec3Arg from, JPH::RVec3Arg to, JPH::ColorArg color) override {
        LineRenderer::GetInstance()->DrawLine(ToEngine(from), ToEngine(to), Color(color.r / 255.0f, color.g / 255.0f, color.b / 255.0f, color.a / 255.0f));
      }

      void DrawText3D(JPH::RVec3Arg, const std::string_view&, JPH::ColorArg, float) override {}
    };
#endif

    struct Pose {
      Vector3    position;
      Quaternion rotation;
      Vector3    scale;
    };

    /// <summary>
    /// 親のある Actor の scale は行列から取り出すので、親が回るだけで下の桁が揺れる。その分は同じとみなす
    /// </summary>
    bool IsSameScale(const Vector3& a, const Vector3& b) {
      const auto isClose = [](float x, float y) { return std::abs(x - y) <= 1.0e-4f * (std::max)(1.0f, std::abs(x)); };
      return isClose(a.x, b.x) && isClose(a.y, b.y) && isClose(a.z, b.z);
    }

    /// <summary>
    /// 世界に作ったボディ 1 つ
    /// </summary>
    struct BodyRecord {
      JPH::BodyID                id;
      PhysicsWorld::BodySettings settings;          ///< kind と actor は、作った後は変わらない
      bool                       isInWorld = true;
      std::vector<Component*>    owners;            ///< 形の通知先（重複なし）

      //Actor の scale が変わったら、形を作り直す
      std::vector<PhysicsWorld::Shape> shapes;
      Vector3                          shapeScale{ 1.0f, 1.0f, 1.0f };  ///< 今の形を作ったときの scale

      //Actor の Transform の控え。同じなら、ボディへ写さない
      bool      hasPose   = false;
      bool      hasParent = false;
      Transform lastLocal{};  ///< 親の無い Actor：最後に写した Transform。Dynamic：最後に Actor へ書いた Transform（違っていれば、外から書き換えられた）
      Matrix4x4 lastWorld{};  ///< 親のある Actor：最後に写したワールド行列

      //Dynamic：直前の 2 ステップの姿勢（その間を補間して Actor へ書く）。Kinematic：current が、追わせる先
      Vector3    previousPosition{};
      Quaternion previousRotation{ 0.0f, 0.0f, 0.0f, 1.0f };
      Vector3    currentPosition{};
      Quaternion currentRotation{ 0.0f, 0.0f, 0.0f, 1.0f };
      bool       needsWriteBack = false;
    };

    /// <summary>
    /// 世界に作ったキャラクター 1 つ。動かすのは CharacterVirtual（世界のボディではない）で、レイ・トリガー・他のボディには、中に持たせた同じ形のボディが見える
    /// </summary>
    struct CharacterRecord {
      JPH::Ref<JPH::CharacterVirtual> character;
      PhysicsWorld::CharacterSettings settings;
      bool                            isInWorld = true;
      Vector3                         shapeScale{ 1.0f, 1.0f, 1.0f };  ///< 今の形を作ったときの scale
      Vector3                         lastTranslate{};                 ///< 最後に Actor へ書いた（か、Actor から読んだ）位置。違っていれば、外から書き換えられた
      Vector3                         groundVelocity{};                ///< 最後のステップで動きに足した、床の速度（ridesGround のとき。床から離れたステップで、キャラクターの速度へ渡す）

      //直前の 2 ステップの位置（その間を補間して Actor へ書く）
      Vector3 previousPosition{};
      Vector3 currentPosition{};
      bool    needsWriteBack = false;
    };

    /// <summary>
    /// キャラクターが実体に触れた所 1 件。世界の計算の後で、キャラクターの Actor に OnControllerColliderHit を配る
    /// </summary>
    struct ControllerHit {
      Component* controller;  ///< キャラクターの通知先
      Component* other;       ///< 触れた形の通知先
      Vector3    point;
      Vector3    normal;
      Vector3    moveDirection;
    };

    struct JointRecord {
      JPH::Ref<JPH::TwoBodyConstraint> constraint;
      PhysicsWorld::BodyHandle         bodies[2]{ PhysicsWorld::kInvalidBody, PhysicsWorld::kInvalidBody };  ///< 2 つ目が kInvalidBody なら、相手はワールド
      PhysicsWorld::JointType          type       = PhysicsWorld::JointType::Fixed;
      float                            breakForce = 0.0f;     ///< これより強い力（N）が掛かると外す。0 なら外さない
      Component*                       owner      = nullptr;  ///< 外したときの通知先
      ActorPair                        ignoredPair{};         ///< 当たらなくした 2 つの Actor（当てるジョイントと、相手がワールドのジョイントは、両方 nullptr）
    };

    /// <summary>
    /// 前のステップで、つないだ点を保つのに掛かった力（N）
    /// </summary>
    float JointForce(const JointRecord& joint) {
      // Jolt が持っているのは力積（N·s）なので、1 ステップの時間で割る
      const JPH::TwoBodyConstraint* constraint = joint.constraint;
      float                         impulse    = 0.0f;
      switch (joint.type) {
      case PhysicsWorld::JointType::Fixed:    impulse = static_cast<const JPH::FixedConstraint*>(constraint)->GetTotalLambdaPosition().Length(); break;
      case PhysicsWorld::JointType::Point:    impulse = static_cast<const JPH::PointConstraint*>(constraint)->GetTotalLambdaPosition().Length(); break;
      case PhysicsWorld::JointType::Hinge:    impulse = static_cast<const JPH::HingeConstraint*>(constraint)->GetTotalLambdaPosition().Length(); break;
      case PhysicsWorld::JointType::Distance: impulse = std::abs(static_cast<const JPH::DistanceConstraint*>(constraint)->GetTotalLambdaPosition()); break;
      }
      return impulse / PhysicsWorld::kFixedDeltaTime;
    }

    /// <summary>
    /// Hinge のモーターを、1 つ目のボディが軸の周りに speed（ラジアン / 秒）で回るように動かす。maxTorque が 0 以下なら止める
    /// </summary>
    void ApplyHingeMotor(JPH::HingeConstraint& hinge, float speed, float maxTorque) {
      if (maxTorque <= 0.0f) {
        hinge.SetMotorState(JPH::EMotorState::Off);
        return;
      }
      hinge.GetMotorSettings().SetTorqueLimit(maxTorque);
      // Jolt の速さは「1 つ目から見た、2 つ目の回る速さ」なので、符号を逆にする
      hinge.SetTargetAngularVelocity(-speed);
      hinge.SetMotorState(JPH::EMotorState::Velocity);
    }

    /// <summary>
    /// Actor の今のワールドの Transform
    /// </summary>
    Pose ReadPose(const Actor& actor) {
      const Transform world = actor.GetParent() ? actor.GetWorldTransform() : actor.GetTransform();
      return { world.translate, Quat::Normalize(Quat::FromEuler(world.rotate)), world.scale };
    }

    /// <summary>
    /// Actor のワールドの Transform が、最後に写した値から変わっていれば返す
    /// </summary>
    std::optional<Pose> TakeChangedPose(BodyRecord& record) {
      const Actor& actor     = *record.settings.actor;
      const bool   hasParent = actor.GetParent() != nullptr;
      const bool   isCached  = record.hasPose && record.hasParent == hasParent;
      Transform    world;
      if (hasParent) {
        const Matrix4x4 matrix = actor.GetWorldMatrix();
        if (isCached && HasSameBits(matrix, record.lastWorld)) {
          return std::nullopt;
        }
        record.lastWorld = matrix;
        world            = actor.GetWorldTransform();
      } else {
        world = actor.GetTransform();
        if (isCached && HasSameBits(world, record.lastLocal)) {
          return std::nullopt;
        }
        record.lastLocal = world;
      }
      record.hasPose   = true;
      record.hasParent = hasParent;
      return Pose{ world.translate, Quat::Normalize(Quat::FromEuler(world.rotate)), world.scale };
    }

    /// <summary>
    /// 頂点から作る形（Mesh と ConvexHull）。scale は軸ごとに掛かる。作れなければ nullptr
    /// </summary>
    JPH::Ref<JPH::Shape> MakeMeshLeaf(const PhysicsWorld::Shape& shape, const Vector3& scale) {
      if (!shape.mesh || shape.mesh->positions.empty()) {
        return nullptr;
      }
      JPH::ShapeSettings::ShapeResult result;
      if (shape.type == PhysicsWorld::ShapeType::ConvexHull) {
        JPH::Array<JPH::Vec3> points;
        points.reserve(shape.mesh->positions.size());
        for (const Vector3& position : shape.mesh->positions) {
          points.push_back(ToJolt(position));
        }
        result = JPH::ConvexHullShapeSettings(points).Create();
      } else {
        JPH::VertexList          vertices;
        JPH::IndexedTriangleList triangles;
        vertices.reserve(shape.mesh->positions.size());
        triangles.reserve(shape.mesh->indices.size() / 3);
        for (const Vector3& position : shape.mesh->positions) {
          vertices.push_back(JPH::Float3(position.x, position.y, position.z));
        }
        const std::vector<uint32_t>& indices = shape.mesh->indices;
        for (size_t i = 0; i + 2 < indices.size(); i += 3) {
          triangles.push_back(JPH::IndexedTriangle(indices[i], indices[i + 1], indices[i + 2], 0));
        }
        result = JPH::MeshShapeSettings(std::move(vertices), std::move(triangles)).Create();
      }
      if (result.HasError()) {
        Logger::Log("PhysicsWorld: failed to build a mesh shape: %s", result.GetError().c_str());
        return nullptr;
      }
      // 通知先は、中の形に持たせる（scale の入れ物は、中の形の値を返す）
      JPH::Ref<JPH::Shape> leaf = result.Get();
      leaf->SetUserData(reinterpret_cast<JPH::uint64>(shape.owner));
      if (!IsSameScale(scale, { 1.0f, 1.0f, 1.0f })) {
        leaf = new JPH::ScaledShape(leaf, ToJolt(scale));
      }
      return leaf;
    }

    /// <summary>
    /// scale の掛け方は Unity と同じ：Box は軸ごと、Sphere は 3 軸の最大、Capsule の半径は X と Z の大きい方・高さは Y
    /// </summary>
    JPH::Ref<JPH::Shape> MakeLeaf(const PhysicsWorld::Shape& shape, const Vector3& scale) {
      if (shape.type == PhysicsWorld::ShapeType::Mesh || shape.type == PhysicsWorld::ShapeType::ConvexHull) {
        return MakeMeshLeaf(shape, scale);
      }
      const Vector3        size{ std::abs(scale.x), std::abs(scale.y), std::abs(scale.z) };
      JPH::Ref<JPH::Shape> leaf;
      switch (shape.type) {
      case PhysicsWorld::ShapeType::Box: {
        const JPH::Vec3 half = JPH::Vec3::sMax(ToJolt(shape.size) * ToJolt(size) * 0.5f, JPH::Vec3::sReplicate(kMinShapeSize));
        // 角の丸みは、一番薄い辺の半分まで
        leaf = new JPH::BoxShape(half, (std::min)(JPH::cDefaultConvexRadius, half.ReduceMin()));
        break;
      }
      case PhysicsWorld::ShapeType::Capsule: {
        const float radius = (std::max)(shape.radius * (std::max)(size.x, size.z), kMinShapeSize);
        // 全長が直径以下なら、球になる
        const float halfCylinder = shape.height * size.y * 0.5f - radius;
        if (halfCylinder > kMinShapeSize) {
          leaf = new JPH::CapsuleShape(halfCylinder, radius);
        } else {
          leaf = new JPH::SphereShape(radius);
        }
        break;
      }
      default:
        leaf = new JPH::SphereShape((std::max)(shape.radius * (std::max)({ size.x, size.y, size.z }), kMinShapeSize));
        break;
      }
      leaf->SetUserData(reinterpret_cast<JPH::uint64>(shape.owner));
      return leaf;
    }

    /// <summary>
    /// shapes を、Actor の scale を掛けて 1 つの形にまとめる。形が 1 つなら、複合にはならない（Jolt が中の形をそのまま返す）
    /// </summary>
    JPH::RefConst<JPH::Shape> MakeShape(std::span<const PhysicsWorld::Shape> shapes, const Vector3& scale) {
      JPH::StaticCompoundShapeSettings compound;
      for (const PhysicsWorld::Shape& shape : shapes) {
        const Vector3 position{ shape.position.x * scale.x, shape.position.y * scale.y, shape.position.z * scale.z };
        const Vector3 shapeScale{ shape.scale.x * scale.x, shape.scale.y * scale.y, shape.scale.z * scale.z };
        // モデルの無い Mesh など、作れなかった形は飛ばす
        if (const JPH::Ref<JPH::Shape> leaf = MakeLeaf(shape, shapeScale)) {
          compound.AddShape(ToJolt(position), ToJolt(Quat::Normalize(shape.rotation)), leaf.GetPtr());
        }
      }
      // 形の無いボディ（Trigger しか持たない Rigidbody など）は、何にも当たらない
      if (compound.mSubShapes.empty()) {
        return new JPH::EmptyShape();
      }
      const JPH::ShapeSettings::ShapeResult result = compound.Create();
      if (result.HasError()) {
        Logger::Log("PhysicsWorld: failed to build a shape: %s", result.GetError().c_str());
        return new JPH::SphereShape(kMinShapeSize);
      }
      return result.Get();
    }

    /// <summary>
    /// ボディの形。Dynamic は、重心を settings.centerOfMassOffset だけずらす（通知先は、中の形から返る）
    /// </summary>
    JPH::RefConst<JPH::Shape> MakeBodyShape(const BodyRecord& record) {
      const JPH::RefConst<JPH::Shape> shape  = MakeShape(record.shapes, record.shapeScale);
      const JPH::Vec3                 offset = ToJolt(record.settings.centerOfMassOffset) * ToJolt(record.shapeScale);
      if (record.settings.kind != PhysicsWorld::BodyKind::Dynamic || offset.IsNearZero()) {
        return shape;
      }
      return new JPH::OffsetCenterOfMassShape(shape, offset);
    }

    /// <summary>
    /// 質量を mass にした、shape の質量と回りにくさ（回りにくさは、形の大きさから決まる）
    /// </summary>
    JPH::MassProperties MakeMassProperties(const JPH::Shape& shape, float mass) {
      JPH::MassProperties properties = shape.GetMassProperties();
      properties.ScaleToMass((std::max)(mass, kMinMass));
      return properties;
    }

    /// <summary>
    /// キャラクターの形（Y 軸に立ったカプセル）。scale の掛け方は、Capsule の形と同じ
    /// </summary>
    JPH::RefConst<JPH::Shape> MakeCharacterShape(const PhysicsWorld::CharacterSettings& settings, const Vector3& scale) {
      PhysicsWorld::Shape capsule;
      capsule.type   = PhysicsWorld::ShapeType::Capsule;
      capsule.radius = settings.radius;
      capsule.height = settings.height;
      capsule.owner  = settings.owner;
      // 中心をずらす入れ物は、通知先を中の形から返す
      const Vector3 offset{ settings.offset.x * scale.x, settings.offset.y * scale.y, settings.offset.z * scale.z };
      return new JPH::RotatedTranslatedShape(ToJolt(offset), JPH::Quat::sIdentity(), MakeLeaf(capsule, scale).GetPtr());
    }

    bool Involves(const OwnerPair& pair, std::span<Component* const> owners) {
      return std::ranges::find(owners, pair.first) != owners.end() || std::ranges::find(owners, pair.second) != owners.end();
    }

    void TraceToLog(const char* format, ...) {
      va_list args;
      va_start(args, format);
      char buffer[1024];
      vsnprintf(buffer, sizeof(buffer), format, args);
      va_end(args);
      Logger::Log(std::string("[Jolt] ") + buffer + "\n");
    }

#ifdef JPH_ENABLE_ASSERTS
    /// <summary>
    /// true を返すと、Jolt がその場でブレークする
    /// </summary>
    bool OnAssertFailed(const char* expression, const char* message, const char* file, JPH::uint line) {
      Logger::Log("[Jolt] assert: %s:%u: (%s) %s", file, line, expression, message ? message : "");
      return true;
    }
#endif

  } // namespace

  /// <summary>
  /// メンバーは下から順に壊れる。世界は、フィルターとスレッドを参照しているので、それらより後に置く
  /// </summary>
  struct PhysicsWorld::Impl {
    explicit Impl(const PhysicsWorld& world) : objectLayerFilter(world), contactCollector(ignoredPairs) {}

    /// <summary>
    /// ボディを pose へ置き直す（速度では追わせない）。scale が変わっていれば、形も作り直す
    /// </summary>
    void Teleport(BodyRecord& record, const Pose& pose);

    /// <summary>
    /// Actor の Transform の変更を、ボディへ写す
    /// </summary>
    /// <param name="isStepping">この後すぐにステップを進めるとき（Kinematic を、速度で追わせる）</param>
    void SyncBody(BodyRecord& record, bool isStepping);

    void SyncBodies(bool isStepping);

    /// <summary>
    /// record.shapes と record.shapeScale から、形を作り直す
    /// </summary>
    void RebuildShape(BodyRecord& record);

    /// <summary>
    /// record.settings のうち、作った後でも変えられる値（層・摩擦・跳ね返りと、Dynamic の質量など）をボディへ入れる
    /// </summary>
    void ApplySettings(const BodyRecord& record);

    /// <summary>
    /// 力や速度を受けられるボディ（世界に入っている Dynamic で、全部の軸は止めていない物）の ID。それ以外は無効な ID
    /// </summary>
    JPH::BodyID FindMovable(BodyHandle handle) const;

    /// <summary>
    /// record.shapes の通知先を覚える
    /// </summary>
    void SetOwners(BodyHandle handle, BodyRecord& record);

    /// <summary>
    /// owners の当たっている組を、Exit を出さずに捨てる
    /// </summary>
    void ForgetOwners(std::span<Component* const> owners);

    /// <summary>
    /// owner のボディが、寝ているか静止か（Jolt は、そういうボディどうしの接触を調べない）
    /// </summary>
    bool IsResting(Component* owner) const;

    /// <summary>
    /// owner のボディが、寝ている Dynamic か（Unity と同じく、寝ている Rigidbody の組には Stay を配らない）
    /// </summary>
    bool IsSleepingRigidbody(Component* owner) const;

    /// <summary>
    /// contacts の両方の Actor の全コンポーネントに、event の通知（OnCollision〜）を呼ぶ。contacts は、呼んだ先で書き換わらない物を渡す
    /// </summary>
    void Dispatch(const ContactList& contacts, ContactEvent event, FrameStats& stats);

    void DispatchPendingExits(FrameStats& stats);

    /// <summary>
    /// StepCharacters が控えた「キャラクターが触れた所」を、キャラクターの Actor の全コンポーネントに配る
    /// </summary>
    void DispatchControllerHits(FrameStats& stats);

    /// <summary>
    /// owners の当たっている組を、次の配布で Exit を出す側へ移す
    /// </summary>
    void MoveToPendingExits(std::span<Component* const> owners);

    /// <summary>
    /// Actor の位置か scale が外から書き換えられていたら、キャラクターをその位置へ置き直す。向きは見ない
    /// </summary>
    /// <param name="isForced">書き換えられていなくても置き直す</param>
    void SyncCharacter(CharacterRecord& record, bool isForced);

    /// <summary>
    /// 全部のキャラクターを 1 ステップ動かす（重力を足し、壁と床に沿わせ、段差を越える）
    /// </summary>
    /// <param name="collisionMasks">層の当たり表（添字 = 層 A、ビット b = 層 B と当たる）</param>
    void StepCharacters(const std::array<uint32_t, kMaxLayers>& collisionMasks);

    /// <summary>
    /// 世界に入っている Dynamic のボディを起こす
    /// </summary>
    void WakeBody(BodyHandle handle);

    /// <summary>
    /// handle につないだジョイントを、両方のボディが世界に入っているときだけ効かせる（世界から出したボディを、Jolt に触らせない）
    /// </summary>
    void RefreshJoints(BodyHandle handle);

    /// <summary>
    /// ジョイントを世界から外し、つながれていたボディを起こす（支えが外れた物が、動き出すように）。当たらなくしていた組も戻す
    /// </summary>
    void RemoveJoint(const JointRecord& joint);

    /// <summary>
    /// Break Force より強い力が掛かったジョイントを外し、通知先の Actor の全コンポーネントに OnJointBreak を配る
    /// </summary>
    void BreakOverloadedJoints(FrameStats& stats);

    /// <summary>
    /// ボディの形のうち、subShape の形の通知先（無ければ nullptr）
    /// </summary>
    Component* FindOwner(const JPH::BodyID& body, const JPH::SubShapeID& subShape) const;

    /// <summary>
    /// レイの当たりを collector に集める。Unity と同じく、始点を中に含む形には当たらない（Jolt の既定は、中から始まると距離 0 で当たる）。形の裏面にも当たらない（設定の既定）
    /// </summary>
    void CastRay(const JPH::RRayCast& ray, JPH::CastRayCollector& collector, uint32_t layerMask, bool hitsTriggers) const;

    /// <summary>
    /// レイの当たり 1 つを、返す形に直す
    /// </summary>
    std::optional<RaycastHit> ToRaycastHit(const JPH::RRayCast& ray, const JPH::RayCastResult& result, float maxDistance) const;

    /// <summary>
    /// shape（owner と position は使わない）を position から direction へ maxDistance まで動かして、最初に当たる所を返す
    /// </summary>
    std::optional<RaycastHit> CastShape(const Shape& shape, const Vector3& position, const Quaternion& rotation, const Vector3& direction,
                                        float maxDistance, uint32_t layerMask, bool hitsTriggers) const;

    /// <summary>
    /// position に置いた shape に重なっているボディの Actor（重複なし）
    /// </summary>
    std::vector<Actor*> CollideShape(const Shape& shape, const Vector3& position, const Quaternion& rotation, uint32_t layerMask, bool hitsTriggers) const;

    BroadPhaseLayers                         broadPhaseLayers;
    ObjectVsBroadPhaseFilter                 objectVsBroadPhaseFilter;
    ObjectLayerFilter                        objectLayerFilter;
    IgnoredPairs                             ignoredPairs;
    ContactCollector                         contactCollector;
    JPH::TempAllocatorImplWithMallocFallback tempAllocator{ kTempAllocatorBytes };
    JPH::JobSystemThreadPool                 jobSystem{ JPH::cMaxPhysicsJobs, JPH::cMaxPhysicsBarriers };
#ifdef JPH_DEBUG_RENDERER
    LineDebugRenderer                        debugRenderer;
#endif
    JPH::PhysicsSystem                       system;

    std::unordered_map<BodyHandle, BodyRecord> bodies;
    std::unordered_map<Component*, BodyHandle> ownerBodies;  ///< 通知先 → その形を持つボディ（キャラクターは、中に持たせたボディ）

    std::unordered_map<CharacterHandle, CharacterRecord> characters;     ///< キーは、中に持たせたボディの ID
    std::unordered_map<JointHandle, JointRecord>         joints;
    JointHandle                                          nextJoint = 0;

    std::vector<ControllerHit> controllerHits;   ///< このステップでキャラクターが触れた所
    std::vector<ControllerHit> dispatchingHits;  ///< 配っている間の入れ物（確保を使い回す）

    //当たりの通知。どれも、通知先の組と、その接触の値の一覧
    ContactList touching;      ///< 今当たっている組（並べ替え済み）
    ContactList pendingExits;  ///< 世界から出したボディの組。次の配布で Exit を出す
    ContactList staying;       ///< Stay を配る間の写し（毎ステップ使うので、確保を使い回す）
  };

  void PhysicsWorld::Impl::Teleport(BodyRecord& record, const Pose& pose)
  {
    if (!IsSameScale(pose.scale, record.shapeScale)) {
      record.shapeScale = pose.scale;
      RebuildShape(record);
    }
    system.GetBodyInterface().SetPositionAndRotation(record.id, ToJolt(pose.position), ToJolt(pose.rotation), ActivationOf(record.settings.kind));
    record.previousPosition = record.currentPosition = pose.position;
    record.previousRotation = record.currentRotation = pose.rotation;
    record.needsWriteBack   = false;
  }

  void PhysicsWorld::Impl::SyncBody(BodyRecord& record, bool isStepping)
  {
    const Actor& actor = *record.settings.actor;
    switch (record.settings.kind) {
    case BodyKind::Static:
    case BodyKind::Trigger:
      if (const std::optional<Pose> pose = TakeChangedPose(record)) {
        Teleport(record, *pose);
      }
      break;
    case BodyKind::Kinematic: {
      // 置き直さずに速度で追わせると、押した物や上に乗った物が正しく動く。追わせるのは、ステップの直前だけ
      if (!isStepping) {
        break;
      }
      JPH::BodyInterface&       bodyInterface = system.GetBodyInterface();
      const std::optional<Pose> pose          = TakeChangedPose(record);
      if (pose) {
        if (!IsSameScale(pose->scale, record.shapeScale)) {
          record.shapeScale = pose->scale;
          RebuildShape(record);
        }
        record.currentPosition = pose->position;
        record.currentRotation = pose->rotation;
      }
      // 着いた後のステップでも呼んで、速度を 0 に戻す
      if (pose || bodyInterface.IsActive(record.id)) {
        bodyInterface.MoveKinematic(record.id, ToJolt(record.currentPosition), ToJolt(record.currentRotation), kFixedDeltaTime);
      }
      break;
    }
    case BodyKind::Dynamic:
      // 物理が動かすボディ。最後に書いた値と違えば、外から Transform を書き換えられたので、その位置へ置き直す
      if (!HasSameBits(actor.GetTransform(), record.lastLocal)) {
        record.lastLocal = actor.GetTransform();
        Teleport(record, ReadPose(actor));
      }
      break;
    }
  }

  void PhysicsWorld::Impl::SyncBodies(bool isStepping)
  {
    for (auto& [handle, record] : bodies) {
      // 世界から出してあるボディは、戻すときに写す
      if (!record.isInWorld) {
        continue;
      }
      SyncBody(record, isStepping);
      if (isStepping) {
        record.previousPosition = record.currentPosition;
        record.previousRotation = record.currentRotation;
      }
    }
  }

  void PhysicsWorld::Impl::RebuildShape(BodyRecord& record)
  {
    JPH::BodyInterface& bodyInterface = system.GetBodyInterface();
    bodyInterface.SetShape(record.id, MakeBodyShape(record), false, ActivationOf(record.settings.kind));
    // 形を 2 つ以上持つボディは、接触を 1 つにまとめさせない（どの形が当たったかを、形ごとに知らせるため）
    bodyInterface.SetUseManifoldReduction(record.id, record.shapes.size() <= 1);
    // 質量は変えずに、回りにくさを新しい形に合わせる
    if (record.settings.kind == BodyKind::Dynamic) {
      ApplySettings(record);
    }
  }

  void PhysicsWorld::Impl::ApplySettings(const BodyRecord& record)
  {
    const BodySettings& settings      = record.settings;
    JPH::BodyInterface& bodyInterface = system.GetBodyInterface();
    bodyInterface.SetObjectLayer(record.id, ToObjectLayer(settings.kind, settings.layer));
    bodyInterface.SetFriction(record.id, (std::max)(settings.friction, 0.0f));
    bodyInterface.SetRestitution(record.id, (std::max)(settings.restitution, 0.0f));
    if (settings.kind != BodyKind::Dynamic) {
      return;
    }

    // 全部の軸を止めたボディは、Jolt では Dynamic にできない。動かない Kinematic にする
    const JPH::EAllowedDOFs allowed  = ToAllowedDofs(settings.lockedAxes);
    const bool              isFrozen = allowed == JPH::EAllowedDOFs::None;
    if (isFrozen) {
      bodyInterface.SetLinearAndAngularVelocity(record.id, JPH::Vec3::sZero(), JPH::Vec3::sZero());
    }
    bodyInterface.SetMotionType(record.id, isFrozen ? JPH::EMotionType::Kinematic : JPH::EMotionType::Dynamic, JPH::EActivation::Activate);
    bodyInterface.SetGravityFactor(record.id, settings.gravityScale);
    bodyInterface.SetMotionQuality(record.id, settings.isContinuous ? JPH::EMotionQuality::LinearCast : JPH::EMotionQuality::Discrete);

    const JPH::BodyLockWrite lock(system.GetBodyLockInterface(), record.id);
    if (!lock.Succeeded()) {
      return;
    }
    JPH::Body&             body   = lock.GetBody();
    JPH::MotionProperties* motion = body.GetMotionPropertiesUnchecked();
    motion->SetLinearDamping((std::max)(settings.linearDamping, 0.0f));
    motion->SetAngularDamping((std::max)(settings.angularDamping, 0.0f));
    if (!isFrozen) {
      motion->SetMassProperties(allowed, MakeMassProperties(*body.GetShape(), settings.mass));
    }
  }

  JPH::BodyID PhysicsWorld::Impl::FindMovable(BodyHandle handle) const
  {
    const auto found = bodies.find(handle);
    if (found == bodies.end() || !found->second.isInWorld || found->second.settings.kind != BodyKind::Dynamic
        || ToAllowedDofs(found->second.settings.lockedAxes) == JPH::EAllowedDOFs::None) {
      return JPH::BodyID();
    }
    return found->second.id;
  }

  void PhysicsWorld::Impl::SetOwners(BodyHandle handle, BodyRecord& record)
  {
    record.owners.clear();
    for (const Shape& shape : record.shapes) {
      if (shape.owner && std::ranges::find(record.owners, shape.owner) == record.owners.end()) {
        record.owners.push_back(shape.owner);
        ownerBodies[shape.owner] = handle;
      }
    }
  }

  void PhysicsWorld::Impl::ForgetOwners(std::span<Component* const> owners)
  {
    const auto involves = [owners](const Contact& contact) { return Involves(contact.pair, owners); };
    std::erase_if(touching, involves);
    std::erase_if(pendingExits, involves);
    for (Component* owner : owners) {
      ownerBodies.erase(owner);
    }
  }

  bool PhysicsWorld::Impl::IsResting(Component* owner) const
  {
    const auto body = ownerBodies.find(owner);
    return body != ownerBodies.end() && !system.GetBodyInterface().IsActive(JPH::BodyID(body->second));
  }

  bool PhysicsWorld::Impl::IsSleepingRigidbody(Component* owner) const
  {
    // キャラクターの中のボディは bodies に無いので、ここでは寝ている扱いにならない
    const auto body = ownerBodies.find(owner);
    if (body == ownerBodies.end()) {
      return false;
    }
    const auto record = bodies.find(body->second);
    return record != bodies.end() && record->second.settings.kind == BodyKind::Dynamic && !system.GetBodyInterface().IsActive(record->second.id);
  }

  void PhysicsWorld::Impl::Dispatch(const ContactList& contacts, ContactEvent event, FrameStats& stats)
  {
    if (contacts.empty()) {
      return;
    }
    void (Component::* handler)(const Collision&) = &Component::OnCollisionExit;
    if (event == ContactEvent::Enter) {
      handler = &Component::OnCollisionEnter;
    } else if (event == ContactEvent::Stay) {
      handler = &Component::OnCollisionStay;
    }
    const Clock::time_point start = Clock::now();
    for (const Contact& contact : contacts) {
      // 前の通知の中でボディを消された通知先には、知らせない
      if (!ownerBodies.contains(contact.pair.first) || !ownerBodies.contains(contact.pair.second)) {
        continue;
      }
      Actor* const actors[2] = { contact.pair.first->GetOwner(), contact.pair.second->GetOwner() };
      for (int side = 0; side < 2; ++side) {
        // 控えてある向きは first → second、速度の差は second − first。受け手から見た値に直す
        Collision collision;
        collision.actor = actors[1 - side];
        if (event != ContactEvent::Exit) {
          collision.point            = contact.point;
          collision.normal           = side == 0 ? -contact.normal : contact.normal;
          collision.relativeVelocity = side == 0 ? contact.relativeVelocity : -contact.relativeVelocity;
          collision.impulse          = event == ContactEvent::Enter ? contact.impulse : 0.0f;
        }
        // 受け手がコンポーネントを足しても壊れないよう、添字で回す
        const auto& components = actors[side]->GetComponents();
        for (size_t i = 0; i < components.size(); ++i) {
          (components[i].get()->*handler)(collision);
        }
      }
      ++stats.callbackCount;
    }
    stats.callbackMs += std::chrono::duration<float, std::milli>(Clock::now() - start).count();
  }

  void PhysicsWorld::Impl::DispatchPendingExits(FrameStats& stats)
  {
    // 通知の中でまた止められても壊れないよう、取り出してから配る
    ContactList exits;
    exits.swap(pendingExits);
    Dispatch(exits, ContactEvent::Exit, stats);
  }

  void PhysicsWorld::Impl::DispatchControllerHits(FrameStats& stats)
  {
    if (controllerHits.empty()) {
      return;
    }
    // 通知の中でキャラクターが作られても壊れないよう、取り出してから配る
    dispatchingHits.clear();
    dispatchingHits.swap(controllerHits);
    const Clock::time_point start = Clock::now();
    for (const ControllerHit& entry : dispatchingHits) {
      // 前の通知の中で消された物には、知らせない
      if (!ownerBodies.contains(entry.controller) || !ownerBodies.contains(entry.other)) {
        continue;
      }
      const ControllerColliderHit hit{ entry.other->GetOwner(), entry.point, entry.normal, entry.moveDirection };
      // 受け手がコンポーネントを足しても壊れないよう、添字で回す
      const auto& components = entry.controller->GetOwner()->GetComponents();
      for (size_t i = 0; i < components.size(); ++i) {
        components[i]->OnControllerColliderHit(hit);
      }
      ++stats.callbackCount;
    }
    stats.callbackMs += std::chrono::duration<float, std::milli>(Clock::now() - start).count();
  }

  void PhysicsWorld::Impl::MoveToPendingExits(std::span<Component* const> owners)
  {
    const auto moved = std::ranges::stable_partition(touching, [owners](const Contact& contact) { return !Involves(contact.pair, owners); });
    pendingExits.insert(pendingExits.end(), moved.begin(), moved.end());
    touching.erase(moved.begin(), moved.end());
  }

  void PhysicsWorld::Impl::SyncCharacter(CharacterRecord& record, bool isForced)
  {
    const Actor& actor    = *record.settings.actor;
    const Pose   pose     = ReadPose(actor);
    const bool   isScaled = !IsSameScale(pose.scale, record.shapeScale);
    // カプセルは傾かないので、スクリプトが Actor を回しても置き直さない（置き直すと、補間が途切れる）
    if (!isForced && !isScaled && HasSameBits(actor.GetTransform().translate, record.lastTranslate)) {
      return;
    }
    if (isScaled) {
      record.shapeScale = pose.scale;
      const JPH::RefConst<JPH::Shape> shape = MakeCharacterShape(record.settings, pose.scale);
      // 重なりは調べずに差し替える（重なった分は、次のステップで押し戻される）
      record.character->SetShape(shape, FLT_MAX, {}, {}, {}, {}, tempAllocator);
      record.character->SetInnerBodyShape(shape);
    }
    record.character->SetPosition(ToJolt(pose.position));
    record.lastTranslate    = actor.GetTransform().translate;
    record.previousPosition = record.currentPosition = pose.position;
    record.needsWriteBack   = false;
  }

  void PhysicsWorld::Impl::StepCharacters(const std::array<uint32_t, kMaxLayers>& collisionMasks)
  {
    const JPH::Vec3 worldGravity = system.GetGravity();
    for (auto& [handle, record] : characters) {
      if (!record.isInWorld) {
        continue;
      }
      SyncCharacter(record, false);
      record.previousPosition = record.currentPosition;

      JPH::CharacterVirtual&   character = *record.character;
      const CharacterSettings& settings  = record.settings;
      const JPH::Vec3          gravity   = worldGravity * settings.gravityScale;
      // 立っている間は、落ちる速さを溜めない
      JPH::Vec3 velocity = character.GetLinearVelocity();
      if (character.GetGroundState() == JPH::CharacterBase::EGroundState::OnGround && velocity.GetY() < 0.0f) {
        velocity.SetY(0.0f);
      }
      // 床に付いていく：何かの上に居る間は、その床の速度を足して動かす（控える速度には入れない）。床から離れたステップで、それまでの床の速度を 1 回だけ渡す
      JPH::Vec3 groundVelocity = JPH::Vec3::sZero();
      if (settings.ridesGround) {
        if (character.IsSupported()) {
          character.UpdateGroundVelocity();
          groundVelocity = character.GetGroundVelocity();
        } else {
          velocity += ToJolt(record.groundVelocity);
        }
        record.groundVelocity = ToEngine(groundVelocity);
      }
      velocity += gravity * kFixedDeltaTime;
      const JPH::Vec3 movement = velocity + groundVelocity;
      character.SetLinearVelocity(movement);

      // 段差の高さは、登るときと、下りで床から離れないよう吸い付くときの両方に使う
      JPH::CharacterVirtual::ExtendedUpdateSettings update;
      update.mWalkStairsStepUp     = JPH::Vec3(0.0f, (std::max)(settings.stepHeight, 0.0f), 0.0f);
      update.mStickToFloorStepDown = -update.mWalkStairsStepUp;
      const LayerMaskFilter  layerFilter(collisionMasks[(std::min)(settings.layer, static_cast<uint32_t>(kMaxLayers - 1))]);
      const OtherActorFilter bodyFilter(settings.actor, ignoredPairs);
      character.ExtendedUpdate(kFixedDeltaTime, gravity, update, {}, layerFilter, bodyFilter, {}, tempAllocator);

      // 触れている実体を控える（Trigger は除く）。向きは、このステップで動こうとした向き
      if (settings.owner) {
        const Vector3 moveDirection = ToEngine(movement.NormalizedOr(JPH::Vec3::sZero()));
        for (const JPH::CharacterContact& contact : character.GetActiveContacts()) {
          if (!contact.mHadCollision || contact.mWasDiscarded || contact.mIsSensorB) {
            continue;
          }
          const JPH::BodyLockRead lock(system.GetBodyLockInterface(), contact.mBodyB);
          if (!lock.Succeeded()) {
            continue;
          }
          auto* other = reinterpret_cast<Component*>(lock.GetBody().GetShape()->GetSubShapeUserData(contact.mSubShapeIDB));
          if (other) {
            controllerHits.push_back({ settings.owner, other, ToEngine(contact.mPosition), ToEngine(contact.mSurfaceNormal), moveDirection });
          }
        }
      }

      const Vector3 position = ToEngine(character.GetPosition());
      // 上へ動こうとして半分も進めなかったら、天井に当たっている。上向きの速さを捨てる（捨てないと、落ち始めるまで天井に張り付く）
      if (movement.GetY() > 0.0f && position.y - record.currentPosition.y < movement.GetY() * kFixedDeltaTime * 0.5f) {
        velocity.SetY((std::min)(velocity.GetY(), 0.0f));
      }
      // 控える速度には、床の分を入れない
      character.SetLinearVelocity(velocity);
      record.currentPosition = position;
      record.needsWriteBack  = true;
    }
  }

  void PhysicsWorld::Impl::WakeBody(BodyHandle handle)
  {
    const auto found = bodies.find(handle);
    if (found != bodies.end() && found->second.isInWorld && found->second.settings.kind == BodyKind::Dynamic) {
      system.GetBodyInterface().ActivateBody(found->second.id);
    }
  }

  void PhysicsWorld::Impl::RefreshJoints(BodyHandle handle)
  {
    // 相手がワールドのジョイントは、2 つ目が kInvalidBody（見つからない）
    const auto isInWorld = [this](BodyHandle body) {
      const auto found = bodies.find(body);
      return found == bodies.end() || found->second.isInWorld;
    };
    for (auto& [id, joint] : joints) {
      if (joint.bodies[0] == handle || joint.bodies[1] == handle) {
        joint.constraint->SetEnabled(isInWorld(joint.bodies[0]) && isInWorld(joint.bodies[1]));
      }
    }
  }

  void PhysicsWorld::Impl::RemoveJoint(const JointRecord& joint)
  {
    system.RemoveConstraint(joint.constraint);
    if (joint.ignoredPair.first) {
      if (const auto found = std::ranges::find(ignoredPairs.byJoint, joint.ignoredPair); found != ignoredPairs.byJoint.end()) {
        ignoredPairs.byJoint.erase(found);
      }
    }
    WakeBody(joint.bodies[0]);
    WakeBody(joint.bodies[1]);
  }

  void PhysicsWorld::Impl::BreakOverloadedJoints(FrameStats& stats)
  {
    // 外す物を先に決める（知らせの中でジョイントが足されても消されても、壊れないように）
    struct Broken {
      Component* owner;
      float      force;
    };
    std::vector<Broken> broken;
    std::erase_if(joints, [&](const auto& entry) {
      const JointRecord& joint = entry.second;
      if (joint.breakForce <= 0.0f || !joint.constraint->GetEnabled()) {
        return false;
      }
      const float force = JointForce(joint);
      if (force <= joint.breakForce) {
        return false;
      }
      RemoveJoint(joint);
      broken.push_back({ joint.owner, force });
      return true;
    });
    if (broken.empty()) {
      return;
    }
    const Clock::time_point start = Clock::now();
    for (const Broken& entry : broken) {
      if (!entry.owner) {
        continue;
      }
      // 受け手がコンポーネントを足しても壊れないよう、添字で回す
      const auto& components = entry.owner->GetOwner()->GetComponents();
      for (size_t i = 0; i < components.size(); ++i) {
        components[i]->OnJointBreak(entry.force);
      }
      ++stats.callbackCount;
    }
    stats.callbackMs += std::chrono::duration<float, std::milli>(Clock::now() - start).count();
  }

  Component* PhysicsWorld::Impl::FindOwner(const JPH::BodyID& body, const JPH::SubShapeID& subShape) const
  {
    const JPH::BodyLockRead lock(system.GetBodyLockInterface(), body);
    return lock.Succeeded() ? reinterpret_cast<Component*>(lock.GetBody().GetShape()->GetSubShapeUserData(subShape)) : nullptr;
  }

  void PhysicsWorld::Impl::CastRay(const JPH::RRayCast& ray, JPH::CastRayCollector& collector, uint32_t layerMask, bool hitsTriggers) const
  {
    const LayerMaskFilter   layerFilter(layerMask);
    const TriggerBodyFilter bodyFilter(hitsTriggers);
    JPH::RayCastSettings    settings;
    settings.mTreatConvexAsSolid = false;
    system.GetNarrowPhaseQuery().CastRay(ray, settings, collector, {}, layerFilter, bodyFilter);
  }

  std::optional<PhysicsWorld::RaycastHit> PhysicsWorld::Impl::ToRaycastHit(const JPH::RRayCast& ray, const JPH::RayCastResult& result, float maxDistance) const
  {
    const JPH::BodyLockRead lock(system.GetBodyLockInterface(), result.mBodyID);
    if (!lock.Succeeded()) {
      return std::nullopt;
    }
    const JPH::Body& body  = lock.GetBody();
    const JPH::RVec3 point = ray.GetPointOnRay(result.mFraction);
    const auto*      owner = reinterpret_cast<const Component*>(body.GetShape()->GetSubShapeUserData(result.mSubShapeID2));
    return RaycastHit{
      .actor    = owner ? owner->GetOwner() : nullptr,
      .point    = ToEngine(point),
      .normal   = ToEngine(body.GetWorldSpaceSurfaceNormal(result.mSubShapeID2, point)),
      .distance = result.mFraction * maxDistance,
    };
  }

  std::optional<PhysicsWorld::RaycastHit> PhysicsWorld::Impl::CastShape(const Shape& shape, const Vector3& position, const Quaternion& rotation, const Vector3& direction,
                                                                        float maxDistance, uint32_t layerMask, bool hitsTriggers) const
  {
    const float length = direction.Length();
    if (length <= 0.0f || maxDistance <= 0.0f) {
      return std::nullopt;
    }
    const JPH::Ref<JPH::Shape>    leaf  = MakeLeaf(shape, { 1.0f, 1.0f, 1.0f });
    const JPH::RMat44             start = JPH::RMat44::sRotationTranslation(ToJolt(Quat::Normalize(rotation)), ToJolt(position));
    const JPH::RShapeCast         cast  = JPH::RShapeCast::sFromWorldTransform(leaf.GetPtr(), JPH::Vec3::sReplicate(1.0f), start, ToJolt(direction) * (maxDistance / length));
    const LayerMaskFilter         layerFilter(layerMask);
    const TriggerBodyFilter       bodyFilter(hitsTriggers);
    const JPH::ShapeCastSettings  settings;
    FirstMovingHitCollector       collector;
    system.GetNarrowPhaseQuery().CastShape(cast, settings, JPH::RVec3::sZero(), collector, {}, layerFilter, bodyFilter);
    if (!collector.hasHit) {
      return std::nullopt;
    }
    const JPH::ShapeCastResult& result = collector.hit;
    const Component*            owner  = FindOwner(result.mBodyID2, result.mSubShapeID2);
    return RaycastHit{
      .actor    = owner ? owner->GetOwner() : nullptr,
      .point    = ToEngine(result.mContactPointOn2),
      // Jolt の向きは、動かした形 → 当たった形
      .normal   = ToEngine(-result.mPenetrationAxis.NormalizedOr(JPH::Vec3::sZero())),
      .distance = result.mFraction * maxDistance,
    };
  }

  std::vector<Actor*> PhysicsWorld::Impl::CollideShape(const Shape& shape, const Vector3& position, const Quaternion& rotation, uint32_t layerMask, bool hitsTriggers) const
  {
    const JPH::Ref<JPH::Shape>      leaf      = MakeLeaf(shape, { 1.0f, 1.0f, 1.0f });
    const JPH::RMat44               transform = JPH::RMat44::sRotationTranslation(ToJolt(Quat::Normalize(rotation)), ToJolt(position));
    const LayerMaskFilter           layerFilter(layerMask);
    const TriggerBodyFilter         bodyFilter(hitsTriggers);
    const JPH::CollideShapeSettings settings;
    JPH::AllHitCollisionCollector<JPH::CollideShapeCollector> collector;
    system.GetNarrowPhaseQuery().CollideShape(leaf.GetPtr(), JPH::Vec3::sReplicate(1.0f), transform, settings, JPH::RVec3::sZero(), collector, {}, layerFilter, bodyFilter);

    // 形を 2 つ以上持つボディとメッシュは、形や三角形ごとに当たりが来る
    std::vector<Actor*> actors;
    for (const JPH::CollideShapeResult& result : collector.mHits) {
      const Component* owner = FindOwner(result.mBodyID2, result.mSubShapeID2);
      if (owner && std::ranges::find(actors, owner->GetOwner()) == actors.end()) {
        actors.push_back(owner->GetOwner());
      }
    }
    return actors;
  }

  std::unique_ptr<PhysicsWorld> PhysicsWorld::instance_ = nullptr;

  PhysicsWorld::PhysicsWorld(Token)
  {
    // Unity と同じく、決めていない組は当たる
    collisionMask_.fill(~0u);
  }

  PhysicsWorld::~PhysicsWorld() = default;

  PhysicsWorld* PhysicsWorld::GetInstance()
  {
    if (!instance_) {
      instance_ = std::make_unique<PhysicsWorld>(Token{});
    }
    return instance_.get();
  }

  void PhysicsWorld::Initialize()
  {
    // Jolt の物を作る前に、確保の関数と型の表を入れる
    JPH::RegisterDefaultAllocator();
    JPH::Trace = TraceToLog;
    JPH_IF_ENABLE_ASSERTS(JPH::AssertFailed = OnAssertFailed;)
    JPH::Factory::sInstance = new JPH::Factory();
    // ライブラリとこのファイルで、ビルドのマクロ（TakoJoltDefines）が食い違っていると、ここで止まる
    JPH::RegisterTypes();

    impl_ = std::make_unique<Impl>(*this);
    impl_->system.Init(kMaxBodies, 0, kMaxBodyPairs, kMaxContactConstraints,
      impl_->broadPhaseLayers, impl_->objectVsBroadPhaseFilter, impl_->objectLayerFilter);
    impl_->system.SetContactListener(&impl_->contactCollector);
  }

  void PhysicsWorld::Finalize()
  {
    impl_.reset();

    JPH::UnregisterTypes();
    delete JPH::Factory::sInstance;
    JPH::Factory::sInstance = nullptr;
  }

  void PhysicsWorld::BeginFrame()
  {
    frameStats_ = {};
    impl_->DispatchPendingExits(frameStats_);
    impl_->SyncBodies(false);
    for (auto& [handle, record] : impl_->characters) {
      if (record.isInWorld) {
        impl_->SyncCharacter(record, false);
      }
    }
  }

  void PhysicsWorld::Step()
  {
    // FixedTick の中で止めたボディの Exit を、入れ直した分の Enter より先に出す
    impl_->DispatchPendingExits(frameStats_);
    impl_->SyncBodies(true);

    const Clock::time_point start    = Clock::now();
    ContactList&            reported = impl_->contactCollector.contacts;
    reported.clear();
    // キャラクターは、世界の計算より先に動かす（Jolt のサンプルと同じ順）
    impl_->StepCharacters(collisionMask_);
    const JPH::EPhysicsUpdateError error = impl_->system.Update(kFixedDeltaTime, 1, &impl_->tempAllocator, &impl_->jobSystem);
    if (error != JPH::EPhysicsUpdateError::None) {
      // 上限（kMaxBodyPairs・kMaxContactConstraints）を越えた分の接触は、このステップでは無視されている
      Logger::Log("PhysicsWorld: too many contacts in one step (error flags %u). Some contacts were ignored", static_cast<uint32_t>(error));
    }
    ++frameStats_.stepCount;
    frameStats_.stepMs += std::chrono::duration<float, std::milli>(Clock::now() - start).count();

    // 起きている Dynamic の、新しい姿勢を控える（Actor へは、EndFrame が補間して書く）
    const JPH::BodyInterface& bodyInterface = impl_->system.GetBodyInterface();
    for (auto& [handle, record] : impl_->bodies) {
      if (record.settings.kind != BodyKind::Dynamic || !record.isInWorld || !bodyInterface.IsActive(record.id)) {
        continue;
      }
      JPH::RVec3 position;
      JPH::Quat  rotation;
      bodyInterface.GetPositionAndRotation(record.id, position, rotation);
      record.currentPosition = ToEngine(position);
      record.currentRotation = { rotation.GetX(), rotation.GetY(), rotation.GetZ(), rotation.GetW() };
      record.needsWriteBack  = true;
    }

    // このステップで触れていた組と、前から当たっていた組を比べる。同じ組の接触が 2 つ以上あれば、力積の大きい 1 つを残す
    std::ranges::sort(reported, [](const Contact& a, const Contact& b) { return a.pair != b.pair ? a.pair < b.pair : a.impulse > b.impulse; });
    const auto duplicates = std::ranges::unique(reported, {}, &Contact::pair);
    reported.erase(duplicates.begin(), duplicates.end());

    ContactList entered;
    std::ranges::set_difference(reported, impl_->touching, std::back_inserter(entered), {}, &Contact::pair, &Contact::pair);
    ContactList exited;
    ContactList touching = reported;
    for (const Contact& contact : impl_->touching) {
      if (std::ranges::binary_search(reported, contact.pair, {}, &Contact::pair)) {
        continue;
      }
      // 寝ているボディどうしは Jolt が調べないので、報告が来ない。離れたのではないので、当たっている扱いを続ける（接触の値は、前のまま）
      if (impl_->IsResting(contact.pair.first) && impl_->IsResting(contact.pair.second)) {
        touching.push_back(contact);
      } else {
        exited.push_back(contact);
      }
    }
    std::ranges::sort(touching, {}, &Contact::pair);
    impl_->touching = std::move(touching);

    impl_->Dispatch(entered, ContactEvent::Enter, frameStats_);
    impl_->Dispatch(exited, ContactEvent::Exit, frameStats_);

    // 当たり続けている組に Stay（Unity と同じく、ステップごと）。このステップで当たり始めた組は次のステップからで、寝ている Rigidbody の組には配らない。
    // 上の通知の中で止められたボディの組は、もう touching に無い
    ContactList& staying = impl_->staying;
    staying.clear();
    for (const Contact& contact : impl_->touching) {
      if (std::ranges::binary_search(entered, contact.pair, {}, &Contact::pair)) {
        continue;
      }
      if (impl_->IsSleepingRigidbody(contact.pair.first) || impl_->IsSleepingRigidbody(contact.pair.second)) {
        continue;
      }
      staying.push_back(contact);
    }
    impl_->Dispatch(staying, ContactEvent::Stay, frameStats_);

    impl_->DispatchControllerHits(frameStats_);
    impl_->BreakOverloadedJoints(frameStats_);
  }

  void PhysicsWorld::EndFrame(float alpha)
  {
    // Dynamic の姿勢を、直前の 2 ステップの間で補間して Actor へ書く（描画がステップより速くても、滑らかに動く）
    for (auto& [handle, record] : impl_->bodies) {
      if (!record.needsWriteBack || !record.isInWorld) {
        continue;
      }
      Actor&     actor     = *record.settings.actor;
      const bool hasParent = actor.GetParent() != nullptr;
      Transform  world     = hasParent ? actor.GetWorldTransform() : actor.GetTransform();
      world.translate = record.previousPosition + (record.currentPosition - record.previousPosition) * alpha;
      world.rotate    = Mat4x4::ExtractEulerAngles(Quat::ToMatrix(Quat::Slerp(record.previousRotation, record.currentRotation, alpha)));
      if (hasParent) {
        actor.SetWorldTransform(world);
      } else {
        actor.SetTransform(world);
      }
      record.lastLocal = actor.GetTransform();
      // 止まった後は、最後の姿勢を 1 回書いて終わる
      record.needsWriteBack = !HasSameBits(record.previousPosition, record.currentPosition) || !HasSameBits(record.previousRotation, record.currentRotation);
    }

    // キャラクターの位置も、同じように補間して書く（向きと scale は、Actor の値のまま）
    for (auto& [handle, record] : impl_->characters) {
      if (!record.needsWriteBack || !record.isInWorld) {
        continue;
      }
      Actor& actor = *record.settings.actor;
      actor.SetWorldPosition(record.previousPosition + (record.currentPosition - record.previousPosition) * alpha);
      record.lastTranslate  = actor.GetTransform().translate;
      record.needsWriteBack = !HasSameBits(record.previousPosition, record.currentPosition);
    }

    impl_->DispatchPendingExits(frameStats_);
  }

  PhysicsWorld::BodyHandle PhysicsWorld::CreateBody(const BodySettings& settings, std::span<const Shape> shapes)
  {
    BodyRecord record;
    record.settings = settings;
    record.shapes.assign(shapes.begin(), shapes.end());
    // Dynamic は、Actor の Transform を「最後に書いた値」として控える（他は、最後に写した値）
    const bool isDynamic = settings.kind == BodyKind::Dynamic;
    const Pose pose      = isDynamic ? ReadPose(*settings.actor) : *TakeChangedPose(record);
    if (isDynamic) {
      record.lastLocal = settings.actor->GetTransform();
    }
    record.shapeScale       = pose.scale;
    record.previousPosition = record.currentPosition = pose.position;
    record.previousRotation = record.currentRotation = pose.rotation;

    const bool       isTrigger = settings.kind == BodyKind::Trigger;
    JPH::EMotionType motion    = JPH::EMotionType::Kinematic;
    if (settings.kind == BodyKind::Static) {
      motion = JPH::EMotionType::Static;
    } else if (isDynamic) {
      motion = JPH::EMotionType::Dynamic;
    }
    const JPH::RefConst<JPH::Shape> shape = MakeBodyShape(record);
    JPH::BodyCreationSettings       creation(shape, ToJolt(pose.position), ToJolt(pose.rotation), motion, ToObjectLayer(settings.kind, settings.layer));
    // Trigger は、動かない実体や他の Trigger との重なりも知らせる
    creation.mIsSensor                     = isTrigger;
    creation.mCollideKinematicVsNonDynamic = isTrigger;
    creation.mUseManifoldReduction         = record.shapes.size() <= 1;
    creation.mUserData                     = reinterpret_cast<JPH::uint64>(settings.actor);
    if (isDynamic) {
      // 形が無くても作れるよう、質量と回りにくさはここで渡す（軸の固定などは、下の ApplySettings が入れる）
      creation.mOverrideMassProperties = JPH::EOverrideMassProperties::MassAndInertiaProvided;
      creation.mMassPropertiesOverride = MakeMassProperties(*shape, settings.mass);
    }

    JPH::BodyInterface& bodies = impl_->system.GetBodyInterface();
    JPH::Body*          body   = bodies.CreateBody(creation);
    if (!body) {
      Logger::Log("PhysicsWorld: too many bodies (max %u). The body was not created", kMaxBodies);
      return kInvalidBody;
    }
    record.id = body->GetID();
    const BodyHandle handle = record.id.GetIndexAndSequenceNumber();
    impl_->SetOwners(handle, record);
    const BodyRecord& stored = impl_->bodies.emplace(handle, std::move(record)).first->second;
    impl_->ApplySettings(stored);
    bodies.AddBody(stored.id, ActivationOf(settings.kind));
    return handle;
  }

  void PhysicsWorld::DestroyBody(BodyHandle body)
  {
    const auto found = impl_->bodies.find(body);
    if (found == impl_->bodies.end()) {
      return;
    }
    // このボディにつないだジョイントを、先に外す（拘束は、ボディを生のポインタで持っている）
    std::erase_if(impl_->joints, [&](const auto& entry) {
      const JointRecord& joint = entry.second;
      if (joint.bodies[0] != body && joint.bodies[1] != body) {
        return false;
      }
      impl_->RemoveJoint(joint);
      return true;
    });

    JPH::BodyInterface& bodies = impl_->system.GetBodyInterface();
    if (found->second.isInWorld) {
      bodies.RemoveBody(found->second.id);
    }
    bodies.DestroyBody(found->second.id);
    impl_->ForgetOwners(found->second.owners);
    impl_->bodies.erase(found);
  }

  void PhysicsWorld::SetBodyEnabled(BodyHandle body, bool enabled)
  {
    const auto found = impl_->bodies.find(body);
    if (found == impl_->bodies.end() || found->second.isInWorld == enabled) {
      return;
    }
    BodyRecord&         record = found->second;
    JPH::BodyInterface& bodies = impl_->system.GetBodyInterface();
    record.isInWorld = enabled;
    if (enabled) {
      // 出ている間に Actor が動いた分を写してから戻す（Kinematic も、ここでは速度で追わせずに置き直す）
      if (record.settings.kind == BodyKind::Kinematic) {
        record.hasPose = false;
        impl_->Teleport(record, *TakeChangedPose(record));
      } else {
        impl_->SyncBody(record, false);
      }
      bodies.AddBody(record.id, ActivationOf(record.settings.kind));
      impl_->RefreshJoints(body);
      return;
    }
    impl_->RefreshJoints(body);
    bodies.RemoveBody(record.id);
    // 当たっていた組は、次の配布で Exit を出す
    impl_->MoveToPendingExits(record.owners);
  }

  void PhysicsWorld::SetBodyShapes(BodyHandle body, std::span<const Shape> shapes)
  {
    const auto found = impl_->bodies.find(body);
    if (found == impl_->bodies.end()) {
      return;
    }
    BodyRecord& record = found->second;
    record.shapes.assign(shapes.begin(), shapes.end());
    impl_->RebuildShape(record);

    // 形から外れた通知先の組だけを捨てる（残る通知先の組は続く）
    std::vector<Component*> removed;
    for (Component* owner : record.owners) {
      if (std::ranges::none_of(record.shapes, [owner](const Shape& shape) { return shape.owner == owner; })) {
        removed.push_back(owner);
      }
    }
    impl_->ForgetOwners(removed);
    impl_->SetOwners(body, record);
  }

  void PhysicsWorld::SetBodySettings(BodyHandle body, const BodySettings& settings)
  {
    const auto found = impl_->bodies.find(body);
    if (found == impl_->bodies.end()) {
      return;
    }
    BodyRecord&    record        = found->second;
    const BodyKind kind          = record.settings.kind;
    Actor* const   actor         = record.settings.actor;
    const bool     isCenterMoved = !HasSameBits(record.settings.centerOfMassOffset, settings.centerOfMassOffset);
    record.settings       = settings;
    record.settings.kind  = kind;
    record.settings.actor = actor;
    // 重心のずれは形に入っているので、変わったら形から作り直す（Dynamic は、その中で設定も入れ直す）
    if (isCenterMoved && kind == BodyKind::Dynamic) {
      impl_->RebuildShape(record);
    } else {
      impl_->ApplySettings(record);
    }
  }

  void PhysicsWorld::AddForce(BodyHandle body, const Vector3& force)
  {
    if (const JPH::BodyID id = impl_->FindMovable(body); !id.IsInvalid()) {
      impl_->system.GetBodyInterface().AddForce(id, ToJolt(force));
    }
  }

  void PhysicsWorld::AddTorque(BodyHandle body, const Vector3& torque)
  {
    if (const JPH::BodyID id = impl_->FindMovable(body); !id.IsInvalid()) {
      impl_->system.GetBodyInterface().AddTorque(id, ToJolt(torque));
    }
  }

  void PhysicsWorld::AddImpulse(BodyHandle body, const Vector3& impulse)
  {
    if (const JPH::BodyID id = impl_->FindMovable(body); !id.IsInvalid()) {
      impl_->system.GetBodyInterface().AddImpulse(id, ToJolt(impulse));
    }
  }

  void PhysicsWorld::AddForceAtPosition(BodyHandle body, const Vector3& force, const Vector3& position)
  {
    if (const JPH::BodyID id = impl_->FindMovable(body); !id.IsInvalid()) {
      impl_->system.GetBodyInterface().AddForce(id, ToJolt(force), ToJolt(position));
    }
  }

  void PhysicsWorld::AddImpulseAtPosition(BodyHandle body, const Vector3& impulse, const Vector3& position)
  {
    if (const JPH::BodyID id = impl_->FindMovable(body); !id.IsInvalid()) {
      impl_->system.GetBodyInterface().AddImpulse(id, ToJolt(impulse), ToJolt(position));
    }
  }

  void PhysicsWorld::SetLinearVelocity(BodyHandle body, const Vector3& velocity)
  {
    if (const JPH::BodyID id = impl_->FindMovable(body); !id.IsInvalid()) {
      impl_->system.GetBodyInterface().SetLinearVelocity(id, ToJolt(velocity));
    }
  }

  void PhysicsWorld::SetAngularVelocity(BodyHandle body, const Vector3& velocity)
  {
    if (const JPH::BodyID id = impl_->FindMovable(body); !id.IsInvalid()) {
      impl_->system.GetBodyInterface().SetAngularVelocity(id, ToJolt(velocity));
    }
  }

  void PhysicsWorld::WakeBody(BodyHandle body)
  {
    impl_->WakeBody(body);
  }

  void PhysicsWorld::SleepBody(BodyHandle body)
  {
    if (const JPH::BodyID id = impl_->FindMovable(body); !id.IsInvalid()) {
      impl_->system.GetBodyInterface().DeactivateBody(id);
    }
  }

  PhysicsWorld::CharacterHandle PhysicsWorld::CreateCharacter(const CharacterSettings& settings)
  {
    const Pose                      pose  = ReadPose(*settings.actor);
    const JPH::RefConst<JPH::Shape> shape = MakeCharacterShape(settings, pose.scale);

    const JPH::Ref<JPH::CharacterVirtualSettings> creation = new JPH::CharacterVirtualSettings();
    creation->mShape         = shape;
    creation->mMaxSlopeAngle = settings.maxSlopeAngle;
    creation->mMaxStrength   = (std::max)(settings.pushStrength, 0.0f);
    creation->mMass          = (std::max)(settings.mass, 0.0f);
    // キャラクター自身は世界のボディではないので、同じ形のボディを中に持たせる（レイ・トリガー・他のボディに見える）
    creation->mInnerBodyShape = shape;
    creation->mInnerBodyLayer = ToObjectLayer(BodyKind::Kinematic, settings.layer);

    CharacterRecord record;
    record.settings         = settings;
    record.shapeScale       = pose.scale;
    record.lastTranslate    = settings.actor->GetTransform().translate;
    record.previousPosition = record.currentPosition = pose.position;
    record.character        = new JPH::CharacterVirtual(creation, ToJolt(pose.position), JPH::Quat::sIdentity(), &impl_->system);
    // 中のボディにも、他のボディと同じく Actor を持たせる（当たらない組を、Actor で引く）
    record.character->SetUserData(reinterpret_cast<JPH::uint64>(settings.actor));

    const JPH::BodyID inner = record.character->GetInnerBodyID();
    if (inner.IsInvalid()) {
      Logger::Log("PhysicsWorld: too many bodies (max %u). The character was not created", kMaxBodies);
      return kInvalidCharacter;
    }
    const CharacterHandle handle = inner.GetIndexAndSequenceNumber();
    if (settings.owner) {
      impl_->ownerBodies[settings.owner] = handle;
    }
    impl_->characters.emplace(handle, std::move(record));
    return handle;
  }

  void PhysicsWorld::DestroyCharacter(CharacterHandle character)
  {
    const auto found = impl_->characters.find(character);
    if (found == impl_->characters.end()) {
      return;
    }
    Component* const owners[] = { found->second.settings.owner };
    impl_->ForgetOwners(owners);
    // 中に持たせたボディは、CharacterVirtual が消す
    impl_->characters.erase(found);
  }

  void PhysicsWorld::SetCharacterEnabled(CharacterHandle character, bool enabled)
  {
    const auto found = impl_->characters.find(character);
    if (found == impl_->characters.end() || found->second.isInWorld == enabled) {
      return;
    }
    CharacterRecord&    record = found->second;
    JPH::BodyInterface& bodies = impl_->system.GetBodyInterface();
    record.isInWorld = enabled;
    if (enabled) {
      // 出ている間に Actor が動いた分を写してから戻す
      impl_->SyncCharacter(record, true);
      bodies.AddBody(record.character->GetInnerBodyID(), JPH::EActivation::Activate);
      return;
    }
    bodies.RemoveBody(record.character->GetInnerBodyID());
    // 当たっていた組は、次の配布で Exit を出す
    Component* const owners[] = { record.settings.owner };
    impl_->MoveToPendingExits(owners);
  }

  void PhysicsWorld::SetCharacterVelocity(CharacterHandle character, const Vector3& velocity)
  {
    if (const auto found = impl_->characters.find(character); found != impl_->characters.end()) {
      found->second.character->SetLinearVelocity(ToJolt(velocity));
    }
  }

  void PhysicsWorld::SetCharacterPushStrength(CharacterHandle character, float pushStrength)
  {
    if (const auto found = impl_->characters.find(character); found != impl_->characters.end()) {
      found->second.settings.pushStrength = pushStrength;
      found->second.character->SetMaxStrength((std::max)(pushStrength, 0.0f));
    }
  }

  void PhysicsWorld::SetCharacterMass(CharacterHandle character, float mass)
  {
    if (const auto found = impl_->characters.find(character); found != impl_->characters.end()) {
      found->second.settings.mass = mass;
      found->second.character->SetMass((std::max)(mass, 0.0f));
    }
  }

  void PhysicsWorld::SetCharacterRidesGround(CharacterHandle character, bool ridesGround)
  {
    if (const auto found = impl_->characters.find(character); found != impl_->characters.end()) {
      found->second.settings.ridesGround = ridesGround;
    }
  }

  bool PhysicsWorld::SetCharacterHeight(CharacterHandle character, float height)
  {
    const auto found = impl_->characters.find(character);
    if (found == impl_->characters.end()) {
      return false;
    }
    CharacterRecord&  record  = found->second;
    CharacterSettings resized = record.settings;
    // 足元（カプセルの下の端）を保つ：伸びた分の半分だけ、中心を上げる
    resized.offset.y += (height - resized.height) * 0.5f;
    resized.height    = height;
    const JPH::RefConst<JPH::Shape> shape = MakeCharacterShape(resized, record.shapeScale);

    // 新しい形が何かに重なるなら、Jolt は形を替えずに false を返す（Jolt のサンプルと同じ許容の深さ）
    const LayerMaskFilter  layerFilter(collisionMask_[(std::min)(resized.layer, static_cast<uint32_t>(kMaxLayers - 1))]);
    const OtherActorFilter bodyFilter(resized.actor, impl_->ignoredPairs);
    const float            maxPenetration = 1.5f * impl_->system.GetPhysicsSettings().mPenetrationSlop;
    if (!record.character->SetShape(shape, maxPenetration, {}, layerFilter, bodyFilter, {}, impl_->tempAllocator)) {
      return false;
    }
    record.character->SetInnerBodyShape(shape);
    record.settings = resized;
    return true;
  }

  PhysicsWorld::JointHandle PhysicsWorld::CreateJoint(const JointSettings& settings)
  {
    const auto body      = impl_->bodies.find(settings.body);
    const auto connected = impl_->bodies.find(settings.connectedBody);
    if (body == impl_->bodies.end() || settings.body == settings.connectedBody) {
      return kInvalidJoint;
    }
    // 相手が無ければ、無効な ID を渡す（Jolt が、ワールドに固定されたボディを相手にする）
    const bool        hasConnected = connected != impl_->bodies.end();
    const JPH::BodyID ids[2]       = { body->second.id, hasConnected ? connected->second.id : JPH::BodyID() };
    const JPH::Vec3   anchor       = ToJolt(settings.anchor);

    JPH::BodyInterface& bodyInterface = impl_->system.GetBodyInterface();
    JointRecord         record;
    switch (settings.type) {
    case JointType::Fixed: {
      // 今の位置と向きのまま固める
      JPH::FixedConstraintSettings fixed;
      fixed.mAutoDetectPoint = true;
      record.constraint      = bodyInterface.CreateConstraint(&fixed, ids[0], ids[1]);
      break;
    }
    case JointType::Point: {
      JPH::PointConstraintSettings point;
      point.mPoint1 = point.mPoint2 = anchor;
      record.constraint             = bodyInterface.CreateConstraint(&point, ids[0], ids[1]);
      break;
    }
    case JointType::Hinge: {
      JPH::HingeConstraintSettings hinge;
      hinge.mPoint1 = hinge.mPoint2 = anchor;
      hinge.mHingeAxis1 = hinge.mHingeAxis2 = ToJolt(settings.axis).NormalizedOr(JPH::Vec3::sAxisY());
      hinge.mNormalAxis1 = hinge.mNormalAxis2 = hinge.mHingeAxis1.GetNormalizedPerpendicular();
      // Jolt の決まり：下限は -π〜0、上限は 0〜π で、同じ値（動けない）にはできない
      hinge.mLimitsMin         = std::clamp(settings.minAngle, -JPH::JPH_PI, 0.0f);
      hinge.mLimitsMax         = std::clamp(settings.maxAngle, hinge.mLimitsMin < 0.0f ? 0.0f : 0.001f, JPH::JPH_PI);
      hinge.mMaxFrictionTorque = (std::max)(settings.friction, 0.0f);
      record.constraint        = bodyInterface.CreateConstraint(&hinge, ids[0], ids[1]);
      break;
    }
    case JointType::Distance: {
      JPH::DistanceConstraintSettings distance;
      distance.mPoint1 = anchor;
      distance.mPoint2 = ToJolt(settings.connectedAnchor);
      // 負の値は、今の距離
      const float current   = (settings.connectedAnchor - settings.anchor).Length();
      distance.mMinDistance = settings.minDistance < 0.0f ? current : settings.minDistance;
      distance.mMaxDistance = (std::max)(settings.maxDistance < 0.0f ? current : settings.maxDistance, distance.mMinDistance);
      // ばね：範囲の端で固く止めずに、ばねで引き戻す
      if (settings.spring > 0.0f) {
        distance.mLimitsSpringSettings.mMode      = JPH::ESpringMode::StiffnessAndDamping;
        distance.mLimitsSpringSettings.mStiffness = settings.spring;
        distance.mLimitsSpringSettings.mDamping   = (std::max)(settings.damper, 0.0f);
      }
      record.constraint = bodyInterface.CreateConstraint(&distance, ids[0], ids[1]);
      break;
    }
    }
    record.bodies[0]  = settings.body;
    record.bodies[1]  = hasConnected ? settings.connectedBody : kInvalidBody;
    record.type       = settings.type;
    record.breakForce = settings.breakForce;
    record.owner      = settings.owner;
    if (settings.type == JointType::Hinge) {
      ApplyHingeMotor(static_cast<JPH::HingeConstraint&>(*record.constraint), settings.motorSpeed, settings.motorTorque);
    }
    // Unity と同じ：つないだ 2 つのボディは、当てると決めない限り当たらない
    if (hasConnected && !settings.enableCollision) {
      record.ignoredPair = MakeActorPair(body->second.settings.actor, connected->second.settings.actor);
      impl_->ignoredPairs.byJoint.push_back(record.ignoredPair);
    }
    impl_->system.AddConstraint(record.constraint);

    const JointHandle handle = impl_->nextJoint++;
    impl_->joints.emplace(handle, std::move(record));
    impl_->RefreshJoints(settings.body);
    impl_->WakeBody(settings.body);
    impl_->WakeBody(settings.connectedBody);
    return handle;
  }

  void PhysicsWorld::DestroyJoint(JointHandle joint)
  {
    if (const auto found = impl_->joints.find(joint); found != impl_->joints.end()) {
      impl_->RemoveJoint(found->second);
      impl_->joints.erase(found);
    }
  }

  void PhysicsWorld::SetJointMotor(JointHandle joint, float speed, float maxTorque)
  {
    const auto found = impl_->joints.find(joint);
    if (found == impl_->joints.end() || found->second.type != JointType::Hinge) {
      return;
    }
    ApplyHingeMotor(static_cast<JPH::HingeConstraint&>(*found->second.constraint), speed, maxTorque);
    impl_->WakeBody(found->second.bodies[0]);
    impl_->WakeBody(found->second.bodies[1]);
  }

  std::optional<PhysicsWorld::RaycastHit> PhysicsWorld::Raycast(const Vector3& origin, const Vector3& direction, float maxDistance, uint32_t layerMask, bool hitsTriggers) const
  {
    const float length = direction.Length();
    if (length <= 0.0f || maxDistance <= 0.0f) {
      return std::nullopt;
    }
    const JPH::RRayCast ray(ToJolt(origin), ToJolt(direction) * (maxDistance / length));
    JPH::ClosestHitCollisionCollector<JPH::CastRayCollector> collector;
    impl_->CastRay(ray, collector, layerMask, hitsTriggers);
    return collector.HadHit() ? impl_->ToRaycastHit(ray, collector.mHit, maxDistance) : std::nullopt;
  }

  std::vector<PhysicsWorld::RaycastHit> PhysicsWorld::RaycastAll(const Vector3& origin, const Vector3& direction, float maxDistance, uint32_t layerMask, bool hitsTriggers) const
  {
    std::vector<RaycastHit> hits;
    const float             length = direction.Length();
    if (length <= 0.0f || maxDistance <= 0.0f) {
      return hits;
    }
    const JPH::RRayCast ray(ToJolt(origin), ToJolt(direction) * (maxDistance / length));
    JPH::AllHitCollisionCollector<JPH::CastRayCollector> collector;
    impl_->CastRay(ray, collector, layerMask, hitsTriggers);
    collector.Sort();
    for (const JPH::RayCastResult& result : collector.mHits) {
      // 形を 2 つ以上持つボディとメッシュは、形や三角形ごとに当たりが来る。Actor ごとに、一番近い 1 つだけを残す
      const std::optional<RaycastHit> hit = impl_->ToRaycastHit(ray, result, maxDistance);
      if (hit && std::ranges::none_of(hits, [&hit](const RaycastHit& kept) { return kept.actor == hit->actor; })) {
        hits.push_back(*hit);
      }
    }
    return hits;
  }

  std::optional<PhysicsWorld::RaycastHit> PhysicsWorld::SphereCast(const Vector3& origin, float radius, const Vector3& direction, float maxDistance, uint32_t layerMask, bool hitsTriggers) const
  {
    return impl_->CastShape({ .type = ShapeType::Sphere, .radius = radius }, origin, { 0.0f, 0.0f, 0.0f, 1.0f }, direction, maxDistance, layerMask, hitsTriggers);
  }

  std::optional<PhysicsWorld::RaycastHit> PhysicsWorld::BoxCast(const Vector3& center, const Vector3& size, const Quaternion& rotation, const Vector3& direction,
                                                                float maxDistance, uint32_t layerMask, bool hitsTriggers) const
  {
    return impl_->CastShape({ .type = ShapeType::Box, .size = size }, center, rotation, direction, maxDistance, layerMask, hitsTriggers);
  }

  std::optional<PhysicsWorld::RaycastHit> PhysicsWorld::CapsuleCast(const Vector3& center, float radius, float height, const Quaternion& rotation, const Vector3& direction,
                                                                    float maxDistance, uint32_t layerMask, bool hitsTriggers) const
  {
    return impl_->CastShape({ .type = ShapeType::Capsule, .radius = radius, .height = height }, center, rotation, direction, maxDistance, layerMask, hitsTriggers);
  }

  std::vector<Actor*> PhysicsWorld::OverlapSphere(const Vector3& center, float radius, uint32_t layerMask, bool hitsTriggers) const
  {
    return impl_->CollideShape({ .type = ShapeType::Sphere, .radius = radius }, center, { 0.0f, 0.0f, 0.0f, 1.0f }, layerMask, hitsTriggers);
  }

  std::vector<Actor*> PhysicsWorld::OverlapBox(const Vector3& center, const Vector3& size, const Quaternion& rotation, uint32_t layerMask, bool hitsTriggers) const
  {
    return impl_->CollideShape({ .type = ShapeType::Box, .size = size }, center, rotation, layerMask, hitsTriggers);
  }

  std::vector<Actor*> PhysicsWorld::OverlapCapsule(const Vector3& center, float radius, float height, const Quaternion& rotation, uint32_t layerMask, bool hitsTriggers) const
  {
    return impl_->CollideShape({ .type = ShapeType::Capsule, .radius = radius, .height = height }, center, rotation, layerMask, hitsTriggers);
  }

  void PhysicsWorld::RemoveLayer(uint32_t index)
  {
    if (index >= layerNames_.size()) {
      return;
    }
    layerNames_.erase(layerNames_.begin() + index);
    RemapCollisionMasks([index](uint32_t layer) -> std::optional<uint32_t> {
      if (layer == index) {
        return std::nullopt;
      }
      return layer > index ? layer - 1 : layer;
    });
  }

  void PhysicsWorld::SwapLayers(uint32_t layerA, uint32_t layerB)
  {
    if (layerA >= layerNames_.size() || layerB >= layerNames_.size()) {
      return;
    }
    std::swap(layerNames_[layerA], layerNames_[layerB]);
    RemapCollisionMasks([layerA, layerB](uint32_t layer) -> std::optional<uint32_t> {
      return layer == layerA ? layerB : layer == layerB ? layerA : layer;
    });
  }

  void PhysicsWorld::RemapCollisionMasks(const std::function<std::optional<uint32_t>(uint32_t)>& remap)
  {
    std::array<uint32_t, kMaxLayers> remapped{};
    uint32_t                         kept = 0;  // 付け替えの先になった層
    for (uint32_t layerA = 0; layerA < kMaxLayers; ++layerA) {
      const std::optional<uint32_t> newA = remap(layerA);
      if (!newA || *newA >= kMaxLayers) {
        continue;
      }
      kept |= 1u << *newA;
      for (uint32_t layerB = 0; layerB < kMaxLayers; ++layerB) {
        if (!CanCollide(layerA, layerB)) {
          continue;
        }
        const std::optional<uint32_t> newB = remap(layerB);
        if (newB && *newB < kMaxLayers) {
          remapped[*newA] |= 1u << *newB;
        }
      }
    }
    // 詰めて空いた番号は、まだ決めていない層なので、全部の層と当たる状態に戻す
    for (uint32_t layer = 0; layer < kMaxLayers; ++layer) {
      remapped[layer] |= ~kept;
      if (!(kept & (1u << layer))) {
        remapped[layer] = ~0u;
      }
    }
    collisionMask_ = remapped;
  }

  void PhysicsWorld::IgnoreCollision(const Actor* a, const Actor* b, bool ignore)
  {
    if (!a || !b || a == b) {
      return;
    }
    const ActorPair         pair  = MakeActorPair(a, b);
    std::vector<ActorPair>& pairs = impl_->ignoredPairs.byRequest;
    const auto              found = std::ranges::find(pairs, pair);
    if (ignore && found == pairs.end()) {
      pairs.push_back(pair);
    } else if (!ignore && found != pairs.end()) {
      pairs.erase(found);
    }
    // 寝ているボディは接触を調べ直さないので、起こす
    for (const auto& [handle, record] : impl_->bodies) {
      if (record.settings.actor == a || record.settings.actor == b) {
        impl_->WakeBody(handle);
      }
    }
  }

  void PhysicsWorld::ClearIgnoredCollisions(const Actor* actor)
  {
    std::erase_if(impl_->ignoredPairs.byRequest, [actor](const ActorPair& pair) { return pair.first == actor || pair.second == actor; });
  }

  void PhysicsWorld::DrawDebug([[maybe_unused]] const Vector3& cameraPosition)
  {
#ifdef JPH_DEBUG_RENDERER
    debugLineCount_ = 0;
    if (!debugDrawEnabled_) {
      return;
    }
    const LineRenderer* lines       = LineRenderer::GetInstance();
    const uint32_t      firstVertex = lines->GetBatchMark();
    // 視点から遠い形は、Jolt が粗い形で描く（線の数を抑える）
    impl_->debugRenderer.SetCameraPos(ToJolt(cameraPosition));
    static_assert(static_cast<int>(DebugColor::Island) == static_cast<int>(JPH::BodyManager::EShapeColor::IslandColor), "DebugColor の並びが Jolt の EShapeColor とずれている");
    JPH::BodyManager::DrawSettings settings;
    settings.mDrawShapeWireframe        = true;
    settings.mDrawShapeColor            = static_cast<JPH::BodyManager::EShapeColor>(debugDrawOptions_.color);
    settings.mDrawVelocity              = debugDrawOptions_.drawsVelocity;
    settings.mDrawCenterOfMassTransform = debugDrawOptions_.drawsCenterOfMass;
    settings.mDrawBoundingBox           = debugDrawOptions_.drawsBoundingBox;
    impl_->system.DrawBodies(settings, &impl_->debugRenderer);
    impl_->system.DrawConstraints(&impl_->debugRenderer);
    debugLineCount_ = (lines->GetBatchMark() - firstVertex) / LineRenderer::kVertexCountLine;
#endif
  }

  void PhysicsWorld::DrawShapes([[maybe_unused]] const Actor& actor, [[maybe_unused]] std::span<const Shape> shapes,
                                [[maybe_unused]] const Vector3& cameraPosition, [[maybe_unused]] bool isUpright)
  {
#ifdef JPH_DEBUG_RENDERER
    const Pose                      pose     = ReadPose(actor);
    const JPH::RefConst<JPH::Shape> shape    = MakeShape(shapes, pose.scale);
    const JPH::Quat                 rotation = isUpright ? JPH::Quat::sIdentity() : ToJolt(pose.rotation);
    impl_->debugRenderer.SetCameraPos(ToJolt(cameraPosition));
    // Jolt の形は重心が原点なので、ボディの原点から重心までを足す
    const JPH::RMat44 transform = JPH::RMat44::sRotationTranslation(rotation, ToJolt(pose.position)).PreTranslated(shape->GetCenterOfMass());
    shape->Draw(&impl_->debugRenderer, transform, JPH::Vec3::sReplicate(1.0f), JPH::Color::sGreen, false, true);
#endif
  }

  void PhysicsWorld::SetCollisionMask(uint32_t layerA, uint32_t layerB, bool canCollide)
  {
    if (layerA >= kMaxLayers || layerB >= kMaxLayers) {
      return;
    }
    const uint32_t bitA = 1u << layerA;
    const uint32_t bitB = 1u << layerB;
    if (canCollide) {
      collisionMask_[layerA] |= bitB;
      collisionMask_[layerB] |= bitA;
    } else {
      collisionMask_[layerA] &= ~bitB;
      collisionMask_[layerB] &= ~bitA;
    }
  }

  void PhysicsWorld::SetLayerNames(std::vector<std::string> names)
  {
    if (names.size() > kMaxLayers) {
      names.resize(kMaxLayers);
    }
    layerNames_ = std::move(names);
  }

  void PhysicsWorld::SetGravity(const Vector3& gravity)
  {
    impl_->system.SetGravity(ToJolt(gravity));
  }

  bool PhysicsWorld::CanCollide(uint32_t layerA, uint32_t layerB) const
  {
    return layerA < kMaxLayers && layerB < kMaxLayers && ((collisionMask_[layerA] >> layerB) & 1u) != 0;
  }

  std::string PhysicsWorld::GetLayerLabel(uint32_t layer) const
  {
    return layer < layerNames_.size() && !layerNames_[layer].empty() ? layerNames_[layer] : "Layer " + std::to_string(layer);
  }

  Vector3 PhysicsWorld::GetGravity() const
  {
    return ToEngine(impl_->system.GetGravity());
  }

  Vector3 PhysicsWorld::GetLinearVelocity(BodyHandle body) const
  {
    const JPH::BodyID id = impl_->FindMovable(body);
    return id.IsInvalid() ? Vector3{} : ToEngine(impl_->system.GetBodyInterface().GetLinearVelocity(id));
  }

  Vector3 PhysicsWorld::GetAngularVelocity(BodyHandle body) const
  {
    const JPH::BodyID id = impl_->FindMovable(body);
    return id.IsInvalid() ? Vector3{} : ToEngine(impl_->system.GetBodyInterface().GetAngularVelocity(id));
  }

  bool PhysicsWorld::IsBodySleeping(BodyHandle body) const
  {
    const JPH::BodyID id = impl_->FindMovable(body);
    return !id.IsInvalid() && !impl_->system.GetBodyInterface().IsActive(id);
  }

  Vector3 PhysicsWorld::GetCharacterVelocity(CharacterHandle character) const
  {
    const auto found = impl_->characters.find(character);
    return found == impl_->characters.end() ? Vector3{} : ToEngine(found->second.character->GetLinearVelocity());
  }

  bool PhysicsWorld::IsCharacterGrounded(CharacterHandle character) const
  {
    const auto found = impl_->characters.find(character);
    return found != impl_->characters.end() && found->second.isInWorld
        && found->second.character->GetGroundState() == JPH::CharacterBase::EGroundState::OnGround;
  }

  bool PhysicsWorld::IsCharacterOnSteepGround(CharacterHandle character) const
  {
    const auto found = impl_->characters.find(character);
    return found != impl_->characters.end() && found->second.isInWorld
        && found->second.character->GetGroundState() == JPH::CharacterBase::EGroundState::OnSteepGround;
  }

  Vector3 PhysicsWorld::GetCharacterGroundNormal(CharacterHandle character) const
  {
    const auto found = impl_->characters.find(character);
    const bool isSupported = found != impl_->characters.end() && found->second.isInWorld && found->second.character->IsSupported();
    return isSupported ? ToEngine(found->second.character->GetGroundNormal()) : Vector3{};
  }

  Actor* PhysicsWorld::GetCharacterGroundActor(CharacterHandle character) const
  {
    const auto found = impl_->characters.find(character);
    if (found == impl_->characters.end() || !found->second.isInWorld || !found->second.character->IsSupported()) {
      return nullptr;
    }
    const JPH::CharacterVirtual& virtualCharacter = *found->second.character;
    const Component*             owner            = impl_->FindOwner(virtualCharacter.GetGroundBodyID(), virtualCharacter.GetGroundSubShapeID());
    return owner ? owner->GetOwner() : nullptr;
  }

  bool PhysicsWorld::HasJoint(JointHandle joint) const
  {
    return impl_->joints.contains(joint);
  }

  size_t PhysicsWorld::GetBodyCount() const
  {
    return impl_->bodies.size() + impl_->characters.size();
  }

  PhysicsWorld::WorldStats PhysicsWorld::GetWorldStats() const
  {
    WorldStats stats;
    stats.bodyCount         = GetBodyCount();
    stats.awakeCount        = impl_->system.GetNumActiveBodies(JPH::EBodyType::RigidBody);
    stats.touchingPairCount = impl_->touching.size();
    const JPH::BodyInterface& bodies = impl_->system.GetBodyInterface();
    for (const auto& [handle, record] : impl_->bodies) {
      stats.enabledCount += record.isInWorld ? 1 : 0;
      ++stats.layerBodyCounts[bodies.GetObjectLayer(record.id) & kLayerBits];
    }
    for (const auto& [handle, record] : impl_->characters) {
      stats.enabledCount += record.isInWorld ? 1 : 0;
      ++stats.layerBodyCounts[(std::min)(record.settings.layer, static_cast<uint32_t>(kMaxLayers - 1))];
    }
    return stats;
  }

} // namespace Tako

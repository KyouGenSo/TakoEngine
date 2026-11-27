#include "PhysicsWorld.h"

// Jolt のヘッダは、Windows.h を読むヘッダより先に置く（Windows.h の min / max マクロに当たる）
#include <Jolt/Jolt.h>

#include <Jolt/Core/Factory.h>
#include <Jolt/Core/JobSystemThreadPool.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Body/BodyLock.h>
#include <Jolt/Physics/Collision/CastResult.h>
#include <Jolt/Physics/Collision/ContactListener.h>
#include <Jolt/Physics/Collision/RayCast.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/CapsuleShape.h>
#include <Jolt/Physics/Collision/Shape/EmptyShape.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>
#include <Jolt/Physics/Collision/Shape/StaticCompoundShape.h>
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
    using PairList  = std::vector<OwnerPair>;

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
    /// ステップの間に触れていた組を集める。計算用のスレッドから同時に呼ばれ、ボディは読むだけ
    /// </summary>
    class ContactCollector final : public JPH::ContactListener {
    public: //メンバー関数
      void OnContactAdded(const JPH::Body& a, const JPH::Body& b, const JPH::ContactManifold& manifold, JPH::ContactSettings&) override { Collect(a, b, manifold); }
      void OnContactPersisted(const JPH::Body& a, const JPH::Body& b, const JPH::ContactManifold& manifold, JPH::ContactSettings&) override { Collect(a, b, manifold); }

    public: //メンバー変数
      PairList pairs;  ///< ステップの外で、メインスレッドが読んで空にする

    private: //非公開関数
      void Collect(const JPH::Body& a, const JPH::Body& b, const JPH::ContactManifold& manifold) {
        auto* ownerA = reinterpret_cast<Component*>(a.GetShape()->GetSubShapeUserData(manifold.mSubShapeID1));
        auto* ownerB = reinterpret_cast<Component*>(b.GetShape()->GetSubShapeUserData(manifold.mSubShapeID2));
        // 通知先の無い形と、同じ Actor の形どうしは知らせない
        if (!ownerA || !ownerB || ownerA->GetOwner() == ownerB->GetOwner()) {
          return;
        }
        const std::lock_guard lock(mutex_);
        pairs.push_back(std::minmax(ownerA, ownerB));
      }

    private: //メンバー変数
      std::mutex mutex_;
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
    /// scale の掛け方は Unity と同じ：Box は軸ごと、Sphere は 3 軸の最大、Capsule の半径は X と Z の大きい方・高さは Y
    /// </summary>
    JPH::Ref<JPH::Shape> MakeLeaf(const PhysicsWorld::Shape& shape, const Vector3& scale) {
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
      case PhysicsWorld::ShapeType::Sphere:
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
      // 形の無いボディ（Trigger しか持たない Rigidbody など）は、何にも当たらない
      if (shapes.empty()) {
        return new JPH::EmptyShape();
      }
      JPH::StaticCompoundShapeSettings compound;
      for (const PhysicsWorld::Shape& shape : shapes) {
        const Vector3 position{ shape.position.x * scale.x, shape.position.y * scale.y, shape.position.z * scale.z };
        const Vector3 shapeScale{ shape.scale.x * scale.x, shape.scale.y * scale.y, shape.scale.z * scale.z };
        compound.AddShape(ToJolt(position), ToJolt(Quat::Normalize(shape.rotation)), MakeLeaf(shape, shapeScale).GetPtr());
      }
      const JPH::ShapeSettings::ShapeResult result = compound.Create();
      if (result.HasError()) {
        Logger::Log("PhysicsWorld: failed to build a shape: %s", result.GetError().c_str());
        return new JPH::SphereShape(kMinShapeSize);
      }
      return result.Get();
    }

    /// <summary>
    /// 質量を mass にした、shape の質量と回りにくさ（回りにくさは、形の大きさから決まる）
    /// </summary>
    JPH::MassProperties MakeMassProperties(const JPH::Shape& shape, float mass) {
      JPH::MassProperties properties = shape.GetMassProperties();
      properties.ScaleToMass((std::max)(mass, kMinMass));
      return properties;
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
    explicit Impl(const PhysicsWorld& world) : objectLayerFilter(world) {}

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
    /// pairs の両方の Actor の全コンポーネントに handler を呼ぶ。pairs は、呼んだ先で書き換わらない物を渡す
    /// </summary>
    void Dispatch(const PairList& pairs, void (Component::* handler)(Actor*), FrameStats& stats);

    void DispatchPendingExits(FrameStats& stats);

    BroadPhaseLayers                         broadPhaseLayers;
    ObjectVsBroadPhaseFilter                 objectVsBroadPhaseFilter;
    ObjectLayerFilter                        objectLayerFilter;
    ContactCollector                         contactCollector;
    JPH::TempAllocatorImplWithMallocFallback tempAllocator{ kTempAllocatorBytes };
    JPH::JobSystemThreadPool                 jobSystem{ JPH::cMaxPhysicsJobs, JPH::cMaxPhysicsBarriers };
#ifdef JPH_DEBUG_RENDERER
    LineDebugRenderer                        debugRenderer;
#endif
    JPH::PhysicsSystem                       system;

    std::unordered_map<BodyHandle, BodyRecord> bodies;
    std::unordered_map<Component*, BodyHandle> ownerBodies;  ///< 通知先 → その形を持つボディ

    //当たりの通知。どれも、通知先の組の一覧
    PairList touching;          ///< 今当たっている組（並べ替え済み）
    PairList pendingExits;      ///< 世界から出したボディの組。次の配布で Exit を出す
    PairList enteredThisFrame;  ///< このフレームに当たり始めた組（Stay は次のフレームから）
    PairList staying;           ///< Stay を配る間の写し（毎フレーム使うので、確保を使い回す）
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
    bodyInterface.SetShape(record.id, MakeShape(record.shapes, record.shapeScale), false, ActivationOf(record.settings.kind));
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
    const auto involves = [owners](const OwnerPair& pair) { return Involves(pair, owners); };
    std::erase_if(touching, involves);
    std::erase_if(pendingExits, involves);
    std::erase_if(enteredThisFrame, involves);
    for (Component* owner : owners) {
      ownerBodies.erase(owner);
    }
  }

  bool PhysicsWorld::Impl::IsResting(Component* owner) const
  {
    const auto body = ownerBodies.find(owner);
    return body != ownerBodies.end() && !system.GetBodyInterface().IsActive(JPH::BodyID(body->second));
  }

  void PhysicsWorld::Impl::Dispatch(const PairList& pairs, void (Component::* handler)(Actor*), FrameStats& stats)
  {
    if (pairs.empty()) {
      return;
    }
    const Clock::time_point start = Clock::now();
    for (const OwnerPair& pair : pairs) {
      // 前の通知の中でボディを消された通知先には、知らせない
      if (!ownerBodies.contains(pair.first) || !ownerBodies.contains(pair.second)) {
        continue;
      }
      Actor* const actors[2] = { pair.first->GetOwner(), pair.second->GetOwner() };
      for (int side = 0; side < 2; ++side) {
        // 受け手がコンポーネントを足しても壊れないよう、添字で回す
        const auto& components = actors[side]->GetComponents();
        for (size_t i = 0; i < components.size(); ++i) {
          (components[i].get()->*handler)(actors[1 - side]);
        }
      }
      ++stats.callbackCount;
    }
    stats.callbackMs += std::chrono::duration<float, std::milli>(Clock::now() - start).count();
  }

  void PhysicsWorld::Impl::DispatchPendingExits(FrameStats& stats)
  {
    // 通知の中でまた止められても壊れないよう、取り出してから配る
    PairList exits;
    exits.swap(pendingExits);
    Dispatch(exits, &Component::OnCollisionExit, stats);
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
  }

  void PhysicsWorld::Step()
  {
    // FixedTick の中で止めたボディの Exit を、入れ直した分の Enter より先に出す
    impl_->DispatchPendingExits(frameStats_);
    impl_->SyncBodies(true);

    const Clock::time_point start    = Clock::now();
    PairList&               reported = impl_->contactCollector.pairs;
    reported.clear();
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

    // このステップで触れていた組と、前から当たっていた組を比べる
    std::ranges::sort(reported);
    const auto duplicates = std::ranges::unique(reported);
    reported.erase(duplicates.begin(), duplicates.end());

    PairList entered;
    std::ranges::set_difference(reported, impl_->touching, std::back_inserter(entered));
    PairList exited;
    PairList touching = reported;
    for (const OwnerPair& pair : impl_->touching) {
      if (std::ranges::binary_search(reported, pair)) {
        continue;
      }
      // 寝ているボディどうしは Jolt が調べないので、報告が来ない。離れたのではないので、当たっている扱いを続ける
      if (impl_->IsResting(pair.first) && impl_->IsResting(pair.second)) {
        touching.push_back(pair);
      } else {
        exited.push_back(pair);
      }
    }
    std::ranges::sort(touching);
    impl_->touching = std::move(touching);
    impl_->enteredThisFrame.insert(impl_->enteredThisFrame.end(), entered.begin(), entered.end());

    impl_->Dispatch(entered, &Component::OnCollisionEnter, frameStats_);
    impl_->Dispatch(exited, &Component::OnCollisionExit, frameStats_);
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

    impl_->DispatchPendingExits(frameStats_);

    // 当たり続けている組に Stay。このフレームに当たり始めた組は、次のフレームから
    PairList& staying = impl_->staying;
    staying.clear();
    std::ranges::sort(impl_->enteredThisFrame);
    std::ranges::set_difference(impl_->touching, impl_->enteredThisFrame, std::back_inserter(staying));
    impl_->enteredThisFrame.clear();
    impl_->Dispatch(staying, &Component::OnCollisionStay, frameStats_);
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
    const JPH::RefConst<JPH::Shape> shape = MakeShape(record.shapes, record.shapeScale);
    JPH::BodyCreationSettings       creation(shape, ToJolt(pose.position), ToJolt(pose.rotation), motion, ToObjectLayer(settings.kind, settings.layer));
    // Trigger は、動かない実体や他の Trigger との重なりも知らせる
    creation.mIsSensor                     = isTrigger;
    creation.mCollideKinematicVsNonDynamic = isTrigger;
    creation.mUseManifoldReduction         = record.shapes.size() <= 1;
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
      return;
    }
    bodies.RemoveBody(record.id);
    // 当たっていた組は、次の配布で Exit を出す
    const auto moved = std::ranges::stable_partition(impl_->touching, [&record](const OwnerPair& pair) { return !Involves(pair, record.owners); });
    impl_->pendingExits.insert(impl_->pendingExits.end(), moved.begin(), moved.end());
    impl_->touching.erase(moved.begin(), moved.end());
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
    BodyRecord&    record = found->second;
    const BodyKind kind   = record.settings.kind;
    Actor* const   actor  = record.settings.actor;
    record.settings       = settings;
    record.settings.kind  = kind;
    record.settings.actor = actor;
    impl_->ApplySettings(record);
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

  std::optional<PhysicsWorld::RaycastHit> PhysicsWorld::Raycast(const Vector3& origin, const Vector3& direction, float maxDistance, uint32_t layerMask, bool hitsTriggers) const
  {
    const float length = direction.Length();
    if (length <= 0.0f || maxDistance <= 0.0f) {
      return std::nullopt;
    }
    const JPH::RRayCast     ray(ToJolt(origin), ToJolt(direction) * (maxDistance / length));
    const LayerMaskFilter   layerFilter(layerMask);
    const TriggerBodyFilter bodyFilter(hitsTriggers);
    JPH::RayCastResult      result;
    if (!impl_->system.GetNarrowPhaseQuery().CastRay(ray, result, {}, layerFilter, bodyFilter)) {
      return std::nullopt;
    }
    const JPH::BodyLockRead lock(impl_->system.GetBodyLockInterface(), result.mBodyID);
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

  void PhysicsWorld::DrawDebug([[maybe_unused]] const Vector3& cameraPosition)
  {
#ifdef JPH_DEBUG_RENDERER
    if (!debugDrawEnabled_) {
      return;
    }
    // 視点から遠い形は、Jolt が粗い形で描く（線の数を抑える）
    impl_->debugRenderer.SetCameraPos(ToJolt(cameraPosition));
    JPH::BodyManager::DrawSettings settings;
    settings.mDrawShapeWireframe = true;
    impl_->system.DrawBodies(settings, &impl_->debugRenderer);
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

  size_t PhysicsWorld::GetBodyCount() const
  {
    return impl_->bodies.size();
  }

  PhysicsWorld::WorldStats PhysicsWorld::GetWorldStats() const
  {
    WorldStats stats;
    stats.bodyCount         = impl_->bodies.size();
    stats.awakeCount        = impl_->system.GetNumActiveBodies(JPH::EBodyType::RigidBody);
    stats.touchingPairCount = impl_->touching.size();
    const JPH::BodyInterface& bodies = impl_->system.GetBodyInterface();
    for (const auto& [handle, record] : impl_->bodies) {
      stats.enabledCount += record.isInWorld ? 1 : 0;
      ++stats.layerBodyCounts[bodies.GetObjectLayer(record.id) & kLayerBits];
    }
    return stats;
  }

} // namespace Tako

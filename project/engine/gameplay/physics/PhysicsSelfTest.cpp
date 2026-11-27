#include "PhysicsSelfTest.h"

#ifdef _DEBUG

#include "Actor.h"
#include "BaseScene.h"
#include "CharacterControllerComponent.h"
#include "ColliderComponent.h"
#include "Component.h"
#include "JointComponent.h"
#include "Logger.h"
#include "Mat4x4Func.h"
#include "Mesh.h"
#include "MeshComponent.h"
#include "Model.h"
#include "Object3d.h"
#include "PhysicsWorld.h"
#include "RigidbodyComponent.h"
#ifdef TAKO_EDITOR
#include "ProjectMigration.h"
#endif

#include <json.hpp>

#include <algorithm>
#include <cassert>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
#include <span>
#include <string>
#include <vector>

namespace Tako {

  namespace {

    // テストに使う層。名前の付いた層（プロジェクトの設定）と重ならないよう、後ろの番号を使う
    constexpr uint32_t kLayerA = 28;
    constexpr uint32_t kLayerB = 29;

    /// <summary>
    /// 届いた当たりの通知を、順に 1 文字ずつ覚える（E = Enter、S = Stay、X = Exit）
    /// </summary>
    class ContactRecorder final : public Component {
    public: //メンバー関数
      void OnCollisionEnter(const Collision& collision) override { Record('E', collision); }
      void OnCollisionStay(const Collision& collision) override { Record('S', collision); }
      void OnCollisionExit(const Collision& collision) override { Record('X', collision); }
      void OnControllerColliderHit(const ControllerColliderHit& hit) override { hits.push_back(hit); }
      void OnJointBreak(float breakForce) override { breakForces.push_back(breakForce); }

    public: //メンバー変数
      std::string                        log;
      std::vector<Collision>             collisions;      ///< 届いた値（log の同じ位置の文字の分）
      Actor*                             last = nullptr;
      std::vector<ControllerColliderHit> hits;            ///< 届いた OnControllerColliderHit（届いた順）
      std::vector<float>                 breakForces;     ///< 届いた OnJointBreak の力（届いた順）

    private: //非公開関数
      void Record(char kind, const Collision& collision)
      {
        log += kind;
        collisions.push_back(collision);
        last = collision.actor;
      }
    };

    /// <summary>
    /// テスト用の Actor と、その通知の記録
    /// </summary>
    struct Probe {
      Actor*           actor    = nullptr;
      ContactRecorder* recorder = nullptr;
    };

    Probe SpawnProbe(BaseScene& scene, const Vector3& position)
    {
      Actor* actor = scene.Spawn("Actor");
      actor->GetTransform().translate = position;
      return { actor, static_cast<ContactRecorder*>(actor->AddComponent(std::make_unique<ContactRecorder>(), "Recorder")) };
    }

    PhysicsWorld::Shape MakeSphere(const Probe& probe, float radius)
    {
      PhysicsWorld::Shape shape;
      shape.type   = PhysicsWorld::ShapeType::Sphere;
      shape.radius = radius;
      shape.owner  = probe.recorder;
      return shape;
    }

    PhysicsWorld::Shape MakeBox(const Probe& probe, const Vector3& size)
    {
      PhysicsWorld::Shape shape;
      shape.type  = PhysicsWorld::ShapeType::Box;
      shape.size  = size;
      shape.owner = probe.recorder;
      return shape;
    }

    PhysicsWorld::Shape MakeCapsule(const Probe& probe, float radius, float height)
    {
      PhysicsWorld::Shape shape;
      shape.type   = PhysicsWorld::ShapeType::Capsule;
      shape.radius = radius;
      shape.height = height;
      shape.owner  = probe.recorder;
      return shape;
    }

    PhysicsWorld::BodyHandle CreateBody(const Probe& probe, PhysicsWorld::BodyKind kind, uint32_t layer, const PhysicsWorld::Shape& shape)
    {
      return PhysicsWorld::GetInstance()->CreateBody({ .kind = kind, .layer = layer, .actor = probe.actor }, std::span(&shape, 1));
    }

    /// <summary>
    /// シーンの UpdateFrame と同じ順で、1 フレーム分を回す
    /// </summary>
    void RunFrame(int steps)
    {
      PhysicsWorld* physics = PhysicsWorld::GetInstance();
      physics->BeginFrame();
      for (int step = 0; step < steps; ++step) {
        physics->Step();
      }
      physics->EndFrame(0.0f);
    }

    bool IsNear(const Vector3& a, const Vector3& b) { return (a - b).Length() < 0.01f; }

    /// <summary>
    /// Trigger どうしの Enter / Stay / Exit。Stay と、止めたボディの Exit は、ステップが 0 回のフレームでも届く
    /// </summary>
    void CheckTriggerEvents()
    {
      using enum PhysicsWorld::BodyKind;
      PhysicsWorld* physics = PhysicsWorld::GetInstance();
      BaseScene     scene;
      const Probe   a = SpawnProbe(scene, { 0.0f, 0.0f, 0.0f });
      const Probe   b = SpawnProbe(scene, { 1.5f, 0.0f, 0.0f });
      scene.UpdateAlways();
      const bool hadPair = physics->CanCollide(kLayerA, kLayerA);
      physics->SetCollisionMask(kLayerA, kLayerA, true);
      const PhysicsWorld::BodyHandle bodyA = CreateBody(a, Trigger, kLayerA, MakeSphere(a, 1.0f));
      const PhysicsWorld::BodyHandle bodyB = CreateBody(b, Trigger, kLayerA, MakeSphere(b, 1.0f));
      assert(bodyA != PhysicsWorld::kInvalidBody && bodyB != PhysicsWorld::kInvalidBody);

      // 当たり始めたフレームは Enter だけ。Stay は次のフレームからで、ステップの無いフレームにも届く
      RunFrame(1);
      assert(a.recorder->log == "E" && b.recorder->log == "E" && a.recorder->last == b.actor && b.recorder->last == a.actor);
      RunFrame(0);
      RunFrame(1);
      assert(a.recorder->log == "ESS" && b.recorder->log == "ESS");

      // Actor を動かすと、ボディが付いていく
      b.actor->GetTransform().translate.x = 10.0f;
      RunFrame(1);
      assert(a.recorder->log == "ESSX" && b.recorder->log == "ESSX");
      b.actor->GetTransform().translate.x = 1.5f;
      RunFrame(1);
      assert(a.recorder->log == "ESSXE");

      // 世界から出すと、ステップを待たずに Exit。戻すと Enter が出直す
      physics->SetBodyEnabled(bodyB, false);
      RunFrame(0);
      assert(a.recorder->log == "ESSXEX" && b.recorder->log == "ESSXEX");
      physics->SetBodyEnabled(bodyB, true);
      RunFrame(1);
      assert(a.recorder->log == "ESSXEXE");

      // 同じフレームの中で出して戻しても、Exit の後に Enter
      physics->SetBodyEnabled(bodyB, false);
      physics->SetBodyEnabled(bodyB, true);
      RunFrame(1);
      assert(a.recorder->log == "ESSXEXEXE" && b.recorder->log == "ESSXEXEXE");

      // 形を作り直しても、当たっている組は続く
      const PhysicsWorld::Shape bigger = MakeSphere(b, 1.2f);
      physics->SetBodyShapes(bodyB, std::span(&bigger, 1));
      RunFrame(1);
      assert(a.recorder->log == "ESSXEXEXES");

      // 消したボディの相手には、Exit も Stay も届かない
      physics->DestroyBody(bodyB);
      RunFrame(1);
      assert(a.recorder->log == "ESSXEXEXES");
      physics->DestroyBody(bodyA);
      physics->SetCollisionMask(kLayerA, kLayerA, hadPair);
    }

    /// <summary>
    /// 当たるのは、層の表で組にした層だけ。Trigger は動かない実体も知らせ、動かない実体どうしは知らせない
    /// </summary>
    void CheckLayersAndKinds()
    {
      using enum PhysicsWorld::BodyKind;
      PhysicsWorld* physics      = PhysicsWorld::GetInstance();
      const bool    hadCrossPair = physics->CanCollide(kLayerA, kLayerB);
      const bool    hadSamePair  = physics->CanCollide(kLayerB, kLayerB);
      BaseScene     scene;
      const Probe   trigger      = SpawnProbe(scene, { 0.0f, 0.0f, 0.0f });
      const Probe   wall         = SpawnProbe(scene, { 0.5f, 0.0f, 0.0f });
      const Probe   overlapping  = SpawnProbe(scene, { 1.0f, 0.0f, 0.0f });
      scene.UpdateAlways();
      physics->SetCollisionMask(kLayerA, kLayerB, false);
      physics->SetCollisionMask(kLayerB, kLayerB, true);
      const PhysicsWorld::BodyHandle triggerBody     = CreateBody(trigger, Trigger, kLayerA, MakeSphere(trigger, 1.0f));
      const PhysicsWorld::BodyHandle wallBody        = CreateBody(wall, Static, kLayerB, MakeBox(wall, { 1.0f, 1.0f, 1.0f }));
      const PhysicsWorld::BodyHandle overlappingBody = CreateBody(overlapping, Static, kLayerB, MakeBox(overlapping, { 1.0f, 1.0f, 1.0f }));

      RunFrame(1);
      assert(trigger.recorder->log.empty() && wall.recorder->log.empty() && overlapping.recorder->log.empty());

      physics->SetCollisionMask(kLayerA, kLayerB, true);
      RunFrame(1);
      assert(trigger.recorder->log == "EE" && wall.recorder->log == "E" && overlapping.recorder->log == "E");

      physics->DestroyBody(triggerBody);
      physics->DestroyBody(wallBody);
      physics->DestroyBody(overlappingBody);
      physics->SetCollisionMask(kLayerA, kLayerB, hadCrossPair);
      physics->SetCollisionMask(kLayerB, kLayerB, hadSamePair);
    }

    /// <summary>
    /// レイは、最初に当たる面までの距離と法線を返す。Trigger にも当たり（引数で素通りにできる）、層のマスクで外せる
    /// </summary>
    void CheckRaycast()
    {
      using enum PhysicsWorld::BodyKind;
      PhysicsWorld* physics = PhysicsWorld::GetInstance();
      BaseScene     scene;
      const Probe   wall = SpawnProbe(scene, { 0.0f, 0.0f, 5.0f });
      const Probe   veil = SpawnProbe(scene, { 0.0f, 0.0f, 2.0f });
      const Probe   wrap = SpawnProbe(scene, { 0.0f, 0.0f, 0.0f });
      scene.UpdateAlways();
      const PhysicsWorld::BodyHandle wallBody = CreateBody(wall, Static, kLayerA, MakeBox(wall, { 2.0f, 2.0f, 2.0f }));
      const PhysicsWorld::BodyHandle veilBody = CreateBody(veil, Trigger, kLayerA, MakeSphere(veil, 0.5f));

      // 既定では、手前の Trigger に当たる。向きは正規化していなくてよい
      const std::optional<PhysicsWorld::RaycastHit> veilHit = physics->Raycast({ 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 2.0f }, 100.0f, 1u << kLayerA);
      assert(veilHit && veilHit->actor == veil.actor && std::abs(veilHit->distance - 1.5f) < 0.01f);

      // Trigger を素通りにすると、奥の実体に当たる
      const std::optional<PhysicsWorld::RaycastHit> hit = physics->Raycast({ 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 2.0f }, 100.0f, 1u << kLayerA, false);
      assert(hit && hit->actor == wall.actor && std::abs(hit->distance - 4.0f) < 0.01f);
      assert(IsNear(hit->point, { 0.0f, 0.0f, 4.0f }) && IsNear(hit->normal, { 0.0f, 0.0f, -1.0f }));
      assert(!physics->Raycast({ 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 1.0f }, 3.0f, 1u << kLayerA, false));
      assert(!physics->Raycast({ 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 1.0f }, 100.0f, 1u << kLayerB));
      assert(!physics->Raycast({ 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, -1.0f }, 100.0f, 1u << kLayerA));

      // 始点を中に含む形（実体でも Trigger でも）には当たらず、その先の物に当たる（Unity と同じ）
      const PhysicsWorld::Shape wraps[] = { MakeSphere(wrap, 1.0f), MakeBox(wrap, { 2.0f, 2.0f, 2.0f }), MakeCapsule(wrap, 0.5f, 3.0f) };
      for (const PhysicsWorld::BodyKind kind : { Static, Trigger }) {
        for (const PhysicsWorld::Shape& shape : wraps) {
          const PhysicsWorld::BodyHandle                wrapBody = CreateBody(wrap, kind, kLayerA, shape);
          const std::optional<PhysicsWorld::RaycastHit> beyond   = physics->Raycast({ 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 1.0f }, 100.0f, 1u << kLayerA);
          assert(beyond);
          assert(beyond->actor != wrap.actor);
          assert(beyond->actor == veil.actor && std::abs(beyond->distance - 1.5f) < 0.01f);
          physics->DestroyBody(wrapBody);
        }
      }

      physics->DestroyBody(wallBody);
      physics->DestroyBody(veilBody);
    }

    /// <summary>
    /// 形の問い合わせ：SphereCast・BoxCast・CapsuleCast は、動かす前から重なっている形を素通りして、最初に当たる所を返す。
    /// RaycastAll は近い順。Overlap は、重なっている Actor を 1 回ずつ返す。どれも、Trigger の扱いと層のマスクに従う
    /// </summary>
    void CheckShapeQueries()
    {
      using enum PhysicsWorld::BodyKind;
      PhysicsWorld*    physics = PhysicsWorld::GetInstance();
      const uint32_t   mask    = 1u << kLayerA;
      const Quaternion upright{ 0.0f, 0.0f, 0.0f, 1.0f };
      const Quaternion lying{ 0.70710678f, 0.0f, 0.0f, 0.70710678f };  // X の周りに 90 度（Y の軸が Z を向く）
      const Vector3    forward{ 0.0f, 0.0f, 1.0f };
      const auto       isNear  = [](float a, float b) { return std::abs(a - b) < 0.01f; };
      BaseScene        scene;
      // 原点を包む球、手前の Trigger（手前の端が z = 1.5）、奥の壁（手前の面が z = 4）
      const Probe wrap = SpawnProbe(scene, { 0.0f, 0.0f, 0.0f });
      const Probe veil = SpawnProbe(scene, { 0.0f, 0.0f, 2.0f });
      const Probe wall = SpawnProbe(scene, { 0.0f, 0.0f, 5.0f });
      scene.UpdateAlways();
      const PhysicsWorld::BodyHandle wrapBody = CreateBody(wrap, Static, kLayerA, MakeSphere(wrap, 1.0f));
      const PhysicsWorld::BodyHandle veilBody = CreateBody(veil, Trigger, kLayerA, MakeSphere(veil, 0.5f));
      const PhysicsWorld::BodyHandle wallBody = CreateBody(wall, Static, kLayerA, MakeBox(wall, { 2.0f, 2.0f, 2.0f }));

      // 半径 0.5 の球を +Z へ：動かす前から重なっている wrap を素通りして、Trigger に触れる所（中心が 1.0 進んだ所）で当たる
      const std::optional<PhysicsWorld::RaycastHit> veilHit = physics->SphereCast({}, 0.5f, forward, 100.0f, mask);
      assert(veilHit && veilHit->actor == veil.actor && isNear(veilHit->distance, 1.0f));

      // Trigger を素通りにすると、壁に触れる所。触れた位置は壁の面で、向きは壁から球へ。向きは正規化していなくてよい
      const std::optional<PhysicsWorld::RaycastHit> wallHit = physics->SphereCast({}, 0.5f, { 0.0f, 0.0f, 2.0f }, 100.0f, mask, false);
      assert(wallHit && wallHit->actor == wall.actor && isNear(wallHit->distance, 3.5f));
      assert(IsNear(wallHit->point, { 0.0f, 0.0f, 4.0f }) && IsNear(wallHit->normal, { 0.0f, 0.0f, -1.0f }));
      assert(!physics->SphereCast({}, 0.5f, forward, 3.0f, mask, false));
      assert(!physics->SphereCast({}, 0.5f, forward, 100.0f, 1u << kLayerB));
      assert(!physics->SphereCast({}, 0.5f, { 0.0f, 0.0f, -1.0f }, 100.0f, mask));

      // 箱（全長 1）とカプセル（半径 0.5）も、前の端が壁に触れる所まで進む。寝かせたカプセル（全長 2）は、端が 1.0 先にある
      const std::optional<PhysicsWorld::RaycastHit> boxHit = physics->BoxCast({}, { 1.0f, 1.0f, 1.0f }, upright, forward, 100.0f, mask, false);
      assert(boxHit && boxHit->actor == wall.actor && isNear(boxHit->distance, 3.5f));
      const std::optional<PhysicsWorld::RaycastHit> capsuleHit = physics->CapsuleCast({}, 0.5f, 2.0f, upright, forward, 100.0f, mask, false);
      assert(capsuleHit && capsuleHit->actor == wall.actor && isNear(capsuleHit->distance, 3.5f));
      const std::optional<PhysicsWorld::RaycastHit> lyingHit = physics->CapsuleCast({}, 0.5f, 2.0f, lying, forward, 100.0f, mask, false);
      assert(lyingHit && lyingHit->actor == wall.actor && isNear(lyingHit->distance, 3.0f));

      // RaycastAll：始点を含む wrap は入らず、近い順に Trigger・壁
      const std::vector<PhysicsWorld::RaycastHit> all = physics->RaycastAll({}, forward, 100.0f, mask);
      assert(all.size() == 2 && all[0].actor == veil.actor && all[1].actor == wall.actor);
      assert(isNear(all[0].distance, 1.5f) && isNear(all[1].distance, 4.0f));
      assert(physics->RaycastAll({}, forward, 100.0f, mask, false).size() == 1);
      assert(physics->RaycastAll({}, forward, 100.0f, 1u << kLayerB).empty());

      // Overlap：原点の半径 1.6 の球は、wrap と Trigger に重なり、壁には届かない
      const std::vector<Actor*> overlaps = physics->OverlapSphere({}, 1.6f, mask);
      assert(overlaps.size() == 2 && std::ranges::find(overlaps, wrap.actor) != overlaps.end() && std::ranges::find(overlaps, veil.actor) != overlaps.end());
      const std::vector<Actor*> solids = physics->OverlapSphere({}, 1.6f, mask, false);
      assert(solids.size() == 1 && solids[0] == wrap.actor);
      assert(physics->OverlapSphere({}, 1.6f, 1u << kLayerB).empty());

      // Trigger と壁の間に置いた箱とカプセル：小さければどちらにも届かず、Z に長ければ両方に重なる
      const Vector3 between{ 0.0f, 0.0f, 3.2f };
      assert(physics->OverlapBox(between, { 1.0f, 1.0f, 1.0f }, upright, mask).empty());
      assert(physics->OverlapBox(between, { 1.0f, 1.0f, 2.0f }, upright, mask).size() == 2);
      assert(physics->OverlapCapsule(between, 0.25f, 3.0f, upright, mask).empty());
      assert(physics->OverlapCapsule(between, 0.25f, 3.0f, lying, mask).size() == 2);

      physics->DestroyBody(wrapBody);
      physics->DestroyBody(veilBody);
      physics->DestroyBody(wallBody);
    }

    /// <summary>
    /// 形の位置は、Actor の回転と親の Transform で、エンジンの行列と同じ所へ行く。親を動かすと、子のボディも付いていく
    /// </summary>
    void CheckPoseFollowsActor()
    {
      PhysicsWorld* physics = PhysicsWorld::GetInstance();
      BaseScene     scene;
      const Probe   parent = SpawnProbe(scene, { 10.0f, 0.0f, 0.0f });
      const Probe   child  = SpawnProbe(scene, { 0.0f, 2.0f, 0.0f });
      scene.UpdateAlways();
      [[maybe_unused]] const bool isParented = child.actor->SetParent(parent.actor, false);
      assert(isParented);
      parent.actor->GetTransform().rotate = { 0.3f, 1.2f, -0.5f };
      child.actor->GetTransform().rotate  = { -0.4f, 0.2f, 0.9f };

      const Vector3       offset{ 3.0f, 0.0f, 0.0f };
      const float         radius = 0.5f;
      PhysicsWorld::Shape shape  = MakeSphere(child, radius);
      shape.position             = offset;
      const PhysicsWorld::BodyHandle body = CreateBody(child, PhysicsWorld::BodyKind::Static, kLayerA, shape);

      // 球の真上から下へ撃つと、球の上の端に当たる
      const auto hitsSphereTop = [&] {
        const Vector3 center = Mat4x4::Transform(child.actor->GetWorldMatrix(), offset);
        const std::optional<PhysicsWorld::RaycastHit> hit = physics->Raycast(center + Vector3{ 0.0f, 10.0f, 0.0f }, { 0.0f, -1.0f, 0.0f }, 20.0f, 1u << kLayerA);
        return hit && hit->actor == child.actor && std::abs(hit->distance - (10.0f - radius)) < 0.01f;
      };
      assert(hitsSphereTop());
      parent.actor->GetTransform().translate = { -4.0f, 3.0f, 7.0f };
      parent.actor->GetTransform().rotate.y  = -2.0f;
      RunFrame(0);
      assert(hitsSphereTop());

      physics->DestroyBody(body);
    }

    /// <summary>
    /// 形の大きさと位置には、Actor の scale が掛かる（Box は軸ごと、Sphere は 3 軸の最大、Capsule の半径は X と Z の大きい方・高さは Y）。親の scale と、後から変えた scale にも付いていく
    /// </summary>
    void CheckScaleFollowsActor()
    {
      using enum PhysicsWorld::BodyKind;
      PhysicsWorld* physics = PhysicsWorld::GetInstance();
      BaseScene     scene;
      const Probe   box     = SpawnProbe(scene, { 0.0f, 0.0f, 0.0f });
      const Probe   sphere  = SpawnProbe(scene, { 20.0f, 0.0f, 0.0f });
      const Probe   capsule = SpawnProbe(scene, { 40.0f, 0.0f, 0.0f });
      const Probe   parent  = SpawnProbe(scene, { 60.0f, 0.0f, 0.0f });
      const Probe   child   = SpawnProbe(scene, { 0.0f, 1.0f, 0.0f });
      scene.UpdateAlways();
      [[maybe_unused]] const bool isParented = child.actor->SetParent(parent.actor, false);
      assert(isParented);
      box.actor->GetTransform().scale     = { 2.0f, 4.0f, 6.0f };
      sphere.actor->GetTransform().scale  = { 1.0f, 3.0f, 2.0f };
      capsule.actor->GetTransform().scale = { 2.0f, 3.0f, 1.0f };
      parent.actor->GetTransform().scale  = { 2.0f, 2.0f, 2.0f };

      PhysicsWorld::Shape offsetSphere = MakeSphere(sphere, 0.5f);
      offsetSphere.position            = { 0.0f, 0.0f, 1.0f };
      const PhysicsWorld::BodyHandle bodies[] = {
        CreateBody(box, Static, kLayerA, MakeBox(box, { 1.0f, 1.0f, 1.0f })),
        CreateBody(sphere, Static, kLayerA, offsetSphere),
        CreateBody(capsule, Static, kLayerA, MakeCapsule(capsule, 0.5f, 2.0f)),
        CreateBody(child, Static, kLayerA, MakeBox(child, { 1.0f, 1.0f, 1.0f })),
      };

      // (x, z) の真上から下へ撃って、当たった高さ
      const auto topAt = [physics](float x, float z) {
        const std::optional<PhysicsWorld::RaycastHit> hit = physics->Raycast({ x, 50.0f, z }, { 0.0f, -1.0f, 0.0f }, 100.0f, 1u << kLayerA);
        return hit ? hit->point.y : -100.0f;
      };
      const auto isNear = [](float a, float b) { return std::abs(a - b) < 0.01f; };
      // Box：高さ 1 × 4 の半分。幅は X が 1 × 2 なので、x = 0.9 は当たり、1.1 は外れる
      assert(isNear(topAt(0.0f, 0.0f), 2.0f) && isNear(topAt(0.9f, 0.0f), 2.0f) && topAt(1.1f, 0.0f) < -99.0f);
      // Sphere：半径 0.5 × 3（最大の軸）。位置は Z が 1 × 2
      assert(isNear(topAt(20.0f, 2.0f), 1.5f));
      // Capsule：全長 2 × 3 の半分。半径 0.5 × 2 なので、x が 0.9 ずれた所も当たる
      assert(isNear(topAt(40.0f, 0.0f), 3.0f) && topAt(40.9f, 0.0f) > 0.0f && topAt(41.1f, 0.0f) < -99.0f);
      // 親の scale 2：子は高さ 2 に居て、Box の半分が 1
      assert(isNear(topAt(60.0f, 0.0f), 3.0f));

      // Play 中に scale を変えても付いていく
      box.actor->GetTransform().scale.y  = 1.0f;
      parent.actor->GetTransform().scale = { 1.0f, 1.0f, 1.0f };
      RunFrame(0);
      assert(isNear(topAt(0.0f, 0.0f), 0.5f) && isNear(topAt(60.0f, 0.0f), 1.5f));

      for (const PhysicsWorld::BodyHandle body : bodies) {
        physics->DestroyBody(body);
      }
    }

    /// <summary>
    /// コライダーを付けたテスト用の Actor
    /// </summary>
    struct ColliderProbe {
      Actor*             actor    = nullptr;
      ContactRecorder*   recorder = nullptr;
      ColliderComponent* collider = nullptr;
    };

    ColliderProbe SpawnTrigger(BaseScene& scene, const Vector3& position)
    {
      const Probe probe    = SpawnProbe(scene, position);
      auto*       collider = static_cast<ColliderComponent*>(probe.actor->AddComponent(std::make_unique<ColliderComponent>(), "Collider"));
      collider->SetTrigger(true);
      return { probe.actor, probe.recorder, collider };
    }

    /// <summary>
    /// ColliderComponent を通した通知：Awake の前に止めた分・Actor のオフ・大きさの変更・親に付いていく子。シーンを閉じると、ボディは残らない
    /// </summary>
    void CheckColliderComponent()
    {
      PhysicsWorld* physics = PhysicsWorld::GetInstance();
      // 既定の層に名前を付けて、当たらない設定にしてあるプロジェクトでは確かめられない
      if (!physics->CanCollide(ColliderComponent::kActorLayer, ColliderComponent::kActorLayer)) {
        return;
      }
      const float stepFrame = 0.017f;  // 物理が 1 ステップ進むフレーム
      const float idleFrame = 0.001f;  // 物理が進まないフレーム

      BaseScene scene;
      scene.BeginPlay();
      const ColliderProbe a      = SpawnTrigger(scene, { 0.0f, 0.0f, 0.0f });
      const ColliderProbe b      = SpawnTrigger(scene, { 1.5f, 0.0f, 0.0f });
      const Probe         parent = SpawnProbe(scene, { 100.0f, 0.0f, 0.0f });
      const ColliderProbe child  = SpawnTrigger(scene, { 0.0f, 0.0f, 0.0f });
      const ColliderProbe target = SpawnTrigger(scene, { 200.0f, 0.0f, 0.0f });
      b.collider->SetActive(false);
      scene.UpdateAlways();
      [[maybe_unused]] const bool isParented = child.actor->SetParent(parent.actor, false);
      assert(isParented);

      // Awake の前に止めたコライダーは、当たらない。戻すと当たる
      scene.UpdateFrame(stepFrame);
      assert(a.recorder->log.empty() && b.recorder->log.empty());
      b.collider->SetActive(true);
      scene.UpdateFrame(stepFrame);
      assert(a.recorder->log == "E" && b.recorder->log == "E" && a.recorder->last == b.actor);

      // Actor をオフにすると、物理が進まないフレームでも Exit。戻すと Enter が出直す
      b.actor->SetActive(false);
      scene.UpdateAlways();
      scene.UpdateFrame(idleFrame);
      assert(a.recorder->log == "EX" && b.recorder->log == "EX");
      b.actor->SetActive(true);
      scene.UpdateAlways();
      scene.UpdateFrame(stepFrame);
      assert(a.recorder->log == "EXE");

      // 大きさを変えても、当たっている組は続く（Enter は出直さない）
      a.collider->SetRadius(1.2f);
      scene.UpdateFrame(stepFrame);
      assert(a.recorder->log == "EXES" && b.recorder->log == "EXES");

      // 親を動かすと、子のコライダーが相手に届く
      assert(child.recorder->log.empty() && target.recorder->log.empty());
      parent.actor->GetTransform().translate.x = 199.0f;
      scene.UpdateFrame(stepFrame);
      assert(child.recorder->log == "E" && target.recorder->log == "E" && target.recorder->last == child.actor);

      scene.Shutdown();
      assert(physics->GetBodyCount() == 0);
    }

    /// <summary>
    /// 実体の Collider を付けたテスト用の Actor
    /// </summary>
    ColliderProbe SpawnSolid(BaseScene& scene, const Vector3& position, ColliderComponent::Shape shape)
    {
      const Probe probe    = SpawnProbe(scene, position);
      auto*       collider = static_cast<ColliderComponent*>(probe.actor->AddComponent(std::make_unique<ColliderComponent>(), "Collider"));
      collider->SetShape(shape);
      collider->SetRadius(0.5f);
      return { probe.actor, probe.recorder, collider };
    }

    RigidbodyComponent* AddRigidbody(Actor* actor)
    {
      return static_cast<RigidbodyComponent*>(actor->AddComponent(std::make_unique<RigidbodyComponent>(), "Rigidbody"));
    }

    void RunFrames(BaseScene& scene, int count)
    {
      for (int frame = 0; frame < count; ++frame) {
        scene.UpdateFrame(PhysicsWorld::kFixedDeltaTime);
      }
    }

    /// <summary>
    /// Rigidbody：落ちて床で止まる・ステップの間を補間して書く・Transform を書き換えると置き直る・寝ても Exit が出ない・
    /// Kinematic が Dynamic を押す・軸の固定・子の Actor の Collider が形に入る。シーンを閉じると、ボディは残らない
    /// </summary>
    void CheckRigidbody()
    {
      PhysicsWorld* physics = PhysicsWorld::GetInstance();
      // 既定の層どうしを、当たらない設定にしてあるプロジェクトでは確かめられない
      if (!physics->CanCollide(ColliderComponent::kActorLayer, ColliderComponent::kActorLayer)) {
        return;
      }
      const float frame  = PhysicsWorld::kFixedDeltaTime;
      const float radius = 0.5f;
      const auto  isNear = [](float a, float b, float tolerance) { return std::abs(a - b) < tolerance; };

      BaseScene scene;
      scene.BeginPlay();
      // 床は、大きさ 1 の箱を Scale で広げた物。上の面が y = 0
      const ColliderProbe floor = SpawnSolid(scene, { 0.0f, -0.5f, 0.0f }, ColliderComponent::Shape::Box);
      floor.actor->GetTransform().scale = { 1000.0f, 1.0f, 1000.0f };
      const ColliderProbe ball     = SpawnSolid(scene, { 0.0f, 3.0f, 0.0f }, ColliderComponent::Shape::Sphere);
      RigidbodyComponent* ballBody = AddRigidbody(ball.actor);
      scene.UpdateAlways();

      // 最初のステップの後は、まだ動く前の姿勢が書かれる（描画は、直前の 2 ステップの間を補間する）
      scene.UpdateFrame(frame);
      assert(ball.actor->GetTransform().translate.y == 3.0f);
      scene.UpdateFrame(frame * 0.5f);
      const float halfway = ball.actor->GetTransform().translate.y;
      assert(halfway < 3.0f && halfway > 2.99f);

      // 落ちて、床の上で止まる。当たり始めの知らせが届き、止まって寝た後も Exit は出ない
      RunFrames(scene, 240);
      assert(isNear(ball.actor->GetTransform().translate.y, radius, 0.05f));
      assert(!ball.recorder->log.empty() && ball.recorder->log.front() == 'E' && ball.recorder->last == floor.actor);
      const size_t settled = ball.recorder->log.size();
      RunFrames(scene, 180);
      assert(physics->GetWorldStats().awakeCount == 0);
      assert(ball.recorder->log.size() > settled && ball.recorder->log.find('X', settled) == std::string::npos);

      // Transform を書き換えると、その位置へ置き直されて、そこからまた落ちる
      ball.actor->GetTransform().translate = { 5.0f, 4.0f, 0.0f };
      RunFrames(scene, 3);
      assert(isNear(ball.actor->GetTransform().translate.x, 5.0f, 0.001f));
      assert(ball.actor->GetTransform().translate.y < 4.0f && ball.actor->GetTransform().translate.y > 3.9f);

      // 速度と軸の固定：Y を止めると落ちずに、与えた速度で進む。全部の軸を止めると、押しても動かない
      ballBody->SetLockedAxes(PhysicsWorld::kLockPositionY);
      ballBody->SetLinearVelocity({ 2.0f, 0.0f, 0.0f });
      const float lockedHeight = ball.actor->GetTransform().translate.y;
      RunFrames(scene, 60);
      assert(isNear(ball.actor->GetTransform().translate.y, lockedHeight, 0.01f) && ball.actor->GetTransform().translate.x > 6.5f);
      ballBody->SetLockedAxes(0x3f);
      // 止めた位置が Actor に書かれるまで待つ（書くのは、直前の 2 ステップの間の姿勢）
      RunFrames(scene, 2);
      const Vector3 frozen = ball.actor->GetTransform().translate;
      ballBody->AddImpulse({ 0.0f, 100.0f, 100.0f });
      RunFrames(scene, 10);
      assert(IsNear(ball.actor->GetTransform().translate, frozen));
      scene.Destroy(ball.actor);

      // Kinematic は Actor の動きに付いていき、床の上の Dynamic を押す
      const ColliderProbe pusher     = SpawnSolid(scene, { 98.0f, 0.5f, 0.0f }, ColliderComponent::Shape::Box);
      RigidbodyComponent* pusherBody = AddRigidbody(pusher.actor);
      pusherBody->SetBodyType(RigidbodyComponent::BodyType::Kinematic);
      const ColliderProbe pushed = SpawnSolid(scene, { 100.0f, radius, 0.0f }, ColliderComponent::Shape::Sphere);
      AddRigidbody(pushed.actor);

      // 子の Actor の Collider は、親の Rigidbody の形になる（知らせは、Collider の付いた子に届く）
      const Probe         parent = SpawnProbe(scene, { 300.0f, 3.0f, 0.0f });
      const ColliderProbe child  = SpawnSolid(scene, { 1.0f, 0.0f, 0.0f }, ColliderComponent::Shape::Box);
      AddRigidbody(parent.actor);
      [[maybe_unused]] const bool isParented = child.actor->SetParent(parent.actor, false);
      assert(isParented);
      scene.UpdateAlways();

      for (int step = 0; step < 80; ++step) {
        pusher.actor->GetTransform().translate.x += 0.05f;
        scene.UpdateFrame(frame);
      }
      RunFrames(scene, 160);
      assert(isNear(pusher.actor->GetTransform().translate.x, 102.0f, 0.001f));
      assert(pushed.actor->GetTransform().translate.x > 102.9f);
      assert(isNear(parent.actor->GetTransform().translate.y, 0.5f, 0.05f));
      assert(!child.recorder->log.empty() && child.recorder->log.front() == 'E' && parent.recorder->log.empty());

      scene.Shutdown();
      assert(physics->GetBodyCount() == 0);
    }

    /// <summary>
    /// Rigidbody の操作：重心から外れた位置への衝撃と力で回り始める・寝かせると宙で止まり、起こすと落ちる・重心を下げた箱は、傾けて置いても起き上がる
    /// </summary>
    void CheckRigidbodyControls()
    {
      PhysicsWorld* physics = PhysicsWorld::GetInstance();
      if (!physics->CanCollide(ColliderComponent::kActorLayer, ColliderComponent::kActorLayer)) {
        return;
      }
      BaseScene scene;
      scene.BeginPlay();
      const ColliderProbe floor = SpawnSolid(scene, { 0.0f, -0.5f, 0.0f }, ColliderComponent::Shape::Box);
      floor.actor->GetTransform().scale = { 1000.0f, 1.0f, 1000.0f };
      // 宙に置いた、大きさ 1 の箱 2 つと球
      const ColliderProbe flicked     = SpawnSolid(scene, { 0.0f, 20.0f, 0.0f }, ColliderComponent::Shape::Box);
      RigidbodyComponent* flickedBody = AddRigidbody(flicked.actor);
      const ColliderProbe pushed      = SpawnSolid(scene, { 10.0f, 20.0f, 0.0f }, ColliderComponent::Shape::Box);
      RigidbodyComponent* pushedBody  = AddRigidbody(pushed.actor);
      const ColliderProbe sleeper     = SpawnSolid(scene, { 20.0f, 20.0f, 0.0f }, ColliderComponent::Shape::Sphere);
      RigidbodyComponent* sleeperBody = AddRigidbody(sleeper.actor);
      // 細長い箱（0.5 × 2 × 0.5）を 40 度傾けて、角が床に付く高さに置く。重心が真ん中なら倒れ、底の近くまで下げてあれば起き上がる
      const auto spawnTilted = [&scene](float x, float centerOfMassY) {
        const ColliderProbe box = SpawnSolid(scene, { x, 0.95f, 0.0f }, ColliderComponent::Shape::Box);
        box.actor->GetTransform().scale  = { 0.5f, 2.0f, 0.5f };
        box.actor->GetTransform().rotate = { 0.0f, 0.0f, 0.7f };
        AddRigidbody(box.actor)->SetCenterOfMass({ 0.0f, centerOfMassY, 0.0f });
        return box.actor;
      };
      const Actor* plain   = spawnTilted(40.0f, 0.0f);
      const Actor* lowered = spawnTilted(60.0f, -0.45f);
      scene.UpdateAlways();
      RunFrames(scene, 2);

      // 重心から +X へ外れた位置を上へ弾くと、上へ動きながら、Z の周りに回り始める
      const Vector3 edge{ 0.5f, 0.0f, 0.0f };
      flickedBody->AddImpulseAtPosition({ 0.0f, 2.0f, 0.0f }, flicked.actor->GetTransform().translate + edge);
      assert(flickedBody->GetAngularVelocity().z > 1.0f && flickedBody->GetLinearVelocity().y > 1.0f);

      // 力は、次の 1 ステップの間に効く
      pushedBody->AddForceAtPosition({ 0.0f, 120.0f, 0.0f }, pushed.actor->GetTransform().translate + edge);
      assert(IsNear(pushedBody->GetAngularVelocity(), {}));
      RunFrames(scene, 1);
      assert(pushedBody->GetAngularVelocity().z > 1.0f);

      // 寝かせた球は、宙で止まったまま動かない。起こすと落ちる
      assert(!sleeperBody->IsSleeping());
      sleeperBody->Sleep();
      assert(sleeperBody->IsSleeping());
      RunFrames(scene, 2);
      const float heldHeight = sleeper.actor->GetTransform().translate.y;
      RunFrames(scene, 30);
      assert(sleeperBody->IsSleeping() && sleeper.actor->GetTransform().translate.y == heldHeight);
      sleeperBody->WakeUp();
      assert(!sleeperBody->IsSleeping());
      RunFrames(scene, 30);
      assert(sleeper.actor->GetTransform().translate.y < heldHeight - 0.5f);

      // 倒れた箱は中心が低く（0.25）、立った箱は高い（1.0）
      RunFrames(scene, 240);
      assert(plain->GetTransform().translate.y < 0.5f && std::abs(plain->GetTransform().rotate.z) > 1.2f);
      assert(lowered->GetTransform().translate.y > 0.9f && std::abs(lowered->GetTransform().rotate.z) < 0.2f);

      scene.Shutdown();
      assert(physics->GetBodyCount() == 0);
    }

    /// <summary>
    /// 当たりの知らせに入る値。落ちた球の Enter：触れた位置が床の面、向きが床から球へ（上）、速度の差が上向き、力積が正。床の側は、向きと速度の差が逆。
    /// Stay は力積が 0、Exit は相手だけ。Trigger の重なりでも、触れた位置が入る（力積は 0）
    /// </summary>
    void CheckCollisionInfo()
    {
      PhysicsWorld* physics = PhysicsWorld::GetInstance();
      if (!physics->CanCollide(ColliderComponent::kActorLayer, ColliderComponent::kActorLayer)) {
        return;
      }
      const auto isNear = [](float a, float b, float tolerance) { return std::abs(a - b) < tolerance; };

      BaseScene scene;
      scene.BeginPlay();
      const ColliderProbe floor = SpawnSolid(scene, { 0.0f, -0.5f, 0.0f }, ColliderComponent::Shape::Box);
      floor.actor->GetTransform().scale = { 1000.0f, 1.0f, 1000.0f };
      // 半径 0.5 の球を、中心の高さ 2 から床（上の面が y = 0）へ落とす
      const ColliderProbe ball = SpawnSolid(scene, { 0.0f, 2.0f, 0.0f }, ColliderComponent::Shape::Sphere);
      AddRigidbody(ball.actor);
      // 半径 1 の Trigger（上の端が y = 11）へ、上から球を落とす
      const ColliderProbe zone    = SpawnTrigger(scene, { 30.0f, 10.0f, 0.0f });
      const ColliderProbe dropped = SpawnSolid(scene, { 30.0f, 12.0f, 0.0f }, ColliderComponent::Shape::Sphere);
      AddRigidbody(dropped.actor);
      scene.UpdateAlways();
      RunFrames(scene, 60);

      // 球の側：相手は床。触れた位置は床の面で、向きは床から球へ。床が球へ近づく向きの速度の差と、ぶつかった強さが入る
      assert(!ball.recorder->log.empty() && ball.recorder->log.front() == 'E');
      const Collision landed = ball.recorder->collisions.front();
      assert(landed.actor == floor.actor && isNear(landed.point.x, 0.0f, 0.05f) && isNear(landed.point.y, 0.0f, 0.05f));
      assert(IsNear(landed.normal, { 0.0f, 1.0f, 0.0f }) && landed.relativeVelocity.y > 3.0f && landed.impulse > 1.0f);

      // 床の側：同じ接触を、逆の向きで受け取る
      assert(!floor.recorder->log.empty() && floor.recorder->log.front() == 'E');
      const Collision struck = floor.recorder->collisions.front();
      assert(struck.actor == ball.actor && IsNear(struck.point, landed.point) && IsNear(struck.normal, { 0.0f, -1.0f, 0.0f }));
      assert(struck.relativeVelocity.y < -3.0f && isNear(struck.impulse, landed.impulse, 0.001f));

      // Stay：位置と向きは入り、力積は 0
      const size_t stay = ball.recorder->log.find('S');
      assert(stay != std::string::npos && ball.recorder->collisions[stay].actor == floor.actor);
      assert(IsNear(ball.recorder->collisions[stay].normal, { 0.0f, 1.0f, 0.0f }) && ball.recorder->collisions[stay].impulse == 0.0f);

      // Exit：相手だけが入る
      ball.actor->GetTransform().translate = { 0.0f, 50.0f, 0.0f };
      RunFrames(scene, 3);
      assert(ball.recorder->log.back() == 'X');
      const Collision left = ball.recorder->collisions.back();
      assert(left.actor == floor.actor && IsNear(left.point, {}) && IsNear(left.normal, {}) && IsNear(left.relativeVelocity, {}) && left.impulse == 0.0f);

      // Trigger の側：入ってきた球に触れた位置（Trigger の上の端のあたり）が入る。向きは球から Trigger へ（下）、力積は 0
      assert(!zone.recorder->log.empty() && zone.recorder->log.front() == 'E');
      const Collision entered = zone.recorder->collisions.front();
      assert(entered.actor == dropped.actor && isNear(entered.point.x, 30.0f, 0.1f) && entered.point.y > 10.8f && entered.point.y < 11.1f);
      assert(entered.normal.y < -0.9f && entered.relativeVelocity.y < -1.0f && entered.impulse == 0.0f);

      scene.Shutdown();
      assert(physics->GetBodyCount() == 0);
    }

    /// <summary>
    /// モデルから作る形：Mesh の床に球と凸包が乗る（三角形の表が外を向いている）、上からのレイが上の面に当たる、Dynamic の下の Mesh は形にならない
    /// </summary>
    void CheckMeshShapes()
    {
      PhysicsWorld* physics = PhysicsWorld::GetInstance();
      if (!physics->CanCollide(ColliderComponent::kActorLayer, ColliderComponent::kActorLayer)) {
        return;
      }
      const char* const kCube  = "EngineResources/Model/white_cube.gltf";
      const auto        isNear = [](float a, float b) { return std::abs(a - b) < 0.05f; };
      const auto        addCube = [kCube](const ColliderProbe& probe) {
        auto* mesh = static_cast<MeshComponent*>(probe.actor->AddComponent(std::make_unique<MeshComponent>(), "Mesh"));
        mesh->SetModel(kCube);
        return mesh;
      };

      BaseScene scene;
      scene.BeginPlay();
      // 箱のモデルを Scale で広げた物が、Mesh の形の床
      const ColliderProbe ground     = SpawnSolid(scene, { 0.0f, 0.0f, 0.0f }, ColliderComponent::Shape::Mesh);
      MeshComponent*      groundMesh = addCube(ground);
      ground.actor->GetTransform().scale = { 50.0f, 1.0f, 50.0f };
      const Mesh* box    = groundMesh->GetObject3d()->GetModel()->GetMesh();
      const float top    = box->GetAABBLocalMax().y;
      const float bottom = box->GetAABBLocalMin().y;

      const ColliderProbe ball = SpawnSolid(scene, { 0.0f, top + 3.0f, 0.0f }, ColliderComponent::Shape::Sphere);
      AddRigidbody(ball.actor);
      const ColliderProbe hull = SpawnSolid(scene, { 5.0f, top + 3.0f, 0.0f }, ColliderComponent::Shape::ConvexHull);
      addCube(hull);
      AddRigidbody(hull.actor);
      const ColliderProbe hollow = SpawnSolid(scene, { 10.0f, top + 3.0f, 0.0f }, ColliderComponent::Shape::Mesh);
      addCube(hollow);
      AddRigidbody(hollow.actor);

      // Dynamic の下の Mesh を飛ばした事の記録は、Console に出さない
      const Logger::Sink console = Logger::SetSink(nullptr);
      scene.UpdateAlways();
      Logger::SetSink(console);
      RunFrames(scene, 240);

      // 球は半径の分、凸包の箱は底までの分だけ、床の上の面より上で止まる
      assert(isNear(ball.actor->GetTransform().translate.y, top + 0.5f));
      assert(isNear(hull.actor->GetTransform().translate.y, top - bottom));
      // 形の無い剛体は、床を抜けて落ち続ける
      assert(hollow.actor->GetTransform().translate.y < bottom - 10.0f);

      const std::optional<PhysicsWorld::RaycastHit> hit = physics->Raycast({ -5.0f, 50.0f, -5.0f }, { 0.0f, -1.0f, 0.0f }, 100.0f);
      assert(hit && hit->actor == ground.actor && isNear(hit->point.y, top) && IsNear(hit->normal, { 0.0f, 1.0f, 0.0f }));
      // 三角形の裏面には当たらない：床のメッシュの中から下へ飛ばしたレイは、底の面を抜ける
      assert(!physics->Raycast({ -5.0f, 0.0f, -5.0f }, { 0.0f, -1.0f, 0.0f }, 100.0f));

      scene.Shutdown();
      assert(physics->GetBodyCount() == 0);
    }

    /// <summary>
    /// Joint：Fixed はワールドに留める・Distance は吊るした物の距離を保つ・Hinge は軸の周りだけ回す・
    /// つないだ Actor をオフにして戻しても続く・相手を消すと、支えが外れて落ちる
    /// </summary>
    void CheckJoints()
    {
      PhysicsWorld* physics  = PhysicsWorld::GetInstance();
      const auto    isNear   = [](float a, float b, float tolerance) { return std::abs(a - b) < tolerance; };
      const auto    addJoint = [](Actor* actor, JointComponent::JointType type, const Actor* connected) {
        auto* joint = static_cast<JointComponent*>(actor->AddComponent(std::make_unique<JointComponent>(), "Joint"));
        joint->SetType(type);
        joint->SetConnectedActor(connected);
        return joint;
      };

      BaseScene scene;
      scene.BeginPlay();
      // ワールドに固めた球は、落ちない
      const ColliderProbe pinned = SpawnSolid(scene, { 0.0f, 10.0f, 0.0f }, ColliderComponent::Shape::Sphere);
      AddRigidbody(pinned.actor);
      addJoint(pinned.actor, JointComponent::JointType::Fixed, nullptr);

      // 動かない hook から upper を、upper から lower を、距離 2 で吊るす
      const ColliderProbe hook = SpawnSolid(scene, { 20.0f, 10.0f, 0.0f }, ColliderComponent::Shape::Sphere);
      AddRigidbody(hook.actor)->SetBodyType(RigidbodyComponent::BodyType::Kinematic);
      const ColliderProbe upper = SpawnSolid(scene, { 20.0f, 8.0f, 0.0f }, ColliderComponent::Shape::Sphere);
      AddRigidbody(upper.actor);
      addJoint(upper.actor, JointComponent::JointType::Distance, hook.actor);
      const ColliderProbe lower = SpawnSolid(scene, { 20.0f, 6.0f, 0.0f }, ColliderComponent::Shape::Sphere);
      AddRigidbody(lower.actor);
      addJoint(lower.actor, JointComponent::JointType::Distance, upper.actor);

      // 箱の横の点を、Z 軸の蝶番でワールドにつなぐと、XY の面の中で振り子になる
      const Vector3       pivot{ 39.0f, 10.0f, 0.0f };
      const ColliderProbe door = SpawnSolid(scene, { 40.0f, 10.0f, 0.0f }, ColliderComponent::Shape::Box);
      AddRigidbody(door.actor);
      JointComponent* hinge = addJoint(door.actor, JointComponent::JointType::Hinge, nullptr);
      hinge->SetAnchor({ -1.0f, 0.0f, 0.0f });
      hinge->SetAxis({ 0.0f, 0.0f, 1.0f });
      scene.UpdateAlways();

      // 両方が起きている間に、つないだ片方をオフにして戻す（オフの間、ジョイントは効かない）
      RunFrames(scene, 10);
      lower.actor->SetActive(false);
      scene.UpdateAlways();
      RunFrames(scene, 5);
      lower.actor->SetActive(true);
      scene.UpdateAlways();
      RunFrames(scene, 105);

      assert(IsNear(pinned.actor->GetTransform().translate, { 0.0f, 10.0f, 0.0f }));
      assert(isNear(upper.actor->GetTransform().translate.y, 8.0f, 0.05f) && isNear(lower.actor->GetTransform().translate.y, 6.0f, 0.05f));
      const Transform swung = door.actor->GetTransform();
      const Vector3   arm   = swung.translate - pivot;
      assert(isNear(arm.Length(), 1.0f, 0.02f) && isNear(arm.z, 0.0f, 0.01f) && swung.translate.y < 9.9f);
      assert(isNear(swung.rotate.x, 0.0f, 0.01f) && isNear(swung.rotate.y, 0.0f, 0.01f));

      // つないだ相手を消すと、ジョイントも外れて、寝ていた物が落ち始める
      scene.Destroy(hook.actor);
      scene.UpdateAlways();
      RunFrames(scene, 60);
      assert(upper.actor->GetTransform().translate.y < 7.0f && lower.actor->GetTransform().translate.y < 5.0f);

      scene.Shutdown();
      assert(physics->GetBodyCount() == 0);
    }

    /// <summary>
    /// Joint の追加の設定と、当たらない組：つないだ相手とは当たらず、Enable Collision で当たる・IgnoreCollision した相手を素通りし、解くと当たる（キャラクターも）・
    /// モーターで回り、つなぎ直さずに速さを変えられる・摩擦で止まる・ばねで伸びる・Break Force より強い力で外れて、OnJointBreak が届く
    /// </summary>
    void CheckJointFeatures()
    {
      using enum JointComponent::JointType;
      PhysicsWorld* physics = PhysicsWorld::GetInstance();
      if (!physics->CanCollide(ColliderComponent::kActorLayer, ColliderComponent::kActorLayer)) {
        return;
      }
      const auto isNear   = [](float a, float b, float tolerance) { return std::abs(a - b) < tolerance; };
      const auto addJoint = [](Actor* actor, JointComponent::JointType type, const Actor* connected) {
        auto* joint = static_cast<JointComponent*>(actor->AddComponent(std::make_unique<JointComponent>(), "Joint"));
        joint->SetType(type);
        joint->SetConnectedActor(connected);
        return joint;
      };
      const auto distance = [](const ColliderProbe& a, const ColliderProbe& b) {
        return (a.actor->GetTransform().translate - b.actor->GetTransform().translate).Length();
      };

      BaseScene scene;
      scene.BeginPlay();
      const auto spawnBall = [&scene](const Vector3& position) {
        const ColliderProbe ball = SpawnSolid(scene, position, ColliderComponent::Shape::Sphere);
        AddRigidbody(ball.actor);
        return ball;
      };
      const ColliderProbe floor = SpawnSolid(scene, { 0.0f, -0.5f, 0.0f }, ColliderComponent::Shape::Box);
      floor.actor->GetTransform().scale = { 1000.0f, 1.0f, 1000.0f };

      // 半径 0.5 の球 2 つを、0.4 だけ離して（重ねて）宙に置き、ゆるい Distance（0〜5）でつなぐ
      const auto spawnOverlapped = [&](float x, bool enablesCollision) {
        const ColliderProbe first  = spawnBall({ x, 50.0f, 0.0f });
        const ColliderProbe second = spawnBall({ x + 0.4f, 50.0f, 0.0f });
        JointComponent*     rope   = addJoint(second.actor, Distance, first.actor);
        rope->SetDistanceLimits(0.0f, 5.0f);
        rope->SetEnableCollision(enablesCollision);
        return std::pair{ first, second };
      };
      const auto [apartA, apartB]   = spawnOverlapped(0.0f, true);
      const auto [mergedA, mergedB] = spawnOverlapped(10.0f, false);

      // 台（上の面が y = 5.25）と、その上に落とす球とキャラクター。どちらも、台とは当たらない組にする
      const ColliderProbe table = SpawnSolid(scene, { 20.0f, 5.0f, 0.0f }, ColliderComponent::Shape::Box);
      table.actor->GetTransform().scale = { 4.0f, 0.5f, 2.0f };
      const ColliderProbe ghost       = spawnBall({ 19.0f, 6.0f, 0.0f });
      const Probe         ghostWalker = SpawnProbe(scene, { 21.0f, 6.5f, 0.0f });
      auto*               walker      = static_cast<CharacterControllerComponent*>(
        ghostWalker.actor->AddComponent(std::make_unique<CharacterControllerComponent>(), "CharacterController"));
      physics->IgnoreCollision(ghost.actor, table.actor);
      physics->IgnoreCollision(table.actor, ghostWalker.actor);

      // 中心を Z 軸の蝶番でワールドにつないだ箱を、モーターで回す
      const ColliderProbe rotor     = SpawnSolid(scene, { 40.0f, 50.0f, 0.0f }, ColliderComponent::Shape::Box);
      RigidbodyComponent* rotorBody = AddRigidbody(rotor.actor);
      JointComponent*     motor     = addJoint(rotor.actor, Hinge, nullptr);
      motor->SetAxis({ 0.0f, 0.0f, 1.0f });
      motor->SetMotor(2.0f, 1000.0f);

      // 横の点を蝶番で留めた箱（振り子）。摩擦が重さ（約 9.8 N·m）に勝つので、下がらない
      const ColliderProbe stiffDoor  = SpawnSolid(scene, { 60.0f, 50.0f, 0.0f }, ColliderComponent::Shape::Box);
      AddRigidbody(stiffDoor.actor);
      JointComponent* stiffHinge = addJoint(stiffDoor.actor, Hinge, nullptr);
      stiffHinge->SetAnchor({ -1.0f, 0.0f, 0.0f });
      stiffHinge->SetAxis({ 0.0f, 0.0f, 1.0f });
      stiffHinge->SetFriction(1000.0f);

      // 動かない hook から、距離 2 のばね（20 N/m）で吊るした球（1 kg）。重さの分（約 0.49）だけ伸びた所で止まる
      const ColliderProbe hook = SpawnSolid(scene, { 80.0f, 50.0f, 0.0f }, ColliderComponent::Shape::Sphere);
      AddRigidbody(hook.actor)->SetBodyType(RigidbodyComponent::BodyType::Kinematic);
      const ColliderProbe bob = spawnBall({ 80.0f, 48.0f, 0.0f });
      addJoint(bob.actor, Distance, hook.actor)->SetSpring(20.0f, 5.0f);

      // ワールドに固めた球（重さ 約 9.8 N）。Break Force が 5 なら外れて落ち、50 なら外れない
      const ColliderProbe weak      = spawnBall({ 100.0f, 50.0f, 0.0f });
      JointComponent*     weakJoint = addJoint(weak.actor, Fixed, nullptr);
      weakJoint->SetBreakForce(5.0f);
      const ColliderProbe strong      = spawnBall({ 110.0f, 50.0f, 0.0f });
      JointComponent*     strongJoint = addJoint(strong.actor, Fixed, nullptr);
      strongJoint->SetBreakForce(50.0f);
      scene.UpdateAlways();
      RunFrames(scene, 30);

      // 当たる組は押し合って離れ、当たらない組（既定）は重なったままで、知らせも届かない
      assert(distance(apartA, apartB) > 0.95f && !apartA.recorder->log.empty());
      assert(isNear(distance(mergedA, mergedB), 0.4f, 0.01f) && mergedA.recorder->log.empty() && mergedB.recorder->log.empty());

      // 弱いジョイントは外れて、その Actor に知らせが届く。強い方は、そのまま
      assert(weak.recorder->breakForces.size() == 1 && weak.recorder->breakForces[0] > 5.0f && weakJoint->IsBroken());
      assert(weak.actor->GetTransform().translate.y < 49.5f);
      assert(strong.recorder->breakForces.empty() && !strongJoint->IsBroken() && IsNear(strong.actor->GetTransform().translate, { 110.0f, 50.0f, 0.0f }));

      // モーター：決めた速さで、軸の周りに回る。速さは、つなぎ直さずに変えられる
      assert(isNear(rotorBody->GetAngularVelocity().z, 2.0f, 0.1f));
      motor->SetMotor(-2.0f, 1000.0f);
      RunFrames(scene, 30);
      assert(isNear(rotorBody->GetAngularVelocity().z, -2.0f, 0.1f) && IsNear(rotor.actor->GetTransform().translate, { 40.0f, 50.0f, 0.0f }));
      assert(isNear(stiffDoor.actor->GetTransform().translate.y, 50.0f, 0.01f));

      // 当たらない組の球とキャラクターは、台を素通りして床まで落ちる
      RunFrames(scene, 60);
      assert(ghost.actor->GetTransform().translate.y < 1.0f && ghost.recorder->last == floor.actor && table.recorder->log.empty());
      assert(walker->IsGrounded() && isNear(ghostWalker.actor->GetTransform().translate.y, 1.0f, 0.05f) && walker->GetGroundActor() == floor.actor);

      // 解いてから台の上へ戻すと、台に乗る
      physics->IgnoreCollision(ghost.actor, table.actor, false);
      physics->IgnoreCollision(ghostWalker.actor, table.actor, false);
      ghost.actor->GetTransform().translate       = { 19.0f, 6.0f, 0.0f };
      ghostWalker.actor->GetTransform().translate = { 21.0f, 6.5f, 0.0f };
      RunFrames(scene, 90);
      assert(isNear(ghost.actor->GetTransform().translate.y, 5.75f, 0.05f) && ghost.recorder->last == table.actor);
      assert(isNear(ghostWalker.actor->GetTransform().translate.y, 6.25f, 0.05f) && walker->GetGroundActor() == table.actor);

      // ばね：重さの分だけ伸びて止まる
      RunFrames(scene, 60);
      assert(distance(hook, bob) > 2.3f && distance(hook, bob) < 2.7f);

      scene.Shutdown();
      assert(physics->GetBodyCount() == 0);
    }

    /// <summary>
    /// Character Controller：床に立つ・段差を登る・壁で止まる・トリガーに知らせが届く・レイが当たる・他のキャラクターと重ならない・
    /// 自分の Actor の Collider には当たらない・位置を書き換えると置き直る・Actor をオフにすると Exit が届く・
    /// Push Strength が 0 なら Dynamic を押さず、既定なら押す
    /// </summary>
    void CheckCharacterController()
    {
      PhysicsWorld* physics = PhysicsWorld::GetInstance();
      if (!physics->CanCollide(ColliderComponent::kActorLayer, ColliderComponent::kActorLayer)) {
        return;
      }
      const auto isNear        = [](float a, float b, float tolerance) { return std::abs(a - b) < tolerance; };
      const auto addController = [](Actor* actor) {
        return static_cast<CharacterControllerComponent*>(actor->AddComponent(std::make_unique<CharacterControllerComponent>(), "CharacterController"));
      };
      const auto spawnBox = [](BaseScene& scene, const Vector3& position, const Vector3& scale) {
        const ColliderProbe box = SpawnSolid(scene, position, ColliderComponent::Shape::Box);
        box.actor->GetTransform().scale = scale;
        return box;
      };
      // カプセル（半径 0.5・全長 2）の中心が Actor の位置。床からは、Jolt が少し（0.02）浮かせて立たせる
      const float standing = 1.0f;

      BaseScene scene;
      scene.BeginPlay();
      // 床（上の面が y = 0）、高さ 0.3 の段（x = 2〜4.5）、その奥の壁（手前の面が x = 4.5）
      const ColliderProbe floor = spawnBox(scene, { 0.0f, -0.5f, 0.0f }, { 1000.0f, 1.0f, 1000.0f });
      spawnBox(scene, { 3.25f, 0.15f, 0.0f }, { 2.5f, 0.3f, 10.0f });
      const ColliderProbe wall = spawnBox(scene, { 5.0f, 1.5f, 0.0f }, { 1.0f, 3.0f, 10.0f });
      // walker には、実体の球の Collider も付いている（自分の Actor の Collider には当たらない）
      const ColliderProbe           walker     = SpawnSolid(scene, { 0.0f, 1.5f, 0.0f }, ColliderComponent::Shape::Sphere);
      CharacterControllerComponent* controller = addController(walker.actor);
      const Probe                   blocker    = SpawnProbe(scene, { -3.0f, 1.5f, 0.0f });
      addController(blocker.actor);
      const ColliderProbe zone    = SpawnTrigger(scene, { 3.3f, 1.9f, 0.0f });
      const ColliderProbe farZone = SpawnTrigger(scene, { 50.0f, 2.0f, 0.0f });
      const ColliderProbe ball    = SpawnSolid(scene, { 52.0f, 0.5f, 0.0f }, ColliderComponent::Shape::Sphere);
      AddRigidbody(ball.actor);
      scene.UpdateAlways();

      // 落ちて、床に立つ
      RunFrames(scene, 60);
      assert(controller->IsGrounded() && isNear(walker.actor->GetTransform().translate.y, standing, 0.05f));
      assert(isNear(walker.actor->GetTransform().translate.x, 0.0f, 0.001f));

      // 自分の位置から下へ飛ばしたレイは、自分のカプセルと Collider を素通りして、床に当たる
      const Vector3                                 feetOrigin = walker.actor->GetTransform().translate;
      const std::optional<PhysicsWorld::RaycastHit> below      = physics->Raycast(feetOrigin, { 0.0f, -1.0f, 0.0f }, 10.0f);
      assert(below && below->actor == floor.actor && isNear(below->distance, feetOrigin.y, 0.01f));

      // 立っている間、床に触れている知らせ（OnControllerColliderHit）が、キャラクターの Actor にだけ届く
      assert(!walker.recorder->hits.empty() && floor.recorder->hits.empty());
      const ControllerColliderHit standingOn = walker.recorder->hits.back();
      assert(standingOn.actor == floor.actor && IsNear(standingOn.normal, { 0.0f, 1.0f, 0.0f }) && isNear(standingOn.point.y, 0.0f, 0.05f));
      assert(IsNear(standingOn.moveDirection, { 0.0f, -1.0f, 0.0f }));
      const auto touched = [&walker](const Actor* actor) {
        return std::ranges::find_if(walker.recorder->hits, [actor](const ControllerColliderHit& hit) { return hit.actor == actor; });
      };

      // +X へ歩くと、段を登って、壁の手前で止まる。途中のトリガーから知らせが届く
      walker.recorder->hits.clear();
      controller->SetVelocity({ 3.0f, 0.0f, 0.0f });
      RunFrames(scene, 180);
      const Vector3 atWall = walker.actor->GetTransform().translate;
      assert(atWall.x > 3.9f && atWall.x < 4.01f && isNear(atWall.y, standing + 0.3f, 0.06f));
      assert(!walker.recorder->log.empty() && walker.recorder->log.front() == 'E' && walker.recorder->last == zone.actor);

      // 壁に押し当てている間は、壁に触れた知らせが届く（面の向きは、壁からキャラクターへ）。Trigger は、この知らせには入らない
      const auto wallHit = touched(wall.actor);
      assert(wallHit != walker.recorder->hits.end() && wallHit->normal.x < -0.9f && wallHit->moveDirection.x > 0.9f);
      assert(touched(zone.actor) == walker.recorder->hits.end());

      // 上からのレイは、カプセルの頭に当たる
      const std::optional<PhysicsWorld::RaycastHit> hit = physics->Raycast({ atWall.x, 10.0f, 0.0f }, { 0.0f, -1.0f, 0.0f }, 20.0f, 0xffffffff, false);
      assert(hit && hit->actor == walker.actor && isNear(hit->point.y, atWall.y + 1.0f, 0.05f));

      // -X へ歩くと、段を下りて、立っている他のキャラクターの手前で止まる（押しのけない）。触れた知らせは、相手のキャラクターについても届く
      walker.recorder->hits.clear();
      controller->SetVelocity({ -3.0f, 0.0f, 0.0f });
      RunFrames(scene, 300);
      const Vector3 atBlocker = walker.actor->GetTransform().translate;
      assert(isNear(blocker.actor->GetTransform().translate.x, -3.0f, 0.05f));
      assert(atBlocker.x - blocker.actor->GetTransform().translate.x > 0.95f && atBlocker.x < -1.9f && isNear(atBlocker.y, standing, 0.05f));
      assert(walker.recorder->log.find('X') != std::string::npos);
      assert(touched(blocker.actor) != walker.recorder->hits.end());

      // 位置を書き換えると、その位置へ置き直されて、そこから落ちる
      controller->SetVelocity({});
      walker.actor->GetTransform().translate = { 50.0f, 5.0f, 0.0f };
      RunFrames(scene, 3);
      assert(isNear(walker.actor->GetTransform().translate.x, 50.0f, 0.001f));
      assert(walker.actor->GetTransform().translate.y < 5.0f && walker.actor->GetTransform().translate.y > 4.9f);

      // 落ちた先のトリガーに入る。Actor をオフにすると Exit が届き、戻すと Enter が出直す
      RunFrames(scene, 120);
      assert(controller->IsGrounded() && farZone.recorder->log.front() == 'E' && farZone.recorder->log.back() == 'S');
      walker.actor->SetActive(false);
      scene.UpdateAlways();
      RunFrames(scene, 2);
      assert(farZone.recorder->log.back() == 'X');
      walker.actor->SetActive(true);
      scene.UpdateAlways();
      RunFrames(scene, 2);
      assert(farZone.recorder->log.back() != 'X' && farZone.recorder->last == walker.actor);

      // Push Strength が 0 なら、歩いて当たった Dynamic の球を押さずに、手前で止まる
      controller->SetPushStrength(0.0f);
      controller->SetVelocity({ 2.0f, 0.0f, 0.0f });
      RunFrames(scene, 90);
      assert(isNear(ball.actor->GetTransform().translate.x, 52.0f, 0.05f) && walker.actor->GetTransform().translate.x < 51.1f);

      // 既定の強さに戻すと、球は押されて動く
      controller->SetPushStrength(100.0f);
      RunFrames(scene, 120);
      assert(ball.actor->GetTransform().translate.x > 52.5f && walker.actor->GetTransform().translate.x > 51.5f);

      scene.Shutdown();
      assert(physics->GetBodyCount() == 0);
    }

    /// <summary>
    /// Character Controller の Mass：中心を蝶番で留めた Dynamic の板の端に立つと、重さの分だけ板が傾く。0 なら傾かない
    /// </summary>
    void CheckCharacterMass()
    {
      PhysicsWorld* physics = PhysicsWorld::GetInstance();
      if (!physics->CanCollide(ColliderComponent::kActorLayer, ColliderComponent::kActorLayer)) {
        return;
      }
      BaseScene scene;
      scene.BeginPlay();
      const auto spawnSeesaw = [&scene](float x, float mass) {
        // 4 × 0.2 × 1 の板。中心を Z 軸の蝶番でワールドにつなぎ、傾きは 0.5 ラジアンまで
        const ColliderProbe plank = SpawnSolid(scene, { x, 3.0f, 0.0f }, ColliderComponent::Shape::Box);
        plank.actor->GetTransform().scale = { 4.0f, 0.2f, 1.0f };
        AddRigidbody(plank.actor);
        auto* hinge = static_cast<JointComponent*>(plank.actor->AddComponent(std::make_unique<JointComponent>(), "Joint"));
        hinge->SetType(JointComponent::JointType::Hinge);
        hinge->SetAxis({ 0.0f, 0.0f, 1.0f });
        hinge->SetAngleLimits(-0.5f, 0.5f);

        // 板の端の上に、キャラクターを立たせる
        const Probe rider      = SpawnProbe(scene, { x + 1.2f, 4.3f, 0.0f });
        auto*       controller = static_cast<CharacterControllerComponent*>(
          rider.actor->AddComponent(std::make_unique<CharacterControllerComponent>(), "CharacterController"));
        controller->SetMass(mass);
        return plank.actor;
      };
      const Actor* heavy      = spawnSeesaw(0.0f, 70.0f);
      const Actor* weightless = spawnSeesaw(20.0f, 0.0f);
      scene.UpdateAlways();

      RunFrames(scene, 90);
      assert(std::abs(heavy->GetTransform().rotate.z) > 0.1f);
      assert(std::abs(weightless->GetTransform().rotate.z) < 0.02f);

      scene.Shutdown();
      assert(physics->GetBodyCount() == 0);
    }

    /// <summary>
    /// Character Controller の足元と形：足元の面の向きと Actor が取れる。動く Kinematic の床の上で、Ride Moving Ground がオフなら置いていかれ、オンなら付いていく。
    /// 低い天井の下では SetHeight で立てず、何も無い所では足元を保って伸びる。急な坂の上では IsOnSteepGround
    /// </summary>
    void CheckCharacterGround()
    {
      PhysicsWorld* physics = PhysicsWorld::GetInstance();
      if (!physics->CanCollide(ColliderComponent::kActorLayer, ColliderComponent::kActorLayer)) {
        return;
      }
      const auto isNear        = [](float a, float b, float tolerance) { return std::abs(a - b) < tolerance; };
      const auto addController = [](Actor* actor) {
        return static_cast<CharacterControllerComponent*>(actor->AddComponent(std::make_unique<CharacterControllerComponent>(), "CharacterController"));
      };
      const auto spawnBox = [](BaseScene& scene, const Vector3& position, const Vector3& scale) {
        const ColliderProbe box = SpawnSolid(scene, position, ColliderComponent::Shape::Box);
        box.actor->GetTransform().scale = scale;
        return box.actor;
      };

      BaseScene scene;
      scene.BeginPlay();
      // 動く足場（Kinematic、上の面が y = 0）を 2 枚と、それぞれの上にキャラクター（カプセルの中心が Actor の位置。全長 2）
      const auto spawnPlatform = [&](float z) {
        Actor* platform = spawnBox(scene, { 0.0f, -0.5f, z }, { 6.0f, 1.0f, 4.0f });
        AddRigidbody(platform)->SetBodyType(RigidbodyComponent::BodyType::Kinematic);
        return platform;
      };
      Actor*                        plainPlatform  = spawnPlatform(0.0f);
      Actor*                        ridingPlatform = spawnPlatform(20.0f);
      const Probe                   left           = SpawnProbe(scene, { 0.0f, 1.2f, 0.0f });
      CharacterControllerComponent* leftBehind     = addController(left.actor);
      const Probe                   rider          = SpawnProbe(scene, { 0.0f, 1.2f, 20.0f });
      CharacterControllerComponent* riding         = addController(rider.actor);
      riding->SetRidesMovingGround(true);

      // 床（上の面が y = 0）と、その上の低い天井（下の面が y = 1.5、x は -2〜2）。全長 1 にしたキャラクターを、天井の下に立たせる
      spawnBox(scene, { 0.0f, -0.5f, 40.0f }, { 12.0f, 1.0f, 6.0f });
      spawnBox(scene, { 0.0f, 2.0f, 40.0f }, { 4.0f, 1.0f, 6.0f });
      const Probe                   croucher  = SpawnProbe(scene, { 0.0f, 1.2f, 40.0f });
      CharacterControllerComponent* crouching = addController(croucher.actor);
      [[maybe_unused]] const bool isShortened = crouching->SetHeight(1.0f);
      assert(isShortened && crouching->GetHeight() == 1.0f);

      // 60 度の坂（登れる角度の既定は 50 度）と、その上に落とすキャラクター
      Actor* ramp = spawnBox(scene, { 0.0f, 3.0f, 60.0f }, { 8.0f, 0.5f, 4.0f });
      ramp->GetTransform().rotate = { 0.0f, 0.0f, 1.0472f };
      const Probe                   slider  = SpawnProbe(scene, { 0.0f, 5.5f, 60.0f });
      CharacterControllerComponent* sliding = addController(slider.actor);
      scene.UpdateAlways();

      // 落ちて立つまでの間に、坂の上のキャラクターは急な面に乗る（立ってはいない。面の向きは、坂の面）
      bool wasOnSteepGround = false;
      for (int frame = 0; frame < 60; ++frame) {
        scene.UpdateFrame(PhysicsWorld::kFixedDeltaTime);
        if (sliding->IsOnSteepGround()) {
          wasOnSteepGround = true;
          assert(!sliding->IsGrounded() && isNear(sliding->GetGroundNormal().y, 0.5f, 0.05f) && sliding->GetGroundActor() == ramp);
        }
      }
      assert(wasOnSteepGround);

      // 足元：面の向きは上、Actor は乗っている足場
      assert(leftBehind->IsGrounded() && riding->IsGrounded() && !riding->IsOnSteepGround());
      assert(IsNear(riding->GetGroundNormal(), { 0.0f, 1.0f, 0.0f }) && riding->GetGroundActor() == ridingPlatform);
      assert(leftBehind->GetGroundActor() == plainPlatform);

      // 足場を +X へ 2 動かす。オフのキャラクターは置いていかれ、オンのキャラクターは付いていく（控えている速度には、床の分は入らない）
      for (int step = 0; step < 100; ++step) {
        plainPlatform->GetTransform().translate.x += 0.02f;
        ridingPlatform->GetTransform().translate.x += 0.02f;
        scene.UpdateFrame(PhysicsWorld::kFixedDeltaTime);
      }
      RunFrames(scene, 10);
      assert(std::abs(left.actor->GetTransform().translate.x) < 0.3f && leftBehind->IsGrounded());
      assert(isNear(rider.actor->GetTransform().translate.x, 2.0f, 0.1f) && riding->IsGrounded());
      assert(isNear(riding->GetVelocity().x, 0.0f, 0.001f));

      // 低い天井の下では、立てない（形は変わらない）
      assert(crouching->IsGrounded() && isNear(croucher.actor->GetTransform().translate.y, 1.0f, 0.05f));
      assert(!crouching->SetHeight(2.0f) && crouching->GetHeight() == 1.0f);

      // 天井の外へ出ると立てる。足元は動かず、頭が 1 上がる（上からのレイが、頭に当たる）
      croucher.actor->GetTransform().translate.x = 4.0f;
      RunFrames(scene, 5);
      [[maybe_unused]] const bool isStanding = crouching->SetHeight(2.0f);
      assert(isStanding && crouching->GetHeight() == 2.0f);
      RunFrames(scene, 30);
      const Vector3 stood = croucher.actor->GetTransform().translate;
      assert(crouching->IsGrounded() && isNear(stood.y, 1.0f, 0.05f));
      const std::optional<PhysicsWorld::RaycastHit> head = physics->Raycast({ 4.0f, 10.0f, 40.0f }, { 0.0f, -1.0f, 0.0f }, 20.0f, 0xffffffff, false);
      assert(head && head->actor == croucher.actor && isNear(head->point.y, stood.y + 1.0f, 0.05f));

      scene.Shutdown();
      assert(physics->GetBodyCount() == 0);
    }

#ifdef TAKO_EDITOR
    /// <summary>
    /// 形式 1 のプロジェクトの移行：isTrigger の無いコライダー（1 ファイルのシーン・Actor ごとのファイル・プレハブ・置いた物の差分）に true が入り、書いてある値と読めないファイルは変わらない
    /// </summary>
    void CheckColliderMigration()
    {
      namespace fs = std::filesystem;
      const fs::path root = fs::temp_directory_path() / "TakoPhysicsSelfTest";
      const fs::path json = root / "GameProject" / "resources" / "Json";
      fs::remove_all(root);
      fs::create_directories(json / "Scenes" / "Split.scene");
      fs::create_directories(json / "Prefabs");
      const auto write = [](const fs::path& path, const char* text) { std::ofstream(path) << text; };
      const auto read  = [](const fs::path& path) {
        std::ifstream file(path);
        return std::string(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
      };
      const auto isTriggerAt = [&read](const fs::path& path, const char* pointer) {
        return nlohmann::json::parse(read(path)).at(nlohmann::json::json_pointer(pointer));
      };

      const fs::path whole  = json / "Scenes" / "Whole.scene.json";
      const fs::path split  = json / "Scenes" / "Split.scene" / "5.json";
      const fs::path broken = json / "Scenes" / "Split.scene" / "7.json";
      const fs::path prefab = json / "Prefabs" / "Crate.prefab.json";
      write(whole, R"({ "actors": [
        { "type": "Actor", "components": {
          "Collider": { "type": "ColliderComponent", "properties": { "radius": 2.0 } },
          "Mesh": { "type": "MeshComponent", "properties": {} } } },
        { "prefab": "Crate", "overrides": [
          { "op": "add", "path": "/components/Extra", "value": { "type": "ColliderComponent", "properties": { "shape": "Box" } } } ] } ] })");
      write(split, R"({ "type": "Actor", "components": { "Collider": { "type": "ColliderComponent", "properties": { "isTrigger": false } } } })");
      write(broken, "{ not json");
      write(prefab, R"({ "actors": [ { "type": "Actor", "components": { "Collider": { "type": "ColliderComponent" } } } ] })");

      // 移行が出す記録は、Console に出さない
      const Logger::Sink              console = Logger::SetSink(nullptr);
      [[maybe_unused]] const std::string error = ProjectMigration::Run(root, 1);
      Logger::SetSink(console);
      assert(error.empty() && fs::is_directory(root / "Generated" / "Backup"));

      assert(isTriggerAt(whole, "/actors/0/components/Collider/properties/isTrigger") == true);
      assert(isTriggerAt(whole, "/actors/0/components/Collider/properties/radius") == 2.0);
      assert(!isTriggerAt(whole, "/actors/0/components/Mesh/properties").contains("isTrigger"));
      assert(isTriggerAt(whole, "/actors/1/overrides/0/value/properties/isTrigger") == true);
      assert(isTriggerAt(split, "/components/Collider/properties/isTrigger") == false);
      assert(isTriggerAt(prefab, "/actors/0/components/Collider/properties/isTrigger") == true);
      assert(read(broken) == "{ not json");

      fs::remove_all(root);
    }
#endif

  } // namespace

  void RunPhysicsSelfTest()
  {
    CheckTriggerEvents();
    CheckLayersAndKinds();
    CheckRaycast();
    CheckShapeQueries();
    CheckPoseFollowsActor();
    CheckScaleFollowsActor();
    CheckColliderComponent();
    CheckRigidbody();
    CheckRigidbodyControls();
    CheckCollisionInfo();
    CheckMeshShapes();
    CheckJoints();
    CheckJointFeatures();
    CheckCharacterController();
    CheckCharacterMass();
    CheckCharacterGround();
#ifdef TAKO_EDITOR
    CheckColliderMigration();
#endif
    assert(PhysicsWorld::GetInstance()->GetBodyCount() == 0);
  }

} // namespace Tako

#endif // _DEBUG

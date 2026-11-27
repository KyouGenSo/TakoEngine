#include "PhysicsSelfTest.h"

#ifdef _DEBUG

#include "Actor.h"
#include "BaseScene.h"
#include "ColliderComponent.h"
#include "Component.h"
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

#include <cassert>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
#include <span>
#include <string>

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
      void OnCollisionEnter(Actor* other) override { Record('E', other); }
      void OnCollisionStay(Actor* other) override { Record('S', other); }
      void OnCollisionExit(Actor* other) override { Record('X', other); }

    public: //メンバー変数
      std::string log;
      Actor*      last = nullptr;

    private: //非公開関数
      void Record(char kind, Actor* other)
      {
        log += kind;
        last = other;
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

      physics->DestroyBody(wallBody);
      physics->DestroyBody(veilBody);
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
    CheckPoseFollowsActor();
    CheckScaleFollowsActor();
    CheckColliderComponent();
    CheckRigidbody();
    CheckMeshShapes();
#ifdef TAKO_EDITOR
    CheckColliderMigration();
#endif
    assert(PhysicsWorld::GetInstance()->GetBodyCount() == 0);
  }

} // namespace Tako

#endif // _DEBUG

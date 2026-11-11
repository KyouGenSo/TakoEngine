#include "SceneSelfTest.h"

#ifdef _DEBUG

#include "Actor.h"
#include "BaseScene.h"
#include "Logger.h"
#include "Mat4x4Func.h"
#include "MeshComponent.h"
#include "Model.h"
#include "Object3d.h"
#include "Prefab.h"
#include "TypeRegistry.h"

#include <json.hpp>

#include <any>
#include <cassert>
#include <cmath>
#include <numbers>
#include <optional>
#include <string>
#include <vector>

namespace Tako {

  namespace {

    /// <summary>
    /// BeginPlay の時点で、同じシーンの相手（ActorRef）を引けたかを覚える
    /// </summary>
    class ResolveOnBeginPlay final : public Component {
    public: //メンバー関数
      explicit ResolveOnBeginPlay(ActorRef target) : target_(target) {}

      void BeginPlay() override { resolved = GetOwner()->Resolve(target_); }

    public: //メンバー変数
      Actor* resolved = nullptr;

    private: //メンバー変数
      ActorRef target_;
    };

    /// <summary>
    /// Tick と LateTick が呼ばれた回数を数える
    /// </summary>
    class TickCounter final : public Component {
    public: //メンバー関数
      explicit TickCounter(bool ticksWhilePaused) : ticksWhilePaused_(ticksWhilePaused) {}

      void Tick([[maybe_unused]] float deltaTime) override { ++ticks; }
      void LateTick([[maybe_unused]] float deltaTime) override { ++lateTicks; }
      bool TickWhilePaused() const override { return ticksWhilePaused_; }
      void OnActiveChanged(bool active) override
      {
        isActive = active;
        ++activeChanges;
      }

    public: //メンバー変数
      int  ticks         = 0;
      int  lateTicks     = 0;
      int  activeChanges = 0;
      bool isActive      = true;

    private: //メンバー変数
      bool ticksWhilePaused_;
    };

    /// <summary>
    /// 呼ばれた順を共有の記録に書く。order は TCLASS(Order = n) の代わり
    /// </summary>
    class CallRecorder final : public Component {
    public: //メンバー関数
      CallRecorder(std::vector<std::string>& calls, std::string label, int32_t order)
        : calls_(calls), label_(std::move(label)), type_{ "CallRecorder", &Component::StaticType(), nullptr, "", "", 0, "", order } {}

      void Awake() override { calls_.push_back("awake " + label_); }
      void BeginPlay() override { calls_.push_back("begin " + label_); }
      void Tick([[maybe_unused]] float deltaTime) override { calls_.push_back("tick " + label_); }

      const TypeInfo& GetType() const override { return type_; }

    private: //メンバー変数
      std::vector<std::string>& calls_;
      std::string               label_;
      TypeInfo                  type_;
    };

    bool IsNear(const Matrix4x4& a, const Matrix4x4& b)
    {
      for (int row = 0; row < 4; ++row) {
        for (int column = 0; column < 4; ++column) {
          if (std::abs(a.m[row][column] - b.m[row][column]) > 1e-3f) { return false; }
        }
      }
      return true;
    }

  } // namespace

  void RunSceneSelfTest()
  {
    // 行列 → Transform → 行列で元に戻る（MakeAffine と Decompose の回転の約束が合っている）
    const Transform source{ { 2.0f, 1.0f, 0.5f }, { 0.7f, -1.1f, 2.4f }, { 3.0f, -4.0f, 5.0f } };
    const Matrix4x4 matrix = Mat4x4::MakeAffine(source.scale, source.rotate, source.translate);
    Transform       decomposed;
    Mat4x4::Decompose(matrix, decomposed.translate, decomposed.rotate, decomposed.scale);
    assert(IsNear(matrix, Mat4x4::MakeAffine(decomposed.scale, decomposed.rotate, decomposed.translate)));

    BaseScene scene;
    Actor*    parent     = scene.Spawn("Actor");
    Actor*    child      = scene.Spawn("Actor");
    Actor*    grandchild = scene.Spawn("Actor");
    scene.UpdateAlways();
    parent->SetName("Parent");
    child->SetName("Child");
    parent->SetTransform({ { 2.0f, 2.0f, 2.0f }, { 0.3f, 1.2f, -0.4f }, { 1.0f, 2.0f, 3.0f } });
    child->SetTransform({ { 1.0f, 1.0f, 1.0f }, { -0.5f, 0.2f, 0.9f }, { -4.0f, 0.5f, 6.0f } });

    // 親を付けても外しても、見た目の位置は変わらない
    const Matrix4x4 childWorld = child->GetWorldMatrix();
    assert(child->SetParent(parent));
    assert(child->GetParent() == parent && parent->GetChildren().size() == 1 && parent->FindChild("Child") == child);
    assert(IsNear(childWorld, child->GetWorldMatrix()));
    assert(grandchild->SetParent(child));

    // 自分の子孫を親にはできない
    assert(!parent->SetParent(grandchild));
    assert(!parent->SetParent(parent));
    assert(parent->GetParent() == nullptr);

    // 保存して読み直しても、親子と位置が同じ
    const nlohmann::json saved = scene.Save();
    BaseScene            loaded;
    loaded.Load(saved);
    loaded.UpdateAlways();
    Actor* loadedChild = loaded.FindActor(child->GetId());
    assert(loadedChild && loadedChild->GetParent() == loaded.FindActor(parent->GetId()));
    assert(IsNear(childWorld, loadedChild->GetWorldMatrix()));
    assert(loaded.Save() == saved);

    // 同じ親の中での並べ替え（親なしどうし）
    Actor* other = scene.Spawn("Actor");
    scene.UpdateAlways();
    scene.MoveActor(other, -1);
    assert(scene.GetActors()[0].get() == other && scene.GetActors()[1].get() == parent);
    scene.MoveActor(other, 1);
    assert(scene.GetActors()[0].get() == parent && scene.GetActors()[1].get() == other);

    // 外すと、ワールドの位置が Transform になる
    assert(child->SetParent(nullptr));
    assert(IsNear(childWorld, Mat4x4::MakeAffine(child->GetTransform().scale, child->GetTransform().rotate, child->GetTransform().translate)));
    assert(child->SetParent(parent));

    // 親を消すと、子と孫も消える
    const ActorId childId      = child->GetId();
    const ActorId grandchildId = grandchild->GetId();
    scene.Destroy(parent);
    scene.UpdateAlways();
    assert(!scene.FindActor(childId) && !scene.FindActor(grandchildId) && scene.GetActors().size() == 1);

    // MeshComponent の描画物は、LateTick の後に Actor と同じワールド行列を返す（Mesh エミッターがこれで位置を決める）
    auto* mesh = static_cast<MeshComponent*>(other->AddComponent(std::make_unique<MeshComponent>(), "Mesh"));
    other->GetTransform().translate = { 4.0f, 5.0f, 6.0f };
    scene.UpdateFrame(0.016f);
    assert(IsNear(mesh->GetObject3d()->GetWorldMatrix(), other->GetWorldMatrix()));

    // レイは、描く位置のメッシュの面までの距離を返す。外れたレイ・逆向きのレイ・つぶれたメッシュには当たらない
    mesh->SetModel("EngineResources/Model/white_cube.gltf");
    Object3d*     cube = mesh->GetObject3d();
    const Mesh*   box  = cube->GetModel()->GetMesh();
    const Vector3 half = (box->GetAABBLocalMax() - box->GetAABBLocalMin()) * 0.5f;
    // 面の中心は 2 枚の三角形の境目なので、少しずらした所を通す
    const Vector3 from{ 4.1f, 5.2f, -4.0f };
    const Vector3 forward{ 0.0f, 0.0f, 1.0f };
    const auto    cast = [&](const Vector3& scale, const Vector3& rotate, const Vector3& origin, const Vector3& direction) {
      other->GetTransform().scale  = scale;
      other->GetTransform().rotate = rotate;
      scene.UpdateFrame(0.016f);
      cube->Update();
      return cube->Raycast(origin, direction);
    };
    std::optional<float> hit = cast({ 2.0f, 3.0f, 4.0f }, {}, from, forward);
    assert(hit && std::abs(*hit - (10.0f - half.z * 4.0f)) < 0.001f);
    // Y 軸で 90 度回すと、ローカルの X がレイの向きに来る
    hit = cast({ 2.0f, 3.0f, 4.0f }, { 0.0f, std::numbers::pi_v<float> * 0.5f, 0.0f }, from, forward);
    assert(hit && std::abs(*hit - (10.0f - half.x * 2.0f)) < 0.001f);
    assert(!cast({ 2.0f, 3.0f, 4.0f }, {}, { 104.0f, 5.2f, -4.0f }, forward));
    assert(!cast({ 2.0f, 3.0f, 4.0f }, {}, from, { 0.0f, 0.0f, -1.0f }));
    assert(!cast({ 0.0f, 3.0f, 4.0f }, {}, from, forward));
    other->GetTransform().scale = { 1.0f, 1.0f, 1.0f };

    // ポーズ中は TickWhilePaused のコンポーネントだけが動く
    auto* normal = static_cast<TickCounter*>(other->AddComponent(std::make_unique<TickCounter>(false), "Normal"));
    auto* always = static_cast<TickCounter*>(other->AddComponent(std::make_unique<TickCounter>(true), "Always"));
    scene.UpdateFrame(0.016f);
    assert(normal->ticks == 1 && normal->lateTicks == 1 && always->ticks == 1);
    scene.SetPaused(true);
    scene.UpdateFrame(0.016f);
    assert(normal->ticks == 1 && normal->lateTicks == 1 && always->ticks == 2 && always->lateTicks == 2);

    // 同時にシーンへ入る Actor は、並びで後ろの相手でも BeginPlay の中で引ける
    BaseScene playing;
    playing.BeginPlay();
    Actor* first  = playing.Spawn("Actor");
    Actor* second = playing.Spawn("Actor");
    auto*  resolver = static_cast<ResolveOnBeginPlay*>(first->AddComponent(std::make_unique<ResolveOnBeginPlay>(ActorRef{ second->GetId() }), "Resolver"));
    playing.UpdateAlways();
    assert(resolver->resolved == second);

    // 全部の Awake が済んでから BeginPlay。Order の小さい物が先で、同じなら並び順。Tick も同じ順
    std::vector<std::string> calls;
    Actor* late = playing.Spawn("Actor");
    late->AddComponent(std::make_unique<CallRecorder>(calls, "late", 10), "Recorder");
    Actor* mixed = playing.Spawn("Actor");
    mixed->AddComponent(std::make_unique<CallRecorder>(calls, "normal", 0), "Recorder");
    mixed->AddComponent(std::make_unique<CallRecorder>(calls, "early", -10), "Early");
    playing.UpdateAlways();
    assert((calls == std::vector<std::string>{ "awake early", "awake normal", "awake late", "begin early", "begin normal", "begin late" }));
    calls.clear();
    playing.UpdateFrame(0.016f);
    assert((calls == std::vector<std::string>{ "tick early", "tick normal", "tick late" }));

    // 親をオフにすると子も Tick しない。切り替えは、次のフレームの頭で OnActiveChanged に届く
    Actor* group  = playing.Spawn("Actor");
    Actor* member = playing.Spawn("Actor");
    auto*  counter = static_cast<TickCounter*>(member->AddComponent(std::make_unique<TickCounter>(false), "Counter"));
    playing.UpdateAlways();
    assert(member->SetParent(group));
    group->SetActive(false);
    playing.UpdateAlways();
    playing.UpdateFrame(0.016f);
    assert(counter->activeChanges == 1 && !counter->isActive && counter->ticks == 0);
    group->SetActive(true);
    playing.UpdateAlways();
    playing.UpdateFrame(0.016f);
    assert(counter->activeChanges == 2 && counter->isActive && counter->ticks == 1);

    // タイマー：1 回の物は 1 回だけ、繰り返す物は取り消すまで呼ばれる。ポーズ中は進まない
    Actor* clock = playing.Spawn("Actor");
    auto*  probe = static_cast<TickCounter*>(clock->AddComponent(std::make_unique<TickCounter>(false), "Probe"));
    playing.UpdateAlways();
    int once     = 0;
    int repeated = 0;
    probe->SetTimer(0.045f, [&once] { ++once; });
    const TimerId repeating = probe->SetTimer(0.033f, [&repeated] { ++repeated; }, true);
    for (int frame = 0; frame < 10; ++frame) {
      playing.UpdateFrame(0.01f);
    }
    assert(once == 1 && repeated == 3);
    probe->ClearTimer(repeating);
    probe->SetTimer(0.005f, [&once] { ++once; });
    playing.SetPaused(true);
    playing.UpdateFrame(0.01f);
    assert(once == 1);
    playing.SetPaused(false);
    for (int frame = 0; frame < 10; ++frame) {
      playing.UpdateFrame(0.01f);
    }
    assert(once == 2 && repeated == 3);

    // イベント：受け取ると決めた名前だけが届き、コンポーネントを外すと届かなくなる
    int heard = 0;
    probe->Subscribe("Ping", [&heard](const std::any& payload) { heard += std::any_cast<int>(payload); });
    playing.SendEvent("Ping", 5);
    playing.SendEvent("Other", 1);
    assert(heard == 5);
    clock->RemoveComponent(probe);
    playing.UpdateAlways();
    playing.SendEvent("Ping", 5);
    assert(heard == 5);

    // タグ
    clock->Load({ { "properties", { { "tags", nlohmann::json::array({ "Enemy" }) } } } });
    assert(clock->HasTag("Enemy") && !clock->HasTag("Friend"));
    assert(playing.FindActorsWithTag("Enemy") == std::vector<Actor*>{ clock } && playing.FindActorsWithTag("Friend").empty());

    // ID の付け替え：組の中の親と ActorRef は新しい ID、組の外の相手は外す。根の無い Actor は、プレハブとのつながりを失う
    nlohmann::json entries = nlohmann::json::array({
      { { "id", ActorId{ 5 } }, { "properties", { { "inner", { { "actor", ActorId{ 6 } } } }, { "outer", { { "actor", ActorId{ 99 } } } } } } },
      { { "id", ActorId{ 6 } }, { "parent", ActorId{ 5 } }, { "prefab", "P" }, { "node", ActorId{ 2 } }, { "root", ActorId{ 77 } } },
    });
    RemapActorIds(entries, { { 5, 10 }, { 6, 11 } }, false);
    assert(entries[0]["id"] == 10 && entries[0]["properties"]["inner"]["actor"] == 11 && entries[0]["properties"]["outer"]["actor"] == 0);
    assert(entries[1]["id"] == 11 && entries[1]["parent"] == 10 && !entries[1].contains("prefab") && !entries[1].contains("root"));

    // 組の複製：子も写り、写しの子は写しの根に付く
    BaseScene stage;
    Actor*    body = stage.Spawn("Actor");
    Actor*    arm  = stage.Spawn("Actor");
    stage.UpdateAlways();
    body->SetName("Body");
    arm->SetName("Arm");
    assert(arm->SetParent(body));
    Actor* copy = stage.SpawnTree(SaveActorTree(*body), true);
    stage.UpdateAlways();
    assert(copy && copy != body && body->GetChildren().size() == 1);
    assert(copy->GetChildren().size() == 1 && copy->GetChildren()[0] != arm && copy->GetChildren()[0]->GetName() == "Arm");

    // 組をプレハブにして置く：根と子が出て、子は根の組に入る
    const nlohmann::json tree    = SaveActorTree(*body);
    const ActorIdMap     nodes   = { { body->GetId(), 1 }, { arm->GetId(), 2 } };
    const char*          kGroup  = "__SelfTestGroup";
    const char*          kSingle = "__SelfTestSingle";
    SetPrefabInMemory(kGroup, MakePrefabContent(kGroup, tree, nodes));
    BaseScene placedScene;
    Actor*    placed = placedScene.Spawn(kGroup);
    placedScene.UpdateAlways();
    assert(placed && placed->IsPrefabRoot() && placed->GetChildren().size() == 1);
    Actor* placedArm = placed->GetChildren()[0];
    assert(placedArm->GetPrefabRoot() == placed->GetId() && placedArm->GetPrefabNode() == 2 && !placedArm->IsPrefabRoot());

    // 置いた物は差分で保存され、読み直しても同じ
    placedArm->GetTransform().translate = { 1.0f, 2.0f, 3.0f };
    const nlohmann::json placedSaved = placedScene.Save();
    assert(placedSaved["actors"].size() == 2 && !placedSaved["actors"][1].contains("type") && placedSaved["actors"][1]["root"] == placed->GetId());
    BaseScene reloaded;
    reloaded.Load(placedSaved);
    reloaded.UpdateAlways();
    assert(reloaded.Save() == placedSaved);
    Actor* reloadedArm = reloaded.FindActor(placedArm->GetId());
    assert(reloadedArm && reloadedArm->GetParent() == reloaded.FindActor(placed->GetId()) && reloadedArm->GetTransform().translate.y == 2.0f);

    // プレハブに Actor を足すと、置いてある組にも出る。前からの差分は残る
    nlohmann::json grown = MakePrefabContent(kGroup, tree, nodes);
    grown["actors"].push_back({ { "id", ActorId{ 3 } }, { "parent", ActorId{ 1 } }, { "type", "Actor" }, { "properties", { { "name", "Tail" } } } });
    SetPrefabInMemory(kGroup, grown);
    BaseScene grownScene;
    grownScene.Load(placedSaved);
    grownScene.UpdateAlways();
    Actor* tail = grownScene.FindActor("Tail");
    assert(grownScene.GetActors().size() == 3 && tail && tail->GetParent() == grownScene.FindActor(placed->GetId()) && tail->GetPrefabNode() == 3);
    assert(grownScene.FindActor(placedArm->GetId())->GetTransform().translate.y == 2.0f);

    // プレハブの根を動かしても、置いた根は動かない（根の Transform はいつも差分として持つ）
    grown["actors"][0]["properties"]["transform"]["translate"] = { 9.0f, 9.0f, 9.0f };
    SetPrefabInMemory(kGroup, grown);
    BaseScene movedScene;
    movedScene.Load(placedSaved);
    movedScene.UpdateAlways();
    assert(movedScene.FindActor(placed->GetId())->GetTransform().translate.x == placed->GetTransform().translate.x);

    // Actor 1 つだった頃のプレハブと、node の無い保存も読める
    SetPrefabInMemory(kSingle, { { "type", "Actor" }, { "properties", { { "name", "Single" } } } });
    BaseScene legacy;
    legacy.Load({ { "actors", nlohmann::json::array({ { { "id", ActorId{ 4 } }, { "prefab", kSingle } } }) } });
    legacy.UpdateAlways();
    const Actor* single = legacy.FindActor(ActorId{ 4 });
    assert(single && single->GetName() == "Single" && single->IsPrefabRoot() && single->GetPrefabNode() == 1);

    // 置いた組の子を消して保存すると、読み直しても戻らない（根が removed に覚える）
    SetPrefabInMemory(kGroup, MakePrefabContent(kGroup, tree, nodes));
    BaseScene trimmed;
    Actor*    trimmedRoot = trimmed.Spawn(kGroup);
    trimmed.UpdateAlways();
    trimmed.Destroy(trimmedRoot->GetChildren()[0]);
    trimmed.UpdateAlways();
    const nlohmann::json trimmedSaved = trimmed.Save();
    assert(trimmedSaved["actors"].size() == 1 && trimmedSaved["actors"][0]["removed"] == nlohmann::json::array({ ActorId{ 2 } }));
    BaseScene trimmedLoaded;
    trimmedLoaded.Load(trimmedSaved);
    trimmedLoaded.UpdateAlways();
    assert(trimmedLoaded.GetActors().size() == 1 && trimmedLoaded.Save() == trimmedSaved);

    // 消した子は戻らないまま、プレハブに後から足した Actor は出る
    SetPrefabInMemory(kGroup, grown);
    BaseScene trimmedGrown;
    trimmedGrown.Load(trimmedSaved);
    trimmedGrown.UpdateAlways();
    assert(trimmedGrown.GetActors().size() == 2 && trimmedGrown.FindActor("Tail") && !trimmedGrown.FindActor("Arm"));

    // 入れ子：別のプレハブを置いた物を子に持つ組をプレハブにすると、その子は「置いた物」として書かれる
    const char* kInner = "__SelfTestInner";
    const char* kOuter = "__SelfTestOuter";
    SetPrefabInMemory(kInner, { { "actors", nlohmann::json::array({ { { "id", ActorId{ 1 } }, { "type", "Actor" }, { "properties", { { "name", "Blade" } } } } }) } });
    BaseScene workshop;
    Actor*    holder = workshop.Spawn("Actor");
    Actor*    blade  = workshop.Spawn(kInner);
    workshop.UpdateAlways();
    holder->SetName("Holder");
    assert(blade->SetParent(holder));
    const nlohmann::json outer = MakePrefabContent(kOuter, SaveActorTree(*holder), { { holder->GetId(), 1 }, { blade->GetId(), 2 } });
    assert(outer["actors"].size() == 2 && !outer["actors"][1].contains("type") && outer["actors"][1]["prefab"] == kInner);
    SetPrefabInMemory(kOuter, outer);
    assert(GetNestedPrefabLink(kOuter, 2).prefab == kInner && GetNestedPrefabLink(kOuter, 1).prefab.empty());

    // 内側を書き換えると、外側を置いた物にも出る（変えた値と、足した Actor）
    BaseScene armory;
    Actor*    placedHolder = armory.Spawn(kOuter);
    armory.UpdateAlways();
    assert(placedHolder && placedHolder->GetChildren().size() == 1 && placedHolder->GetChildren()[0]->GetPrefab() == kOuter);
    const nlohmann::json armorySaved = armory.Save();
    SetPrefabInMemory(kInner, { { "actors", nlohmann::json::array({
      { { "id", ActorId{ 1 } }, { "type", "Actor" }, { "properties", { { "name", "Blade" }, { "active", false } } } },
      { { "id", ActorId{ 2 } }, { "parent", ActorId{ 1 } }, { "type", "Actor" }, { "properties", { { "name", "Gem" } } } },
    }) } });
    BaseScene armoryLoaded;
    armoryLoaded.Load(armorySaved);
    armoryLoaded.UpdateAlways();
    Actor* loadedHolder = armoryLoaded.FindActor(placedHolder->GetId());
    Actor* loadedBlade  = armoryLoaded.FindActor("Blade");
    Actor* gem          = armoryLoaded.FindActor("Gem");
    assert(armoryLoaded.GetActors().size() == 3 && loadedBlade && !loadedBlade->IsActive() && gem && gem->GetParent() == loadedBlade);
    assert(gem->GetPrefab() == kOuter && (gem->GetPrefabNode() & kImplicitPrefabNode) != 0);

    // 外側を置いた組から書き直しても（Apply）、入れ子は写しにならない
    const nlohmann::json reapplied = MakePrefabContent(kOuter, SaveActorTree(*loadedHolder),
      { { loadedHolder->GetId(), 1 }, { loadedBlade->GetId(), 2 }, { gem->GetId(), gem->GetPrefabNode() } });
    assert(reapplied["actors"].size() == 3 && reapplied["actors"][0].contains("type"));
    assert(reapplied["actors"][1]["prefab"] == kInner && reapplied["actors"][2]["prefab"] == kInner && reapplied["actors"][2]["root"] == 2);

    // 循環（内側の中に外側）は、その分を落として止まる。わざと出す警告は Console に出さない
    SetPrefabInMemory(kInner, { { "actors", nlohmann::json::array({
      { { "id", ActorId{ 1 } }, { "type", "Actor" }, { "properties", { { "name", "Blade" } } } },
      { { "id", ActorId{ 2 } }, { "prefab", kOuter }, { "node", ActorId{ 1 } } },
    }) } });
    const Logger::Sink console = Logger::SetSink(nullptr);
    assert(GetPrefabNodes(kOuter).size() == 2 && GetPrefabNodes(kInner).size() == 1);
    Logger::SetSink(console);

    for (const char* name : { kGroup, kSingle, kInner, kOuter }) {
      SetPrefabInMemory(name, nullptr);
    }
  }

} // namespace Tako

#endif // _DEBUG

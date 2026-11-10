#include "SceneSelfTest.h"

#ifdef _DEBUG

#include "Actor.h"
#include "BaseScene.h"
#include "Mat4x4Func.h"
#include "MeshComponent.h"
#include "Object3d.h"

#include <json.hpp>

#include <cassert>
#include <cmath>

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

    public: //メンバー変数
      int ticks     = 0;
      int lateTicks = 0;

    private: //メンバー変数
      bool ticksWhilePaused_;
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
  }

} // namespace Tako

#endif // _DEBUG

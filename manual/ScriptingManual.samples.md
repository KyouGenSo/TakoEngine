# TakoEngine C++ スクリプト マニュアル：サンプルの全文

`ScriptingManual.md` のコードの例の、全文です。マニュアルの例がコンパイルできるかを確かめるために置いています（サイトには出しません）。
マニュアルの ```cpp の行は、どれもこの中にあります。マニュアルに新しい行を足すときは、先にここへ足して、Debug・Develop・Release でコンパイルを確かめます。

## Script/Mover.h

```cpp
// Script/Mover.h
#pragma once
#include "Script.h"
#include "Vector3.h"

/// <summary>
/// BeginPlay の位置から offset まで行って戻るのを繰り返す
/// </summary>
TCLASS()
class Mover : public Tako::Script {
    GENERATED_BODY()

public: //メンバー関数
    void BeginPlay() override;

    void Tick(float deltaTime) override;

private: //メンバー変数
    TPROPERTY(Edit, Tooltip = "BeginPlay の位置から一番遠くまでの移動量", Speed = 0.05)
    Tako::Vector3 offset_{ 0.0f, 0.0f, 5.0f };
    TPROPERTY(Edit, Tooltip = "往復 1 回の秒数", ClampMin = 0.01, Speed = 0.05)
    float period_ = 2.0f;

    Tako::Vector3 origin_{};
    float         time_ = 0.0f;
};
```

## Script/Mover.cpp

```cpp
// Script/Mover.cpp
#include "Mover.h"
#include "Actor.h"

#include <cmath>
#include <numbers>

void Mover::BeginPlay() {
    origin_ = GetOwner()->GetTransform().translate;
    time_   = 0.0f;
}

void Mover::Tick(float deltaTime) {
    time_ += deltaTime;
    // 0 → 1 → 0 と動く（1 - cos の半分）
    const float t = (1.0f - std::cos(2.0f * std::numbers::pi_v<float> * time_ / period_)) * 0.5f;
    GetOwner()->GetTransform().translate = origin_ + offset_ * t;
}
```

## Script/PropertySample.h

```cpp
// Script/PropertySample.h
#pragma once
#include "ActorRef.h"
#include "Script.h"
#include "Vector3.h"
#include "Vector4.h"

#include <cstdint>
#include <numbers>
#include <string>
#include <vector>

/// <summary>
/// TPROPERTY の指定子の見本
/// </summary>
TCLASS()
class PropertySample : public Tako::Script {
    GENERATED_BODY()

public: //構造体
    TENUM()
    enum class Mode { Idle, Patrol, Chase };

    TSTRUCT()
    struct Wave {
        GENERATED_STRUCT_BODY()

        TPROPERTY(Edit, ClampMin = 0)
        int32_t count = 3;
        TPROPERTY(Edit, ClampMin = 0, Speed = 0.1)
        float interval = 1.0f;
    };

private: //メンバー変数
    TPROPERTY(Edit)
    bool enabled_ = true;
    TPROPERTY(Edit, ClampMin = 0, ClampMax = 100)
    int32_t hp_ = 100;
    TPROPERTY(Edit, Category = "Move", Tooltip = "1 秒あたりの移動量", ClampMin = 0, Speed = 0.05)
    float speed_ = 3.0f;
    TPROPERTY(Edit, Category = "Move", Angle, Tooltip = "1 秒あたりの回転。欄は度、値はラジアン")
    float turnSpeed_ = std::numbers::pi_v<float>;
    TPROPERTY(Edit, Color)
    Tako::Vector4 tint_ = { 1.0f, 1.0f, 1.0f, 1.0f };
    TPROPERTY(Edit)
    Mode mode_ = Mode::Idle;
    TPROPERTY(Edit)
    Wave wave_;
    TPROPERTY(Edit, Tooltip = "巡回する位置")
    std::vector<Tako::Vector3> waypoints_;
    TPROPERTY(Edit, Asset = "Prefab")
    std::string bulletPrefab_;
    TPROPERTY(Edit, Asset = "Sound")
    std::string hitSound_;
    TPROPERTY(Edit, Required, ActorWith = "ColliderComponent")
    Tako::ActorRef target_;
    TPROPERTY(Visible)
    int32_t killCount_ = 0;
    TPROPERTY(Edit, Transient)
    bool showsDebug_ = false;
    TPROPERTY()
    uint32_t seed_ = 0;

    float timer_ = 0.0f;
};
```

## Script/Follow.h

```cpp
// Script/Follow.h
#pragma once
#include "ActorRef.h"
#include "Script.h"

/// <summary>
/// target の Actor へ、止まる距離まで一定の速さで近づく
/// </summary>
TCLASS()
class Follow : public Tako::Script {
    GENERATED_BODY()

public: //メンバー関数
    void Tick(float deltaTime) override;

private: //メンバー変数
    TPROPERTY(Edit, Tooltip = "追う相手。Hierarchy の行をここへドロップしても入る")
    Tako::ActorRef target_;
    TPROPERTY(Edit, Tooltip = "1 秒あたりの移動量", ClampMin = 0, Speed = 0.05)
    float speed_ = 3.0f;
    TPROPERTY(Edit, Tooltip = "相手との距離がこれ以下なら動かない", ClampMin = 0, Speed = 0.05)
    float stopDistance_ = 1.5f;
};
```

## Script/Follow.cpp

```cpp
// Script/Follow.cpp
#include "Follow.h"
#include "Actor.h"

#include <algorithm>

void Follow::Tick(float deltaTime) {
    const Tako::Actor* target = GetOwner()->Resolve(target_);
    if (!target) {
        return;
    }
    const Tako::Vector3 position = GetOwner()->GetWorldPosition();
    const Tako::Vector3 toTarget = target->GetWorldPosition() - position;
    const float         distance = toTarget.Length();
    if (distance <= stopDistance_) {
        return;
    }
    const float step = (std::min)(speed_ * deltaTime, distance - stopDistance_);
    GetOwner()->SetWorldPosition(position + toTarget * (step / distance));
}
```

## Script/Spawner.h

```cpp
// Script/Spawner.h
#pragma once
#include "Script.h"

#include <string>

/// <summary>
/// 一定の間隔で、プレハブを自分のまわりに出す
/// </summary>
TCLASS()
class Spawner : public Tako::Script {
    GENERATED_BODY()

public: //メンバー関数
    void BeginPlay() override;

private: //非公開関数
    void SpawnOne();

private: //メンバー変数
    TPROPERTY(Edit, Asset = "Prefab", Required)
    std::string prefab_;
    TPROPERTY(Edit, Tooltip = "出す位置の、自分からの距離", ClampMin = 0, Speed = 0.1)
    float radius_ = 5.0f;
    TPROPERTY(Edit, Tooltip = "出した Actor が持っているタグ。出ている数を数えるのに使う")
    std::string tag_ = "Enemy";

    Tako::TimerId timer_ = 0;
};
```

## Script/Spawner.cpp

```cpp
// Script/Spawner.cpp
#include "Spawner.h"
#include "Actor.h"
#include "BaseScene.h"
#include "RandomEngine.h"

#include "Source/Settings/GameTuning.h"

void Spawner::BeginPlay() {
    const GameTuning& tuning = Tako::GameSettings::Get<GameTuning>();
    // 3 つ目の引数が true なら、止めるまで spawnInterval 秒ごとに呼ばれる
    timer_ = SetTimer(tuning.spawnInterval, [this] { SpawnOne(); }, true);
}

void Spawner::SpawnOne() {
    Tako::BaseScene*  scene  = GetOwner()->GetScene();
    const GameTuning& tuning = Tako::GameSettings::Get<GameTuning>();
    if (static_cast<int32_t>(scene->FindActorsWithTag(tag_).size()) >= tuning.maxEnemies) {
        return;
    }
    Tako::Actor* actor = scene->Spawn(prefab_);
    if (!actor) {
        // プレハブが無い。出し続けても同じなので、タイマーを止める
        ClearTimer(timer_);
        return;
    }
    const Tako::Vector3 offset = Tako::RandomEngine::GetInstance()->GetRandomDirectionXZ() * radius_;
    actor->GetTransform().translate = GetOwner()->GetWorldPosition() + offset;
}
```

## Script/Coin.h

```cpp
// Script/Coin.h
#pragma once
#include "Script.h"

#include <cstdint>

/// <summary>
/// Player のタグを持つ Actor に当たったら、点を知らせて消える
/// </summary>
TCLASS(Requires = "ColliderComponent")
class Coin : public Tako::Script {
    GENERATED_BODY()

public: //メンバー関数
    void OnCollisionEnter(const Tako::Collision& collision) override;

private: //メンバー変数
    TPROPERTY(Edit, ClampMin = 1)
    int32_t point_ = 1;
};
```

## Script/Coin.cpp

```cpp
// Script/Coin.cpp
#include "Coin.h"
#include "Actor.h"
#include "BaseScene.h"

void Coin::OnCollisionEnter(const Tako::Collision& collision) {
    // collision.actor は、相手の Actor
    if (!collision.actor->HasTag("Player")) {
        return;
    }
    GetOwner()->GetScene()->SendEvent("CoinCollected", point_);
    GetOwner()->Destroy();
}
```

## Script/ScoreCounter.h

```cpp
// Script/ScoreCounter.h
#pragma once
#include "Script.h"

#include <cstdint>
#include <string>

/// <summary>
/// コインの点を数え、目標に届いたら次のシーンへ進む
/// </summary>
TCLASS(Order = -100)
class ScoreCounter : public Tako::Script {
    GENERATED_BODY()

public: //メンバー関数
    void Awake() override;

#ifdef TAKO_EDITOR
    bool DrawCustomImGui() override;
#endif

    //=====================================
    //Getter
    //=====================================
    int32_t GetScore() const { return score_; }

private: //メンバー変数
    TPROPERTY(Edit, Asset = "Scene", Tooltip = "目標の点に届いたら移るシーン")
    std::string nextScene_;

    int32_t score_ = 0;
};
```

## Script/ScoreCounter.cpp

```cpp
// Script/ScoreCounter.cpp
#include "ScoreCounter.h"
#include "Actor.h"
#include "Logger.h"
#include "SceneManager.h"

#include "Source/Settings/GameTuning.h"

#include <json.hpp>

#include <any>

#ifdef TAKO_EDITOR
#include "imgui.h"
#endif

void ScoreCounter::Awake() {
    // 他のスクリプトが BeginPlay の中で送っても受け取れるよう、Awake で待ち始める
    Subscribe("CoinCollected", [this](const std::any& payload) {
        score_ += std::any_cast<int32_t>(payload);
        Tako::Logger::Log("Score: %d\n", score_);
        if (score_ < Tako::GameSettings::Get<GameTuning>().clearScore) {
            return;
        }
        // シーンを替えても残したい値は、SessionData に入れる
        Tako::SceneManager::GetInstance()->GetSessionData()["score"] = score_;
        if (!nextScene_.empty()) {
            Tako::SceneManager::GetInstance()->ChangeScene(nextScene_);
        }
    });
}

#ifdef TAKO_EDITOR
bool ScoreCounter::DrawCustomImGui() {
    ImGui::Text("Score: %d", score_);
    if (ImGui::Button("Add 1")) {
        ++score_;
    }
    return false;
}
#endif
```

## Script/PauseToggle.h

```cpp
// Script/PauseToggle.h
#pragma once
#include "Script.h"

/// <summary>
/// Esc か Start ボタンで、ゲームを止める・再開する。止めている間だけ、同じ Actor の UI Layout を出す
/// </summary>
TCLASS(Requires = "UILayoutComponent")
class PauseToggle : public Tako::Script {
    GENERATED_BODY()

public: //メンバー関数
    void BeginPlay() override;

    void Tick(float deltaTime) override;

    // ポーズ中も Tick を呼んでもらう（そうしないと、再開のキーを読めない）
    bool TickWhilePaused() const override { return true; }
};
```

## Script/PauseToggle.cpp

```cpp
// Script/PauseToggle.cpp
#include "PauseToggle.h"
#include "Actor.h"
#include "BaseScene.h"
#include "Input.h"
#include "UILayoutComponent.h"

void PauseToggle::BeginPlay() {
    if (Tako::UILayoutComponent* ui = GetOwner()->GetComponent<Tako::UILayoutComponent>()) {
        ui->SetVisible(false);
    }
}

void PauseToggle::Tick([[maybe_unused]] float deltaTime) {
    const Tako::Input* input = Tako::Input::GetInstance();
    if (!input->TriggerKey(DIK_ESCAPE) && !input->TriggerButton(Tako::GamepadButton::Start)) {
        return;
    }
    Tako::BaseScene* scene  = GetOwner()->GetScene();
    const bool       paused = !scene->IsPaused();
    scene->SetPaused(paused);
    if (Tako::UILayoutComponent* ui = GetOwner()->GetComponent<Tako::UILayoutComponent>()) {
        ui->SetVisible(paused);
    }
}
```

## Script/PlayerMove.h

```cpp
// Script/PlayerMove.h
#pragma once
#include "Script.h"

/// <summary>
/// WASD か左スティックで XZ 平面を動き、スペースか A ボタンで跳ぶ
/// </summary>
TCLASS(Order = -50)
class PlayerMove : public Tako::Script {
    GENERATED_BODY()

public: //メンバー関数
    void BeginPlay() override;

    void Tick(float deltaTime) override;

private: //メンバー変数
    TPROPERTY(Edit, ClampMin = 0, Speed = 0.05)
    float speed_ = 6.0f;
    TPROPERTY(Edit, ClampMin = 0, Speed = 0.05)
    float jumpSpeed_ = 8.0f;
    TPROPERTY(Edit, ClampMin = 0, Speed = 0.05)
    float gravity_ = 20.0f;

    float groundY_   = 0.0f;
    float velocityY_ = 0.0f;
};
```

## Script/PlayerMove.cpp

```cpp
// Script/PlayerMove.cpp
#include "PlayerMove.h"
#include "Actor.h"
#include "Input.h"

void PlayerMove::BeginPlay() {
    groundY_   = GetOwner()->GetTransform().translate.y;
    velocityY_ = 0.0f;
}

void PlayerMove::Tick(float deltaTime) {
    const Tako::Input* input = Tako::Input::GetInstance();

    Tako::Vector2 move = input->GetLeftStick();
    if (input->PushKey(DIK_D)) { move.x += 1.0f; }
    if (input->PushKey(DIK_A)) { move.x -= 1.0f; }
    if (input->PushKey(DIK_W)) { move.y += 1.0f; }
    if (input->PushKey(DIK_S)) { move.y -= 1.0f; }

    Tako::Vector3& position = GetOwner()->GetTransform().translate;
    position.x += move.x * speed_ * deltaTime;
    position.z += move.y * speed_ * deltaTime;

    const bool isOnGround = position.y <= groundY_;
    if (isOnGround && (input->TriggerKey(DIK_SPACE) || input->TriggerButton(Tako::GamepadButton::A))) {
        velocityY_ = jumpSpeed_;
    }
    velocityY_ -= gravity_ * deltaTime;
    position.y += velocityY_ * deltaTime;
    if (position.y < groundY_) {
        position.y = groundY_;
        velocityY_ = 0.0f;
    }
}
```

## Script/ActionMove.h

```cpp
// Script/ActionMove.h
#pragma once
#include "Script.h"

/// <summary>
/// Engine Settings の Input で作った軸 "Move" とアクション "Dash" で動く
/// </summary>
TCLASS()
class ActionMove : public Tako::Script {
    GENERATED_BODY()

public: //メンバー関数
    void Tick(float deltaTime) override;

private: //メンバー変数
    TPROPERTY(Edit, ClampMin = 0, Speed = 0.05)
    float speed_ = 6.0f;
    TPROPERTY(Edit, ClampMin = 0, Speed = 0.05)
    float dashSpeed_ = 12.0f;
};
```

## Script/ActionMove.cpp

```cpp
// Script/ActionMove.cpp
#include "ActionMove.h"
#include "Actor.h"
#include "Input.h"

void ActionMove::Tick(float deltaTime) {
    const Tako::Input*  input = Tako::Input::GetInstance();
    const Tako::Vector2 move  = input->GetAxis("Move");
    const float         speed = input->PushAction("Dash") ? dashSpeed_ : speed_;

    Tako::Vector3& position = GetOwner()->GetTransform().translate;
    position.x += move.x * speed * deltaTime;
    position.z += move.y * speed * deltaTime;
}
```

## Script/DestroyOnHit.h

```cpp
// Script/DestroyOnHit.h
#pragma once
#include "Script.h"

#include <string>

/// <summary>
/// 同じ Actor の ColliderComponent が他の Actor に当たったら、この Actor を消す
/// </summary>
TCLASS()
class DestroyOnHit : public Tako::Script {
    GENERATED_BODY()

public: //メンバー関数
    void OnCollisionEnter(const Tako::Collision& collision) override;

private: //メンバー変数
    TPROPERTY(Edit, Tooltip = "空ならどの Actor でも消える。名前を書くと、その名前の Actor に当たったときだけ消える")
    std::string otherName_;
};
```

## Script/DestroyOnHit.cpp

```cpp
// Script/DestroyOnHit.cpp
#include "DestroyOnHit.h"
#include "Actor.h"

void DestroyOnHit::OnCollisionEnter(const Tako::Collision& collision) {
    if (!otherName_.empty() && collision.actor->GetName() != otherName_) {
        return;
    }
    GetOwner()->Destroy();
}
```

## Source/Settings/GameTuning.h

```cpp
// Source/Settings/GameTuning.h
#pragma once
#include "GameSettings.h"

#include <cstdint>

/// <summary>
/// ゲーム全体の調整値
/// </summary>
TCLASS()
class GameTuning : public Tako::GameSettings {
    GENERATED_BODY()

public: //メンバー変数
    TPROPERTY(Edit, Category = "Spawn", Tooltip = "敵を出す間隔（秒）", ClampMin = 0.1, Speed = 0.05)
    float spawnInterval = 2.0f;
    TPROPERTY(Edit, Category = "Spawn", Tooltip = "同時に出しておく数の上限", ClampMin = 0)
    int32_t maxEnemies = 10;
    TPROPERTY(Edit, Category = "Score", Tooltip = "次のシーンへ進む点", ClampMin = 1)
    int32_t clearScore = 5;
};
```

## Script/ActorSamples.h

```cpp
// Script/ActorSamples.h
#pragma once
#include "ActorRef.h"
#include "Script.h"

/// <summary>
/// Actor とシーンを、スクリプトから操作する例
/// </summary>
TCLASS()
class ActorSamples : public Tako::Script {
    GENERATED_BODY()

public: //メンバー関数
    void BeginPlay() override;

private: //非公開関数
    void UseTransform();
    void UseHierarchy();
    void UseLookup();
    void UseComponents();

private: //メンバー変数
    TPROPERTY(Edit, Tooltip = "UseLookup で引く相手")
    Tako::ActorRef target_;
};
```

## Script/ActorSamples.cpp

```cpp
// Script/ActorSamples.cpp
#include "ActorSamples.h"
#include "Actor.h"
#include "BaseScene.h"
#include "ColliderComponent.h"
#include "Logger.h"
#include "MeshComponent.h"

#include "Script/Mover.h"

#include <numbers>
#include <vector>

void ActorSamples::BeginPlay() {
    UseTransform();
    UseHierarchy();
    UseLookup();
    UseComponents();
}

void ActorSamples::UseTransform() {
    Tako::Actor* owner = GetOwner();
    // Transform は親からの相対（親が無ければワールド）。rotate はラジアン
    Tako::Transform& transform = owner->GetTransform();
    transform.translate.y += 1.0f;
    transform.rotate.y = std::numbers::pi_v<float> * 0.5f;
    transform.scale    = { 2.0f, 2.0f, 2.0f };
    // 親があっても、ワールドの位置で読み書きできる
    const Tako::Vector3 world = owner->GetWorldPosition();
    owner->SetWorldPosition(world + Tako::Vector3{ 0.0f, 0.0f, 1.0f });
}

void ActorSamples::UseHierarchy() {
    Tako::Actor* owner = GetOwner();
    // 直接の子を、名前で探す
    if (Tako::Actor* muzzle = owner->FindChild("Muzzle")) {
        // オフにすると、その Actor と子は動かず、描かれず、当たらない
        muzzle->SetActive(false);
    }
    for (Tako::Actor* child : owner->GetChildren()) {
        Tako::Logger::Log("child: %s\n", child->GetName().c_str());
    }
    // 親を外す。keepWorld が true なら、見た目の位置・向き・大きさは変わらない
    if (owner->GetParent()) {
        owner->SetParent(nullptr, true);
    }
}

void ActorSamples::UseLookup() {
    Tako::Actor*     owner = GetOwner();
    Tako::BaseScene* scene = owner->GetScene();
    // 名前で探す（最初に見つかった 1 つ。無ければ nullptr）
    if (Tako::Actor* player = scene->FindActor("Player")) {
        Tako::Logger::Log("player is at y = %f\n", player->GetWorldPosition().y);
    }
    // タグで探す（Inspector の Tags に書いた印）
    const std::vector<Tako::Actor*> enemies = scene->FindActorsWithTag("Enemy");
    Tako::Logger::Log("enemies: %d\n", static_cast<int>(enemies.size()));
    // ActorRef で指した相手を取る
    if (Tako::Actor* target = owner->Resolve(target_)) {
        if (target->HasTag("Boss")) {
            Tako::Logger::Log("the target is a boss\n");
        }
    }
}

void ActorSamples::UseComponents() {
    Tako::Actor* owner = GetOwner();
    // 同じ Actor のコンポーネントを、型で取る（無ければ nullptr）
    if (Tako::MeshComponent* mesh = owner->GetComponent<Tako::MeshComponent>()) {
        mesh->SetVisible(true);
    }
    // 同じ Actor の他のスクリプトを、型で取る
    if (owner->GetScript<Mover>()) {
        Tako::Logger::Log("%s has a Mover\n", owner->GetName().c_str());
    }
    // 同じ型を 2 つ付けたときは、コンポーネントに付けた名前で取る
    Tako::Component* explode = owner->FindComponent("Explode");
    // 実行中に足す・外す。外れるのは次のフレームの頭
    Tako::ColliderComponent* collider = owner->AddComponent<Tako::ColliderComponent>();
    // 重なりを知らせるだけの Collider にする（呼ばなければ、押し合う実体になる）
    collider->SetTrigger(true);
    collider->SetRadius(0.5f);
    if (explode) {
        owner->RemoveComponent(explode);
    }
}
```

## Script/ColorSample.h

```cpp
// Script/ColorSample.h
#pragma once
#include "ColorFunc.h"
#include "Gradient.h"
#include "Script.h"

/// <summary>
/// 色の作り方・変換の例。HP の割合で、Mesh の色を変える
/// </summary>
TCLASS()
class ColorSample : public Tako::Script {
    GENERATED_BODY()

public: //メンバー関数
    void BeginPlay() override;

    void Tick(float deltaTime) override;

private: //非公開関数
    void UseColors();

private: //メンバー変数
    TPROPERTY(Edit, Tooltip = "HP が満タンのときの色")
    Tako::Color fullColor_ = Tako::Color::kGreen;
    TPROPERTY(Edit, Tooltip = "HP が 0 のときの色")
    Tako::Color emptyColor_ = Tako::Color::kRed;
    TPROPERTY(Edit, Tooltip = "HP の割合（0〜1）ごとの色。段階が無ければ、上の 2 色の間を取る")
    Tako::Gradient hpGradient_;
    TPROPERTY(Edit, ClampMin = 1, Speed = 1)
    float maxHp_ = 100.0f;

    float hp_ = 0.0f;
};
```

## Script/ColorSample.cpp

```cpp
// Script/ColorSample.cpp
#include "ColorSample.h"
#include "Actor.h"
#include "Logger.h"
#include "MeshComponent.h"
#include "Object3d.h"
#include "RandomEngine.h"

void ColorSample::BeginPlay() {
    hp_ = maxHp_;
    UseColors();
}

void ColorSample::Tick(float deltaTime) {
    // 5 秒で 0 まで減り、満タンに戻るのを繰り返す（色の変わり方を見る用）
    hp_ -= maxHp_ / 5.0f * deltaTime;
    if (hp_ <= 0.0f) {
        hp_ = maxHp_;
    }
    Tako::MeshComponent* mesh   = GetOwner()->GetComponent<Tako::MeshComponent>();
    Tako::Object3d*      object = mesh ? mesh->GetObject3d() : nullptr;
    if (!object) {
        return;
    }
    // HP の割合で色を決める。段階が無ければ、0 と満タンの色の間を色相環に沿って取る
    const float ratio = hp_ / maxHp_;
    const Tako::Color color = hpGradient_.keys.empty() ? Tako::Color::LerpHSV(emptyColor_, fullColor_, ratio) : hpGradient_.Evaluate(ratio);
    object->SetMaterialColor(color);
}

void ColorSample::UseColors() {
    // 作る：名前・数（a を省くと 1）・16 進数・0〜255・HSV・色温度
    const Tako::Color red    = Tako::Color::kRed;
    const Tako::Color orange = Tako::Color(1.0f, 0.5f, 0.0f);
    const Tako::Color sky    = Tako::Color::FromHex(0x3FA7FF);
    const Tako::Color paint  = Tako::Color32(255, 136, 0);
    const Tako::Color violet = Tako::Color::FromHSV(0.8f, 0.6f, 1.0f);
    const Tako::Color candle = Tako::Color::FromKelvin(1900.0f);
    // 変える：アルファだけ・2 色の間（成分ごと・色相環に沿って）・色で染める
    const Tako::Color ghost  = sky.WithAlpha(0.5f);
    const Tako::Color middle = Tako::Color::Lerp(red, orange, 0.5f);
    const Tako::Color yellow = Tako::Color::LerpHSV(Tako::Color::kGreen, red, 0.5f);
    const Tako::Color tinted = paint * Tako::Color::kGray;
    // 変換する：HSV で色相を半周回す（反対の色）・0〜255 と 16 進数
    Tako::ColorHSV hsv = violet.ToHSV();
    hsv.h += 0.5f;
    const Tako::Color   opposite = hsv.ToColor();
    const Tako::Color32 bytes    = candle.ToColor32();
    Tako::Logger::Log("candle: #%06X (%d, %d, %d)\n", bytes.ToHex(), bytes.r, bytes.g, bytes.b);
    // ランダム：色相だけ・2 色の間
    Tako::RandomEngine* random  = Tako::RandomEngine::GetInstance();
    const Tako::Color   rainbow = random->GetHueColor(0.8f, 1.0f);
    const Tako::Color   between = random->GetColor(fullColor_, emptyColor_);
    Tako::Logger::Log("yellow: %.2f %.2f %.2f, opposite: %.2f %.2f %.2f\n", yellow.r, yellow.g, yellow.b, opposite.r, opposite.g, opposite.b);
    Tako::Logger::Log("ghost a: %.2f, middle g: %.2f, tinted r: %.2f, rainbow s: %.2f, between g: %.2f\n", ghost.a, middle.g, tinted.r, rainbow.ToHSV().s, between.g);
}
```

## Script/ComponentSamples.h

```cpp
// Script/ComponentSamples.h
#pragma once
#include "Script.h"

/// <summary>
/// 同じ Actor のコンポーネントを、スクリプトから使う例
/// </summary>
TCLASS()
class ComponentSamples : public Tako::Script {
    GENERATED_BODY()

public: //メンバー関数
    void BeginPlay() override;

private: //非公開関数
    void UseMesh();
    void UseCollider();
    void UseAudio();
    void UseParticle();
    void UseUI();
    void UseBehaviorTree();
    void UseCamera();
    void UseScript();
    void UseRaycast();
    void UseShapeQueries();
    void UseIgnoreCollision();
};
```

## Script/ComponentSamples.cpp

```cpp
// Script/ComponentSamples.cpp
#include "ComponentSamples.h"
#include "Actor.h"
#include "AudioSourceComponent.h"
#include "BaseScene.h"
#include "BehaviorTree.h"
#include "BehaviorTreeComponent.h"
#include "CameraComponent.h"
#include "ColliderComponent.h"
#include "EmitterManager.h"
#include "GPUParticleEmitter.h"
#include "Logger.h"
#include "MeshComponent.h"
#include "Object3d.h"
#include "ParticleSystemComponent.h"
#include "PhysicsWorld.h"
#include "SceneManager.h"
#include "ScriptComponent.h"
#include "UILayout.h"
#include "UILayoutComponent.h"

#include <json.hpp>

#include <cstdint>
#include <memory>
#include <numbers>
#include <string>

void ComponentSamples::BeginPlay() {
    UseMesh();
    UseCollider();
    UseAudio();
    UseParticle();
    UseUI();
    UseBehaviorTree();
    UseCamera();
    UseScript();
    UseRaycast();
    UseShapeQueries();
    UseIgnoreCollision();
}

void ComponentSamples::UseMesh() {
    Tako::MeshComponent* mesh = GetOwner()->GetComponent<Tako::MeshComponent>();
    if (!mesh) {
        return;
    }
    // 見た目だけを Actor からずらす（Actor の位置と当たり判定は動かない）
    mesh->SetOffset({ { 1.0f, 1.0f, 1.0f }, {}, { 0.0f, 0.5f, 0.0f } });
    mesh->SetVisible(true);
    // 描画物（Object3d）を取って、色を変える
    if (Tako::Object3d* object = mesh->GetObject3d()) {
        object->SetMaterialColor(Tako::Color(1.0f, 0.3f, 0.3f));
    }
}

void ComponentSamples::UseCollider() {
    Tako::ColliderComponent* collider = GetOwner()->GetComponent<Tako::ColliderComponent>();
    if (!collider) {
        return;
    }
    // 形を実行中に変える（Sphere なら半径、Box なら全長）
    collider->SetRadius(2.0f);
    collider->SetOffset({ 0.0f, 1.0f, 0.0f });
    // 当たり判定を一時的に止める（攻撃の間だけ当てる、などに使う）
    collider->SetActive(false);
}

void ComponentSamples::UseRaycast() {
    // 足元から下へ、10 の長さまで調べる
    const Tako::Vector3 origin = GetOwner()->GetWorldPosition();
    if (const auto hit = Tako::PhysicsWorld::GetInstance()->Raycast(origin, { 0.0f, -1.0f, 0.0f }, 10.0f)) {
        Tako::Logger::Log("hit %s at distance %.2f\n", hit->actor->GetName().c_str(), hit->distance);
    }
}

void ComponentSamples::UseShapeQueries() {
    Tako::PhysicsWorld* physics = Tako::PhysicsWorld::GetInstance();
    const Tako::Vector3 origin  = GetOwner()->GetWorldPosition();
    // 半径 0.3 の球を、前へ 5 の長さまで飛ばす（太さのある視線、カメラが壁にめり込まない位置、など）
    if (const auto hit = physics->SphereCast(origin, 0.3f, { 0.0f, 0.0f, 1.0f }, 5.0f)) {
        Tako::Logger::Log("sphere hit %s at distance %.2f\n", hit->actor->GetName().c_str(), hit->distance);
    }
    // 半径 3 の球に重なっている Actor を、全部調べる（爆発の範囲、近くの敵、など）
    for (const Tako::Actor* actor : physics->OverlapSphere(origin, 3.0f)) {
        Tako::Logger::Log("near %s\n", actor->GetName().c_str());
    }
}

void ComponentSamples::UseIgnoreCollision() {
    // 親の Actor とは、当たらないようにする（弾が、撃った本人に当たらないようにする、など）
    if (const Tako::Actor* parent = GetOwner()->GetParent()) {
        Tako::PhysicsWorld::GetInstance()->IgnoreCollision(GetOwner(), parent);
    }
}

void ComponentSamples::UseAudio() {
    if (Tako::AudioSourceComponent* audio = GetOwner()->GetComponent<Tako::AudioSourceComponent>()) {
        // 最初から鳴らす（鳴っていれば、止めてから）
        audio->Play();
    }
}

void ComponentSamples::UseParticle() {
    Tako::ParticleSystemComponent* particle = GetOwner()->GetComponent<Tako::ParticleSystemComponent>();
    if (!particle) {
        return;
    }
    // 出す・止める
    particle->SetActive(true);
    // 今の位置にエミッターの写しを作り、0.5 秒だけ出す（爆発など、1 回きりの演出）
    particle->EmitTemporary(0.5f);
    // エミッターそのものを取って、数や頻度を変える
    if (const std::shared_ptr<Tako::GPUParticleEmitter> emitter = particle->GetEmitter()) {
        emitter->SetParticleCount(64);
        emitter->SetFrequency(0.1f);
    }
    // Emitter Name を付けたエミッターは、シーンの EmitterManager から名前で操作できる
    Tako::EmitterManager& emitters = GetOwner()->GetScene()->GetEmitterManager();
    if (emitters.HasEmitter("Explosion")) {
        emitters.SetEmitterActive("Explosion", true);
    }
}

void ComponentSamples::UseUI() {
    Tako::UILayoutComponent* ui = GetOwner()->GetComponent<Tako::UILayoutComponent>();
    if (!ui) {
        return;
    }
    // 要素は、UI Editor で付けた名前で指す。無い名前を渡しても、何も起きない
    Tako::UILayout& layout = ui->GetLayout();
    const int32_t score = Tako::SceneManager::GetInstance()->GetSessionData().value("score", 0);
    layout.SetText("ScoreText", "Score: " + std::to_string(score));
    layout.SetVisible("PausePanel", false);
    layout.SetColor("StartButton", { 1.0f, 0.8f, 0.2f, 1.0f });
    // Animation Editor で作ったクリップを、要素に再生する
    layout.GetAnimator().Play("PopScale", "StartButton");
}

void ComponentSamples::UseBehaviorTree() {
    Tako::BehaviorTreeComponent* behavior = GetOwner()->GetComponent<Tako::BehaviorTreeComponent>();
    if (!behavior) {
        return;
    }
    // ツリーのノードと値をやり取りする入れ物（Blackboard）に、値を入れる
    Tako::BTBlackboard* blackboard = behavior->GetTree().GetBlackboard();
    blackboard->SetFloat("attackRange", 3.0f);
    blackboard->SetVector3("home", GetOwner()->GetWorldPosition());
}

void ComponentSamples::UseCamera() {
    if (Tako::CameraComponent* camera = GetOwner()->GetComponent<Tako::CameraComponent>()) {
        // 縦の視野角（ラジアン）
        camera->SetFovY(std::numbers::pi_v<float> / 3.0f);
    }
}

void ComponentSamples::UseScript() {
    // Script コンポーネントから、中のスクリプトの型名を取る
    if (Tako::ScriptComponent* component = GetOwner()->GetComponent<Tako::ScriptComponent>()) {
        Tako::Logger::Log("script: %s\n", component->GetScriptName().c_str());
    }
}
```

## Script/GlowSample.h

```cpp
// Script/GlowSample.h
#pragma once
#include "ColorFunc.h"
#include "Script.h"

/// <summary>
/// 同じ Actor の Mesh を、1 を超える色（HDR）で光らせる
/// </summary>
TCLASS()
class GlowSample : public Tako::Script {
    GENERATED_BODY()

public: //メンバー関数
    void Tick(float deltaTime) override;

private: //メンバー変数
    TPROPERTY(Edit, HDR, Tooltip = "1 を超えると光る（シーンでは 1 を超えたまま計算され、ブルームの元になる）")
    Tako::Color glowColor_ = Tako::Color(4.0f, 2.0f, 0.5f);
};
```

## Script/GlowSample.cpp

```cpp
// Script/GlowSample.cpp
#include "GlowSample.h"
#include "Actor.h"
#include "MeshComponent.h"
#include "Object3d.h"

void GlowSample::Tick(float) {
    Tako::MeshComponent* mesh   = GetOwner()->GetComponent<Tako::MeshComponent>();
    Tako::Object3d*      object = mesh ? mesh->GetObject3d() : nullptr;
    if (!object) {
        return;
    }
    // 毎フレーム渡す（Play 中に Inspector で色を変えても、すぐに効く）
    object->SetMaterialColor(glowColor_);
}
```

## Script/ToneMapSample.h

```cpp
// Script/ToneMapSample.h
#pragma once
#include "ColorFunc.h"
#include "RenderEvent.h"
#include "Script.h"

#include <cstdint>

/// <summary>
/// トーンマップ（T）・露出（↑↓）・ブルーム（B）を、キーで切り替えて見比べる
/// </summary>
TCLASS()
class ToneMapSample : public Tako::Script {
    GENERATED_BODY()

public: //メンバー関数
    void BeginPlay() override;

    void Tick(float deltaTime) override;

    void EndPlay() override;

    //=====================================
    //Getter
    //=====================================
    bool IsBloomOn() const { return bloomId_ != 0; }

private: //メンバー変数
    TPROPERTY(Edit, Tooltip = "B で足すブルームの色")
    Tako::Color bloomTint_ = Tako::Color::kWhite;
    TPROPERTY(Edit, Tooltip = "これより明るい所が光る（リニアの明るさ。1 なら、ほぼ 1 を超える色だけ）", ClampMin = 0, Speed = 0.05)
    float bloomThreshold_ = 1.0f;

    Tako::ToneMapSettings saved_;            // Play を始めたときの値
    bool                  changed_ = false;  // トーンマップか露出を、このスクリプトが変えたか
    uint32_t              bloomId_ = 0;      // 足したブルームの番号（0 なら足していない）
};
```

## Script/ToneMapSample.cpp

```cpp
// Script/ToneMapSample.cpp
#include "ToneMapSample.h"
#include "Input.h"
#include "PostEffectManager.h"
#include "RenderPipeline.h"

void ToneMapSample::BeginPlay() {
    saved_ = Tako::RenderPipeline::GetInstance()->GetToneMap();
}

void ToneMapSample::Tick(float) {
    const Tako::Input*           input    = Tako::Input::GetInstance();
    Tako::RenderPipeline*        pipeline = Tako::RenderPipeline::GetInstance();
    const Tako::ToneMapSettings& toneMap  = pipeline->GetToneMap();
    // T：トーンマップを None と ACES で切り替える
    if (input->TriggerKey(DIK_T)) {
        pipeline->SetToneMapping(toneMap.mode == Tako::ToneMapping::None ? Tako::ToneMapping::ACES : Tako::ToneMapping::None);
        changed_ = true;
    }
    // ↑・↓：露出を 0.5 EV ずつ変える（1 EV 上げると 2 倍の明るさ）
    if (input->TriggerKey(DIK_UP)) {
        pipeline->SetExposure(toneMap.exposure + 0.5f);
        changed_ = true;
    }
    if (input->TriggerKey(DIK_DOWN)) {
        pipeline->SetExposure(toneMap.exposure - 0.5f);
        changed_ = true;
    }
    // B：ブルームを足す・外す。tint も画面の値（sRGB）で書く（エンジンがリニアに直す）
    if (input->TriggerKey(DIK_B)) {
        Tako::PostEffectManager* effects = Tako::PostEffectManager::GetInstance();
        if (bloomId_ == 0) {
            bloomId_ = effects->AddEffect("Bloom", Tako::BloomParam{ .intensity = 1.0f, .threshold = bloomThreshold_, .scatter = 0.7f, .tint = { bloomTint_.r, bloomTint_.g, bloomTint_.b } });
        } else {
            effects->RemoveEffect(bloomId_);
            bloomId_ = 0;
        }
    }
}

void ToneMapSample::EndPlay() {
    // 足したブルームを外し、このスクリプトが変えた値だけを Play の前に戻す（Engine Settings の値を書き換えたままにしない）
    if (bloomId_ != 0) {
        Tako::PostEffectManager::GetInstance()->RemoveEffect(bloomId_);
        bloomId_ = 0;
    }
    if (changed_) {
        Tako::RenderPipeline::GetInstance()->SetToneMapping(saved_.mode);
        Tako::RenderPipeline::GetInstance()->SetExposure(saved_.exposure);
    }
}
```

## Script/Rocket.h

```cpp
// Script/Rocket.h
#pragma once
#include "Script.h"

namespace Tako {
    class RigidbodyComponent;
}

/// <summary>
/// Rigidbody を、上向きの力で押し続ける。何かに当たると、当たった面から離れる向きへ弾かれる
/// </summary>
TCLASS(Requires = "RigidbodyComponent")
class Rocket : public Tako::Script {
    GENERATED_BODY()

public: //メンバー関数
    void BeginPlay() override;

    void FixedTick(float fixedDeltaTime) override;

    void OnCollisionEnter(const Tako::Collision& collision) override;

private: //メンバー変数
    Tako::RigidbodyComponent* body_ = nullptr;
};
```

## Script/Rocket.cpp

```cpp
// Script/Rocket.cpp
#include "Rocket.h"
#include "Actor.h"
#include "RigidbodyComponent.h"

void Rocket::BeginPlay() {
    body_ = GetOwner()->GetComponent<Tako::RigidbodyComponent>();
}

// 押し続ける力は、FixedTick で毎回掛ける（フレームレートが変わっても、同じ強さになる）
void Rocket::FixedTick(float) {
    body_->AddForce({ 0.0f, 30.0f, 0.0f });
}

// 1 回だけの衝撃は、AddImpulse で掛ける（ジャンプ、弾が当たった、など）。ここでは、当たった面から離れる向きへ弾く
void Rocket::OnCollisionEnter(const Tako::Collision& collision) {
    body_->AddImpulse(collision.normal * 5.0f);
}
```

## Script/Hero.h

```cpp
// Script/Hero.h
#pragma once
#include "Script.h"

namespace Tako {
    class CharacterControllerComponent;
}

/// <summary>
/// Character Controller の付いた Actor を、W / A / S / D で歩かせて、Space で跳ばせる
/// </summary>
TCLASS(Requires = "CharacterControllerComponent")
class Hero : public Tako::Script {
    GENERATED_BODY()

public: //メンバー関数
    void BeginPlay() override;

    void Tick(float deltaTime) override;

    void FixedTick(float fixedDeltaTime) override;

    void OnControllerColliderHit(const Tako::ControllerColliderHit& hit) override;

private: //メンバー変数
    TPROPERTY(Edit, Tooltip = "歩く速さ（1 秒あたり）", ClampMin = 0, Speed = 0.1)
    float speed_ = 5.0f;
    TPROPERTY(Edit, Tooltip = "跳ぶときの、上向きの速さ", ClampMin = 0, Speed = 0.1)
    float jumpSpeed_ = 6.0f;

    Tako::CharacterControllerComponent* controller_ = nullptr;
    bool                                wantsJump_  = false;
};
```

## Script/Hero.cpp

```cpp
// Script/Hero.cpp
#include "Hero.h"
#include "Actor.h"
#include "CharacterControllerComponent.h"
#include "Input.h"
#include "Logger.h"

void Hero::BeginPlay() {
    controller_ = GetOwner()->GetComponent<Tako::CharacterControllerComponent>();
}

// 押した瞬間の入力は Tick で読んで覚えておく（FixedTick は、1 フレームに 0 回のことも 2 回以上のこともある）
void Hero::Tick(float) {
    wantsJump_ |= Tako::Input::GetInstance()->TriggerKey(DIK_SPACE);
}

void Hero::FixedTick(float) {
    const Tako::Input* input = Tako::Input::GetInstance();
    // y（落ちる速さ）は残して、歩く向きの x と z だけを入れる
    Tako::Vector3 velocity = controller_->GetVelocity();
    velocity.x = (input->PushKey(DIK_D) ? speed_ : 0.0f) - (input->PushKey(DIK_A) ? speed_ : 0.0f);
    velocity.z = (input->PushKey(DIK_W) ? speed_ : 0.0f) - (input->PushKey(DIK_S) ? speed_ : 0.0f);
    // 立っているときだけ、上向きの速さを入れて跳ぶ
    if (wantsJump_ && controller_->IsGrounded()) {
        velocity.y = jumpSpeed_;
    }
    wantsJump_ = false;
    controller_->SetVelocity(velocity);
}

// 触れている実体（壁・床・Rigidbody・他の Character Controller）ごとに、物理のステップの後で呼ばれる。Trigger では呼ばれない
void Hero::OnControllerColliderHit(const Tako::ControllerColliderHit& hit) {
    // 上向きの面（足元の床）は飛ばして、横や上からぶつかった相手だけを見る
    if (hit.normal.y > 0.5f) {
        return;
    }
    if (hit.actor->HasTag("Enemy")) {
        Tako::Logger::Log("%s touched %s\n", GetOwner()->GetName().c_str(), hit.actor->GetName().c_str());
    }
}
```

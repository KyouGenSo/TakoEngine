# TakoEngine C++ スクリプト マニュアル

TakoEngine でゲームの動きを書くための手引きです。C++ は書けるが、このエンジンは初めて、という人に向けて書いています。
プロジェクトの始め方は、[エンジンのリポジトリ](https://github.com/KyouGenSo/TakoEngine)の README を見てください。

コードの例は、Debug・Develop・Release の 3 つの構成でコンパイルできることを確かめてあります。関数の細かい決まりは、ヘッダーのコメントに書いてあります（[付録 ヘッダーの場所](#付録-ヘッダーの場所)）。

## 目次

1. [基本](#1-基本)
2. [最初のスクリプト](#2-最初のスクリプト)
3. [決まり](#3-決まり)
4. [呼ばれる関数](#4-呼ばれる関数)
5. [プロパティ](#5-プロパティ)
6. [TCLASS の指定子](#6-tclass-の指定子)
7. [Actor を操作する](#7-actor-を操作する)
8. [シーンを操作する](#8-シーンを操作する)
9. [タイマー](#9-タイマー)
10. [入力](#10-入力)
11. [当たり判定](#11-当たり判定)
12. [調整値](#12-調整値)
13. [色](#13-色)
14. [ログとデバッグ表示](#14-ログとデバッグ表示)
15. [コンポーネントの関数](#15-コンポーネントの関数)
16. [よくある失敗](#16-よくある失敗)
- [付録 ヘッダーの場所](#付録-ヘッダーの場所)

## 1. 基本

スクリプトは、`Tako::Script` を継承した C++ のクラスです（Unity の MonoBehaviour に当たります）。
Actor に **Script コンポーネント**を付け、その中にスクリプトを 1 つ入れて使います。1 つの Actor に、Script コンポーネントをいくつも付けられます。

| 言葉 | 意味 |
|---|---|
| シーン | Actor の入れ物。`GameProject\resources\Json\Scenes\名前.scene.json` に保存される |
| Actor | シーンに置く物。名前・Transform・タグを持つ。見た目や動きは、コンポーネントで足す |
| コンポーネント | Actor に付ける機能の単位（Mesh・Collider・Light など）。エンジンが用意している |
| スクリプト | ゲームが書くコンポーネント。Script コンポーネントの中に入る |
| プレハブ | Actor（と子）をひとまとめにして保存した物。実行中に `Spawn` で出せる |
| Hierarchy | エディタの、シーンの Actor の一覧 |
| Inspector | エディタの、選んだ Actor の値を編集する窓 |

エディタには、Edit と Play の 2 つの状態があります。

- **Edit**：シーンを編集する状態。スクリプトの `Tick` は呼ばれません
- **Play**：ゲームが動いている状態。F9 か、ツールバーの再生ボタンで切り替えます。Stop すると、シーンは Play を始める前の中身に戻ります

エディタは、起動した直後は Play で始まります（前の回に落ちたときだけ、Edit で始まります）。シーンを編集するときは、先に F9 で Stop します。

ビルドの構成は 3 つです。Debug と Develop では、スクリプトを、エディタを閉じずにコンパイルして差し替えられます。

| 構成 | 中身 | 使いどころ | 定義されるマクロ |
|---|---|---|---|
| Debug | エディタ。最適化なし | デバッガーで 1 行ずつ追うとき | `TAKO_EDITOR`、`_DEBUG` |
| Develop | エディタ。最適化あり | 普段の作業 | `TAKO_EDITOR` |
| Release | 配る用の 1 つの exe（`Generated\outputs\Release` にできる）。エディタは付かない | 配る物を作るとき | どちらも無し |

## 2. 最初のスクリプト

Actor を行って戻らせるスクリプト `Mover` を作ります。

### 2.1 作る

1. エディタの Assets で `Script` フォルダーを開き、何も無い所を右クリックして `New Script...` を選びます
2. クラス名に `Mover` と入れて、`Create` を押します

`Mover.h` と `Mover.cpp` ができ、ゲームのプロジェクト（`.vcxproj`）に登録されて、コンパイルが始まります。Visual Studio を開いていると、プロジェクトの再読み込みを聞かれます。

### 2.2 書く

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

- `TCLASS()` と `GENERATED_BODY()` は、このクラスをエンジンに知らせる印です。これが無いと、エディタの一覧に出ません
- `BeginPlay` は Play が始まったときに 1 回、`Tick` は Play 中の毎フレーム呼ばれます。`deltaTime` は前のフレームからの秒数です
- `GetOwner()` は、このスクリプトが付いている Actor を返します
- `TPROPERTY(Edit, …)` を付けたメンバーは、Inspector で編集でき、シーンファイルに保存されます。付けていない `origin_` と `time_` は、Inspector に出ず、保存もされません

### 2.3 コンパイルする

ファイルを保存してエディタのウィンドウに戻ると、自動でコンパイルが始まります（手動は Ctrl+B）。Visual Studio でゲームのプロジェクトをビルドしても、同じです。

- 終わると、Console に `Scripts reloaded` と出ます
- Play 中にコンパイルすると、Play は止まって Edit に戻ります
- コンパイルエラーは Console に出ます。エラーの行をダブルクリックすると、Visual Studio でその場所が開きます

### 2.4 Actor に付ける

1. Edit の状態にします（Play 中に付けた物は、Stop で消えます）
2. Hierarchy で Actor を選び、Inspector の `Add Component` を押します
3. 一覧の `Scripts` の下から `Mover` を選びます

Inspector に、`Offset` と `Period` の欄が出ます。

### 2.5 動かす

F9 で Play に入ると、Actor が行って戻るのを繰り返します。もう一度 F9 を押すと Stop します。

- Play 中に Inspector で変えた値は、Stop で捨てられます。残したいときは、コンポーネントの見出しを右クリックして `Keep Values on Stop` を選んでおきます
- シーンの保存は、Edit 中に Hierarchy で Ctrl+S です

## 3. 決まり

| 決まり | 理由 |
|---|---|
| クラス名とファイル名（`.h`）を同じにする | Assets からのドロップと、スクリプトを開く処理が、名前でファイルを探すため |
| スクリプトは `GameProject\Script` の中に置く（サブフォルダーに分けてよい） | Assets がスクリプトとして扱うのは、このフォルダーの `.h` と `.cpp` だけ |
| スクリプトでないコード（設定・計算・状態など）は `GameProject\Source` に置く | 自動コンパイルが変更を見るのは `Script` と `Source` の 2 つ |
| クラスの直前に `TCLASS()`、本体の先頭に `GENERATED_BODY()` を書く | この 2 つを読んで、ビルドのときに型の登録のコードが作られる |
| `Tako::Script` を継承する | Add Component の `Scripts` に出るのは、`Script` の派生だけ |
| ファイルを `.vcxproj` に登録する | `New Script...` で作れば、自動で登録される。手で作ったファイルは、Visual Studio の「既存の項目の追加」で足す |

include の書き方です。

- エンジンのヘッダーと、同じフォルダーのヘッダーは、ファイル名だけで書けます：`#include "Actor.h"`
- ゲームの中の別のフォルダーのヘッダーは、`GameProject` からのパスで書きます：`#include "Source/Settings/GameTuning.h"`、`#include "Script/Mover.h"`

`TCLASS` や `TPROPERTY` の書き方が間違っていると、ビルドのときに、コード `TAKO001` のエラーが出ます。主な物です。

| エラーの文 | 直し方 |
|---|---|
| TCLASS には GENERATED_BODY() を書く | クラスの本体の先頭に `GENERATED_BODY()` を足す |
| TCLASS 名前 は TObject かその派生を継承する | `Tako::Script`（か `Tako::GameSettings` など）を継承する |
| TCLASS を他のクラスの中に書けない | クラスを外に出す（`TSTRUCT` と `TENUM` は、クラスの中に書ける） |
| TPROPERTY は TCLASS / TSTRUCT の本体の直下に書く | 関数の中や、入れ子の無名の構造体の中に書かない |
| TPROPERTY を static メンバーに付けられない | `static` と `constexpr` のメンバーには付けない |
| TPROPERTY に配列は使えない | `float a[3]` ではなく `std::vector<float>` か `Vector3` を使う |
| 知らない TPROPERTY の指定子: 名前 | 指定子の綴りを直す（[5. プロパティ](#5-プロパティ)） |
| TCLASS を #if の中に書けない | `TCLASS`・`TPROPERTY` などの印を、`#if` や `#ifdef` で囲まない |

## 4. 呼ばれる関数

スクリプトは、次の関数を override して使います。どれも、書かなければ何もしません。

| 関数 | いつ呼ばれるか |
|---|---|
| `void Awake()` | `BeginPlay` の前に 1 回。同時にシーンへ入る全部の Actor の `Awake` が済んでから、`BeginPlay` が始まる。他のスクリプトの `BeginPlay` から使われる物は、ここで用意する |
| `void BeginPlay()` | Play が始まったとき、または Play 中にシーンへ足されたときに 1 回 |
| `void Tick(float deltaTime)` | Play 中の毎フレーム |
| `void LateTick(float deltaTime)` | 全部の `Tick` の後。他の Actor が動いた後の位置を使う処理（追いかけるカメラなど）を書く |
| `void EndPlay()` | Play が終わるとき、または Actor が消される・コンポーネントが外されるとき（`BeginPlay` が呼ばれた物だけ） |
| `void Draw()` | 毎フレーム。Edit 中と一時停止中も呼ばれる。描画物の登録を書く |
| `void OnCollisionEnter(Tako::Actor* other)` | 同じ Actor の Collider が、他のコライダーに当たり始めた |
| `void OnCollisionStay(Tako::Actor* other)` | 当たっている間、毎フレーム |
| `void OnCollisionExit(Tako::Actor* other)` | 離れた |
| `void OnActiveChanged(bool active)` | Actor の Active（自分か親のどれか）が切り替わった次のフレームの頭。Edit 中も呼ばれる |
| `void OnSiblingRemoved(Tako::Component* removed)` | 同じ Actor の他のコンポーネントが、これから外される。Edit 中も呼ばれる |
| `bool TickWhilePaused() const` | `true` を返すと、シーンのポーズ中も `Tick` と `LateTick` が呼ばれる |
| `void PostEditChange()` | Inspector で値が変わった後。他の値に合わせた補正を書く |
| `void PostLoad(const nlohmann::json& raw)` | 保存した値を読んだ後。古いキーの読み替えなどに使う |
| `void PostSave(nlohmann::json& out)` | 値を保存した後。`TPROPERTY` で表せない中身を足す |
| `bool DrawCustomImGui()` | Inspector の、自動で出る欄の後。エディタがある構成だけ（[14. ログとデバッグ表示](#14-ログとデバッグ表示)） |

### 4.1 1 フレームの順番

Play 中の 1 フレームは、次の順に進みます。

```text
フレームの頭
  1. 予約されていた Destroy      … EndPlay を呼んでから、シーンから消す
  2. 予約されていた RemoveComponent
  3. 予約されていた Spawn        … シーンに入れる → 入った全部の Awake → 入った全部の BeginPlay
  4. Active が変わった Actor に OnActiveChanged
更新（Play 中だけ）
  5. コンポーネントごとに、タイマー → Tick
  6. 全部の LateTick
  7. 当たり判定                  … OnCollisionEnter / Stay / Exit（ポーズ中は行わない）
描画（Edit 中も）
  8. 全部の Draw
```

- `Destroy`・`RemoveComponent`・`Spawn` は、呼んだその場では行われません。次のフレームの頭にまとめて行われます。そのため、`Tick` や当たりの途中でも安全に呼べます
- Actor がオフ（自分か親の Active がオフ）の間は、`Tick`・`LateTick`・`Draw` が呼ばれず、タイマーも進みません
- ポーズ中は、`TickWhilePaused` が `true` のスクリプトだけ `Tick` と `LateTick` が呼ばれます

### 4.2 呼ぶ順を決める

`Awake`・`BeginPlay`・`Tick`・`LateTick` を呼ぶ順は、次のように決まります。

1. `TCLASS(Order = n)` の `n` が小さい順（書かなければ 0）
2. 同じ値なら、Hierarchy の並び順
3. 同じ Actor の中では、コンポーネントの並び順

順番が要るスクリプトは、`Order` で決めます。Hierarchy の並びは、編集しているうちに変わります。

```cpp
TCLASS(Order = -100)
class ScoreCounter : public Tako::Script {
```

`Draw` だけは、Hierarchy の並び順に呼ばれます（`Order` は効きません）。

## 5. プロパティ

メンバー変数の直前の行に `TPROPERTY(...)` を書くと、その値が Inspector に出て、シーンファイルとプレハブに保存されます。

### 5.1 使える型

| 型 | Inspector での見え方 |
|---|---|
| `bool` | チェックボックス |
| `int32_t`、`uint32_t`、`float` | 数値（ドラッグで変える） |
| `Tako::Vector2`、`Tako::Vector3`、`Tako::Vector4` | 成分ごとの数値。`Color` を付けると色 |
| `Tako::Color` | 色（[13. 色](#13-色)） |
| `Tako::Gradient` | 色の段階の一覧。`Evaluate(t)` で、位置 `t`（0〜1）の色を取る |
| `std::string` | 文字。`Asset` を付けるとアセットの欄 |
| `Tako::Transform` | 位置・回転・大きさ |
| `Tako::ActorRef` | 同じシーンの Actor を選ぶ欄 |
| `TENUM()` を付けた enum | 列挙子の名前から選ぶ |
| `TSTRUCT()` を付けた構造体 | 中の `TPROPERTY` をまとめて出す |
| 上の型の `std::vector` | 要素を並べた一覧 |

この表に無い型（`double`・`uint64_t`・ポインタ・生の配列など）には付けられません。

### 5.2 指定子

`TPROPERTY(Edit, ClampMin = 0, Speed = 0.05)` のように、コンマで並べます。

| 指定子 | 意味 |
|---|---|
| `Edit` | Inspector で編集できる |
| `Visible` | Inspector に、読み取り専用で出す |
| （`Edit` も `Visible` も無し） | Inspector に出さず、保存だけする |
| `Transient` | 保存しない |
| `Category = "名前"` | Inspector で、同じ名前の物を 1 つの見出しにまとめる |
| `Tooltip = "説明"` | 欄にカーソルを載せたときの説明 |
| `DisplayName = "名前"` | 欄の名前。書かなければ、メンバー名を単語に分けた物（`stopDistance_` なら Stop Distance） |
| `ClampMin = 値`、`ClampMax = 値` | 入れられる範囲 |
| `Speed = 値` | ドラッグしたときの変わる速さ |
| `Color` | `Vector3`・`Vector4` を、色として編集する（`Tako::Color` には要らない） |
| `HDR` | 色の欄で、1 を超える値を入れられるようにする（[13. 色](#13-色)） |
| `Angle` | ラジアンの `float` を、度で編集する（`ClampMin`・`ClampMax`・`Speed` も度で書く） |
| `EnumAsInt` | enum を、名前でなく数値で保存する |
| `Asset = "種類"` | `std::string` を、アセットの欄にする（[5.4](#54-アセットの欄)） |
| `ActorWith = "型名"` | `ActorRef` の一覧を、その型のコンポーネントかスクリプトを持つ Actor に絞る |
| `Required` | `ActorRef`・`std::string` が空の間、欄を赤くする |

```cpp
    TPROPERTY(Edit, Category = "Move", Tooltip = "1 秒あたりの移動量", ClampMin = 0, Speed = 0.05)
    float speed_ = 3.0f;
    TPROPERTY(Edit, Category = "Move", Angle, Tooltip = "1 秒あたりの回転。欄は度、値はラジアン")
    float turnSpeed_ = std::numbers::pi_v<float>;
    TPROPERTY(Edit, Tooltip = "巡回する位置")
    std::vector<Tako::Vector3> waypoints_;
    TPROPERTY(Edit, Asset = "Prefab")
    std::string bulletPrefab_;
    TPROPERTY(Edit, Required, ActorWith = "ColliderComponent")
    Tako::ActorRef target_;
    TPROPERTY(Visible)
    int32_t killCount_ = 0;
```

enum と構造体は、次のように書きます（スクリプトのクラスの中に書けます）。

```cpp
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
```

### 5.3 保存のされ方

- 保存のキーは、メンバー名から末尾の `_` を外した名前です（`speed_` は `"speed"`）。メンバー名を変えると、前に保存した値は読まれなくなります
- スクリプトの値は、Script コンポーネントの `"script"`（スクリプトの型名）と同じ階層に並びます。そのため、スクリプトは `script_` という名前のプロパティを持てません
- `TPROPERTY` の無いメンバーは保存されません。Stop したときとコンパイルの後は、スクリプトが作り直されるので、初期値に戻ります

### 5.4 アセットの欄

`TPROPERTY(Edit, Asset = "種類")` を `std::string` に付けると、一覧から選ぶ・Assets からドロップする・`Open` でエディタを開く、ができる欄になります。
値として入る文字列は、その種類を読む関数にそのまま渡せます。

| 種類 | 指す物 |
|---|---|
| `Texture`、`Cubemap` | テクスチャ、キューブマップ |
| `Model` | モデル |
| `Material` | マテリアル |
| `Scene` | シーン（`SceneManager::ChangeScene` に渡す名前） |
| `Prefab` | プレハブ（`BaseScene::Spawn` に渡す名前） |
| `ParticlePreset` | パーティクルのプリセット |
| `AnimationClip` | Animation Editor で作ったクリップ |
| `UILayout` | UI Editor で作ったレイアウト |
| `BehaviorTree` | ビヘイビアツリー |
| `PostEffectProfile` | ポストエフェクトのプロファイル |
| `Sound` | 音 |
| `Font` | フォント |
| `Script` | スクリプトの型名 |

### 5.5 Actor を指す

他の Actor を指すには、ポインタではなく `Tako::ActorRef` を使います。Actor の ID で保存されるので、シーンを開き直しても同じ相手を指します。
Inspector では、一覧から選ぶか、Hierarchy の行を欄にドロップします。

```cpp
    TPROPERTY(Edit, Tooltip = "追う相手。Hierarchy の行をここへドロップしても入る")
    Tako::ActorRef target_;
```

相手は、`Actor::Resolve` で取ります。指していないか、相手がもう居なければ `nullptr` が返ります。

```cpp
    const Tako::Actor* target = GetOwner()->Resolve(target_);
    if (!target) {
        return;
    }
```

Actor のポインタを、フレームをまたいでメンバーに持ち続けないでください。相手が `Destroy` されると、無効になります。`ActorRef` で持ち、使うたびに `Resolve` します。

## 6. TCLASS の指定子

`TCLASS(Order = -50, Requires = "ColliderComponent")` のように書きます。どれも省けます。

| 指定子 | 意味 |
|---|---|
| `Order = n` | `Awake`・`BeginPlay`・`Tick`・`LateTick` を呼ぶ順。小さいほど先（[4.2](#42-呼ぶ順を決める)） |
| `Requires = "型名"` | 同じ Actor に要るコンポーネント。無ければ、Inspector が警告と、足すボタンを出す |
| `DisplayName = "名前"` | エディタの一覧に出す名前 |
| `Category = "分類"` | エディタの一覧の分類（ビヘイビアツリーのノードで使う） |
| `Color = 0xRRGGBB` | エディタで見分ける色（ビヘイビアツリーのノードで使う） |

## 7. Actor を操作する

スクリプトが付いている Actor は、`GetOwner()` で取れます。ヘッダーは `Actor.h` です。

### 7.1 位置・回転・大きさ

```cpp
    Tako::Actor* owner = GetOwner();
    // Transform は親からの相対（親が無ければワールド）。rotate はラジアン
    Tako::Transform& transform = owner->GetTransform();
    transform.translate.y += 1.0f;
    transform.rotate.y = std::numbers::pi_v<float> * 0.5f;
    transform.scale    = { 2.0f, 2.0f, 2.0f };
    // 親があっても、ワールドの位置で読み書きできる
    const Tako::Vector3 world = owner->GetWorldPosition();
    owner->SetWorldPosition(world + Tako::Vector3{ 0.0f, 0.0f, 1.0f });
```

| 関数 | 意味 |
|---|---|
| `GetTransform()` | 親からの相対の位置・回転・大きさ（`translate`・`rotate`・`scale`）。参照なので、そのまま書き換えられる |
| `SetTransform(transform)` | Transform をまとめて入れる |
| `GetWorldPosition()`、`SetWorldPosition(position)` | ワールドでの位置 |
| `GetWorldTransform()`、`SetWorldTransform(world)` | ワールドでの位置・回転・大きさ。`SetWorldTransform` は、親の大きさが 0 の間は何もしない |
| `GetWorldMatrix()` | Transform に、親のワールド行列を掛けた行列 |

回転はオイラー角（ラジアン）です。行ベクトルで、X → Y → Z の順に掛けます。

### 7.2 親子・Active・タグ

| 関数 | 意味 |
|---|---|
| `GetParent()` | 親の Actor。無ければ `nullptr` |
| `SetParent(parent, keepWorld = true)` | 親を替える（`nullptr` で外す）。`keepWorld` が `true` なら、見た目の位置・向き・大きさは変わらない。自分の子孫は親にできない。替えられなければ `false` |
| `GetChildren()` | 直接の子の一覧 |
| `FindChild(name)` | 直接の子を名前で探す。無ければ `nullptr` |
| `SetActive(active)`、`IsActive()` | 自分の Active。オフにすると、自分と子は動かず、描かれず、当たらない |
| `IsActiveInHierarchy()` | 自分と親が全部オンなら `true` |
| `HasTag(tag)` | Inspector の `Tags` に、その印があれば `true` |
| `GetName()`、`SetName(name)` | Actor の名前 |
| `GetId()` | シーンの中で一意の ID |
| `GetScene()` | 入っているシーン |
| `Destroy()` | シーンから消す。子も一緒に消える |

### 7.3 他の Actor を探す

```cpp
    Tako::Actor*     owner = GetOwner();
    Tako::BaseScene* scene = owner->GetScene();
    // 名前で探す（最初に見つかった 1 つ。無ければ nullptr）
    if (Tako::Actor* player = scene->FindActor("Player")) {
        Tako::Logger::Log("player is at y = %f\n", player->GetWorldPosition().y);
    }
    // タグで探す（Inspector の Tags に書いた印）
    const std::vector<Tako::Actor*> enemies = scene->FindActorsWithTag("Enemy");
```

決まった相手は、`ActorRef` のプロパティにして Inspector で指すのが確実です（[5.5](#55-actor-を指す)）。名前で探す方法は、名前を変えると動かなくなります。

### 7.4 コンポーネントとスクリプトを取る

```cpp
    Tako::Actor* owner = GetOwner();
    // 同じ Actor のコンポーネントを、型で取る（無ければ nullptr）
    if (Tako::MeshComponent* mesh = owner->GetComponent<Tako::MeshComponent>()) {
        mesh->SetVisible(true);
    }
    // 同じ Actor の他のスクリプトを、型で取る
    if (owner->GetScript<Mover>()) {
        Tako::Logger::Log("%s has a Mover\n", owner->GetName().c_str());
    }
```

| 関数 | 意味 |
|---|---|
| `GetComponent<T>()` | `T` か `T` の派生の、最初のコンポーネント。無ければ `nullptr` |
| `GetScript<T>()` | Script コンポーネントに入っている、`T` か `T` の派生のスクリプト。無ければ `nullptr` |
| `FindComponent(name)` | コンポーネントを名前で探す（同じ型を 2 つ付けたときに使う）。無ければ `nullptr` |
| `GetComponents()` | 全部のコンポーネント |
| `AddComponent<T>(name = {})` | `T` を作って足す。Play 中なら `Awake` と `BeginPlay` も呼ばれる。名前が空なら型名になる |
| `RemoveComponent(component)` | コンポーネントを外す |
| `Resolve(ref)` | `ActorRef` が指す Actor（[5.5](#55-actor-を指す)） |

スクリプトは Script コンポーネントの中に入っているので、`GetComponent<Mover>()` では取れません。`GetScript<Mover>()` を使います。
相手の Actor のスクリプトの関数を呼ぶときも同じです：`other->GetScript<Player>()->OnHit(damage)`。

## 8. シーンを操作する

シーンは `GetOwner()->GetScene()` で取れます。ヘッダーは `BaseScene.h` です。

| 関数 | 意味 |
|---|---|
| `Spawn(typeOrPrefab)` | プレハブ名（か Actor の型名）から Actor を作る。プレハブなら、子も全部作る。返るのは根の Actor。作れなければ `nullptr` |
| `Destroy(actor)` | Actor を消す。子も一緒に消える（`actor->Destroy()` と同じ） |
| `FindActor(name)`、`FindActor(id)` | 名前か ID で探す。無ければ `nullptr` |
| `FindActorsWithTag(tag)` | そのタグを持つ全部の Actor |
| `SendEvent(name, payload = {})` | その名前を `Subscribe` している全部のコンポーネントとスクリプトの関数を、その場で呼ぶ |
| `SetPaused(paused)`、`IsPaused()` | ゲームの進行を止める・戻す。描画は続く |
| `GetEmitterManager()` | このシーンのパーティクルのエミッター |
| `GetForceFieldManager()` | このシーンのフォースフィールド（パーティクルに掛かる力） |
| `GetSceneName()` | シーンファイルの名前 |

### 8.1 Actor を出す

実行中に出す物は、プレハブにしておきます。プレハブは、Hierarchy で Actor を右クリックし、メニューの Prefab の欄に名前を入れて `Save as Prefab` で作ります。

```cpp
    TPROPERTY(Edit, Asset = "Prefab", Required)
    std::string prefab_;
```

`Spawn` が返した Actor には、すぐに値を入れられます。

```cpp
    Tako::Actor* actor = scene->Spawn(prefab_);
    if (!actor) {
        // プレハブが無い。出し続けても同じなので、タイマーを止める
        ClearTimer(timer_);
        return;
    }
    const Tako::Vector3 offset = Tako::RandomEngine::GetInstance()->GetRandomDirectionXZ() * radius_;
    actor->GetTransform().translate = GetOwner()->GetWorldPosition() + offset;
```

### 8.2 イベントで知らせる

スクリプトどうしを直接つながずに知らせ合うには、イベントを使います。

- 受け取る側：`Subscribe(名前, 関数)`。スクリプトが消えると、受け取りも終わります
- 送る側：`GetOwner()->GetScene()->SendEvent(名前, 値)`

値は `std::any` で渡ります。型は、送る側と受け取る側で決めておき、`std::any_cast` で取り出します。

コインに当たったら点を知らせる例です。送る `Coin` は、誰が点を数えているかを知りません。

```cpp
void Coin::OnCollisionEnter(Tako::Actor* other) {
    // other は相手の Actor。Actor に付いていないコライダーなら nullptr
    if (!other || !other->HasTag("Player")) {
        return;
    }
    GetOwner()->GetScene()->SendEvent("CoinCollected", point_);
    GetOwner()->Destroy();
}
```

受け取る `ScoreCounter` です。点が目標に届いたら、次のシーンへ移ります。

```cpp
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
```

### 8.3 シーンを替える

ヘッダーは `SceneManager.h` です。`Tako::SceneManager::GetInstance()` から呼びます。

| 関数 | 意味 |
|---|---|
| `ChangeScene(sceneName)` | 次のシーンへ移る（既定のフェード）。名前は、シーンファイルの名前 |
| `ChangeScene(sceneName, transitionTime)` | 遷移の秒数を指定する |
| `ChangeScene(sceneName, effectType, transitionTime)` | 演出を選ぶ。`Tako::TransitionManager::EffectType` の `Fade`（白）・`BlackFade`（黒）・`ScaleExpand`・`ScaleShrink` |
| `GetCurrentScene()`、`GetCurrentSceneName()` | 今のシーン |
| `GetSceneNames()` | シーンファイルの名前の一覧 |
| `GetSessionData()` | シーンを替えても残る値（`nlohmann::json`）。キーと値は、ゲームが決める |

- 遷移の途中でもう一度 `ChangeScene` を呼んでも、2 回目からは無視されます
- `GetSessionData()` の中身は、エディタの Stop で空に戻ります。使う `.cpp` で `<json.hpp>` を include します

次のシーンで値を読む例です。

```cpp
    const int32_t score = Tako::SceneManager::GetInstance()->GetSessionData().value("score", 0);
```

### 8.4 ポーズ

`GetOwner()->GetScene()->SetPaused(true)` で、シーンの `Tick`・`LateTick`・当たり判定・タイマーが止まります。描画は続きます。
ポーズ中も動かしたいスクリプト（メニューなど）は、`TickWhilePaused` で `true` を返します。

```cpp
    // ポーズ中も Tick を呼んでもらう（そうしないと、再開のキーを読めない）
    bool TickWhilePaused() const override { return true; }
```

## 9. タイマー

「何秒後に 1 回」「何秒ごとに」は、`Tick` で時間を数えずに、タイマーで書けます。`Script` の関数なので、そのまま呼べます。

| 関数 | 意味 |
|---|---|
| `SetTimer(seconds, callback, loops = false)` | `seconds` 秒後に `callback` を呼ぶ。`loops` が `true` なら、`seconds` 秒ごとに呼び続ける。戻り値は、止めるときに使う番号（`Tako::TimerId`） |
| `ClearTimer(id)` | タイマーを取り消す。タイマーの関数の中からも呼べる |

```cpp
    // 3 つ目の引数が true なら、止めるまで spawnInterval 秒ごとに呼ばれる
    timer_ = SetTimer(tuning.spawnInterval, [this] { SpawnOne(); }, true);
```

スクリプトが消えると、タイマーも一緒に消えます。`EndPlay` で止める必要はありません。

## 10. 入力

ヘッダーは `Input.h` です。`Tako::Input::GetInstance()` から呼びます。どの種類にも、3 つの読み方があります。

| 読み方 | 意味 |
|---|---|
| `Push…` | 押している間、ずっと `true` |
| `Trigger…` | 押した瞬間の 1 フレームだけ `true` |
| `Release…` | 離した瞬間の 1 フレームだけ `true` |

| 関数 | 意味 |
|---|---|
| `PushKey(key)`、`TriggerKey(key)`、`ReleaseKey(key)` | キーボード。キーは DirectInput の `DIK_` で指定する（`DIK_SPACE`・`DIK_W` など） |
| `PushMouse(button)`、`TriggerMouse(button)`、`ReleaseMouse(button)` | マウスのボタン。0 が左、1 が右、2 が中央 |
| `GetMousePos()` | マウスの位置（スクリーン座標） |
| `PushButton(button)`、`TriggerButton(button)`、`ReleaseButton(button)` | ゲームパッド。ボタンは `Tako::GamepadButton::A` など |
| `GetLeftStick()`、`GetRightStick()` | スティック。-1 〜 1 の `Vector2`。デッドゾーンの中は 0 |
| `GetLeftTrigger()`、`GetRightTrigger()` | トリガー。0 〜 1 |
| `IsConnect()` | ゲームパッドがつながっていれば `true` |
| `SetVibration(leftMotor, rightMotor, duration = 0)`、`StopVibration()` | 振動。強さは 0 〜 1、`duration` は秒（0 以下なら止めるまで） |
| `PushAction(name)`、`TriggerAction(name)`、`ReleaseAction(name)` | アクション（名前を付けたキーとボタンの組） |
| `GetAxis(name)` | 軸（4 方向のキーとスティックを合わせた `Vector2`） |

`GamepadButton` には、`A`・`B`・`X`・`Y`・`DPad_Up`・`DPad_Down`・`DPad_Left`・`DPad_Right`・`L_Shoulder`・`R_Shoulder`・`L_Thumbstick`・`R_Thumbstick`・`Start`・`Back` があります。

```cpp
    const Tako::Input* input = Tako::Input::GetInstance();

    Tako::Vector2 move = input->GetLeftStick();
    if (input->PushKey(DIK_D)) { move.x += 1.0f; }
    if (input->PushKey(DIK_A)) { move.x -= 1.0f; }
    if (input->PushKey(DIK_W)) { move.y += 1.0f; }
    if (input->PushKey(DIK_S)) { move.y -= 1.0f; }
```

### 10.1 アクションと軸

キーやボタンをコードに直接書く代わりに、名前を付けて割り当てておけます。割り当ては、Engine Settings の Input の `Actions` と `Axes` で行います。

```cpp
    const Tako::Input*  input = Tako::Input::GetInstance();
    const Tako::Vector2 move  = input->GetAxis("Move");
    const float         speed = input->PushAction("Dash") ? dashSpeed_ : speed_;
```

- アクションは、割り当てたキーやボタンの **どれか 1 つ** でも押されていれば、押されている扱いです
- 軸は、4 方向のキー（それぞれ -1 / 0 / 1）とスティックの和です。長さは 1 に丸められません
- 作っていない名前を渡すと、assert で止まります
- Engine Settings の `Name Header` にヘッダーのパスを書いておくと、名前の定数を並べたヘッダーが書き出されます。文字列を直接書く代わりに、その定数を使えます

## 11. 当たり判定

当たり判定は、Actor に **Collider** コンポーネントを付けて行います。当たると、同じ Actor の全部のコンポーネントとスクリプトの `OnCollisionEnter`・`OnCollisionStay`・`OnCollisionExit` が呼ばれます（例は [8.2](#82-イベントで知らせる) の `Coin`）。

- 当たる **両方** の Actor に、Collider が要ります
- 引数の `other` は、相手の Actor です。相手が Actor に付いていないコライダー（ゲームがコードで作った物）なら、`nullptr` です
- エンジンが行うのは、当たったことを知らせる所までです。押し戻しや跳ね返りは、スクリプトで書きます

どのコライダーどうしが当たるかは、層（Layer）で決まります。

- 層の名前と、層どうしが当たるかどうかの表（`Collision Matrix`）は、Engine Settings の Physics で決めます
- Collider の `Layer` の欄で、そのコライダーの層を選びます
- 何も変えなければ、Collider は専用の層（番号 31）に入り、その層どうしだけが当たります

## 12. 調整値

ゲーム全体で 1 つだけあればよい値（重力、敵を出す間隔など）は、`Tako::GameSettings` を継承したクラスに置きます。スクリプトごとに違ってよい値（この敵の HP など）は、スクリプトの `TPROPERTY` にします。

- 型ごとに、値は 1 つだけです。`GameProject\resources\Json\GameSettings\型名.json` に保存されます
- エディタでは、Engine Settings の左の一覧の `Game` の下に型の名前が出て、そこで編集できます
- スクリプトではないので、`Script` ではなく `Source` に置きます

```cpp
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

読むときは、`Tako::GameSettings::Get<型>()` です（ヘッダーは `GameSettings.h`）。初めて呼ばれたときに、ファイルから読まれます。

```cpp
    const GameTuning& tuning = Tako::GameSettings::Get<GameTuning>();
```

| 関数 | 意味 |
|---|---|
| `Tako::GameSettings::Get<T>()` | `T` のただ 1 つの値（参照） |
| `Save()` | 今の値をファイルに書く。書けなければ `false` |
| `Reload()` | ファイルから読み直す |

## 13. 色

色は `Tako::Color`（`r`・`g`・`b`・`a`）で表します。ヘッダーは `ColorFunc.h` です。エンジンの色の関数（`SetMaterialColor`・`SetColor` など）は、どれも `Color` を受け取ります。

| 書き方 | 意味 |
|---|---|
| `Tako::Color(r, g, b, a = 1)` | 0〜1 の数から作る |
| `Color::kWhite`・`kBlack`・`kClear`・`kRed`・`kGreen`・`kBlue`・`kYellow`・`kCyan`・`kMagenta`・`kGray` | 名前の付いた色（`kClear` は透明、`kGray` は 0.5） |
| `Color::FromHex(0xRRGGBB, a = 1)` | 16 進数から作る |
| `Tako::Color32(r, g, b, a = 255)` | 0〜255 の整数から作る。`Color` に自動で変わる |
| `Color::FromHSV(h, s, v, a = 1)` | HSV から作る。どれも 0〜1（`h` は 1 で 1 周） |
| `WithAlpha(a)` | アルファだけを替えた色 |
| `Color::Lerp(from, to, t)`、`Color::LerpHSV(from, to, t)` | 2 色の間。`Lerp` は成分ごと、`LerpHSV` は色相環に沿う |

- 色の値は、画面に出る値（sRGB）で書きます。Inspector の色の欄・16 進数・`Color32` も同じです
- `Color` と `Vector4` は、互いに自動で変わります。ただし、三項演算子（`条件 ? a : b`）の左右に混ぜると、コンパイルエラーになります
- 1 を超える色は光る色で、ブルームの元になります。Inspector の欄で 1 を超える値を入れるには、`HDR` の指定子を付けます

```cpp
    TPROPERTY(Edit, HDR, Tooltip = "1 を超えると光る（シーンでは 1 を超えたまま計算され、ブルームの元になる）")
    Tako::Color glowColor_ = Tako::Color(4.0f, 2.0f, 0.5f);
```

## 14. ログとデバッグ表示

### 14.1 ログ

ヘッダーは `Logger.h` です。出した文字は、エディタの Console と、Visual Studio の出力ウィンドウに出ます。`printf` と同じ書式で、行の終わりには `\n` を書きます。

```cpp
    Tako::Logger::Log("Score: %d\n", score_);
```

### 14.2 Inspector に追加の表示を出す

`DrawCustomImGui` を override すると、Inspector の、自動で出る欄の下に、好きな表示やボタンを足せます。ImGui をそのまま使います。

```cpp
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

- `DrawCustomImGui` があるのは、エディタがある構成（Debug と Develop）だけです。宣言・定義・`#include "imgui.h"` を、どれも `#ifdef TAKO_EDITOR` で囲みます。囲まないと、Release のビルドが通りません
- 戻り値は、値を変えたら `true` です。表示だけなら `false` を返します
- この関数の中で `ImGui::Begin` を呼んで、別の窓を作らないでください（表示が点滅します）

### 14.3 処理の時間を見る

どのスクリプトが重いかは、エディタの Performance ウィンドウ（F6）の `CPU Update` で見られます。値は、直近 0.5 秒の、1 フレームあたりの平均です。

| 行 | 意味 |
|---|---|
| `Components` の下の、クラス名の行 | そのクラスの `Tick`・`LateTick`・タイマーの関数に掛かった時間（`CPU ms`）と、動いている個数（`Count`）。スクリプトは、スクリプトのクラスごとに出る |
| `Collision` | 当たり判定 |
| `Callbacks` | `OnCollisionEnter`・`OnCollisionStay`・`OnCollisionExit` の呼び出し |

- 時間は Develop で見ます。Debug は最適化が無いので、長く出ます
- `Measure is off` と出ているときは、Render Pipeline ウィンドウの Frame タブで `Measure` をオンにします
- Release では、exe に `--profile` を付けて起動すると、同じ内訳と FPS が画面に重ねて出ます（新しく作ったプロジェクトには、これで起動する `RunProfile.bat` がルートに入っています）

## 15. コンポーネントの関数

エンジンが用意しているコンポーネントです。Inspector の `Add Component` で Actor に付け、スクリプトからは `GetOwner()->GetComponent<Tako::型名>()` で取ります。ヘッダーの名前は、型名と同じです（`MeshComponent.h` など）。

| 一覧での名前 | 型名 | すること |
|---|---|---|
| Mesh | `MeshComponent` | Actor の位置にモデルを描く |
| Collider | `ColliderComponent` | 当たり判定（[11. 当たり判定](#11-当たり判定)） |
| Animator | `AnimatorComponent` | 同じ Actor の Mesh のモデルを動かす |
| Audio Source | `AudioSourceComponent` | 音を鳴らす（位置による聞こえ方の違いは無い） |
| Camera | `CameraComponent` | Actor の位置と向きから見るカメラ。複数あるときは、Priority が一番大きい物が使われる |
| Light | `LightComponent` | Actor の位置から、Actor の +Z の向きへ照らす。平行光源は、シーンに 1 つだけ効く。点光源とスポットライトは、Cast Shadow をオンにすると影を落とす（スポットライトを 1 枚・点光源を 6 枚と数えて、合わせて 16 枚まで） |
| Particle System | `ParticleSystemComponent` | パーティクルのプリセットを、Actor の位置から出す（Play 中だけ） |
| Post Process | `PostProcessComponent` | シーンのポストエフェクトを、プロファイルで決める |
| Sky Box | `SkyBoxComponent` | キューブマップの空を描く |
| UI Layout | `UILayoutComponent` | UI Editor で作ったレイアウトを、画面に出す |
| Behavior Tree | `BehaviorTreeComponent` | Actor をビヘイビアツリーで動かす |
| Script | `ScriptComponent` | スクリプトを 1 つ入れる |

Animator・Light・Sky Box には、スクリプトから呼ぶ関数はありません。値は Inspector で決めます。

### Mesh

| 関数 | 意味 |
|---|---|
| `SetModel(model)` | モデルを替える |
| `SetVisible(visible)`、`IsVisible()` | 描く・描かない（描かない間は、影も落とさない） |
| `SetOffset(offset)`、`GetOffset()` | Actor からのずれ。Actor の位置を変えずに、見た目だけを動かす（揺らす演出など） |
| `SetGeneratedModel(model)` | ファイルではなく、コードで作ったモデルを描く。Inspector の Model の欄は空にしておく |
| `GetObject3d()` | 描画物（`Tako::Object3d`）。`BeginPlay` からも使える |

```cpp
    Tako::MeshComponent* mesh = GetOwner()->GetComponent<Tako::MeshComponent>();
    if (!mesh) {
        return;
    }
    // 描画物（Object3d）を取って、色を変える
    if (Tako::Object3d* object = mesh->GetObject3d()) {
        object->SetMaterialColor(Tako::Color(1.0f, 0.3f, 0.3f));
    }
```

`Object3d`（`Object3d.h`）の、よく使う関数です。

| 関数 | 意味 |
|---|---|
| `SetMaterialColor(color)`、`GetMaterialColor()` | マテリアルの色を上書きする。`std::nullopt` を渡すと、元に戻す |
| `SetTransparent(isTransparent)` | 半透明として描く |
| `SetCastShadow(castShadow)` | 影を落とす・落とさない |
| `SetTexture(fileName)` | テクスチャを差し替える。空の文字列で、元に戻す |
| `SetMeshVisible(name, visible)`、`GetMeshNames()` | モデルの中のメッシュを、名前で出す・隠す |
| `GetWorldMatrix()` | ワールド行列 |
| `GetAnimator()` | このモデルを動かす `Tako::AnimationPlayer`（関数は [UI Layout](#ui-layout) の表） |

### Collider

| 関数 | 意味 |
|---|---|
| `SetActive(active)`、`IsActive()` | 当たり判定を一時的に止める・戻す（攻撃の間だけ当てる、などに使う） |
| `SetRadius(radius)` | 球（Sphere）の半径を変える |
| `SetSize(size)` | 箱（Box）の全長を変える |
| `SetOffset(offset)` | Actor からのずれを変える |
| `GetLayer()` | 層の番号 |

### Audio Source

| 関数 | 意味 |
|---|---|
| `Play()` | 最初から鳴らす（鳴っていれば、止めてから） |
| `Stop()` | 止める |

Actor をオフにすると止まり、オンに戻すと（Play On Start なら）最初から鳴ります。

### Camera

| 関数 | 意味 |
|---|---|
| `SetFovY(fovY)` | 縦の視野角（ラジアン） |

カメラを動かすには、Actor の Transform を動かします。

### Particle System

| 関数 | 意味 |
|---|---|
| `SetActive(active)` | 出す・止める |
| `EmitTemporary(lifeTime)` | 今の位置にエミッターの写しを作り、`lifeTime` 秒だけ出す。このコンポーネントが消えても、最後まで出る |
| `GetEmitter()` | エミッター（`std::shared_ptr<Tako::GPUParticleEmitter>`）。プリセットが空か、読めなければ `nullptr` |

```cpp
    Tako::ParticleSystemComponent* particle = GetOwner()->GetComponent<Tako::ParticleSystemComponent>();
    if (!particle) {
        return;
    }
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
```

`GPUParticleEmitter`（`GPUParticleEmitter.h`）の、よく使う関数です。

| 関数 | 意味 |
|---|---|
| `SetActive(isActive)`、`SetEmitting(cond)` | エミッターを動かす・止める、出す・止める |
| `SetParticleCount(count)`、`SetFrequency(frequency)` | 1 回に出す数と、出す間隔 |
| `SetPosition(position)` | 位置 |
| `SetColor(color)`、`SetColors(startColor, endColor)` | 色 |
| `SetLifeTimeRange(range)`、`SetSpeedRange(range)` | 寿命と速さの範囲 |

`EmitterManager`（`EmitterManager.h`）の、よく使う関数です。名前は、Inspector の `Emitter Name` です。

| 関数 | 意味 |
|---|---|
| `HasEmitter(name)`、`GetEmitterByName(name)` | 名前のエミッターがあるか、取る |
| `SetEmitterActive(name, isActive)` | 出す・止める |
| `SetEmitterPosition(name, position)` | 動かす |
| `SetEmitterCount(name, count)` | 1 回に出す数 |
| `CreateTemporaryEmitterFrom(sourceName, newName, lifeTime)` | 写しを作って、`lifeTime` 秒だけ出す |

### Post Process

このコンポーネントには、スクリプトから呼ぶ関数はありません。スクリプトからエフェクトを足す・外すときは、`Tako::PostEffectManager::GetInstance()`（`PostEffectManager.h`）を使います。

| 関数 | 意味 |
|---|---|
| `AddEffect(name, param)` | エフェクトを足し、番号を返す（`"Bloom"` と `Tako::BloomParam` など。値の構造体は `PostEffectStruct.h`）。無い名前なら 0 |
| `RemoveEffect(id)` | `AddEffect` が返した番号のエフェクトを外す |

足したエフェクトは、シーンが始まるとき（シーンの切り替え・Play と Stop）と、Post Process がプロファイルを読むときに消えます。

### UI Layout

| 関数 | 意味 |
|---|---|
| `GetLayout()` | レイアウト（`Tako::UILayout&`）。`BeginPlay` からも使える |
| `SetVisible(visible)`、`IsVisible()` | レイアウト全体を出す・隠す |

```cpp
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
```

`UILayout`（`UILayout.h`）の、よく使う関数です。どれも、要素を名前で指します。見つからなければ、入れる関数は何もせず、読む関数は 0・`false`・空の文字列を返します。

| 関数 | 意味 |
|---|---|
| `SetVisible(name, visible)`、`IsVisible(name)` | 要素を出す・隠す（子も一緒に） |
| `SetText(name, text)`、`GetText(name)` | 文字の要素の文字列（UTF-8） |
| `SetColor(name, color)`、`SetAlpha(name, alpha)` | 色と、不透明度 |
| `SetPosition(name, position)`、`SetSize(name, size)` | 位置と大きさ（基準解像度の px） |
| `SetRotation(name, rotation)` | 回転（度） |
| `SetTexture(name, texture)` | 画像を差し替える |
| `GetScreenRectMin(name)`、`GetScreenRectSize(name)` | 画面上の矩形（クリックの判定などに使う） |
| `GetAnimator()` | 要素を動かす `Tako::AnimationPlayer` |

`AnimationPlayer`（`AnimationPlayer.h`）の関数です。Animation Editor で作ったクリップを再生します。

| 関数 | 意味 |
|---|---|
| `Play(clipName, target = "", options = {})` | クリップを最初から再生する。`target` は要素の名前。クリップが見つからなければ再生せず、Console に警告を出す |
| `Stop(clipName, target = "")`、`StopAll()` | 止める |
| `IsPlaying(clipName, target = "")` | 再生中なら `true` |

### Behavior Tree

| 関数 | 意味 |
|---|---|
| `GetTree()` | 実行中のツリー（`Tako::BehaviorTree&`） |
| `GetTreeName()` | ツリーの名前 |

```cpp
    Tako::BehaviorTreeComponent* behavior = GetOwner()->GetComponent<Tako::BehaviorTreeComponent>();
    if (!behavior) {
        return;
    }
    // ツリーのノードと値をやり取りする入れ物（Blackboard）に、値を入れる
    Tako::BTBlackboard* blackboard = behavior->GetTree().GetBlackboard();
    blackboard->SetFloat("attackRange", 3.0f);
    blackboard->SetVector3("home", GetOwner()->GetWorldPosition());
```

Blackboard には、`BeginPlay` から値を入れられます。キー `"owner"` には、この Actor が入っています。

`BehaviorTree`（`BehaviorTree.h`）と `BTBlackboard`（`BTBlackboard.h`）の、よく使う関数です。

| 関数 | 意味 |
|---|---|
| `Tick(deltaTime)` | ツリーを 1 フレーム実行する（Inspector の Auto Tick を外したとき） |
| `Reset()` | ツリー全体を最初に戻す |
| `GetBlackboard()` | Blackboard |
| `SetInt(key, value)`、`GetInt(key, defaultValue = 0)` | 整数 |
| `SetFloat(key, value)`、`GetFloat(key, defaultValue = 0)` | 小数 |
| `SetVector3(key, value)`、`GetVector3(key)` | ベクトル |
| `SetValue(key, value)`、`GetValue<T>(key)` | 好きな型の値。`GetValue` は、無ければ `std::nullopt` |
| `SetPtr(key, ptr)`、`GetPtr<T>(key)` | ポインタ。`GetPtr` は、無ければ `nullptr` |
| `HasKey(key)`、`RemoveKey(key)` | キーがあるか、消す |

### Script

普通は、Script コンポーネントを直接は触りません。`Actor::GetScript<T>()` で、中のスクリプトを取ります。

| 関数 | 意味 |
|---|---|
| `GetScript()` | 中のスクリプト（`Tako::Script*`）。無ければ `nullptr` |
| `GetScriptName()` | スクリプトの型名 |
| `SetScript(typeName)` | スクリプトを替える。同じキーの値は引き継ぐ |

## 16. よくある失敗

| 症状 | 原因 | 直し方 |
|---|---|---|
| スクリプトが `Add Component` の一覧に出ない | コンパイルが通っていない | Console のエラーを直して、もう一度コンパイルする（Ctrl+B） |
| | `TCLASS()` か `GENERATED_BODY()` が無い、`Tako::Script` を継承していない | [3. 決まり](#3-決まり)のとおりに書く |
| | ファイルが `.vcxproj` に入っていない | `New Script...` で作るか、Visual Studio で「既存の項目の追加」をする |
| Assets からスクリプトを開けない、Script の欄にドロップできない | ファイル名とクラス名が違う、`Script` フォルダーの外にある | 名前をそろえて、`GameProject\Script` の中に置く |
| ビルドで `TAKO001` のエラーが出る | `TCLASS`・`TPROPERTY` の書き方の間違い | エラーの文のとおりに直す（[3. 決まり](#3-決まり)の表） |
| Debug は通るのに、Release のビルドが通らない | `DrawCustomImGui` や `imgui.h` を `#ifdef TAKO_EDITOR` で囲んでいない | 宣言・定義・include を囲む。区切りのよい所で、Release もビルドして確かめる |
| Develop のエディタで、Inspector の追加の表示が出ない | `DrawCustomImGui` を `#ifdef _DEBUG` で囲んでいる（`_DEBUG` は Debug だけ） | `#ifdef TAKO_EDITOR` に替える |
| Develop のエディタでコンパイルすると、Console に `has no Develop\|x64 configuration` と出る | ゲームの `.vcxproj` に、構成 Develop が無い（2.1.4 より前に作ったプロジェクト） | エンジンのひな形（`Engine\TakoEngine\templates\Default`）の `.vcxproj` と `.sln` から、`Develop` の付いた所を写す（エンジンの `CHANGELOG.md` の 2.1.4 に書いてある） |
| `BeginPlay` で、他のスクリプトが用意するはずの物がまだ無い | 相手の `BeginPlay` が、自分より後に呼ばれている | 用意する側を `Awake` に移すか、`TCLASS(Order = n)` で順を決める |
| `BeginPlay` で全体をリセットしたら、当たらなくなった | Collider は `Awake` で登録される。`BeginPlay` で `CollisionManager::Reset()` を呼ぶと、登録が全部消える | シーンをまたぐ物を消すのは、`EndPlay` で行う |
| 当たり判定が呼ばれない | 片方にしか Collider が無い | 両方の Actor に Collider を付ける |
| | 層の組み合わせが、当たらない設定になっている | Engine Settings の Physics の `Collision Matrix` を見る |
| | Actor か Collider がオフ、または Edit 中 | Active を見る。当たり判定は Play 中だけ働く |
| `OnCollisionEnter` で落ちる | `other` が `nullptr`（相手が Actor に付いていないコライダー） | 最初に `if (!other) return;` を書く |
| `GetAxis`・`PushAction` で止まる（assert） | 作っていない名前を渡している | Engine Settings の Input で作った名前と、綴りをそろえる |
| 消したはずの Actor を触って落ちる | Actor のポインタを、メンバーに持ち続けている | `ActorRef` で持ち、使うたびに `Resolve` する |
| Play 中に変えた値が、Stop で戻る | 仕様（Stop は、Play の前の中身に戻す） | 残したい物は、コンポーネントの見出しの右クリックで `Keep Values on Stop` を選ぶ |
| コンパイルの後に、値が消える | `TPROPERTY` の無いメンバーと、ゲームのコードの `static` 変数・グローバル変数は、コンパイルのたびに作り直される | 残したい値に `TPROPERTY` を付ける |
| コンパイルの後に、エディタが落ちる | ゲームが `RegisterPostEffect`・`RenderPipeline::AddRenderPass`・`TransitionManager::SetCurrentEffect` でエンジンに登録した物が、古いコードを指したまま残っている | 登録した物は、`EndPlay` で外す |
| メンバー名を変えたら、Inspector の値が初期値に戻った | 保存のキーは、メンバー名から決まる | 名前を戻すか、`PostLoad` で古いキーを読み替える |
| クラス名を変えたら、Actor のスクリプトが見つからなくなった | Script コンポーネントは、スクリプトを型名で覚えている | 名前を戻すか、Script の欄で選び直す（保存してあった値は捨てられず、シーンファイルに残る） |
| Inspector の表示が点滅する | `DrawCustomImGui` の中で `ImGui::Begin` を呼んでいる | 別の窓を作らず、Inspector の中に描く |
| エンジンを新しくしたら、エディタが落ちる | 古い `Game.dll` が、新しいエンジンと合っていない | ゲームをビルドし直してから、エディタを開く |

## 付録 ヘッダーの場所

どれも、プロジェクトの `Engine\TakoEngine\project\engine` からのパスです。include するときは、ファイル名だけを書きます。

| ヘッダー | 中身 |
|---|---|
| `gameplay/actor/Script.h` | スクリプトの基底クラス |
| `gameplay/actor/Component.h` | 呼ばれる関数、タイマー、`Subscribe` |
| `gameplay/actor/Actor.h`、`ActorRef.h` | Actor と、Actor を指す値 |
| `gameplay/actor/ScriptComponent.h` | Script コンポーネント |
| `gameplay/scene/BaseScene.h` | シーン（`Spawn`・`FindActor`・`SendEvent`・ポーズ） |
| `gameplay/scene/SceneManager.h` | シーンの切り替え、シーンをまたぐ値 |
| `gameplay/scene/transition/TransitionManager.h` | シーンの切り替えの演出 |
| `gameplay/component/*.h` | エンジンのコンポーネント |
| `gameplay/collision/CollisionManager.h` | 当たり判定の層と、当たる組み合わせ |
| `gameplay/ai/BehaviorTree/Runtime/BehaviorTree.h`、`Core/BTBlackboard.h` | ビヘイビアツリーと Blackboard |
| `core/reflection/Reflection.h` | `TCLASS`・`TPROPERTY` などの印と、指定子の説明 |
| `core/reflection/TObject.h` | `PostEditChange`・`PostLoad`・`PostSave`・`DrawCustomImGui` |
| `core/reflection/GameSettings.h` | 調整値の基底クラス |
| `core/io/Input.h` | 入力 |
| `core/utility/Logger.h` | ログ |
| `core/math/Vector2.h`、`Vector3.h`、`Vector4.h`、`Transform.h` | ベクトルと Transform |
| `core/math/ColorFunc.h`、`Gradient.h` | 色（`Color`・`ColorHSV`・`Color32`）と、色の段階（`Gradient`） |
| `core/math/RandomEngine.h` | 乱数（`GetFloat(min, max)`・`GetInt(min, max)`・`GetBool(probability)`・`GetRandomDirectionXZ()`・`GetHueColor(s, v)` など） |
| `core/math/EaseFunc.h` | イージング |
| `core/time/FrameTimer.h` | 時間（`GetDeltaTime()`・`SetTimeScale(scale)`・`SetTimeScaleForDuration(scale, duration)`） |
| `graphics/2d/UILayout.h` | UI のレイアウト |
| `graphics/3d/Object/Object3d.h` | モデルの描画物 |
| `graphics/animation/AnimationPlayer.h` | Animation Editor のクリップの再生 |
| `graphics/effect/Particle/Emitter/EmitterManager.h`、`GPUParticleEmitter.h` | パーティクルのエミッター |
| `graphics/effect/PostEffect/PostEffectManager.h`、`PostEffectStruct.h` | ポストエフェクトを足す・外す、エフェクトの値（`BloomParam` など） |

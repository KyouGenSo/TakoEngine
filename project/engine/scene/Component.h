#pragma once
#include "TObject.h"

#include <string>

namespace Tako {

  class Actor;

  /// <summary>
  /// Actor に付ける機能の単位（UE の UActorComponent）。C++ のスクリプトは Script を継承し、ScriptComponent に入れる
  /// </summary>
  TCLASS()
  class Component : public TObject {
    GENERATED_BODY()

  public: //メンバー関数
    ~Component() override = default;

    /// <summary>
    /// BeginPlay の前に 1 回呼ばれる。同時にシーンへ入る全 Actor の Awake が済んでから、BeginPlay が始まる。
    /// 他の BeginPlay から名前や参照で引かれる物は、ここで用意する
    /// </summary>
    virtual void Awake() {}

    /// <summary>
    /// Play が始まったとき、または Play 中に足されたときに 1 回呼ばれる。
    /// 呼ぶ順は TCLASS(Order = n) の小さい順（同じなら Hierarchy の並び、同じ Actor の中ではコンポーネントの並び）。Awake / Tick / LateTick も同じ
    /// </summary>
    virtual void BeginPlay() {}

    /// <summary>
    /// Play 中だけ毎フレーム呼ばれる。Tick の中で足されたコンポーネントは、次のフレームから呼ばれる
    /// </summary>
    virtual void Tick([[maybe_unused]] float deltaTime) {}

    /// <summary>
    /// 全 Actor の Tick の後、シーンの Update（当たり判定）の前に呼ばれる。他の Actor が動いた後の位置を使う処理を書く
    /// </summary>
    virtual void LateTick([[maybe_unused]] float deltaTime) {}

    /// <summary>
    /// true を返すと、シーンのポーズ中（BaseScene::SetPaused）も Tick と LateTick が呼ばれる。ポーズのメニューなどに使う
    /// </summary>
    virtual bool TickWhilePaused() const { return false; }

    /// <summary>
    /// Play が終わるとき、または外される・消されるときに呼ばれる（BeginPlay を呼んだものだけ）
    /// </summary>
    virtual void EndPlay() {}

    /// <summary>
    /// 毎フレーム（Edit 中・一時停止中も）呼ばれる。描画物の登録を書く
    /// </summary>
    virtual void Draw() {}

    /// <summary>
    /// Actor の Active（自分か親のどれか）が切り替わった次のフレームの頭に呼ばれる（Edit 中も）。
    /// オフの間は Tick / LateTick / Draw が呼ばれないので、それ以外で動いている物（当たり判定・音・エミッター）をここで止める
    /// </summary>
    virtual void OnActiveChanged([[maybe_unused]] bool active) {}

    /// <summary>
    /// 同じ Actor の他のコンポーネントが、これから外される（Edit 中も）。そのコンポーネントの物を指しているなら、ここで離す
    /// </summary>
    virtual void OnSiblingRemoved([[maybe_unused]] Component* removed) {}

    /// <summary>
    /// 同じ Actor の ColliderComponent が当たり始めた。other は相手の Actor（Actor でないコライダーなら nullptr）
    /// </summary>
    virtual void OnCollisionEnter([[maybe_unused]] Actor* other) {}

    virtual void OnCollisionStay([[maybe_unused]] Actor* other) {}

    virtual void OnCollisionExit([[maybe_unused]] Actor* other) {}

    //=====================================
    //Getter
    //=====================================
    Actor* GetOwner() const { return owner_; }

    const std::string& GetName() const { return name_; }

  protected: //メンバー関数
    // 直接は作らない（Add Component の一覧にも出ない）
    Component() = default;

  private: //メンバー変数
    friend class Actor;
    friend class ScriptComponent;

    Actor*      owner_ = nullptr;
    std::string name_;             ///< Actor の中で一意。保存ファイルのキー
  };

} // namespace Tako

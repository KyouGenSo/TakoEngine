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
    /// Play が始まったとき、または Play 中に足されたときに 1 回呼ばれる
    /// </summary>
    virtual void BeginPlay() {}

    /// <summary>
    /// Play 中だけ毎フレーム呼ばれる
    /// </summary>
    virtual void Tick([[maybe_unused]] float deltaTime) {}

    /// <summary>
    /// Play が終わるとき、または外される・消されるときに呼ばれる（BeginPlay を呼んだものだけ）
    /// </summary>
    virtual void EndPlay() {}

    /// <summary>
    /// 毎フレーム（Edit 中・一時停止中も）呼ばれる。描画物の登録を書く
    /// </summary>
    virtual void Draw() {}

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
    std::string name_;            ///< Actor の中で一意。保存ファイルのキー
  };

} // namespace Tako

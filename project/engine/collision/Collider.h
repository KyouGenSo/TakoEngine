#pragma once
#include "Vector3.h"
#include "Transform.h"

#include <cstdint>
#include <type_traits>

namespace Tako {

  /// <summary>
  /// 衝突判定用の基底クラス。全てのコライダーはこのクラスを継承する
  /// </summary>
  class Collider {
  public: //メンバー関数
    /// <summary>
    /// デストラクタ（仮想デストラクタ）
    /// </summary>
    virtual ~Collider() = default;

    /// <summary>
    /// 衝突開始時のコールバック（他のコライダーと衝突を開始した最初のフレームで呼ばれる）
    /// </summary>
    /// <param name="other">衝突相手のコライダー</param>
    virtual void OnCollisionEnter([[maybe_unused]] Collider* other) {}

    /// <summary>
    /// 衝突継続中のコールバック（衝突が継続している間、毎フレーム呼ばれる）
    /// </summary>
    /// <param name="other">衝突相手のコライダー</param>
    virtual void OnCollisionStay([[maybe_unused]] Collider* other) {}

    /// <summary>
    /// 衝突終了時のコールバック（他のコライダーとの衝突が終了したフレームで呼ばれる）
    /// </summary>
    /// <param name="other">衝突相手のコライダー</param>
    virtual void OnCollisionExit([[maybe_unused]] Collider* other) {}

    //=====================================
    //Setter
    //=====================================
    void SetTransform(Transform* transform) { transform_ = transform; }
    void SetActive(bool active) { isActive_ = active; }
    void SetTypeID(uint32_t id) { typeID_ = id; }
    void SetOwner(void* owner) { owner_ = owner; }

    /// <summary>
    /// 層の enum をキャストせずに渡せる版
    /// </summary>
    template<class E> requires std::is_enum_v<E>
    void SetTypeID(E id) { typeID_ = static_cast<uint32_t>(id); }

    //=====================================
    //Getter
    //=====================================
    /// <summary>
    /// コライダーの中心座標を取得する（純粋仮想関数。ワールド空間）
    /// </summary>
    /// <returns>コライダーの中心座標（ワールド空間）</returns>
    virtual Vector3 GetCenter() const = 0;

    Transform* GetTransform() const { return transform_; }
    bool IsActive() const { return isActive_; }
    uint32_t GetTypeID() const { return typeID_; }
    void* GetOwner() const { return owner_; }

    /// <summary>
    /// 型 ID が層の enum の id と一致するか
    /// </summary>
    template<class E> requires std::is_enum_v<E>
    bool IsType(E id) const { return typeID_ == static_cast<uint32_t>(id); }

  protected: //メンバー変数
    //基本状態
    Transform* transform_ = nullptr;  ///< 対象オブジェクトの Transform（位置・回転・スケール情報）
    uint32_t   typeID_    = 0;        ///< コライダーの型 ID（CollisionManager の層名の添字）
    bool       isActive_  = true;     ///< コライダーの有効/無効状態（false の場合は衝突判定を行わない）
    void*      owner_     = nullptr;  ///< このコライダーを所有するオブジェクトへのポインタ
  };

} // namespace Tako
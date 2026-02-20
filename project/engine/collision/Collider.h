#pragma once
#include "Vector3.h"
#include "Transform.h"

namespace Tako {

  /// <summary>
  /// 衝突判定用の基底クラス。全てのコライダーはこのクラスを継承する
  /// </summary>
  class Collider {
  protected:
    Transform* transform_ = nullptr;  ///< 対象オブジェクトの Transform（位置・回転・スケール情報）
    uint32_t typeID_ = 0;             ///< コライダーの型 ID（CollisionTypeIdDef 参照）
    bool isActive_ = true;            ///< コライダーの有効/無効状態（false の場合は衝突判定を行わない）
    void* owner_ = nullptr;           ///< このコライダーを所有するオブジェクトへのポインタ

  public:
    /// <summary>
    /// デストラクタ（仮想デストラクタ）
    /// </summary>
    virtual ~Collider() = default;

    /// <summary>
    /// コライダーの中心座標を取得する（純粋仮想関数）
    /// </summary>
    /// <returns>コライダーの中心座標（ワールド空間）</returns>
    virtual Vector3 GetCenter() const = 0;

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

    /// <summary>
    /// Transform を設定する
    /// </summary>
    /// <param name="transform">設定する Transform オブジェクトへのポインタ</param>
    void SetTransform(Transform* transform) { transform_ = transform; }

    /// <summary>
    /// Transform を取得する
    /// </summary>
    /// <returns>現在設定されている Transform へのポインタ</returns>
    Transform* GetTransform() const { return transform_; }

    /// <summary>
    /// コライダーの有効/無効を設定する
    /// </summary>
    /// <param name="active">有効にする場合は true、無効にする場合は false</param>
    void SetActive(bool active) { isActive_ = active; }

    /// <summary>
    /// コライダーが有効かどうかを取得する
    /// </summary>
    /// <returns>有効な場合は true、無効な場合は false</returns>
    bool IsActive() const { return isActive_; }

    /// <summary>
    /// コライダーの型 ID を取得する
    /// </summary>
    /// <returns>型 ID（CollisionTypeIdDef で定義された値）</returns>
    uint32_t GetTypeID() const { return typeID_; }

    /// <summary>
    /// コライダーの型 ID を設定する
    /// </summary>
    /// <param name="id">設定する型 ID（CollisionTypeIdDef で定義された値）</param>
    void SetTypeID(uint32_t id) { typeID_ = id; }

    /// <summary>
    /// このコライダーを所有するオブジェクトを設定する
    /// </summary>
    /// <param name="owner">所有者オブジェクトへのポインタ</param>
    void SetOwner(void* owner) { owner_ = owner; }

    /// <summary>
    /// このコライダーを所有するオブジェクトを取得する
    /// </summary>
    /// <returns>所有者オブジェクトへのポインタ</returns>
    void* GetOwner() const { return owner_; }
  };

} // namespace Tako
#pragma once
#include "ActorRef.h"
#include "Component.h"
#include "Matrix4x4.h"
#include "TObject.h"
#include "Transform.h"

#include <json.hpp>

#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace Tako {

  class BaseScene;
  class Script;

  /// <summary>
  /// シーンに置く物（UE の AActor）。見た目や動きはコンポーネントで足す。継承して独自の Actor も作れる
  /// </summary>
  TCLASS()
  class Actor : public TObject {
    GENERATED_BODY()

  public: //メンバー関数
    Actor();
    ~Actor() override;

    /// <summary>
    /// T を作って足す。name が空なら型名（重なれば連番を付ける）
    /// </summary>
    template<class T>
    T* AddComponent(std::string name = {})
    {
      auto component = std::make_unique<T>();
      T*   raw       = component.get();
      AddComponent(std::move(component), std::move(name));
      return raw;
    }

    /// <summary>
    /// Play 中なら BeginPlay も呼ぶ
    /// </summary>
    Component* AddComponent(std::unique_ptr<Component> component, std::string name);

    /// <summary>
    /// 次のフレームの頭で外す（Tick や当たりの途中でも呼べる）
    /// </summary>
    void RemoveComponent(Component* component);

    /// <summary>
    /// 次のフレームの頭でシーンから消す
    /// </summary>
    void Destroy();

    void BeginPlay();

    /// <param name="isPaused">true なら TickWhilePaused のコンポーネントだけ呼ぶ</param>
    void Tick(float deltaTime, bool isPaused = false);

    void EndPlay();

    /// <summary>
    /// 全 Actor の Tick の後に呼ばれる
    /// </summary>
    void LateTick(float deltaTime, bool isPaused = false);

    void Draw();

    /// <summary>
    /// 親を替える（nullptr で外す）。自分の子孫を親にはできない
    /// </summary>
    /// <param name="keepWorld">true なら見た目の位置・向き・大きさを保つよう Transform を計算し直す</param>
    /// <returns>替えられなければ false</returns>
    bool SetParent(Actor* parent, bool keepWorld = true);

    /// <summary>
    /// 型・ID・TPROPERTY・コンポーネントを JSON にする（シーンファイルとプレハブの 1 件）
    /// </summary>
    nlohmann::json Save();

    /// <summary>
    /// Save の内容で TPROPERTY とコンポーネントを作り直す（ID は json にあるときだけ変える）
    /// </summary>
    void Load(const nlohmann::json& json);

    //=====================================
    //Setter
    //=====================================
    void SetName(const std::string& name) { name_ = name; }

    void SetTransform(const Transform& transform) { transform_ = transform; }

    void SetPrefab(const std::string& prefab) { prefab_ = prefab; }

    /// <summary>
    /// ワールドでの位置がこの値になるよう、Transform の translate を決める
    /// </summary>
    void SetWorldPosition(const Vector3& position);

    //=====================================
    //Getter
    //=====================================
    ActorId GetId() const { return id_; }

    const std::string& GetName() const { return name_; }

    Transform& GetTransform() { return transform_; }

    const Transform& GetTransform() const { return transform_; }

    /// <summary>
    /// Transform（親からの相対）に親のワールド行列を掛けたもの
    /// </summary>
    Matrix4x4 GetWorldMatrix() const;

    /// <summary>
    /// ワールドでの位置・回転・大きさ。親が無ければ Transform と同じ
    /// </summary>
    Transform GetWorldTransform() const;

    Vector3 GetWorldPosition() const;

    Actor* GetParent() const { return parent_; }

    /// <summary>
    /// 直接の子（シーンの並び順）
    /// </summary>
    const std::vector<Actor*>& GetChildren() const { return children_; }

    /// <summary>
    /// 直接の子を名前で探す。無ければ nullptr
    /// </summary>
    Actor* FindChild(std::string_view name) const;

    BaseScene* GetScene() const { return scene_; }

    /// <summary>
    /// 元にしたプレハブの名前（無ければ空）
    /// </summary>
    const std::string& GetPrefab() const { return prefab_; }

    bool HasBegunPlay() const { return hasBegunPlay_; }

    const std::vector<std::unique_ptr<Component>>& GetComponents() const { return components_; }

    /// <summary>
    /// T か T の派生の最初のコンポーネント。無ければ nullptr
    /// </summary>
    template<class T>
    T* GetComponent() const
    {
      for (const std::unique_ptr<Component>& component : components_) {
        if (component->IsA<T>()) { return static_cast<T*>(component.get()); }
      }
      return nullptr;
    }

    Component* FindComponent(const std::string& name) const;

    /// <summary>
    /// ScriptComponent に入っている T か T の派生のスクリプト。無ければ nullptr
    /// </summary>
    template<class T>
    T* GetScript() const { return static_cast<T*>(FindScript(T::StaticType())); }

    /// <summary>
    /// ref が指す同じシーンの Actor。指していないか、もう無ければ nullptr
    /// </summary>
    Actor* Resolve(const ActorRef& ref) const;

  private: //非公開関数
    friend class BaseScene;

    /// <summary>
    /// RemoveComponent で予約したものを外す
    /// </summary>
    void FlushPending();

    /// <summary>
    /// name が空なら型名にし、他のコンポーネントと重ならない名前にする
    /// </summary>
    std::string MakeUniqueName(std::string name, const Component& component) const;

    Script* FindScript(const TypeInfo& type) const;

  private: //メンバー変数
    TPROPERTY(Edit)
    std::string name_ = "Actor";
    TPROPERTY(Edit)
    Transform transform_{ { 1.0f, 1.0f, 1.0f }, {}, {} };

    ActorId                                 id_           = 0;        ///< シーンの中で一意。0 は未割り当て
    ActorId                                 parentId_     = 0;        ///< 保存する親。0 は親なし
    Actor*                                  parent_       = nullptr;  ///< parentId_ の Actor。BaseScene が張る
    std::vector<Actor*>                     children_;
    std::string                             prefab_;
    BaseScene*                              scene_        = nullptr;
    std::vector<std::unique_ptr<Component>> components_;
    std::vector<Component*>                 pendingRemoves_;
    bool                                    hasBegunPlay_ = false;
  };

} // namespace Tako

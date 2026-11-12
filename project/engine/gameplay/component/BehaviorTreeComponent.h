#pragma once
#include "Component.h"

#include <memory>
#include <string>
#include <vector>

namespace Tako {

  class BehaviorTree;

  /// <summary>
  /// Actor をビヘイビアツリーで動かす。Play が始まるとツリーのファイル（resources/Json/BT/名前.json）を読む。
  /// Blackboard の "owner"（kBTOwnerKey）にこの Actor が入る。Blackboard は作った時からあるので、スクリプトは BeginPlay で他の値を入れられる
  /// </summary>
  TCLASS(DisplayName = "Behavior Tree")
  class TAKO_API BehaviorTreeComponent : public Component {
    GENERATED_BODY()

  public: //メンバー関数
    BehaviorTreeComponent();
    ~BehaviorTreeComponent() override;

    /// <summary>
    /// ツリーを読む。スクリプトが BeginPlay でツリーを使えるよう、BeginPlay より前に行う
    /// </summary>
    void Awake() override;

    void Tick(float deltaTime) override;

    void EndPlay() override;

    //=====================================
    //Getter
    //=====================================
    /// <summary>
    /// Play 中の全コンポーネント（BT エディタが、同じツリーを動かしている物を探すのに使う）
    /// </summary>
    static const std::vector<BehaviorTreeComponent*>& GetPlaying();

    BehaviorTree& GetTree() const { return *runtime_; }

    const std::string& GetTreeName() const { return tree_; }

  private: //メンバー変数
    TPROPERTY(Edit, Asset = "BehaviorTree")
    std::string tree_;
    TPROPERTY(Edit, Tooltip = "毎フレーム自動で実行する。スクリプトが GetTree().Tick を呼ぶなら外す")
    bool        autoTick_ = true;

    std::unique_ptr<BehaviorTree> runtime_;
  };

} // namespace Tako

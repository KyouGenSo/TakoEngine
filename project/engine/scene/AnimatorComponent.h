#pragma once
#include "Component.h"

#include <string>

namespace Tako {

  class Model;

  /// <summary>
  /// 同じ Actor の MeshComponent のモデルを動かす。AnimationClip（Animation Editor で作るもの）は Play 中に再生し、
  /// モデルに入っているスケルタルアニメーションは Edit 中も名前で切り替える
  /// </summary>
  TCLASS()
  class AnimatorComponent : public Component {
    GENERATED_BODY()

  public: //メンバー関数
    void BeginPlay() override;

    void EndPlay() override;

    /// <summary>
    /// スケルタルアニメーションの名前かモデルが変わっていれば、モデルに渡す
    /// </summary>
    void Draw() override;

  private: //メンバー変数
    TPROPERTY(Edit, Tooltip = "ループするかはクリップの設定", Asset = "AnimationClip")
    std::string clip_;
    TPROPERTY(Edit)
    bool        playOnStart_       = true;
    TPROPERTY(Edit, ClampMin = 0, Speed = 0.01)
    float       speed_             = 1.0f;
    TPROPERTY(Edit, Tooltip = "モデル（glTF）に入っているアニメーションの名前。空なら変えない")
    std::string skeletalAnimation_;

    const Model* appliedModel_    = nullptr;
    std::string  appliedSkeletal_;
  };

} // namespace Tako

#pragma once
#include "Component.h"

#include <string>

namespace Tako {

  /// <summary>
  /// シーンのポストエフェクトをプロファイルで決める（UE の PostProcessVolume の、範囲を持たない形）
  /// </summary>
  TCLASS()
  class PostProcessComponent : public Component {
    GENERATED_BODY()

  public: //メンバー関数
    /// <summary>
    /// PostEffectManager が最後に読んだプロファイルと違うときだけ読む（作り直されたコンポーネントが、未保存の編集を上書きしないため）
    /// </summary>
    void Draw() override;

  private: //メンバー変数
    TPROPERTY(Edit, Asset = "PostEffectProfile")
    std::string profile_;
  };

} // namespace Tako

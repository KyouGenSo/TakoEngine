#pragma once
#include "Component.h"

#include <memory>
#include <string>

namespace Tako {

  class SkyBox;

  /// <summary>
  /// キューブマップの空を描く。テクスチャの差し替えは描画の直前に行う
  /// </summary>
  TCLASS(DisplayName = "Sky Box")
  class SkyBoxComponent : public Component {
    GENERATED_BODY()

  public: //メンバー関数
    SkyBoxComponent();
    ~SkyBoxComponent() override;

    void Draw() override;

  private: //メンバー変数
    TPROPERTY(Edit, Asset = "Cubemap")
    std::string texture_;

    std::unique_ptr<SkyBox> skyBox_;
    std::string             appliedTexture_;
    bool                    hasTexture_     = false;
  };

} // namespace Tako

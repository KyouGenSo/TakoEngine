#pragma once
#include "Component.h"
#include "Transform.h"

#include <memory>
#include <string>

namespace Tako {

  class Object3d;

  /// <summary>
  /// Actor の位置にモデルを描く。モデルの読み替えは描画の直前に行う（GPU が使用中のモデルを消さないため）
  /// </summary>
  TCLASS()
  class MeshComponent : public Component {
    GENERATED_BODY()

  public: //メンバー関数
    MeshComponent();
    ~MeshComponent() override;

    void Draw() override;

    //=====================================
    //Setter
    //=====================================
    void SetModel(const std::string& model) { model_ = model; }

    //=====================================
    //Getter
    //=====================================
    Object3d* GetObject3d() const { return object_.get(); }

  private: //メンバー変数
    TPROPERTY(Edit, Asset = "Model")
    std::string model_;
    TPROPERTY(Edit, Tooltip = "Object Settings の名前（空なら使わない）")
    std::string settingsName_;
    TPROPERTY(Edit, DisplayName = "Offset")
    Transform offset_{ { 1.0f, 1.0f, 1.0f }, {}, {} };

    std::unique_ptr<Object3d> object_;
    std::string               loadedModel_;
    std::string               appliedSettingsName_;
  };

} // namespace Tako

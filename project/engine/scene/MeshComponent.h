#pragma once
#include "Component.h"
#include "ObjectSettings.h"
#include "Transform.h"

#include <memory>
#include <string>

namespace Tako {

  class Object3d;

  /// <summary>
  /// Actor の位置にモデルを描く（Unity の MeshFilter + MeshRenderer）。マテリアルと描画フラグは自分で持ち、シーンとプレハブに保存する。
  /// モデルの読み替えとマテリアルの解決は描画の直前に行う（GPU が使用中の物を消さないため）
  /// </summary>
  TCLASS()
  class MeshComponent : public Component {
    GENERATED_BODY()

  public: //メンバー関数
    MeshComponent();
    ~MeshComponent() override;

    void Draw() override;

    /// <summary>
    /// renderSettings（ObjectSettings.json の 1 件と同じ形）を読む
    /// </summary>
    void PostLoad(const nlohmann::json& raw) override;

    /// <summary>
    /// 描画設定があれば renderSettings として out に足す
    /// </summary>
    void PostSave(nlohmann::json& out) override;

#ifdef _DEBUG
    /// <summary>
    /// Rendering の欄（Object Settings ウィンドウの 1 件分と同じ）
    /// </summary>
    bool DrawCustomImGui() override;
#endif

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
    TPROPERTY(Edit, DisplayName = "Offset")
    Transform   offset_ = { { 1.0f, 1.0f, 1.0f }, {}, {} };

    std::unique_ptr<Object3d> object_;
    std::string               loadedModel_;
    ObjectOverrides           overrides_;              ///< object_ が SetOverrides で指す
    bool                      overridesDirty_ = true;  ///< 次の Draw でマテリアルを解決し直す
  };

} // namespace Tako

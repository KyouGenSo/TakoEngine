#include "MeshComponent.h"
#include "Actor.h"
#include "Logger.h"
#include "Model.h"
#include "Object3d.h"

#ifdef _DEBUG
#include "ObjectSettingsEditor.h"
#include "imgui.h"
#endif

namespace Tako {

  MeshComponent::MeshComponent()
    : object_(std::make_unique<Object3d>())
  {
    object_->Initialize();
  }

  MeshComponent::~MeshComponent() = default;

  void MeshComponent::Draw()
  {
    EnsureLoaded();
    if (overridesDirty_) {
      overrides_.ResolveMaterials();
      object_->SetOverrides(&overrides_);
      overridesDirty_ = false;
    }
    if (!object_->GetModel() || !visible_) {
      return;
    }
    object_->SetTransform(offset_);
    object_->SetParentMatrix(GetOwner()->GetWorldMatrix());
    object_->Update();
    object_->Draw();
  }

  void MeshComponent::SetGeneratedModel(std::unique_ptr<Model> model)
  {
    object_->SetModel(std::move(model));
    // Model の欄（空）は読み込み済みの扱いにして、EnsureLoaded に消させない
    loadedModel_ = model_;
  }

  Object3d* MeshComponent::GetObject3d()
  {
    EnsureLoaded();
    return object_.get();
  }

  void MeshComponent::EnsureLoaded()
  {
    if (model_ == loadedModel_) {
      return;
    }
    // 無いファイルはモデル無しになる（記録は ModelManager が 1 回だけ出す）
    if (model_.empty()) {
      object_->SetModel(std::unique_ptr<Model>());
    } else {
      object_->SetModel(model_);
    }
    loadedModel_ = model_;
  }

  void MeshComponent::PostLoad(const nlohmann::json& raw)
  {
    try {
      overrides_ = ObjectOverrides::Load(raw.value("renderSettings", nlohmann::json::object()));
    } catch (const nlohmann::json::exception& e) {
      Logger::Log("MeshComponent: renderSettings could not be read: %s", e.what());
      overrides_ = {};
    }
    overridesDirty_ = true;
  }

  void MeshComponent::PostSave(nlohmann::json& out)
  {
    nlohmann::json settings = overrides_.Save();
    if (!settings.empty()) {
      out["renderSettings"] = std::move(settings);
    }
  }

#ifdef _DEBUG
  bool MeshComponent::DrawCustomImGui()
  {
    if (!ImGui::TreeNode("Rendering")) {
      return false;
    }
    const bool changed = DrawObjectOverrides(overrides_, object_.get());
    ImGui::TreePop();
    overridesDirty_ |= changed;
    return changed;
  }
#endif

} // namespace Tako

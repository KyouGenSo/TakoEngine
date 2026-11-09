#include "MeshComponent.h"
#include "Actor.h"
#include "EnginePaths.h"
#include "Model.h"
#include "ModelBasic.h"
#include "ModelManager.h"
#include "Object3d.h"

#include <filesystem>

namespace Tako {

  namespace {

    /// <summary>
    /// 入力の途中の名前を読みに行かないよう、ファイルがあるときだけ true
    /// </summary>
    bool ModelFileExists(const std::string& model)
    {
      if (model.starts_with(EnginePaths::kEngineModels)) {
        return std::filesystem::is_regular_file(model);
      }
      ModelBasic* basic = ModelManager::GetInstance()->GetModelBasic();
      return std::filesystem::is_regular_file(basic->GetDirectoryFolderName() + "/" + basic->GetModelFolderName() + "/" + model);
    }

  } // namespace

  MeshComponent::MeshComponent()
    : object_(std::make_unique<Object3d>())
  {
    object_->Initialize();
  }

  MeshComponent::~MeshComponent() = default;

  void MeshComponent::Draw()
  {
    if (model_ != loadedModel_) {
      if (model_.empty()) {
        object_->SetModel(std::unique_ptr<Model>());
        loadedModel_.clear();
      } else if (ModelFileExists(model_)) {
        object_->SetModel(model_);
        loadedModel_ = model_;
      }
    }
    if (settingsName_ != appliedSettingsName_) {
      object_->SetName(settingsName_);
      appliedSettingsName_ = settingsName_;
    }
    if (loadedModel_.empty()) {
      return;
    }
    object_->SetTransform(offset_);
    object_->SetParentMatrix(GetOwner()->GetWorldMatrix());
    object_->Update();
    object_->Draw();
  }

} // namespace Tako

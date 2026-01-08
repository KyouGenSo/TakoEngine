#include"ModelManager.h"

#include"DX12Basic.h"
#include <ranges>

#ifdef _DEBUG
#include "DebugUIManager.h"
#endif

namespace Tako {

  std::unique_ptr<ModelManager> ModelManager::instance_ = nullptr;

  ModelManager* ModelManager::GetInstance()
  {
    if (!instance_) {
      instance_ = std::unique_ptr<ModelManager>(new ModelManager());
    }
    return instance_.get();
  }

  void ModelManager::Initialize(DX12Basic* dx12)
  {
    pModelBasic_ = std::make_unique<ModelBasic>();
    pModelBasic_->Initialize(dx12);
  }

  void ModelManager::Finalize()
  {
    pModelBasic_.reset();

    // 各モデルインスタンスに対してFinalize呼び出し
    for (auto& model : models_) {
      if (model.second) {
        model.second->Finalize();
      }
    }
    // モデルインスタンスの解放
    models_.clear();

    instance_.reset();
  }

  void ModelManager::LoadModel(const std::string& fileName)
  {
    // すでにロード済みのファイル名をチェック

    if (models_.contains(fileName)) {
#ifdef _DEBUG
      DebugUIManager::GetInstance()->AddLog(
        "ModelManager: Model already loaded: " + fileName,
        DebugUIManager::LogType::Info);
#endif

      return;
    }


    // 新しいModelインスタンスを作成
    std::unique_ptr<Model> newModel = std::make_unique<Model>();
    newModel->Initialize(pModelBasic_.get(), fileName);

    models_.insert(std::make_pair(fileName, std::move(newModel)));
  }


  Model* ModelManager::GetModel(const std::string& fileName)
  {
    if (models_.contains(fileName)) {
      // モデルがすでに存在する場合はそのポインタを返す
      return models_.at(fileName).get()->Clone();
    }

    // 新しいModelインスタンスを作成
    std::unique_ptr<Model> newModel = std::make_unique<Model>();
    newModel->Initialize(pModelBasic_.get(), fileName);

    // モデルデータの登録
    Model* modelPtr = newModel.get();
    models_.insert(std::make_pair(fileName, std::move(newModel)));

    return modelPtr->Clone();
  }

} // namespace Tako

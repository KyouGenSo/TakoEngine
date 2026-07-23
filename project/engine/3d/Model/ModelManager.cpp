#include"ModelManager.h"

#include"DX12Basic.h"
#include "EnginePaths.h"
#include <ranges>
#include <algorithm>

#ifdef _DEBUG
#include "DebugUIManager.h"
#endif

namespace Tako {

  std::unique_ptr<ModelManager> ModelManager::instance_ = nullptr;

  ModelManager* ModelManager::GetInstance()
  {
    if (!instance_) {
      instance_ = std::make_unique<ModelManager>(Token{});
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

    // 各モデルインスタンスに対して Finalize 呼び出し
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
    // fileName が "EngineResources/Model/" で始まる場合はエンジン用ロードに委譲
    if (fileName.starts_with(EnginePaths::kEngineModels)) {
      LoadEngineModel(fileName.substr(EnginePaths::kEngineModels.size()));
      return;
    }

    // すでにロード済みのファイル名をチェック
    if (models_.contains(fileName)) {
#ifdef _DEBUG
      DebugUIManager::GetInstance()->AddLog(
        "ModelManager: Model already loaded: " + fileName,
        DebugUIManager::LogType::Info);
#endif

      return;
    }


    // 新しい Model インスタンスを作成
    std::unique_ptr<Model> newModel = std::make_unique<Model>();
    newModel->Initialize(pModelBasic_.get(), fileName);

    models_.insert(std::make_pair(fileName, std::move(newModel)));
  }


  std::unique_ptr<Model> ModelManager::GetModel(const std::string& fileName)
  {
    // fileName が "EngineResources/Model/" で始まる場合はエンジン用取得に委譲
    if (fileName.starts_with(EnginePaths::kEngineModels)) {
      return GetEngineModel(fileName.substr(EnginePaths::kEngineModels.size()));
    }

    if (models_.contains(fileName)) {
      // モデルがすでに存在する場合はクローンを返す
      return models_.at(fileName)->Clone();
    }

    // 新しい Model インスタンスを作成
    auto newModel = std::make_unique<Model>();
    newModel->Initialize(pModelBasic_.get(), fileName);

    // モデルデータの登録
    Model* modelPtr = newModel.get();
    models_.insert(std::make_pair(fileName, std::move(newModel)));

    return modelPtr->Clone();
  }

  std::vector<std::string> ModelManager::GetLoadedModelNames() const
  {
    // 読み込み済みモデルのキー(ファイル名)を列挙してソートして返す。
    std::vector<std::string> names;
    names.reserve(models_.size());
    for (const auto& [name, _] : models_) {
      names.push_back(name);
    }
    std::sort(names.begin(), names.end());
    return names;
  }

  void ModelManager::LoadEngineModel(const std::string& fileName)
  {
    // エンジン用キーはプレフィックス付きでゲーム用と分離
    const std::string key = std::string(EnginePaths::kEngineModels) + fileName;

    if (models_.contains(key)) {
#ifdef _DEBUG
      DebugUIManager::GetInstance()->AddLog(
        "ModelManager: Engine model already loaded: " + fileName,
        DebugUIManager::LogType::Info);
#endif
      return;
    }

    // ModelBasic のディレクトリを一時的にエンジン用に切り替え
    const std::string savedDir = pModelBasic_->GetDirectoryFolderName();
    const std::string savedModelDir = pModelBasic_->GetModelFolderName();
    // "EngineResources/Model" に分割
    pModelBasic_->SetDirectoryFolderName("EngineResources");
    pModelBasic_->SetModelFolderName("Model");

    auto newModel = std::make_unique<Model>();
    newModel->Initialize(pModelBasic_.get(), fileName);

    // 元のディレクトリ設定に戻す
    pModelBasic_->SetDirectoryFolderName(savedDir);
    pModelBasic_->SetModelFolderName(savedModelDir);

    models_.insert(std::make_pair(key, std::move(newModel)));
  }

  std::unique_ptr<Model> ModelManager::GetEngineModel(const std::string& fileName)
  {
    const std::string key = std::string(EnginePaths::kEngineModels) + fileName;

    if (!models_.contains(key)) {
      LoadEngineModel(fileName);
    }

    return models_.at(key)->Clone();
  }

} // namespace Tako

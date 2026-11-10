#include"ModelManager.h"

#include"DX12Basic.h"
#include "EnginePaths.h"
#include "Logger.h"
#include <ranges>
#include <algorithm>
#include <filesystem>

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

    if (!CanLoad(fileName, pModelBasic_->GetDirectoryFolderName() + "/" + pModelBasic_->GetModelFolderName() + "/" + fileName)) {
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

    if (!models_.contains(fileName)) {
      LoadModel(fileName);
    }
    const auto it = models_.find(fileName);
    return it != models_.end() ? it->second->Clone() : nullptr;
  }

  bool ModelManager::CanLoad(const std::string& key, const std::string& path)
  {
    if (std::filesystem::exists(path)) {
      return true;
    }
    // 毎フレーム引かれても記録は 1 回
    if (failedFiles_.insert(key).second) {
      Logger::Log("Model not found: %s", path.c_str());
    }
    return false;
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

    if (!CanLoad(key, key)) {
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
    const auto it = models_.find(key);
    return it != models_.end() ? it->second->Clone() : nullptr;
  }

} // namespace Tako

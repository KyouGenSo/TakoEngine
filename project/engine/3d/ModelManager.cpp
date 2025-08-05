#include"ModelManager.h"

#include <ranges>

#include"Model.h"
#include"DX12Basic.h"
#include "Logger.h"

ModelManager* ModelManager::instance_ = nullptr;

ModelManager* ModelManager::GetInstance()
{
	if (instance_ == nullptr)
	{
		instance_ = new ModelManager();
	}
	return instance_;
}

void ModelManager::Initialize(DX12Basic* dx12)
{
	pModelBasic_ = new ModelBasic();
	pModelBasic_->Initialize(dx12);
}

void ModelManager::Finalize()
{
  delete pModelBasic_;

  // 各モデルインスタンスに対してFinalize呼び出し
  for (auto& model : models_) {
    if (model.second) {
      model.second->Finalize();
    }
  }
  // モデルインスタンスの解放
  models_.clear();

	if (instance_ != nullptr)
	{
		delete instance_;
		instance_ = nullptr;
	}
}

void ModelManager::LoadModel(const std::string& fileName)
{
  // すでにロード済みのファイル名をチェック
#ifdef _DEBUG
  if (models_.contains(fileName))
  {
    Logger::Log("ModelManager: Model already loaded: " + fileName);
    return;
  }
#endif

  // 新しいModelインスタンスを作成
  std::unique_ptr<Model> newModel = std::make_unique<Model>();
  newModel->Initialize(pModelBasic_, fileName);

  models_.insert(std::make_pair(fileName, std::move(newModel)));
}


Model* ModelManager::GetModel(const std::string& fileName)
{
  if (models_.contains(fileName))
  {
    // モデルがすでに存在する場合はそのポインタを返す
    return models_.at(fileName).get()->Clone();
  }

  // 新しいModelインスタンスを作成
  std::unique_ptr<Model> newModel = std::make_unique<Model>();
  newModel->Initialize(pModelBasic_, fileName);

  // モデルデータの登録
  Model* modelPtr = newModel.get();
  models_.insert(std::make_pair(fileName, std::move(newModel)));

  return modelPtr->Clone();
}

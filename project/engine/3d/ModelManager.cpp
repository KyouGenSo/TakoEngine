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

  // ロード済みファイルリストをクリア
  loadedFiles_.clear();

	if (instance_ != nullptr)
	{
		delete instance_;
		instance_ = nullptr;
	}
}

//void ModelManager::LoadModel(const std::string& fileName)
//{
//	// 読み込み済みの場合は何もしない
//	if (models_.contains(fileName))
//	{
//		return;
//	}
//
//	// モデルの読み込み、初期化
//	std::unique_ptr<Model> model = std::make_unique<Model>();
//	model->Initialize(pModelBasic_, fileName, false, false);
//
//	// モデルデータの登録
//	models_.insert(std::make_pair(fileName, std::move(model)));
//}

//void ModelManager::LoadModel(const std::string& fileName, bool hasAnimation)
//{
//	// 読み込み済みの場合は何もしない
//	if (models_.contains(fileName))
//	{
//		return;
//	}
//
//	// モデルの読み込み、初期化
//	std::unique_ptr<Model> model = std::make_unique<Model>();
//	model->Initialize(pModelBasic_, fileName, hasAnimation, false);
//
//	// モデルデータの登録
//	models_.insert(std::make_pair(fileName, std::move(model)));
//}

//void ModelManager::LoadModel(const std::string& fileName, bool hasAnimation, bool hasSkeleton)
//{
//	// 読み込み済みの場合は何もしない
//	if (models_.contains(fileName))
//	{
//		return;
//	}
//
//	// モデルの読み込み、初期化
//	std::unique_ptr<Model> model = std::make_unique<Model>();
//	model->Initialize(pModelBasic_, fileName, hasAnimation, hasSkeleton);
//
//	// モデルデータの登録
//	models_.insert(std::make_pair(fileName, std::move(model)));
//}

Model* ModelManager::GetModel(const std::string& fileName)
{
  return GetModel(fileName, false, false);
}

Model* ModelManager::GetModel(const std::string& fileName, bool hasAnimation, bool hasSkeleton)
{
  if (models_.contains(fileName))
  {
    // モデルがすでに存在する場合はそのポインタを返す
    return models_.at(fileName).get()->Clone();
  }

  // 新しいModelインスタンスを作成
  std::unique_ptr<Model> newModel = std::make_unique<Model>();
  newModel->Initialize(pModelBasic_, fileName, hasAnimation, hasSkeleton);

  // モデルデータの登録
  Model* modelPtr = newModel.get();
  models_.insert(std::make_pair(fileName, std::move(newModel)));

  return modelPtr->Clone();
}

//Model* ModelManager::FindModel(const std::string& fileName)
//{
//	// モデルが存在する場合はポインタを返す
//	if (models_.contains(fileName))
//	{
//    return models_.at(fileName).get();
//	}
//
//  // エラーログを出力
//  Logger::Log("ModelManager::FindModel: Model not found: " + fileName);
//
//	// モデルが存在しない場合はnullptrを返す
//	return nullptr;
//}

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
  for (auto& model : modelInstances_) {
    if (model) {
      model->Finalize();
    }
  }
  // モデルインスタンスの解放
  modelInstances_.clear();

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

Model* ModelManager::CreateModelInstance(const std::string& fileName)
{
  return CreateModelInstance(fileName, false, false);
}

Model* ModelManager::CreateModelInstance(const std::string& fileName, bool hasAnimation, bool hasSkeleton)
{
  // ファイルが未ロードの場合はロード済みとしてマーク
  if (loadedFiles_.find(fileName) == loadedFiles_.end()) {
    loadedFiles_.insert(fileName);
  }

  // 新しいModelインスタンスを作成
  std::unique_ptr<Model> newModel = std::make_unique<Model>();
  newModel->Initialize(pModelBasic_, fileName, hasAnimation, hasSkeleton);

  // ポインタを保存してから返す
  Model* modelPtr = newModel.get();
  modelInstances_.push_back(std::move(newModel));
  return modelPtr;
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

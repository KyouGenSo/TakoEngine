#include"ModelManager.h"
#include"Model.h"
#include"DX12Basic.h"

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
	m_modelBasic_ = new ModelBasic();
	m_modelBasic_->Initialize(dx12);
}

void ModelManager::Finalize()
{
	if (instance_ != nullptr)
	{
		delete instance_;
		instance_ = nullptr;
	}
}

void ModelManager::LoadModel(const std::string& fileName)
{
	// 読み込み済みの場合は何もしない
	if (models_.contains(fileName))
	{
		return;
	}

	// モデルの読み込み、初期化
	std::unique_ptr<Model> model = std::make_unique<Model>();
	model->Initialize(m_modelBasic_, fileName);

	// モデルデータの登録
	models_.insert(std::make_pair(fileName, std::move(model)));
}

Model* ModelManager::FindModel(const std::string& fileName)
{
	// モデルが存在する場合はポインタを返す
	if (models_.contains(fileName))
	{
		return models_.at(fileName).get();
	}

	// モデルが存在しない場合はnullptrを返す
	return nullptr;
}

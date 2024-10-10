#pragma once
#include <string>
#include <map>
#include <memory>
#include"ModelBasic.h"

class Model;

class ModelBasic;

class DX12Basic;

class ModelManager
{
private: // シングルトン設定

	// インスタンス
	static ModelManager* instance_;

	ModelManager() = default;
	~ModelManager() = default;
	ModelManager(ModelManager&) = delete;
	ModelManager& operator=(ModelManager&) = delete;

public: // メンバー関数

	/// <summary>
	/// インスタンスの取得
	/// </summary>
	static ModelManager* GetInstance();

	/// <summary>
	/// 初期化
	/// </summary>
	void Initialize(DX12Basic* dx12);

	/// <summary>
	/// 終了処理
	/// </summary>
	void Finalize();

	///<summary>
	///モデルの読み込む
	///	</summary>
	void LoadModel(const std::string& fileName);

	///<summary>
	///モデルの検索
	///	</summary>
	Model* FindModel(const std::string& fileName);

	//-----------------------------------------Getter-----------------------------------------//
	ModelBasic* GetModelBasic() { return m_modelBasic_; }

private: // メンバー変数

	// モデル基本クラス
	ModelBasic* m_modelBasic_;

	// モデルデータ
	std::map<std::string, std::unique_ptr<Model>> models_;
};
#pragma once
#include <string>
#include <map>
#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <vector>

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

	/// <summary>
	/// モデルの読み込む
	///	</summary>
	void LoadModel(const std::string& fileName);
	void LoadModel(const std::string& fileName, bool hasAnimation);
	void LoadModel(const std::string& fileName, bool hasAnimation, bool hasSkeleton);

  /// <summary>
  /// モデルの検索
  ///	</summary>
  Model* GetModel(const std::string& fileName);
  Model* GetModel(const std::string& fileName, bool hasAnimation, bool hasSkeleton);

	//-----------------------------------------Getter-----------------------------------------//
	ModelBasic* GetModelBasic() { return pModelBasic_; }

private: // メンバー変数

	// モデル基本クラス
	ModelBasic* pModelBasic_;

	// モデルデータ
  // models_の代わりにロード済みファイル名のセットを使用
  std::unordered_set<std::string> loadedFiles_;

  // modelInstancesは保持
  std::unordered_map<std::string, std::unique_ptr<Model>> models_;

};
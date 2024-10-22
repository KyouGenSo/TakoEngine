#pragma once
#include "AbstractSceneFactory.h"


class SceneManager
{
private: // シングルトン設定

	// インスタンス
	static SceneManager* instance_;

	SceneManager() = default;
	~SceneManager() = default;
	SceneManager(SceneManager&) = delete;
	SceneManager& operator=(SceneManager&) = delete;

public: // メンバ関数

	///<summary>
	///インスタンスの取得
	///	</summary>
	static SceneManager* GetInstance();

	/// <summary>
	/// 更新
	/// </summary>
	void Update();

	/// <summary>
	/// 描画
	/// </summary>
	void Draw();

	/// <summary>
	/// imguiの描画
	/// </summary>
	void DrawImGui();

	/// <summary>
	/// 終了処理
	/// </summary>
	void Finalize();

	/// <summary>
	/// 次のシーン予約
	/// </summary>
	/// <param name="scene">　次のシーン </param>
	void ChangeScene(const std::string& sceneName);
	void ChangeScene(const std::string& sceneName, float transitionTime);

	/// <summary>
	/// シーンファクトリーの設定
	/// </summary>
	void SetSceneFactory(AbstractSceneFactory* sceneFactory) { m_sceneFactory_ = sceneFactory; }

private: // メンバ変数

	// シーン
	BaseScene* scene_ = nullptr;

	// 次のシーン
	BaseScene* nextScene_ = nullptr;

	// シーンファクトリー
	AbstractSceneFactory* m_sceneFactory_ = nullptr;

	// シーン遷移アニメーション時間
	float transitionTime_ = 0.5f;
};

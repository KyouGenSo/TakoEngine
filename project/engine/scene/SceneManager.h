#pragma once
#include "AbstractSceneFactory.h"
#include "TransitionManager.h"
#include <memory>

class ITransitionEffect;

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
  void DrawWithoutEffect();

	/// <summary>
	/// imguiの描画
	/// </summary>
	void DrawImGui();

	/// <summary>
	/// 終了処理
	/// </summary>
	void Finalize();

	/// <summary>
	/// 次のシーン予約（既存のメソッド）
	/// </summary>
	/// <param name="sceneName">次のシーン名</param>
	void ChangeScene(const std::string& sceneName);

	/// <summary>
	/// 次のシーン予約（時間指定）
	/// </summary>
	/// <param name="sceneName">次のシーン名</param>
	/// <param name="transitionTime">遷移時間</param>
	void ChangeScene(const std::string& sceneName, float transitionTime);

	/// <summary>
	/// 次のシーン予約（エフェクトタイプ指定）
	/// </summary>
	/// <param name="sceneName">次のシーン名</param>
	/// <param name="effectType">エフェクトタイプ</param>
	/// <param name="transitionTime">遷移時間</param>
	void ChangeScene(const std::string& sceneName,
	                TransitionManager::EffectType effectType,
	                float transitionTime);

	/// <summary>
	/// 次のシーン予約（エフェクト名指定）
	/// </summary>
	/// <param name="sceneName">次のシーン名</param>
	/// <param name="effectName">エフェクト名</param>
	/// <param name="transitionTime">遷移時間</param>
	void ChangeScene(const std::string& sceneName,
	                const std::string& effectName,
	                float transitionTime);

	/// <summary>
	/// 次のシーン予約（エフェクトインスタンス直接指定）
	/// </summary>
	/// <param name="sceneName">次のシーン名</param>
	/// <param name="effect">エフェクトインスタンス</param>
	/// <param name="transitionTime">遷移時間</param>
	void ChangeScene(const std::string& sceneName,
	                std::unique_ptr<ITransitionEffect> effect,
	                float transitionTime);

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

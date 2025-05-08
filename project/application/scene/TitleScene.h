#pragma once
#include "BaseScene.h"
#include"Sprite.h"
#include"Object3d.h"
#include <vector>
#include <memory>
#include "AABB.h"

class TitleScene : public BaseScene
{
public: // メンバ関数

	/// <summary>
	/// 初期化
	/// </summary>
	void Initialize() override;

	/// <summary>
	/// 終了処理
	/// </summary>
	void Finalize() override;

	/// <summary>
	/// 更新
	/// </summary>
	void Update() override;

	/// <summary>
	/// 描画
	/// </summary>
  void Draw() override;
  void DrawWithoutEffect() override;

	/// <summary>
	/// ImGuiの描画
	/// </summary>
	void DrawImGui() override;

  void ApplyGlobalVariables();

  void InitParticle();

  void InitVariables();

	struct EmitterParam {
		std::string name_;
		Transform transform_;
		Vector3 velocity_;
		AABB range_;
		Vector4 color_;
		float lifeTime;
		int count_;
		float frequency_;
		bool isRandomColor_;
		bool isVisualize_;
	};

private: // メンバ変数

	bool isDebug_ = false;

	EmitterParam emitterParam_;
	EmitterParam emitterParam2_;

};

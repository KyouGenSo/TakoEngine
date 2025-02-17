#pragma once
#include"TakoFramework.h"
#include"Vector2.h"

class MyGame : public TakoFramework
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

  struct PostEffectParam
  {
    float vignettePower;
    float vignetteRange;
    float bloomThreshold;
    float bloomIntensity;
    float bloomSigma;
    Vector4 fogColor;
    float fogDensity;
  };

private: // メンバ変数

  PostEffectParam postEffectParam;

  bool FPSWindowVisible = true;
  bool PostEffectWindowVisible = false;

	enum PostEffectType
	{
		NoEffect,
		VignetteRed,
		VignetteRedBloom,
		GrayScale,
		VigRedGrayScale,
		Bloom,
		BloomFog,
	};

	PostEffectType postEffectType = NoEffect;

};
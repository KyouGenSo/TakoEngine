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
    int bloomKernelSize;
    int bloomSampleCount;
    int downSampleFactor;

    Vector4 fogColor;
    float fogDensity;

    Vector2 radialBlurCenter;
    float radialBlurWidth;
    int32_t radialBlurSampleCount;

    float bwFilterThreshold;

    float rgbSplitIntensity;
    Vector2 redOffset;
    Vector2 blueOffset;
    Vector2 greenOffset;
  };

private: // メンバ変数

  PostEffectParam postEffectParam;

  bool FPSWindowVisible = true;
  bool PostEffectWindowVisible = false;
  Vector3 cameraPos{ 0.0f, 0.0f, 0.0f };
  Vector3 cameraRotate{ 0.0f, 0.0f, 0.0f };

  enum PostEffectType
	{
		NoEffect,
		VignetteRed,
		VignetteRedBloom,
		GrayScale,
		VigRedGrayScale,
		Bloom,
    NewBloom,
		BloomFog,
    RadialBlur,
    BWFilter,
    RGBSplit,
	};

	PostEffectType postEffectType = NoEffect;

  uint32_t spriteBasicOnresizeId = 0;
};
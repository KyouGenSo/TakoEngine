#pragma once
#include <d3d12.h>
#include<unordered_map>
#include<string>
#include<wrl.h>

#include "Vector2.h"
#include"Vector4.h"
#include "PostEffectStruct.h"

class DX12Basic;

class PostEffect {
private:
  // シングルトン設定
  static PostEffect* instance_;

  PostEffect() = default;
  ~PostEffect() = default;

public:
  PostEffect(PostEffect&) = delete;
  PostEffect& operator=(PostEffect&) = delete;

public: // メンバ関数

  // ComPtrのエイリアス
  template<class T> using ComPtr = Microsoft::WRL::ComPtr<T>;

  // インスタンスの取得
  static PostEffect* GetInstance();

  // 初期化
  void Initialize(DX12Basic* dx12);

  // 終了処理
  void Finalize();

  // 描画前の処理
  void BeginDrawEffectTarget();
  void BegineDrawNonEffectTarget();

  // 描画
  void Draw();
  void DrawPostEffect(const std::string& effectName);
  void DrawMultiPassNewBloom(); // マルチパスブルーム描画用
  void DrawMultiPassBloom();
  void DrawFinalResult();

  // レンダーテクスチャの再作成
  void RecreateRenderTexture(uint32_t width, uint32_t height);

  // バリアの設定
  void SetBarrier(D3D12_RESOURCE_STATES stateBefore, D3D12_RESOURCE_STATES stateAfter);
  void SetBarrier(D3D12_RESOURCE_STATES stateBefore, D3D12_RESOURCE_STATES stateAfter, ID3D12Resource* resource);

  // レンダーテクスチャの取得
  ID3D12Resource* GetRenderTextureResource() { return originRenderTexResource_.Get(); }

  void SetEffectType(std::string effectName) { currentEffectName_ = effectName; }

  void SetVignettePower(float power);

  void SetVignetteRange(float range);

  void SetBloomThreshold(float threshold);

  void SetBloomIntensity(float intensity);

  void SetBloomSigma(float sigma);

  void SetBloomKernelSize(int kernelSize);

  void SetBloomSampleCount(int32_t count);

  void SetDownSampleFactor(int factor);

  void SetBloomIteration(int iteration) { bloomIteration_ = iteration; }

  void SetFogColor(const Vector4& color);

  void SetFogDensity(float density);

  void SetRadialBlurCenter(const Vector2& center);

  void SetRadialBlurWidth(float width);

  void SetBWFilterThreshold(float bwFilterThreshold) { BWFilterParam_->threshold = bwFilterThreshold; }

  void SetRGBSplitOffsets(const Vector2& red, const Vector2& green, const Vector2& blue);

  void SetRGBSplitIntensity(float intensity) { rgbSplitParam_->intensity = intensity; }

private: // プライベートメンバー関数

  // レンダーテクスチャの初期化
  void CreateRenderTexture();

  // DownSample用のテクスチャを生成
  void CreateBloomTextures();

  // HorizontalBlur用のテクスチャを生成
  //void CreateHorizontalBlurTexture();

  // 深度バッファのSRVを生成
  void CreateDepthBufferSRV();

  // ルートシグネチャの生成
  void CreateRootSignature(const std::string& effectName);

  // パイプラインステートの生成
  void CreatePSO(const std::string& effectName);

  // VignetteParamを生成
  void CreateVignetteParam();

  // VignetteRedBloomParamを生成
  void CreateVignetteRedBloomParam();

  // BloomParamを生成
  void CreateBloomParam();

  // NewBloomParamを生成
  void CreateNewBloomParam();

  // FogParamを生成
  void CreateFogParam();

  // RadialBlurParamを生成
  void CreateRadialBlurParam();

  // CameraForGPUを生成
  void CreateCameraForGPU();

  // BWFilterParamを生成
  void CreateBWFilterParam();

  // RGBSplitParamを生成
  void CreateRGBSplitParam();

  // パラメーターリソースの設定
  void SetParamResource(const std::string& effectName);

private: // メンバ変数

  std::string currentEffectName_;

  // DX12の基本情報
  DX12Basic* m_dx12_ = nullptr;

  // Effect適用するオブジェクト用レンダーテクスチャ
  ComPtr<ID3D12Resource> originRenderTexResource_;
  D3D12_CPU_DESCRIPTOR_HANDLE originRenderTexRTVHandle_;
  uint32_t originRtvSrvIndex_ = 0;

  // 最終結果用のレンダーテクスチャ
  ComPtr<ID3D12Resource> resultRenderTexResource_;
  D3D12_CPU_DESCRIPTOR_HANDLE resultRenderTexRTVHandle_;
  uint32_t resultRtvSrvIndex_ = 0;

  // 高輝度部分抽出用のレンダーテクスチャ
  ComPtr<ID3D12Resource> highLumResource_;
  D3D12_CPU_DESCRIPTOR_HANDLE highLumRTVHandle_;
  uint32_t highLumSrvIndex_ = 0;

  // 高輝度部分のダウンサンプル用のレンダーテクスチャ
  ComPtr<ID3D12Resource> highLumShrinkResource_;
  D3D12_CPU_DESCRIPTOR_HANDLE highLumShrinkRTVHandle_;
  uint32_t highLumShrinkSrvIndex_ = 0;

  // Bloomの結果用のレンダーテクスチャ
  ComPtr<ID3D12Resource> bloomResultResource_;
  D3D12_CPU_DESCRIPTOR_HANDLE bloomResultRTVHandle_;
  uint32_t bloomResultSrvIndex_ = 0;

  // 深度バッファのSRV
  uint32_t dsvSrvIndex_ = 0;

  // ダウンサンプル倍率（1/N）
  int downSampleFactor_ = 8; // 1/4サイズ

  uint32_t downSampleWidth_ = 0;
  uint32_t downSampleHeight_ = 0;

  // bloomのイテレーション数
  int bloomIteration_ = 8;

  // レンダーテクスチャのclearColor
  const Vector4 kOriginRenderTexClearColor_ = { 0.17f, 0.17f, 0.17f, 1.0f };
  Vector4 resultRenderTexClearColor_ = { 0.0, 0.0, 0.0, 0.0 };

  // ルートシグネチャ
  std::unordered_map <std::string, ComPtr<ID3D12RootSignature>> rootSignatures_;

  // パイプラインステート
  std::unordered_map<std::string, ComPtr<ID3D12PipelineState>> pipelineStates_;

  // パラメーターリソース
  Microsoft::WRL::ComPtr<ID3D12Resource> vignetteParamResource_;
  Microsoft::WRL::ComPtr<ID3D12Resource> vignetteRedBloomParamResource_;
  Microsoft::WRL::ComPtr<ID3D12Resource> bloomParamResource_;
  Microsoft::WRL::ComPtr<ID3D12Resource> bloomParamResource2_;
  Microsoft::WRL::ComPtr<ID3D12Resource> newBloomParamResource_;
  Microsoft::WRL::ComPtr<ID3D12Resource> fogParamResource_;
  Microsoft::WRL::ComPtr<ID3D12Resource> radialBlurParamResource_;
  Microsoft::WRL::ComPtr<ID3D12Resource> cameraForGPUResource_;
  Microsoft::WRL::ComPtr<ID3D12Resource> BWFilterParamResource_;
  Microsoft::WRL::ComPtr<ID3D12Resource> rgbSplitParamResource_;

  // パラメーターデータ
  VignetteParam* vignetteParam_;
  VignetteRedBloomParam* vignetteRedBloomParam_;
  BloomParam* bloomParam_;
  BloomParam* bloomParam2_;
  NewBloomParam* newBloomParam_;
  FogParam* fogParam_;
  RadialBlurParam* radialBlurParam_;
  CameraForGPU* cameraForGPU_;
  BWFilterParam* BWFilterParam_;
  RGBSplitParam* rgbSplitParam_;
};

#pragma once
#include <d3d12.h>
#include <string>
#include<wrl.h>

class DX12Basic;

class IPostEffect
{
public: // メンバー関数

  // 初期化
  virtual void Initialize(DX12Basic* dx12, std::string shaderName);

  // 描画
  virtual void Draw() = 0;

  // 描画設定
  virtual void SetDrawSetting() = 0;

  // Debug用のパラメータを設定
  virtual void DrawImgui() = 0;

  // 入力画像のRTVハンドルを設定
  void SetInputRtvHandle(uint32_t rtvHandle) {
    inputImageRtvHandle_ = rtvHandle;
  }

  // 出力画像のRTVハンドルを設定
  void SetOutputRtvHandle(uint32_t rtvHandle) {
    outputImageRtvHandle_ = rtvHandle;
  }

  // 入力画像のRTVハンドルを取得
  uint32_t GetInputRtvHandle() const {
    return inputImageRtvHandle_;
  }

  // 出力画像のRTVハンドルを取得
  uint32_t GetOutputRtvHandle() const {
    return outputImageRtvHandle_;
  }

protected: // プライベートメンバー関数

  // ComPtrのエイリアス
  template<class T> using ComPtr = Microsoft::WRL::ComPtr<T>;

  virtual void CreateRootSignature() = 0;
  virtual void CreatePSO() = 0;
  virtual void CreateCBV() = 0;

protected: // メンバー変数

  // DX12の基本情報
  DX12Basic* m_dx12_ = nullptr;

  uint32_t inputImageRtvHandle_ = 0; // 入力画像のRTVハンドル

  uint32_t outputImageRtvHandle_ = 0; // 出力画像のRTVハンドル

  // ルートシグネチャ
  ComPtr<ID3D12RootSignature> rootSignature_;

  // パイプラインステート
  ComPtr<ID3D12PipelineState> pipelineState_;

  // パラメーターリソース
  ComPtr<ID3D12Resource> paramResource_;

  // Shaderの名前
  std::string shaderName_ = "DefaultShader";
};


#pragma once
#include <d3d12.h>
#include <dxgi1_6.h>
#include<wrl.h>
#include "DX12Basic.h"

class SpriteBasic {

public: // メンバー関数

	// ComPtrのエイリアス
	template<class T> using ComPtr = Microsoft::WRL::ComPtr<T>;

	/// <summary>
	/// 初期化
	/// </summary>
	void Initialize(DX12Basic* dx12);

	/// <summary>
	/// 共通描画設定
	/// <summary>
	void SetCommonRenderSetting();

	//-----------------------------------Getters-----------------------------------//
	DX12Basic* GetDX12Basic() { return dx12_; }

private: // プライベートメンバー関数
	/// <summary>
	/// ルートシグネチャの作成
	/// </summary>
	void CreateRootSignature();

	/// <summary>
	/// パイプラインステートの生成
	/// </summary>
	void CreatePSO();

private: // メンバー変数

	// DX12Basicクラスのインスタンス
	DX12Basic* dx12_;

	// ルートシグネチャ
	ComPtr<ID3D12RootSignature> rootSignature_;

	// パイプラインステート
	ComPtr<ID3D12PipelineState> pipelineState_;
};
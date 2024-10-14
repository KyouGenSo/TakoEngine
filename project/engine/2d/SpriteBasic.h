#pragma once
#include <d3d12.h>
#include <dxgi1_6.h>
#include<wrl.h>
#include "DX12Basic.h"

class SpriteBasic {
private: // シングルトン設定

	// インスタンス
	static SpriteBasic* instance_;

	SpriteBasic() = default;
	~SpriteBasic() = default;
	SpriteBasic(SpriteBasic&) = delete;
	SpriteBasic& operator=(SpriteBasic&) = delete;

public: // メンバー関数

	// ComPtrのエイリアス
	template<class T> using ComPtr = Microsoft::WRL::ComPtr<T>;

	/// <summary>
	/// インスタンスの取得
	/// </summary>
	static SpriteBasic* GetInstance();

	/// <summary>
	/// 初期化
	/// </summary>
	void Initialize(DX12Basic* dx12);

	/// <summary>
	/// 終了処理
	/// </summary>
	void Finalize();

	/// <summary>
	/// 共通描画設定
	/// <summary>
	void SetCommonRenderSetting();

	//-----------------------------------Getters-----------------------------------//
	DX12Basic* GetDX12Basic() { return m_dx12_; }

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
	DX12Basic* m_dx12_;

	// ルートシグネチャ
	ComPtr<ID3D12RootSignature> rootSignature_;

	// パイプラインステート
	ComPtr<ID3D12PipelineState> pipelineState_;
};
#pragma once
#include <d3d12.h>
#include<wrl.h>

class DX12Basic;

class Camera;

class Object3dBasic {
public:
	///<summary>
	///初期化
	/// </summary>
	void Initialize(DX12Basic* dx12);

	///<summary>
	///共通描画設定
	/// </summary>
	void SetCommonRenderSetting();

	// -----------------------------------Getters-----------------------------------//
	DX12Basic* GetDX12Basic() const { return m_dx12_; }
	Camera* GetDefaultCamera() const { return defaultCamera_; }

	// -----------------------------------Getters-----------------------------------//
	void SetDefaultCamera(Camera* camera) { defaultCamera_ = camera; }

private: // プライベートメンバー関数

	///<summary>
	///ルートシグネチャの作成
	/// 	/// </summary>
	void CreateRootSignature();

	///<summary>
	///パイプラインステートの生成
	/// </summary>
	void CreatePSO();

private: // メンバー変数
	// DX12Basicクラスのインスタンス
	DX12Basic* m_dx12_ = nullptr;

	// デフォルトカメラ
	Camera* defaultCamera_ = nullptr;

	// ルートシグネチャ
	Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature_;

	// パイプラインステート
	Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineState_;
};

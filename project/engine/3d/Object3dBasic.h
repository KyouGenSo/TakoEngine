#pragma once
#include <d3d12.h>
#include<wrl.h>
#include "Matrix4x4.h"

class DX12Basic;

class Camera;

class Object3dBasic {
private: // シングルトン設定

	// インスタンス
	static Object3dBasic* instance_;

	Object3dBasic() = default;
	~Object3dBasic() = default;
	Object3dBasic(Object3dBasic&) = delete;
	Object3dBasic& operator=(Object3dBasic&) = delete;

public: // メンバー関数

	///<summary>
	///　インスタンスの取得
	///	</summary>
	static Object3dBasic* GetInstance();

	///<summary>
	///　初期化
	/// </summary>
	void Initialize(DX12Basic* dx12);

	///<summary>
	///　更新
	/// </summary>
	void Update();

	/// <summary>
	/// 終了処理
	/// </summary>
	void Finalize();

	///<summary>
	///　共通描画設定
	/// </summary>
	void SetCommonRenderSetting();

	// -----------------------------------Getters-----------------------------------//
	DX12Basic* GetDX12Basic() const { return m_dx12_; }
	Camera* GetCamera() const { return camera_; }
	bool GetDebug() const { return isDebug_; }

	// -----------------------------------Getters-----------------------------------//
	void SetCamera(Camera* camera) { camera_ = camera; }
	void SetDebug(bool isDebug) { isDebug_ = isDebug; }

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
	Camera* camera_ = nullptr;

	// ビュープロジェクション行列
	Matrix4x4 viewProjectionMatrix_;

	// デバッグフラグ
	bool isDebug_ = false;

	// ルートシグネチャ
	Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature_;

	// パイプラインステート
	Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineState_;
};

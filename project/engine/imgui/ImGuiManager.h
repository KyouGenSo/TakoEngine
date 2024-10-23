#pragma once
#include"imgui.h"
#include <d3d12.h>
#include<wrl.h>

class WinApp;

class DX12Basic;

class ImGuiManager
{
public: // メンバ関数

	/// <summary>
	/// 初期化
	/// </summary>
	void Initialize(WinApp* winApp, DX12Basic* dx12);

	/// <summary>
	/// dx12用初期化
	/// </summary>
	void InitializeForDX12();

	/// <summary>
	/// 描画開始
	/// </summary>
	void Begin();

	/// <summary>
	/// 描画
	/// </summary>
	void Draw();

	/// <summary>
	/// 描画終了
	/// </summary>
	void End();

	/// <summary>
	/// 終了処理
	/// </summary>
	void Shutdown();

	/// <summary>
	/// SRV用のディスクリプタヒープの生成
	/// </summary>
	void CreateImGuiSrvHeap();

	/// <summary>
	/// ImGuiのスタイルの設定
	/// </summary>
	void SetStyleBoorstrapDark();
	void SetStyleMoonLight();

private: // メンバ変数

	// WinAppクラスのインスタンス
	WinApp* m_winApp_ = nullptr;

	// DX12Basicクラスのインスタンス
	DX12Basic* m_dx12_ = nullptr;

	// SRV用のディスクリプタヒープ
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> srvHeap_;

};
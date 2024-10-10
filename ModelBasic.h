#pragma once

class DX12Basic;

class ModelBasic
{
public:
	/// <summary>
	/// 初期化
	/// </summary>
	void Initialize(DX12Basic* dx12);


	//-----------------------------------------Getter-----------------------------------------//
	DX12Basic* GetDX12Basic() { return m_dx12_; }

private:
	DX12Basic* m_dx12_;
};

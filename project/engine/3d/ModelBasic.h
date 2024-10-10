#pragma once
#include <string>

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
	const std::string& GetDirectoryFolderName() const { return directoryFolderName_; }
	const std::string& GetModelFolderName() const { return ModelFolderName_; }

	//-----------------------------------------Setter-----------------------------------------//
	void SetDirectoryFolderName(const std::string& directoryFolderName) { directoryFolderName_ = directoryFolderName; }
	void SetModelFolderName(const std::string& ModelFolderName) { ModelFolderName_ = ModelFolderName; }

private:
	DX12Basic* m_dx12_;

	std::string directoryFolderName_;

	std::string ModelFolderName_;
};

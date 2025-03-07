#pragma once
#include <string>
#include <d3d12.h>
#include<wrl.h>

class DX12Basic;

class ModelBasic
{
public:
	/// <summary>
	/// 初期化
	/// </summary>
	void Initialize(DX12Basic* dx12);

  ///<summary>
  ///　描画設定
  /// </summary>
  void SetSkinningCSSetting();

	//-----------------------------------------Getter-----------------------------------------//
	DX12Basic* GetDX12Basic() { return m_dx12_; }
	const std::string& GetDirectoryFolderName() const { return directoryFolderName_; }
	const std::string& GetModelFolderName() const { return ModelFolderName_; }

	//-----------------------------------------Setter-----------------------------------------//
	void SetDirectoryFolderName(const std::string& directoryFolderName) { directoryFolderName_ = directoryFolderName; }
	void SetModelFolderName(const std::string& ModelFolderName) { ModelFolderName_ = ModelFolderName; }

private: // プライベートメンバー関数
  ///<summary>
  /// ルートシグネチャの作成
  /// </summary>
  void CreateCSRootSignature();

  ///<summary>
  /// パイプラインステートの生成
  /// </summary>
  void CreateCSPSO();

private:
	DX12Basic* m_dx12_;

	std::string directoryFolderName_;

	std::string ModelFolderName_;

  Microsoft::WRL::ComPtr<ID3D12RootSignature> csRootSignature_;
  Microsoft::WRL::ComPtr<ID3D12PipelineState> csPipelineState_;

};

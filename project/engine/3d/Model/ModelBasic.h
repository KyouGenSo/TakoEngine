#pragma once
#include <string>
#include <d3d12.h>
#include<wrl.h>

class DX12Basic;

/// <summary>
/// モデル基盤クラス
/// コンピュートシェーダーによるスキニング処理を管理
/// </summary>
class ModelBasic
{
public:
	/// <summary>
	/// 初期化
	/// </summary>
	void Initialize(DX12Basic* dx12);

  /// <summary>
  /// 描画設定
  /// </summary>
  void SetSkinningCSSetting();

	//-----------------------------------------Getter-----------------------------------------//
	/// <summary>
	/// DirectX12基盤を取得
	/// </summary>
	/// <returns>DX12Basicポインタ</returns>
	DX12Basic* GetDX12Basic() { return m_dx12_; }

	/// <summary>
	/// ディレクトリフォルダ名を取得
	/// </summary>
	/// <returns>ディレクトリフォルダ名</returns>
	const std::string& GetDirectoryFolderName() const { return directoryFolderName_; }

	/// <summary>
	/// モデルフォルダ名を取得
	/// </summary>
	/// <returns>モデルフォルダ名</returns>
	const std::string& GetModelFolderName() const { return ModelFolderName_; }

	//-----------------------------------------Setter-----------------------------------------//
	/// <summary>
	/// ディレクトリフォルダ名を設定
	/// </summary>
	/// <param name="directoryFolderName">ディレクトリフォルダ名</param>
	void SetDirectoryFolderName(const std::string& directoryFolderName) { directoryFolderName_ = directoryFolderName; }

	/// <summary>
	/// モデルフォルダ名を設定
	/// </summary>
	/// <param name="ModelFolderName">モデルフォルダ名</param>
	void SetModelFolderName(const std::string& ModelFolderName) { ModelFolderName_ = ModelFolderName; }

private: // プライベートメンバー関数
  /// <summary>
  /// ルートシグネチャの作成
  /// </summary>
  void CreateCSRootSignature();

  /// <summary>
  /// パイプラインステートの生成
  /// </summary>
  void CreateCSPSO();

private:
	DX12Basic* m_dx12_;  ///< DirectX12基盤システムへのポインタ

	std::string directoryFolderName_;  ///< ディレクトリフォルダパス

	std::string ModelFolderName_;  ///< モデルフォルダ名

	Microsoft::WRL::ComPtr<ID3D12RootSignature> csRootSignature_;  ///< コンピュートシェーダー用ルートシグネチャ
	Microsoft::WRL::ComPtr<ID3D12PipelineState> csPipelineState_;  ///< コンピュートシェーダー用パイプラインステート

};

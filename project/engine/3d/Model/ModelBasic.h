#pragma once
#include <string>
#include <d3d12.h>
#include<wrl.h>

namespace Tako {

  class DX12Basic;

  /// <summary>
  /// モデル基盤クラス
  /// コンピュートシェーダーによるスキニング処理を管理
  /// </summary>
  class ModelBasic
  {
  public: //定数
    // スキニング CS のルートパラメータ番号
    static constexpr UINT kPaletteParam      = 0;  ///< t0: パレット（ボーン行列）
    static constexpr UINT kVertexInputParam  = 1;  ///< t1: 入力頂点
    static constexpr UINT kInfluenceParam    = 2;  ///< t2: 頂点影響度
    static constexpr UINT kVertexOutputParam = 3;  ///< u0: 出力頂点
    static constexpr UINT kSkinningInfoParam = 4;  ///< b0: スキニング情報

  public: //メンバー関数
    /// <summary>
    /// 初期化
    /// </summary>
    void Initialize(DX12Basic* dx12);

    /// <summary>
    /// 描画設定
    /// </summary>
    void SetSkinningCSSetting();

    //============================================================
    //Setter
    //============================================================
    void SetDirectoryFolderName(const std::string& directoryFolderName) { directoryFolderName_ = directoryFolderName; }
    void SetModelFolderName(const std::string& modelFolderName) { modelFolderName_ = modelFolderName; }

    //============================================================
    //Getter
    //============================================================
    DX12Basic* GetDX12Basic() { return dx12_; }
    const std::string& GetDirectoryFolderName() const { return directoryFolderName_; }
    const std::string& GetModelFolderName() const { return modelFolderName_; }

  private: //非公開関数
    /// <summary>
    /// ルートシグネチャの作成
    /// </summary>
    void CreateCSRootSignature();

    /// <summary>
    /// パイプラインステートの生成
    /// </summary>
    void CreateCSPSO();

  private: //メンバー変数
    DX12Basic*                                  dx12_;               ///< DirectX12基盤システムへのポインタ
    std::string                                 directoryFolderName_;  ///< ディレクトリフォルダパス
    std::string                                 modelFolderName_;      ///< モデルフォルダ名
    Microsoft::WRL::ComPtr<ID3D12RootSignature> csRootSignature_;      ///< コンピュートシェーダー用ルートシグネチャ
    Microsoft::WRL::ComPtr<ID3D12PipelineState> csPipelineState_;      ///< コンピュートシェーダー用パイプラインステート
  };

} // namespace Tako

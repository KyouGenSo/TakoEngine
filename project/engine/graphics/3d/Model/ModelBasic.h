#pragma once
#include <string>
#include <d3d12.h>
#include<wrl.h>

#include "PipelineCache.h"

namespace Tako {

  class DX12Basic;

  /// <summary>
  /// モデル基盤クラス
  /// コンピュートシェーダーによるスキニング処理を管理
  /// </summary>
  class ModelBasic
  {
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

  private: //メンバー変数
    DX12Basic*          dx12_;                 ///< DirectX12基盤システムへのポインタ
    std::string         directoryFolderName_;  ///< ディレクトリフォルダパス
    std::string         modelFolderName_;      ///< モデルフォルダ名
    ComputePipelineDesc skinningPipeline_{};   ///< スキニング CS
  };

} // namespace Tako

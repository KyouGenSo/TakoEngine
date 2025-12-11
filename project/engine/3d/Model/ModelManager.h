#pragma once
#include <string>
#include <map>
#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include"ModelBasic.h"
#include"Model.h"

class DX12Basic;

namespace Tako {

  /// <summary>
  /// モデルリソースの一元管理を行うシングルトンクラス
  /// モデルのキャッシュ管理とAssimpベースのモデル読み込みを提供
  /// </summary>
  class ModelManager
  {
  private: // シングルトン設定

    // インスタンス
    static ModelManager* instance_;

    ModelManager() = default;
    ~ModelManager() = default;
    ModelManager(ModelManager&) = delete;
    ModelManager& operator=(ModelManager&) = delete;

  public: // メンバー関数

    /// <summary>
    /// インスタンスの取得
    /// </summary>
    /// <returns>ModelManagerのシングルトンインスタンス</returns>
    static ModelManager* GetInstance();

    /// <summary>
    /// 初期化
    /// </summary>
    /// <param name="dx12">DirectX12基盤システムへのポインタ</param>
    void Initialize(DX12Basic* dx12);

    /// <summary>
    /// 終了処理
    /// </summary>
    void Finalize();

    /// <summary>
    /// モデルの読み込み
    /// </summary>
    /// <param name="fileName">モデルファイル名</param>
    void LoadModel(const std::string& fileName);

    /// <summary>
    /// モデルの検索
    /// </summary>
    /// <param name="fileName">モデルファイル名</param>
    /// <returns>モデルポインタ（見つからない場合nullptr）</returns>
    Model* GetModel(const std::string& fileName);

    // ===== Getter =====
    /// <summary>
    /// モデル基本システムを取得
    /// </summary>
    /// <returns>ModelBasicポインタ</returns>
    ModelBasic* GetModelBasic() { return pModelBasic_.get(); }

  private: // メンバー変数

    std::unique_ptr<ModelBasic> pModelBasic_; ///< モデル基本システムへのポインタ

    ///< モデルのマップ（キー:ファイル名、値:モデルインスタンス）
    std::unordered_map<std::string, std::unique_ptr<Model>> models_;

  };

} // namespace Tako
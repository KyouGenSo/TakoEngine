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
  /// モデルのキャッシュ管理と Assimp ベースのモデル読み込みを提供
  /// </summary>
  class ModelManager
  {
  private: //シングルトン設定
    static std::unique_ptr<ModelManager> instance_;

    struct Token {};  ///< 外部からの直接生成を防ぐ生成キー
    ~ModelManager() = default;

    friend struct std::default_delete<ModelManager>;

  public:
    explicit ModelManager(Token) {}
    ModelManager(const ModelManager&) = delete;
    ModelManager& operator=(const ModelManager&) = delete;

  public: //メンバー関数

    /// <summary>
    /// インスタンスの取得
    /// </summary>
    /// <returns>ModelManager のシングルトンインスタンス</returns>
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
    /// モデルの読み込み（ファイルが無ければ記録を 1 回出して何もしない）
    /// </summary>
    /// <param name="fileName">モデルファイル名</param>
    void LoadModel(const std::string& fileName);

    /// <summary>
    /// エンジン用モデルの読み込み（EngineResources/Model/ 配下から）
    /// </summary>
    /// <param name="fileName">エンジン用モデルファイル名</param>
    void LoadEngineModel(const std::string& fileName);

    //============================================================
    //Getter
    //============================================================
    /// <summary>
    /// モデルのクローン。読んでいなければその場で読む
    /// </summary>
    /// <param name="fileName">モデルファイル名</param>
    /// <returns>モデルのクローン（ファイルが無ければ nullptr）</returns>
    std::unique_ptr<Model> GetModel(const std::string& fileName);

    /// <summary>
    /// エンジン用モデルのクローン。読んでいなければその場で読む
    /// </summary>
    /// <param name="fileName">エンジン用モデルファイル名</param>
    /// <returns>モデルのクローン（ファイルが無ければ nullptr）</returns>
    std::unique_ptr<Model> GetEngineModel(const std::string& fileName);

    ModelBasic* GetModelBasic() { return pModelBasic_.get(); }

    /// <summary>
    /// 読み込み済みモデルのファイル名(キー)一覧を取得する。
    /// エンジンモデルは "EngineResources/Model/" プレフィックス付きキーで含まれる。
    /// </summary>
    /// <returns>ソート済みのモデル名リスト</returns>
    std::vector<std::string> GetLoadedModelNames() const;

    size_t GetLoadedModelCount() const { return models_.size(); }

  private: //非公開関数
    /// <summary>
    /// path があれば true。無ければ key ごとに 1 回だけ記録を出す
    /// </summary>
    bool CanLoad(const std::string& key, const std::string& path);

  private: //メンバー変数
    std::unique_ptr<ModelBasic>                             pModelBasic_;  ///< モデル基本システムへのポインタ
    std::unordered_map<std::string, std::unique_ptr<Model>> models_;       ///< モデルのマップ（キー:ファイル名、値:モデルインスタンス）
    std::unordered_set<std::string>                         failedFiles_;  ///< 無かったキー
  };

} // namespace Tako
#pragma once
#include <unordered_map>
#include <functional>
#include <vector>
#include <string>
#include <memory>
#include "BTNode.h"
#include "BTNodeMeta.h"

namespace Tako {

  /// <summary>
  /// BT ノード型のファクトリレジストリ (シングルトン)。
  /// 各ノード型を文字列キー (例 "BTSelector") で登録し、
  /// JSON ロード時の動的生成およびエディタのパレット表示に使用する。
  /// </summary>
  /// <remarks>
  /// エンジン側の Initialize() で標準コンポジット (Selector/Sequence/RandomSelector/Parallel)
  /// が自動登録される。ゲーム側は起動時に独自ノード (攻撃・移動・条件等) を
  /// RegisterNode&lt;T&gt;() で追加登録する想定。
  /// </remarks>
  class BTNodeRegistry {
  public:
    /// <summary>
    /// シングルトンインスタンスの取得 (遅延初期化)。
    /// </summary>
    /// <returns>BTNodeRegistry のインスタンス</returns>
    static BTNodeRegistry* GetInstance();

    BTNodeRegistry(const BTNodeRegistry&) = delete;
    BTNodeRegistry& operator=(const BTNodeRegistry&) = delete;

    /// <summary>
    /// 初期化。標準コンポジット (BTSelector / BTSequence / BTRandomSelector / BTParallel)
    /// を事前登録する。複数回呼んでも安全 (重複登録は上書き)。
    /// </summary>
    void Initialize();

    /// <summary>
    /// 終了処理。登録情報を全クリアする。
    /// </summary>
    void Finalize();

    /// <summary>
    /// ノード型を登録 (デフォルトコンストラクタ呼び出しのみ)。
    /// </summary>
    /// <typeparam name="T">BTNode 派生クラス</typeparam>
    /// <param name="typeName">ノード型名 (JSON シリアライズ・エディタ表示で使う)</param>
    /// <param name="meta">メタ情報</param>
    template<class T>
    void RegisterNode(const std::string& typeName, const NodeMeta& meta) {
      factories_[typeName] = [] {
        return std::static_pointer_cast<BTNode>(std::make_shared<T>());
        };
      metas_[typeName] = meta;
    }

    /// <summary>
    /// 任意ファクトリ関数で登録 (引数付きコンストラクタが必要な型用)。
    /// </summary>
    /// <param name="typeName">ノード型名</param>
    /// <param name="factory">ノード生成関数</param>
    /// <param name="meta">メタ情報</param>
    void RegisterFactory(const std::string& typeName,
      std::function<BTNodePtr()> factory,
      const NodeMeta& meta);

    /// <summary>
    /// 登録済みの typeName からノードを生成。
    /// </summary>
    /// <param name="typeName">ノード型名</param>
    /// <returns>生成されたノード、未登録なら nullptr</returns>
    BTNodePtr Create(const std::string& typeName) const;

    /// <summary>
    /// 登録済みの全タイプ名取得。
    /// </summary>
    /// <returns>全 typeName のリスト (順序は unordered)</returns>
    std::vector<std::string> GetAllTypes() const;

    /// <summary>
    /// カテゴリでフィルタしたタイプ名取得。
    /// </summary>
    /// <param name="category">カテゴリ</param>
    /// <returns>該当 typeName のリスト</returns>
    std::vector<std::string> GetTypesByCategory(NodeCategory category) const;

    /// <summary>
    /// メタ情報の取得。
    /// </summary>
    /// <param name="typeName">ノード型名</param>
    /// <returns>メタ情報へのポインタ、未登録なら nullptr</returns>
    const NodeMeta* GetMeta(const std::string& typeName) const;

    /// <summary>
    /// 登録済みかチェック。
    /// </summary>
    /// <param name="typeName">ノード型名</param>
    /// <returns>登録済みなら true</returns>
    bool IsRegistered(const std::string& typeName) const;

  private:
    BTNodeRegistry() = default;
    ~BTNodeRegistry() = default;
    friend struct std::default_delete<BTNodeRegistry>;

    /// シングルトン実体
    static std::unique_ptr<BTNodeRegistry> instance_;

    /// 各 typeName から生成関数へのマップ
    std::unordered_map<std::string, std::function<BTNodePtr()>> factories_;

    /// 各 typeName からメタ情報へのマップ
    std::unordered_map<std::string, NodeMeta> metas_;
  };

} // namespace Tako

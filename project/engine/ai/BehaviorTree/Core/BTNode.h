#pragma once
#include <memory>
#include <vector>
#include <string>
#include <json.hpp>

namespace Tako {

  class BTBlackboard;

  /// <summary>
  /// ビヘイビアツリーのノード状態。
  /// </summary>
  enum class BTNodeStatus {
    Success,  ///< 成功
    Failure,  ///< 失敗
    Running   ///< 実行中
  };

  /// <summary>
  /// ビヘイビアツリーノードの基底クラス。
  /// </summary>
  class BTNode {
  public: //メンバー関数
    virtual ~BTNode() = default;

    /// <summary>
    /// ノードの実行
    /// </summary>
    /// <param name="blackboard">ブラックボード (共有データ)</param>
    /// <returns>実行結果 (Success / Failure / Running)</returns>
    virtual BTNodeStatus Execute(BTBlackboard* blackboard) = 0;

    /// <summary>
    /// ノードのリセット。状態を Failure に戻す。
    /// 派生クラスでフレーム間状態を保持する場合は override して
    /// 追加のクリア処理を行う。
    /// </summary>
    virtual void Reset() {
      status_ = BTNodeStatus::Failure;
    }

    /// <summary>
    /// JSON からパラメータを適用 (ロード時)。
    /// 既定実装は何もしない (パラメータを持たないノード用)。
    /// </summary>
    /// <param name="params">パラメータ JSON</param>
    virtual void ApplyParameters(const nlohmann::json& params) {
      (void)params;
    }

    /// <summary>
    /// パラメータを JSON として抽出 (保存用)。
    /// 既定実装は空オブジェクトを返す (パラメータを持たないノード用)。
    /// </summary>
    /// <returns>パラメータ JSON</returns>
    virtual nlohmann::json ExtractParameters() const {
      return {};
    }

#ifdef _DEBUG
    /// <summary>
    /// ImGui でパラメータ編集 UI を描画 (Debug ビルド限定、エディタ用)。
    /// </summary>
    /// <returns>パラメータ変更があれば true</returns>
    virtual bool DrawImGui() { return false; }
#endif

    //===================================
    //Setter
    //===================================
    void SetName(const std::string& name) { name_ = name; }

    //===================================
    //Getter
    //===================================
    const std::string& GetName() const { return name_; }

    BTNodeStatus GetStatus() const { return status_; }

    bool IsRunning() const { return status_ == BTNodeStatus::Running; }

    /// <summary>
    /// コンポジットノード (子ノードを持てる) かどうか。
    /// 既定は false。BTComposite で true を返すよう override する。
    /// </summary>
    /// <returns>コンポジットなら true</returns>
    virtual bool IsComposite() const { return false; }

  protected: //メンバー変数
    //状態
    BTNodeStatus status_ = BTNodeStatus::Failure;  ///< 現在の状態 (初期値 Failure)
    std::string  name_   = "BTNode";               ///< ノード名 (エディタ表示・デバッグログ用)
  };

  using BTNodePtr = std::shared_ptr<BTNode>;

} // namespace Tako

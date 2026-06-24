#pragma once
#include "BTComposite.h"
#include "BTBlackboard.h"

namespace Tako {

  /// <summary>
  /// 回数制限デコレータ。子を最大 maxUses_ 回だけ成功させる。
  /// 上限到達後は子を実行せず Failure を返す。
  /// 子が Success を返した時のみ消費し、Failure / Running は透過する。
  /// </summary>
  class BTOnce : public BTComposite {
  public: //メンバー関数
    BTOnce();

    virtual ~BTOnce() = default;

    /// <summary>
    /// ノードの実行。上限到達後は Failure。
    /// 未到達なら先頭の子を実行し、Success 時に消費数を増やす。
    /// </summary>
    /// <param name="blackboard">ブラックボード</param>
    /// <returns>実行結果</returns>
    BTNodeStatus Execute(BTBlackboard* blackboard) override;

    /// <summary>
    /// JSON からパラメータを適用。
    /// </summary>
    /// <param name="params">パラメータ JSON</param>
    void ApplyParameters(const nlohmann::json& params) override;

    /// <summary>
    /// パラメータを JSON として抽出。
    /// </summary>
    /// <returns>maxUses フィールドを含む JSON</returns>
    nlohmann::json ExtractParameters() const override;

#ifdef _DEBUG
    /// <summary>
    /// ImGui でパラメータ編集 UI を描画。
    /// </summary>
    /// <returns>変更があれば true</returns>
    bool DrawImGui() override;
#endif

    //===========================
    //Setter
    //===========================
    void SetMaxUses(int maxUses) { maxUses_ = maxUses; }

    //===========================
    //Getter
    //===========================
    int GetMaxUses() const { return maxUses_; }

    int GetUsedCount() const { return usedCount_; }

  private: //メンバー変数
    int maxUses_   = 1;  ///< 成功を許可する回数
    int usedCount_ = 0;  ///< 成功した回数。Reset() では消さず戦闘中ラッチを保持する (ツリー再生成で 0 に戻る)
  };

} // namespace Tako

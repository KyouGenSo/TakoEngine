#pragma once
#ifdef _DEBUG

#include <string>

namespace Tako {

  /// <summary>
  /// エンジンの新しいリリースがあるかを、GitHub に 1 日 1 回だけ聞く。返事は個人設定の隣の UpdateCheck.json に覚えておく
  /// </summary>
  class TAKO_API UpdateChecker {
  public: //メンバー関数
    UpdateChecker() = default;
    ~UpdateChecker();
    // 子プロセスとパイプのハンドルを持つので複製しない
    UpdateChecker(const UpdateChecker&) = delete;
    UpdateChecker& operator=(const UpdateChecker&) = delete;

    /// <summary>
    /// 今日まだ聞いていなければ、聞き始める（返事は Update が受け取る）。もう聞いていれば、覚えている返事を使う
    /// </summary>
    void Start();

    /// <summary>
    /// 返事が来ていれば読む。毎フレーム呼ぶ
    /// </summary>
    void Update();

    //=====================================
    //Getter
    //=====================================
    /// <summary>
    /// このエンジンより新しいリリースのバージョン
    /// </summary>
    /// <returns>無いか、まだ分からなければ空</returns>
    const std::string& GetNewVersion() const { return newVersion_; }

  private: //非公開関数
    /// <summary>
    /// 最新のリリースのタグ（v2.2.0 など）を受け取り、このエンジンより新しければ newVersion_ に入れて Console に出す
    /// </summary>
    void SetLatest(const std::string& tag);

  private: //メンバー変数
    void*       process_    = nullptr;  ///< curl のプロセス（HANDLE）。nullptr なら聞いていない
    void*       outputRead_ = nullptr;  ///< curl の出力を読む側のパイプ（HANDLE）
    std::string output_;
    std::string newVersion_;
  };

} // namespace Tako

#endif // _DEBUG

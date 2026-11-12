#pragma once
#ifdef _DEBUG

#include <chrono>
#include <filesystem>
#include <string>

namespace Tako {

  /// <summary>
  /// ゲームのプロジェクトを MSBuild でビルドする。VS のデバッグ中は VS からビルドできないので、エディタが子プロセスで動かす
  /// </summary>
  class TAKO_API ScriptCompiler {
  public: //メンバー関数
    ScriptCompiler() = default;
    ~ScriptCompiler();
    // 子プロセスとパイプのハンドルを持つので複製しない
    ScriptCompiler(const ScriptCompiler&) = delete;
    ScriptCompiler& operator=(const ScriptCompiler&) = delete;

    /// <summary>
    /// ビルドを始める。ビルド中なら何もしない。結果は Update が Console に出す
    /// </summary>
    void Compile();

    /// <summary>
    /// ビルドの出力を Console に流す。Auto Compile がオンなら、ウィンドウが前面に戻ったときにソースが変わっていればビルドを始める
    /// </summary>
    void Update();

    //=====================================
    //Setter
    //=====================================
    void SetAutoCompile(bool enabled) { isAutoCompile_ = enabled; }

    //=====================================
    //Getter
    //=====================================
    bool IsCompiling() const { return process_ != nullptr; }

    bool IsAutoCompile() const { return isAutoCompile_; }

  private: //非公開関数
    /// <summary>
    /// 子プロセスの出力を、今ある分だけ読む（待たない）。エラーと警告の行を Console に出す
    /// </summary>
    void ReadOutput();

    void LogLine(const std::string& line);

    /// <summary>
    /// 終わった子プロセスの結果を Console に出し、ハンドルを閉じる
    /// </summary>
    void Finish();

  private: //メンバー変数
    void*                                 process_             = nullptr;  ///< MSBuild のプロセス（HANDLE）。nullptr ならビルドしていない
    void*                                 outputRead_          = nullptr;  ///< MSBuild の出力を読む側のパイプ（HANDLE）
    std::string                           pendingOutput_;                  ///< 改行がまだ来ていない出力
    int                                   errorCount_          = 0;
    std::chrono::steady_clock::time_point startTime_;
    std::filesystem::file_time_type       attemptedSourceTime_;            ///< 最後にビルドを始めたときの、一番新しいソースの更新時刻（同じ内容でやり直さない）
    bool                                  isAutoCompile_       = true;
    bool                                  hasCheckedStartup_   = false;
    bool                                  wasFocused_          = false;
  };

} // namespace Tako

#endif // _DEBUG

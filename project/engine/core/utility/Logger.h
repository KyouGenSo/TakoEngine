#pragma once
#include <string>

namespace Tako {

  /// <summary>
  /// ログ出力ユーティリティ名前空間
  /// デバッグコンソールへのメッセージ出力機能を提供
  /// OutputDebugString 経由で Visual Studio の出力ウィンドウに表示
  /// </summary>
  namespace Logger
  {
    /// <summary>
    /// 文字列をログ出力
    /// </summary>
    /// <param name="messege">出力するメッセージ</param>
    void Log(const std::string& messege);

    /// <summary>
    /// 可変長引数でフォーマット済み文字列をログ出力
    /// printf 形式の書式指定をサポート
    /// </summary>
    /// <param name="format">フォーマット文字列</param>
    /// <param name="...">可変長引数</param>
    void Log(const char* format, ...);

    using Sink = void (*)(const std::string& message);

    /// <summary>
    /// Log の内容をもう 1 か所（エディタの Console）にも渡す。nullptr で外す
    /// </summary>
    /// <returns>前に渡してあった物（一時的に外して、戻すときに使う）</returns>
    Sink SetSink(Sink sink);
  }

} // namespace Tako

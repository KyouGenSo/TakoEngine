#pragma once
#ifdef _DEBUG

#include <string>

/// <summary>
/// 子プロセスを、出力をパイプで受けて起動する（MSBuild・vswhere・curl・cmd）
/// </summary>
namespace Tako::ChildProcess {

  /// <summary>
  /// 標準出力と標準エラーをパイプにつないで、ウィンドウなしで起動する
  /// </summary>
  /// <param name="process">起動したプロセス（HANDLE。使い終わったら CloseHandle）</param>
  /// <param name="outputRead">出力を読む側のパイプ（HANDLE。使い終わったら CloseHandle）</param>
  /// <returns>起動できなければ false</returns>
  TAKO_API bool Spawn(std::wstring commandLine, void*& process, void*& outputRead);

  /// <summary>
  /// outputRead に今ある分だけを out の後ろへ足す（待たない）
  /// </summary>
  TAKO_API void ReadAvailable(void* outputRead, std::string& out);

  /// <summary>
  /// 終わるまで待って、出力を全部返す（すぐ終わるコマンド用）
  /// </summary>
  /// <param name="exitCode">終了コードの入れ先（起動できなければ -1 が入る）</param>
  TAKO_API std::string Run(std::wstring commandLine, int* exitCode = nullptr);

} // namespace Tako::ChildProcess

#endif // _DEBUG

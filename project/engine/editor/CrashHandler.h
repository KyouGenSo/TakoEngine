#pragma once
#ifdef _DEBUG

namespace Tako {

  /// <summary>
  /// 処理されない例外で落ちるときに、Generated/Crashes へミニダンプ（VS で開くと、落ちた場所と呼び出し履歴が見える）を書くようにする。
  /// デバッガーが付いているときは、デバッガーが先に受け取るので書かれない
  /// </summary>
  TAKO_API void InstallCrashHandler();

  /// <summary>
  /// 前回の起動がダンプを書いて落ちていれば true（その印は消す）。起動のたびに 1 回呼ぶ
  /// </summary>
  TAKO_API bool ConsumeCrashMarker();

} // namespace Tako

#endif // _DEBUG

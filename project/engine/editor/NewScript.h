#pragma once

#ifdef _DEBUG

#include <string>

namespace Tako {

  /// <summary>
  /// C++ スクリプトのひな形（名前.h / 名前.cpp）を directory に作り、作業フォルダの .vcxproj と .filters に足す。
  /// 使えるようになるのは、ゲームをビルドし直してから
  /// </summary>
  /// <param name="name">クラス名（ファイル名と同じになる）</param>
  /// <param name="directory">作業フォルダからの相対パス（Script かその下）</param>
  /// <returns>失敗した理由。成功なら空</returns>
  std::string CreateScriptFiles(const std::string& name, const std::string& directory);

  /// <summary>
  /// 名前を入れる窓を、次の DrawNewScriptPopup で開く
  /// </summary>
  /// <param name="directory">作る場所（作業フォルダからの相対パス）</param>
  void RequestNewScriptPopup(const std::string& directory);

  /// <summary>
  /// 名前を入れる窓を描く。毎フレーム 1 回、他のウィンドウの外で呼ぶ
  /// </summary>
  void DrawNewScriptPopup();

} // namespace Tako

#endif // _DEBUG

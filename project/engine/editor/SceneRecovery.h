#pragma once
#ifdef _DEBUG

#include <string>
#include <vector>

/// <summary>
/// 保存していないシーンの中身を、エディタが落ちたときのために書いておく（Generated/Recovery/シーン名.scene.json）。
/// 保存したときと正常に終了したときに消すので、起動時に残っていれば、前回は落ちている
/// </summary>
namespace Tako::SceneRecovery {

  struct Entry {
    std::string sceneName;
    std::string content;  ///< BaseScene::Save の JSON の文字列
  };

  TAKO_API void Write(const std::string& sceneName, const std::string& content);

  TAKO_API void Remove(const std::string& sceneName);

  /// <summary>
  /// 残っている物を全部読む。エディタの起動時に、自分が書き始める前に呼ぶ
  /// </summary>
  TAKO_API std::vector<Entry> ReadAll();

} // namespace Tako::SceneRecovery

#endif // _DEBUG

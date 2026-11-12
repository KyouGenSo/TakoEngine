#pragma once

#include <functional>

/// <summary>
/// ゲームのコード（スクリプト・BT のノード・GameSettings）のまとまり。Debug は Game.dll、Release は exe の中にある
/// </summary>
namespace Tako::GameModule {

  /// <summary>
  /// ゲームの型を TypeRegistry に登録する。Debug は先に Game.dll を読む
  /// </summary>
  /// <returns>Game.dll を読めなければ false（ゲームの型が無いまま動く）</returns>
  TAKO_API bool Load();

  /// <summary>
  /// ゲームの型の登録を外し、Game.dll を外す。その型の物（Actor のスクリプト・BT のノード）は、呼ぶ前に全部消しておく。Release では何もしない
  /// </summary>
  TAKO_API void Unload();

  /// <summary>
  /// ビルドし直した Game.dll に差し替える（Debug だけ）
  /// </summary>
  /// <param name="beforeUnload">古い DLL を外す直前に呼ぶ。ゲームの型の物を全部消す</param>
  /// <returns>差し替えたら true（新しい DLL が読めなかった場合も）。リンカがまだ書いていて写せなければ、何もせず false</returns>
  TAKO_API bool Reload(const std::function<void()>& beforeUnload);

  //=====================================
  //Getter
  //=====================================
  /// <summary>
  /// ビルドした Game.dll が、読んである物と違えば true（Release では常に false）
  /// </summary>
  TAKO_API bool IsOutdated();

} // namespace Tako::GameModule

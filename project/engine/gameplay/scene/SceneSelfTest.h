#pragma once

#ifdef _DEBUG

namespace Tako {

  /// <summary>
  /// Actor の親子・ワールド行列・保存の往復を assert で確かめる。Debug の起動時に 1 回呼ぶ（エンジンの初期化の後）
  /// </summary>
  void RunSceneSelfTest();

} // namespace Tako

#endif // _DEBUG

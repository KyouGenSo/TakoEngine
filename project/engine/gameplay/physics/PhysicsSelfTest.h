#pragma once

#ifdef _DEBUG

namespace Tako {

  /// <summary>
  /// 物理の計算を assert で確かめる。Debug の起動時に 1 回呼ぶ（PhysicsWorld::Initialize の後）
  /// </summary>
  TAKO_API void RunPhysicsSelfTest();

} // namespace Tako

#endif // _DEBUG

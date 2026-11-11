#pragma once

#ifdef _DEBUG

namespace Tako {

  class Object3d;
  struct ObjectOverrides;

  /// <summary>
  /// 描画設定 1 件分の欄（描画フラグ・全メッシュのマテリアル・メッシュごとの表示とマテリアル）。灰色は既定、印はコードで上書き中
  /// </summary>
  /// <param name="object">既定の値・メッシュ名・コードの上書きを読む Object3d（無ければ nullptr）</param>
  /// <returns>変わったら true（呼び出し側がマテリアルを解決し直す）</returns>
  bool DrawObjectOverrides(ObjectOverrides& overrides, const Object3d* object);

} // namespace Tako

#endif // _DEBUG

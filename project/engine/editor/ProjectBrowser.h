#pragma once
#ifdef _DEBUG

namespace Tako {

  /// <summary>
  /// 新しいプロジェクトの名前と場所を入れる窓を、次の DrawNewProjectPopup で開く
  /// </summary>
  TAKO_API void RequestNewProjectPopup();

  /// <summary>
  /// 名前と場所を入れる窓を描く。Create でプロジェクトを作り、別のエディタで開く。毎フレーム 1 回、他のウィンドウの外で呼ぶ
  /// </summary>
  TAKO_API void DrawNewProjectPopup();

  /// <summary>
  /// File メニューの中身（New Project / Open Project / Open Recent）を描く。開く操作は、どれも別のエディタを起動する
  /// </summary>
  TAKO_API void DrawProjectMenuItems();

} // namespace Tako

#endif // _DEBUG

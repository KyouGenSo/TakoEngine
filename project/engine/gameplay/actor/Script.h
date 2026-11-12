#pragma once
#include "Component.h"

namespace Tako {

  /// <summary>
  /// C++ スクリプトの基底（Unity の MonoBehaviour）。ScriptComponent の中に入れて使い、Add Component の一覧には出ない。
  /// クラス名とファイル名（.h）を同じにする。Assets からのドロップとファイルを開く処理が名前で探すため
  /// </summary>
  TCLASS()
  class TAKO_API Script : public Component {
    GENERATED_BODY()

  protected: //メンバー関数
    Script() = default;
  };

} // namespace Tako

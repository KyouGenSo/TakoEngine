#include "TakoFramework.h"

#ifdef TAKO_SHARED
#include "Project.h"

#include <optional>
#endif

// Release はエンジンの静的ライブラリに入り、ゲーム側が WinMain を定義するとリンカはそちらを使う（この .obj は取り込まれない）。
// Debug は TakoEditor.exe のただ 1 つのソース。どちらの場合も、他のシンボルをここに置かない
int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int)
{
#ifdef TAKO_SHARED
  if (const std::optional<int> exitCode = Tako::Project::Startup()) {
    return *exitCode;
  }
#endif
  Tako::TakoFramework framework;
  framework.Run();
  return 0;
}

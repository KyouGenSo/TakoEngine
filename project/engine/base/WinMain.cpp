#include "TakoFramework.h"

// ゲーム側が WinMain を定義すると、リンカはそちらを使う（この .obj は取り込まれない）。他のシンボルをここに置かない
int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int)
{
  Tako::TakoFramework framework;
  framework.Run();
  return 0;
}

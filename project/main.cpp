#include"MyGame.h"

//Windowsプログラムのエントリーポイント
int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int)
{

	// MyGameのインスタンス
	MyGame myGame;

	// 初期化
	myGame.Initialize();

	//---------------------------------------------------GAMELOOP-----------------------------------------------------//

	// ウィンドウが閉じられるまでループ
	while (true)
	{
		myGame.Update();

		if (myGame.GetEndFlag()) {
			// ループを抜ける
			break;
		}

		myGame.Draw();

	}

	//-------------------------------------------------GAMELOOP-----------------------------------------------------/

	// 終了処理
	myGame.Finalize();

	return 0;
}
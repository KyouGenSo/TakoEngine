#pragma once
#include "WinApp.h"
#include<wrl.h>
#include "Vector2.h"

#define DIRECTINPUT_VERSION 0x0800 // DirectInputのバージョン指定
#include <dinput.h>
#pragma comment(lib, "dinput8.lib")
#pragma comment(lib, "dxguid.lib")

class Input {
private: 	// シングルトン
	// インスタンス
	static Input* instance_;

	Input() = default;
	Input(const Input&) = delete;
	Input& operator=(const Input&) = delete;
	~Input() = default;
public:
	// ComPtrのエイリアス
	template<class T> using ComPtr = Microsoft::WRL::ComPtr<T>;

public:
	///<summary>
	///インスタンスの取得
	///	</summary>
	static Input* GetInstance();

	/// <summary>
	/// 初期化
	/// </summary>
	void Initialize(WinApp* winApp);

	/// <summary>
	/// 終了処理
	/// </summary>
	void Finalize();

	/// <summary>
	/// 更新
	/// </summary>
	void Update();

	/// <summary>
	/// マウスの座標を更新
	/// </summary>
	void UpdateMousePos();

	/// <summary>
	/// キーの押下状態を取得
	/// </summary>
	/// <param name="key">取得したいキー</param>
	bool PushKey(BYTE keyNum) const;

	/// <summary>
	/// キーのトリガー状態を取得
	/// </summary>
	/// <param name="key">取得したいキー</param>
	bool TriggerKey(BYTE keyNum) const;

	/// <summary>
	/// キーのリリース状態を取得
	/// </summary>
	/// <param name="key">取得したいキー</param>
	bool ReleaseKey(BYTE keyNum) const;

	/// <summary>
	/// マウスの押下状態を取得
	/// </summary>
	/// <param name="button"></param>
	/// <returns></returns>
	bool PushMouse(int button) const;

	/// <summary>
	/// マウスのトリガー状態を取得
	/// </summary>
	/// <param name="button"></param>
	/// <returns></returns>
	bool TriggerMouse(int button) const;

	/// <summary>
	/// マウスのリリース状態を取得
	/// </summary>
	/// <param name="button"></param>
	/// <returns></returns>
	bool ReleaseMouse(int button) const;

	/// <summary>
	/// マウスの座標を取得
	/// </summary>
	/// <returns></returns>
	Vector2 GetMousePos();

	/// <summary>
	/// マウスの座標を設定
	/// </summary>
	/// <param name="x"></param>
	/// <param name="y"></param>
	void SetMousePos(int x, int y);


private:
	// WinAppクラスのインスタンス
	WinApp* winApp_ = nullptr;

	// DirectInputオブジェクト
	ComPtr<IDirectInput8> directInput_;

	// キーボードデバイス
	ComPtr<IDirectInputDevice8> keyboardDevice_;

	// マウスデバイス
	ComPtr<IDirectInputDevice8> mouseDevice_;

	// マウスの状態
	DIMOUSESTATE mouseState_;

	// 前フレームのマウスの状態
	DIMOUSESTATE prevMouseState_;

	// マウスの座標
	POINT mousePos_ = {};

	// キーボードの入力状態
	BYTE keys_[256] = {};

	// 前フレームのキーボード入力状態
	BYTE prevKeys_[256] = {};

};

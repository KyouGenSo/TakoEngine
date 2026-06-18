#pragma once
#include "WinApp.h"
#include<wrl.h>
#include <memory>
#include "Vector2.h"
#include "Xinput.h"
#pragma comment(lib, "XInput.lib")

#define DIRECTINPUT_VERSION 0x0800 // DirectInput のバージョン指定
#include <dinput.h>
#pragma comment(lib, "dinput8.lib")
#pragma comment(lib, "dxguid.lib")

namespace Tako {

  /// <summary>
  /// ゲームパッドボタン定数（XInput ビットフラグ値）
  /// wButtons のビットマスクとして直接使用
  /// </summary>
  struct GamepadButton
  {
    static constexpr WORD A = XINPUT_GAMEPAD_A;                      ///< A ボタン
    static constexpr WORD B = XINPUT_GAMEPAD_B;                      ///< B ボタン
    static constexpr WORD X = XINPUT_GAMEPAD_X;                      ///< X ボタン
    static constexpr WORD Y = XINPUT_GAMEPAD_Y;                      ///< Y ボタン
    static constexpr WORD DPad_Up = XINPUT_GAMEPAD_DPAD_UP;          ///< DPad 上
    static constexpr WORD DPad_Down = XINPUT_GAMEPAD_DPAD_DOWN;      ///< DPad 下
    static constexpr WORD DPad_Left = XINPUT_GAMEPAD_DPAD_LEFT;      ///< DPad 左
    static constexpr WORD DPad_Right = XINPUT_GAMEPAD_DPAD_RIGHT;    ///< DPad 右
    static constexpr WORD L_Shoulder = XINPUT_GAMEPAD_LEFT_SHOULDER; ///< 左ショルダー
    static constexpr WORD R_Shoulder = XINPUT_GAMEPAD_RIGHT_SHOULDER;///< 右ショルダー
    static constexpr WORD L_Thumbstick = XINPUT_GAMEPAD_LEFT_THUMB;  ///< 左スティック押込
    static constexpr WORD R_Thumbstick = XINPUT_GAMEPAD_RIGHT_THUMB; ///< 右スティック押込
    static constexpr WORD Start = XINPUT_GAMEPAD_START;              ///< Start ボタン
    static constexpr WORD Back = XINPUT_GAMEPAD_BACK;                ///< Back ボタン

    /// <summary>
    /// デバッグ列挙用の全ボタン配列
    /// </summary>
    static constexpr int COUNT = 14;
    static constexpr WORD ALL[COUNT] = {
        A, B, X, Y, DPad_Up, DPad_Down, DPad_Left, DPad_Right,
        L_Shoulder, R_Shoulder, L_Thumbstick, R_Thumbstick, Start, Back
    };
  };

  /// <summary>
  /// 統合入力管理クラス
  /// DirectInput と XInput でキーボード、マウス、ゲームパッド入力を処理
  /// </summary>
  class Input {
  private: 	// シングルトン
    static std::unique_ptr<Input> instance_;

    Input() = default;
    ~Input() = default;

    friend struct std::default_delete<Input>;

  public:
    Input(const Input&) = delete;
    Input& operator=(const Input&) = delete;

  public: //構造体
    template<class T> using ComPtr = Microsoft::WRL::ComPtr<T>; ///< ComPtr のエイリアス

  public: //メンバー関数
    /// <summary>
    /// インスタンスの取得
    /// </summary>
    static Input* GetInstance();

    /// <summary>
    /// 初期化
    /// </summary>
    /// <param name="winApp">ウィンドウアプリケーション</param>
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
    /// <param name="keyNum">取得したいキーコード</param>
    /// <returns>押下されている場合 true</returns>
    bool PushKey(BYTE keyNum) const;

    /// <summary>
    /// キーのトリガー状態を取得
    /// </summary>
    /// <param name="keyNum">取得したいキーコード</param>
    /// <returns>押した瞬間のみ true</returns>
    bool TriggerKey(BYTE keyNum) const;

    /// <summary>
    /// キーのリリース状態を取得
    /// </summary>
    /// <param name="keyNum">取得したいキーコード</param>
    /// <returns>離した瞬間のみ true</returns>
    bool ReleaseKey(BYTE keyNum) const;

    /// <summary>
    /// マウスの押下状態を取得
    /// </summary>
    /// <param name="button">マウスボタン番号（0:左, 1:右, 2:中央）</param>
    /// <returns>押下されている場合 true</returns>
    bool PushMouse(int button) const;

    /// <summary>
    /// マウスのトリガー状態を取得（押した瞬間のみ true）
    /// </summary>
    /// <param name="button">マウスボタン番号（0:左, 1:右, 2:中央）</param>
    /// <returns>押した瞬間のみ true</returns>
    bool TriggerMouse(int button) const;

    /// <summary>
    /// マウスのリリース状態を取得（離した瞬間のみ true）
    /// </summary>
    /// <param name="button">マウスボタン番号（0:左, 1:右, 2:中央）</param>
    /// <returns>離した瞬間のみ true</returns>
    bool ReleaseMouse(int button) const;

    /// <summary>
    /// ゲームパッドの押下状態を取得
    /// </summary>
    /// <param name="button">ボタンビットフラグ（GamepadButton 定数を使用）</param>
    /// <returns>押下されている場合 true</returns>
    bool PushButton(WORD button) const;

    /// <summary>
    /// ゲームパッドのトリガー状態を取得（押した瞬間のみ true）
    /// </summary>
    /// <param name="button">ボタンビットフラグ（GamepadButton 定数を使用）</param>
    /// <returns>押した瞬間のみ true</returns>
    bool TriggerButton(WORD button) const;

    /// <summary>
    /// ゲームパッドのリリース状態を取得（離した瞬間のみ true）
    /// </summary>
    /// <param name="button">ボタンビットフラグ（GamepadButton 定数を使用）</param>
    /// <returns>離した瞬間のみ true</returns>
    bool ReleaseButton(WORD button) const;

    /// <summary>
    /// ゲームパッドの左スティックがデッドゾーン内かどうか
    /// </summary>
    /// <returns>デッドゾーン内の場合 true</returns>
    bool LStickInDeadZone() const;

    /// <summary>
    /// ゲームパッドの右スティックがデッドゾーン内かどうか
    /// </summary>
    /// <returns>デッドゾーン内の場合 true</returns>
    bool RStickInDeadZone() const;

    /// <summary>
    /// ゲームパッドの振動を停止
    /// </summary>
    void StopVibration();

    //============================================================
    //Setter
    //============================================================
    /// <summary>
    /// マウスの座標を設定
    /// </summary>
    /// <param name="x">X 座標（スクリーン座標）</param>
    /// <param name="y">Y 座標（スクリーン座標）</param>
    void SetMousePos(int x, int y);

    /// <summary>
    /// ゲームパッドの振動を設定
    /// </summary>
    /// <param name="leftMotor">左モーターの強度（0.0 ~ 1.0）</param>
    /// <param name="rightMotor">右モーターの強度（0.0 ~ 1.0）</param>
    /// <param name="duration">振動継続時間（秒）。0以下で無限に振動</param>
    void SetVibration(float leftMotor, float rightMotor, float duration = 0.0f);

    //============================================================
    //Getter
    //============================================================
    /// <summary>
    /// マウスの座標を取得
    /// </summary>
    /// <returns>スクリーン座標系でのマウス位置</returns>
    Vector2 GetMousePos() const;

    /// <summary>
    /// ゲームパッドの接続状態を取得
    /// </summary>
    /// <returns>接続されている場合 true</returns>
    bool IsConnect() const;

    /// <summary>
    /// ゲームパッドの左スティックの値を取得
    /// </summary>
    /// <returns>正規化された左スティックの値（-1.0 ~ 1.0）</returns>
    Vector2 GetLeftStick() const;

    /// <summary>
    /// ゲームパッドの右スティックの値を取得
    /// </summary>
    /// <returns>正規化された右スティックの値（-1.0 ~ 1.0）</returns>
    Vector2 GetRightStick() const;

    /// <summary>
    /// ゲームパッドの左トリガーの値を取得
    /// </summary>
    /// <returns>正規化されたトリガー値（0.0 ~ 1.0）</returns>
    float GetLeftTrigger() const;

    /// <summary>
    /// ゲームパッドの右トリガーの値を取得
    /// </summary>
    /// <returns>正規化されたトリガー値（0.0 ~ 1.0）</returns>
    float GetRightTrigger() const;

  private: //メンバー変数
    //基盤・入力デバイス
    WinApp*                     winApp_         = nullptr;  ///< WinApp クラスのインスタンス
    ComPtr<IDirectInput8>       directInput_;               ///< DirectInput オブジェクト
    ComPtr<IDirectInputDevice8> keyboardDevice_;            ///< キーボードデバイス
    ComPtr<IDirectInputDevice8> mouseDevice_;               ///< マウスデバイス

    //マウス
    DIMOUSESTATE mouseState_;           ///< マウスの状態
    DIMOUSESTATE prevMouseState_;       ///< 前フレームのマウスの状態
    POINT        mousePos_       = {};  ///< マウスの座標

    //キーボード
    BYTE keys_[256]     = {};  ///< キーボードの入力状態
    BYTE prevKeys_[256] = {};  ///< 前フレームのキーボード入力状態

    //ゲームパッド
    XINPUT_STATE state_{};              ///< ゲームパッドの状態
    WORD         prevButtons_ = 0;      ///< 前フレームのボタンビットマスク
    bool         isConnected_ = false;  ///< ゲームパッドの接続状態

    //振動制御
    float vibrationDuration_ = 0.0f;   ///< 振動継続時間（秒）。0以下で無限
    float vibrationTimer_    = 0.0f;   ///< 振動経過時間
    bool  isVibrating_       = false;  ///< 振動中フラグ

  };

} // namespace Tako

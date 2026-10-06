#pragma once
#include "WinApp.h"
#include<wrl.h>
#include <algorithm>
#include <map>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>
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

    struct Token {};  ///< 外部からの直接生成を防ぐ生成キー
    ~Input() = default;

    friend struct std::default_delete<Input>;

  public:
    explicit Input(Token) {}
    Input(const Input&) = delete;
    Input& operator=(const Input&) = delete;

  public: //構造体
    template<class T> using ComPtr = Microsoft::WRL::ComPtr<T>; ///< ComPtr のエイリアス

    enum class Stick { None, Left, Right };

    /// <summary>
    /// 設定ファイルや UI で使うキー/ボタンの名前
    /// </summary>
    template<class T>
    struct NamedCode {
      T           code;
      const char* name;
    };

    /// <summary>
    /// どれか 1 つでも押されていればアクションが押されている扱い
    /// </summary>
    struct ActionBinding {
      std::vector<BYTE> keys;          ///< DIK コード
      std::vector<WORD> buttons;       ///< GamepadButton のビット
      std::vector<int>  mouseButtons;  ///< PushMouse と同じ番号（0:左 1:右 2:中央 3:X1）
    };

    /// <summary>
    /// 4 方向キーとスティックを合成した 2D 軸
    /// </summary>
    struct AxisBinding {
      BYTE  up    = 0;  ///< DIK コード。0 は未割当
      BYTE  down  = 0;
      BYTE  left  = 0;
      BYTE  right = 0;
      Stick stick = Stick::None;
    };

    using ActionMap   = std::map<std::string, ActionBinding, std::less<>>;
    using AxisMap     = std::map<std::string, AxisBinding, std::less<>>;
    using NameAliases = std::map<std::string, std::string, std::less<>>;  ///< 旧名 → 今の名前（空は削除済み）

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

    /// <summary>
    /// アクションに割り当てたキー/ボタンのいずれかが押されているか（未定義の名前は assert）
    /// </summary>
    bool PushAction(std::string_view name) const;

    /// <summary>
    /// アクションが押された瞬間か（割当全体で判定するため、複数キーを押し替えても再発火しない）
    /// </summary>
    bool TriggerAction(std::string_view name) const;

    /// <summary>
    /// アクションが離された瞬間か（割当がすべて離れたときのみ true）
    /// </summary>
    bool ReleaseAction(std::string_view name) const;

    /// <summary>
    /// アクション名を変える。旧名で呼ぶコード（再ビルド前）にも新しい名前の割当を返す
    /// </summary>
    void RenameAction(const std::string& from, const std::string& to);

    /// <summary>
    /// アクションを削除する。削除した名前で呼ぶコード（再ビルド前）は assert せず未入力として扱う
    /// </summary>
    void RemoveAction(const std::string& name);

    /// <summary>
    /// 軸名を変える。旧名で呼ぶコード（再ビルド前）にも新しい名前の割当を返す
    /// </summary>
    void RenameAxis(const std::string& from, const std::string& to);

    /// <summary>
    /// 軸を削除する。削除した名前で呼ぶコード（再ビルド前）は assert せず入力なしとして扱う
    /// </summary>
    void RemoveAxis(const std::string& name);

    /// <summary>
    /// 名前表から code の名前を引く
    /// </summary>
    /// <returns>表に無ければ nullptr</returns>
    template<class T>
    static const char* FindName(std::span<const NamedCode<T>> table, T code) {
      const auto it = std::ranges::find(table, code, &NamedCode<T>::code);
      return it != table.end() ? it->name : nullptr;
    }

    /// <summary>
    /// 名前表から name の code を引く
    /// </summary>
    template<class T>
    static std::optional<T> FindCode(std::span<const NamedCode<T>> table, std::string_view name) {
      const auto it = std::ranges::find_if(table, [name](const NamedCode<T>& entry) { return name == entry.name; });
      return it != table.end() ? std::optional<T>(it->code) : std::nullopt;
    }

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

    /// <summary>
    /// キーボード/マウス入力の遮断を設定（次の Update から反映。ゲームパッドは対象外）
    /// </summary>
    /// <param name="blocked">true の間、キーとマウスボタン/移動量を未入力として扱う</param>
    void SetBlocked(bool blocked) { isBlocked_ = blocked; }

    /// <summary>
    /// マウスボタンだけを未入力として扱う（次の Update から反映。移動量は遮断しない）
    /// </summary>
    void SetMouseButtonsBlocked(bool blocked) { isMouseButtonsBlocked_ = blocked; }

    /// <summary>
    /// キーボードだけを未入力として扱う（次の Update から反映）
    /// </summary>
    void SetKeyboardBlocked(bool blocked) { isKeyboardBlocked_ = blocked; }

    /// <summary>
    /// スティック各軸の入力をこの割合（0〜1）以下なら 0 として扱う
    /// </summary>
    void SetLeftStickDeadZone(float deadZone) { leftStickDeadZone_ = deadZone; }
    void SetRightStickDeadZone(float deadZone) { rightStickDeadZone_ = deadZone; }

    void SetActions(ActionMap actions) { actions_ = std::move(actions); }
    void SetAxes(AxisMap axes) { axes_ = std::move(axes); }

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
    /// <returns>正規化された左スティックの値（-1.0 ~ 1.0）。デッドゾーン内は 0</returns>
    Vector2 GetLeftStick() const;

    /// <summary>
    /// ゲームパッドの右スティックの値を取得
    /// </summary>
    /// <returns>正規化された右スティックの値（-1.0 ~ 1.0）。デッドゾーン内は 0</returns>
    Vector2 GetRightStick() const;

    /// <summary>
    /// 軸に割り当てた 4 方向キー（各 -1/0/1）とスティックの和（長さは丸めない。未定義の名前は assert）
    /// </summary>
    Vector2 GetAxis(std::string_view name) const;

    float GetLeftStickDeadZone() const { return leftStickDeadZone_; }
    float GetRightStickDeadZone() const { return rightStickDeadZone_; }
    ActionMap& GetActions() { return actions_; }
    AxisMap& GetAxes() { return axes_; }

    /// <summary>
    /// 割当可能なキーの DIK コードと名前（名前は DIK_ を除いたもの）
    /// </summary>
    static std::span<const NamedCode<BYTE>> GetKeyTable();

    /// <summary>
    /// ゲームパッドボタンのビットと名前（名前は GamepadButton のメンバ名）
    /// </summary>
    static std::span<const NamedCode<WORD>> GetButtonTable();

    /// <summary>
    /// マウスボタンの番号と名前
    /// </summary>
    static std::span<const NamedCode<int>> GetMouseButtonTable();

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

  private: //非公開関数
    /// <summary>
    /// 実行中に改名した旧名も引ける。削除済みなら nullptr、一度も無かった名前は assert し nullptr を返す
    /// </summary>
    const ActionBinding* FindAction(std::string_view name) const;

    /// <summary>
    /// 割当のいずれかが押されているか
    /// </summary>
    /// <param name="previous">true なら前フレームの状態で判定する</param>
    bool IsActionDown(const ActionBinding& action, bool previous) const;

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

    bool isBlocked_             = false;  ///< キーボード/マウス入力の遮断フラグ
    bool isMouseButtonsBlocked_ = false;
    bool isKeyboardBlocked_     = false;

    //ゲームパッド
    XINPUT_STATE state_{};              ///< ゲームパッドの状態
    WORD         prevButtons_ = 0;      ///< 前フレームのボタンビットマスク
    bool         isConnected_ = false;  ///< ゲームパッドの接続状態

    //振動制御
    float vibrationDuration_ = 0.0f;   ///< 振動継続時間（秒）。0以下で無限
    float vibrationTimer_    = 0.0f;   ///< 振動経過時間
    bool  isVibrating_       = false;  ///< 振動中フラグ

    //デッドゾーン（既定値は XInput 推奨値）
    float leftStickDeadZone_  = XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE / 32767.0f;
    float rightStickDeadZone_ = XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE / 32767.0f;

    //アクションマップ
    ActionMap   actions_;
    AxisMap     axes_;
    NameAliases actionAliases_;  ///< 実行中に改名・削除した旧名。再ビルド前のコードが旧名で呼んでも落ちないよう再起動まで持つ（SetActions でも消さない）
    NameAliases axisAliases_;

  };

} // namespace Tako

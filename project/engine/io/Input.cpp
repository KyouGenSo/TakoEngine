#include "Input.h"
#include <cassert>
#include <cstdlib>
#include "Vector2.h"
#include "FrameTimer.h"

namespace Tako {

  namespace {
#define TAKO_KEY(name) Input::NamedCode<BYTE>{ DIK_##name, #name }
    constexpr Input::NamedCode<BYTE> kKeyTable[] = {
      TAKO_KEY(A), TAKO_KEY(B), TAKO_KEY(C), TAKO_KEY(D), TAKO_KEY(E), TAKO_KEY(F), TAKO_KEY(G), TAKO_KEY(H), TAKO_KEY(I),
      TAKO_KEY(J), TAKO_KEY(K), TAKO_KEY(L), TAKO_KEY(M), TAKO_KEY(N), TAKO_KEY(O), TAKO_KEY(P), TAKO_KEY(Q), TAKO_KEY(R),
      TAKO_KEY(S), TAKO_KEY(T), TAKO_KEY(U), TAKO_KEY(V), TAKO_KEY(W), TAKO_KEY(X), TAKO_KEY(Y), TAKO_KEY(Z),
      TAKO_KEY(1), TAKO_KEY(2), TAKO_KEY(3), TAKO_KEY(4), TAKO_KEY(5), TAKO_KEY(6), TAKO_KEY(7), TAKO_KEY(8), TAKO_KEY(9), TAKO_KEY(0),
      TAKO_KEY(F1), TAKO_KEY(F2), TAKO_KEY(F3), TAKO_KEY(F4), TAKO_KEY(F5), TAKO_KEY(F6),
      TAKO_KEY(F7), TAKO_KEY(F8), TAKO_KEY(F9), TAKO_KEY(F10), TAKO_KEY(F11), TAKO_KEY(F12),
      TAKO_KEY(UP), TAKO_KEY(DOWN), TAKO_KEY(LEFT), TAKO_KEY(RIGHT),
      TAKO_KEY(SPACE), TAKO_KEY(RETURN), TAKO_KEY(ESCAPE), TAKO_KEY(TAB), TAKO_KEY(BACKSPACE), TAKO_KEY(CAPSLOCK),
      TAKO_KEY(LSHIFT), TAKO_KEY(RSHIFT), TAKO_KEY(LCONTROL), TAKO_KEY(RCONTROL), TAKO_KEY(LALT), TAKO_KEY(RALT),
      TAKO_KEY(INSERT), TAKO_KEY(DELETE), TAKO_KEY(HOME), TAKO_KEY(END), TAKO_KEY(PGUP), TAKO_KEY(PGDN),
      TAKO_KEY(MINUS), TAKO_KEY(EQUALS), TAKO_KEY(LBRACKET), TAKO_KEY(RBRACKET), TAKO_KEY(SEMICOLON), TAKO_KEY(APOSTROPHE),
      TAKO_KEY(GRAVE), TAKO_KEY(BACKSLASH), TAKO_KEY(COMMA), TAKO_KEY(PERIOD), TAKO_KEY(SLASH),
      TAKO_KEY(NUMPAD0), TAKO_KEY(NUMPAD1), TAKO_KEY(NUMPAD2), TAKO_KEY(NUMPAD3), TAKO_KEY(NUMPAD4),
      TAKO_KEY(NUMPAD5), TAKO_KEY(NUMPAD6), TAKO_KEY(NUMPAD7), TAKO_KEY(NUMPAD8), TAKO_KEY(NUMPAD9),
      TAKO_KEY(NUMPADPLUS), TAKO_KEY(NUMPADMINUS), TAKO_KEY(NUMPADSTAR), TAKO_KEY(NUMPADSLASH), TAKO_KEY(NUMPADPERIOD), TAKO_KEY(NUMPADENTER),
    };
#undef TAKO_KEY

#define TAKO_BUTTON(name) Input::NamedCode<WORD>{ GamepadButton::name, #name }
    constexpr Input::NamedCode<WORD> kButtonTable[] = {
      TAKO_BUTTON(A), TAKO_BUTTON(B), TAKO_BUTTON(X), TAKO_BUTTON(Y),
      TAKO_BUTTON(DPad_Up), TAKO_BUTTON(DPad_Down), TAKO_BUTTON(DPad_Left), TAKO_BUTTON(DPad_Right),
      TAKO_BUTTON(L_Shoulder), TAKO_BUTTON(R_Shoulder), TAKO_BUTTON(L_Thumbstick), TAKO_BUTTON(R_Thumbstick),
      TAKO_BUTTON(Start), TAKO_BUTTON(Back),
    };
#undef TAKO_BUTTON

    // c_dfDIMouse の rgbButtons は 4 個
    constexpr Input::NamedCode<int> kMouseButtonTable[] = {
      { 0, "Left" }, { 1, "Right" }, { 2, "Middle" }, { 3, "X1" },
    };

    // XInput 定数での判定と同じく、各軸が閾値以下なら入力なし
    bool InDeadZone(short x, short y, float deadZone) {
      const float threshold = deadZone * 32767.0f;
      return std::abs(x) <= threshold && std::abs(y) <= threshold;
    }

    Vector2 StickValue(short x, short y, float deadZone) {
      if (InDeadZone(x, y, deadZone)) {
        return Vector2(0.0f, 0.0f);
      }
      return Vector2(static_cast<float>(x) / 32768.0f, static_cast<float>(y) / 32768.0f);
    }

    // 実行中に改名した旧名は今の名前へ読み替える。削除済みなら nullptr、一度も無かった名前（綴り間違い）は assert
    template<class Map>
    const typename Map::mapped_type* FindByName(const Map& map, const Input::NameAliases& aliases, std::string_view name) {
      auto it = map.find(name);
      if (it == map.end()) {
        const auto alias = aliases.find(name);
        assert(alias != aliases.end() && "undefined input action or axis");
        if (alias != aliases.end()) {
          it = map.find(alias->second);
        }
      }
      return it != map.end() ? &it->second : nullptr;
    }

    template<class Map>
    void RenameEntry(Map& map, Input::NameAliases& aliases, const std::string& from, const std::string& to) {
      auto node = map.extract(from);
      if (!node) {
        return;
      }
      node.key() = to;
      map.insert(std::move(node));
      // A→B→C と続けて改名しても、最初の A で呼ぶコードが C を引けるようにする
      for (auto& [old, current] : aliases) {
        if (current == from) {
          current = to;
        }
      }
      aliases[from] = to;
      // to が以前の旧名なら、今は実在する名前として直接引ける
      aliases.erase(to);
    }

    template<class Map>
    void RemoveEntry(Map& map, Input::NameAliases& aliases, const std::string& name) {
      if (map.erase(name) == 0) {
        return;
      }
      for (auto& [old, current] : aliases) {
        if (current == name) {
          current.clear();
        }
      }
      aliases[name].clear();
    }
  }

  std::unique_ptr<Input> Input::instance_ = nullptr;

  Input* Input::GetInstance()
  {
    if (!instance_) {
      instance_ = std::make_unique<Input>(Token{});
    }
    return instance_.get();
  }

  void Input::Initialize(WinApp* winApp) {
    this->winApp_ = winApp;

    HRESULT hr;

    // DirectInput の初期化
      // DirectInput オブジェクトの生成
    hr = DirectInput8Create(winApp->GetHInstance(), DIRECTINPUT_VERSION, IID_IDirectInput8, reinterpret_cast<void**>(directInput_.GetAddressOf()), nullptr);
    assert(SUCCEEDED(hr));

    // KeyboardDevice の生成
    hr = directInput_->CreateDevice(GUID_SysKeyboard, keyboardDevice_.GetAddressOf(), NULL);
    assert(SUCCEEDED(hr));

    // KeyboardDevice のフォーマット設定
    hr = keyboardDevice_->SetDataFormat(&c_dfDIKeyboard);
    assert(SUCCEEDED(hr));

    // KeyboardDevice の協調レベル設定
    hr = keyboardDevice_->SetCooperativeLevel(winApp->GetHWnd(), DISCL_FOREGROUND | DISCL_NONEXCLUSIVE | DISCL_NOWINKEY);
    assert(SUCCEEDED(hr));

    // マウスデバイスの生成
    hr = directInput_->CreateDevice(GUID_SysMouse, mouseDevice_.GetAddressOf(), NULL);
    assert(SUCCEEDED(hr));

    // マウスデバイスのフォーマット設定
    hr = mouseDevice_->SetDataFormat(&c_dfDIMouse);
    assert(SUCCEEDED(hr));

    // マウスデバイスの協調レベル設定
    hr = mouseDevice_->SetCooperativeLevel(winApp->GetHWnd(), DISCL_FOREGROUND | DISCL_NONEXCLUSIVE);
    assert(SUCCEEDED(hr));

    // マウスの座標を取得
    POINT point;
    GetCursorPos(&point);
    ScreenToClient(winApp_->GetHWnd(), &point);
    mousePos_.x = point.x;
    mousePos_.y = point.y;

    // ゲームパッドの初期化
    ZeroMemory(&state_, sizeof(XINPUT_STATE));
    prevButtons_ = 0;
    isConnected_ = false;

  }

  void Input::Finalize()
  {
    StopVibration();
    instance_.reset();
  }

  void Input::Update() {
    // 前フレーム状態を保存（キーボード・マウス・ゲームパッド統一）
    memcpy(prevKeys_, keys_, sizeof(keys_));
    prevMouseState_ = mouseState_;
    prevButtons_ = state_.Gamepad.wButtons;

    // キーボード情報の取得
    keyboardDevice_->Acquire();
    keyboardDevice_->GetDeviceState(sizeof(keys_), keys_);

    // マウス情報の取得
    mouseDevice_->Acquire();
    mouseDevice_->GetDeviceState(sizeof(DIMOUSESTATE), &mouseState_);

    if (isBlocked_) {
      memset(keys_, 0, sizeof(keys_));
      mouseState_ = {};
    }
    if (isKeyboardBlocked_) {
      memset(keys_, 0, sizeof(keys_));
    }
    if (isMouseButtonsBlocked_) {
      memset(mouseState_.rgbButtons, 0, sizeof(mouseState_.rgbButtons));
    }

    // マウスの座標を取得
    UpdateMousePos();

    // ゲームパッドの状態を取得
    DWORD result = XInputGetState(0, &state_);
    isConnected_ = (result == ERROR_SUCCESS);
    if (!isConnected_)
    {
      ZeroMemory(&state_.Gamepad, sizeof(XINPUT_GAMEPAD));
    }

    // 振動タイマーの更新（一時停止中に鳴り続けないよう実時間で数える）
    if (isVibrating_ && vibrationDuration_ > 0.0f) {
      vibrationTimer_ += FrameTimer::GetInstance()->GetUnscaledDeltaTime();
      if (vibrationTimer_ >= vibrationDuration_) {
        StopVibration();
      }
    }
  }

  void Input::UpdateMousePos()
  {
    // マウスの座標を取得
    POINT point;
    GetCursorPos(&point);
    ScreenToClient(winApp_->GetHWnd(), &point);
    mousePos_.x = point.x;
    mousePos_.y = point.y;
  }

  bool Input::PushKey(BYTE keyNum) const
  {
    return keys_[keyNum] != 0;
  }

  bool Input::TriggerKey(BYTE keyNum) const
  {
    return keys_[keyNum] && !prevKeys_[keyNum];
  }

  bool Input::ReleaseKey(BYTE keyNum) const
  {
    return !keys_[keyNum] && prevKeys_[keyNum];
  }

  bool Input::PushMouse(int button) const
  {
    return mouseState_.rgbButtons[button] != 0;
  }

  bool Input::TriggerMouse(int button) const
  {
    return mouseState_.rgbButtons[button] && !prevMouseState_.rgbButtons[button];
  }

  bool Input::ReleaseMouse(int button) const
  {
    return !mouseState_.rgbButtons[button] && prevMouseState_.rgbButtons[button];
  }

  Vector2 Input::GetMousePos() const
  {
    return Vector2(static_cast<float>(mousePos_.x), static_cast<float>(mousePos_.y));
  }

  void Input::SetMousePos(int x, int y)
  {
    POINT point;
    point.x = x;
    point.y = y;
    ClientToScreen(winApp_->GetHWnd(), &point);
    SetCursorPos(point.x, point.y);
  }

  bool Input::IsConnect() const
  {
    return isConnected_;
  }

  bool Input::PushButton(WORD button) const
  {
    return (state_.Gamepad.wButtons & button) != 0;
  }

  bool Input::TriggerButton(WORD button) const
  {
    return (state_.Gamepad.wButtons & button) && !(prevButtons_ & button);
  }

  bool Input::ReleaseButton(WORD button) const
  {
    return !(state_.Gamepad.wButtons & button) && (prevButtons_ & button);
  }

  bool Input::LStickInDeadZone() const
  {
    return InDeadZone(state_.Gamepad.sThumbLX, state_.Gamepad.sThumbLY, leftStickDeadZone_);
  }

  bool Input::RStickInDeadZone() const
  {
    return InDeadZone(state_.Gamepad.sThumbRX, state_.Gamepad.sThumbRY, rightStickDeadZone_);
  }

  Vector2 Input::GetLeftStick() const
  {
    return StickValue(state_.Gamepad.sThumbLX, state_.Gamepad.sThumbLY, leftStickDeadZone_);
  }

  Vector2 Input::GetRightStick() const
  {
    return StickValue(state_.Gamepad.sThumbRX, state_.Gamepad.sThumbRY, rightStickDeadZone_);
  }

  bool Input::PushAction(std::string_view name) const
  {
    const ActionBinding* action = FindAction(name);
    return action && IsActionDown(*action, false);
  }

  bool Input::TriggerAction(std::string_view name) const
  {
    const ActionBinding* action = FindAction(name);
    return action && IsActionDown(*action, false) && !IsActionDown(*action, true);
  }

  bool Input::ReleaseAction(std::string_view name) const
  {
    const ActionBinding* action = FindAction(name);
    return action && !IsActionDown(*action, false) && IsActionDown(*action, true);
  }

  void Input::RenameAction(const std::string& from, const std::string& to)
  {
    RenameEntry(actions_, actionAliases_, from, to);
  }

  void Input::RemoveAction(const std::string& name)
  {
    RemoveEntry(actions_, actionAliases_, name);
  }

  void Input::RenameAxis(const std::string& from, const std::string& to)
  {
    RenameEntry(axes_, axisAliases_, from, to);
  }

  void Input::RemoveAxis(const std::string& name)
  {
    RemoveEntry(axes_, axisAliases_, name);
  }

  Vector2 Input::GetAxis(std::string_view name) const
  {
    const AxisBinding* found = FindByName(axes_, axisAliases_, name);
    if (!found) {
      return Vector2(0.0f, 0.0f);
    }

    const AxisBinding& axis    = *found;
    const auto         pressed = [this](BYTE key) { return key != 0 && PushKey(key); };
    Vector2 value(static_cast<float>(pressed(axis.right) - pressed(axis.left)),
                  static_cast<float>(pressed(axis.up) - pressed(axis.down)));
    switch (axis.stick) {
    case Stick::Left:  value += GetLeftStick();  break;
    case Stick::Right: value += GetRightStick(); break;
    default: break;
    }
    return value;
  }

  std::span<const Input::NamedCode<BYTE>> Input::GetKeyTable()
  {
    return kKeyTable;
  }

  std::span<const Input::NamedCode<WORD>> Input::GetButtonTable()
  {
    return kButtonTable;
  }

  std::span<const Input::NamedCode<int>> Input::GetMouseButtonTable()
  {
    return kMouseButtonTable;
  }

  const Input::ActionBinding* Input::FindAction(std::string_view name) const
  {
    return FindByName(actions_, actionAliases_, name);
  }

  bool Input::IsActionDown(const ActionBinding& action, bool previous) const
  {
    const BYTE*         keys    = previous ? prevKeys_ : keys_;
    const WORD          buttons = previous ? prevButtons_ : state_.Gamepad.wButtons;
    const DIMOUSESTATE& mouse   = previous ? prevMouseState_ : mouseState_;
    return std::ranges::any_of(action.keys, [keys](BYTE key) { return keys[key] != 0; }) ||
           std::ranges::any_of(action.buttons, [buttons](WORD button) { return (buttons & button) != 0; }) ||
           std::ranges::any_of(action.mouseButtons, [&mouse](int button) { return mouse.rgbButtons[button] != 0; });
  }

  float Input::GetLeftTrigger() const
  {
    // 左トリガーの値を取得
    BYTE trigger = state_.Gamepad.bLeftTrigger;

    if (trigger > XINPUT_GAMEPAD_TRIGGER_THRESHOLD) {
      return static_cast<float>(trigger) / 255.0f;
    }

    return 0.0f;
  }

  float Input::GetRightTrigger() const
  {
    // 右トリガーの値を取得
    BYTE trigger = state_.Gamepad.bRightTrigger;

    if (trigger > XINPUT_GAMEPAD_TRIGGER_THRESHOLD) {
      return static_cast<float>(trigger) / 255.0f;
    }

    return 0.0f;
  }

  void Input::SetVibration(float leftMotor, float rightMotor, float duration)
  {
    // モーターの振動設定
    XINPUT_VIBRATION vibration;
    ZeroMemory(&vibration, sizeof(XINPUT_VIBRATION));

    vibration.wLeftMotorSpeed = static_cast<WORD>(leftMotor * 65535.0f);
    vibration.wRightMotorSpeed = static_cast<WORD>(rightMotor * 65535.0f);

    XInputSetState(0, &vibration);

    // 振動タイマーの設定
    vibrationDuration_ = duration;
    vibrationTimer_ = 0.0f;
    isVibrating_ = (leftMotor > 0.0f || rightMotor > 0.0f);
  }

  void Input::StopVibration()
  {
    // モーターの振動停止
    XINPUT_VIBRATION vibration;
    ZeroMemory(&vibration, sizeof(XINPUT_VIBRATION));

    vibration.wLeftMotorSpeed = 0;
    vibration.wRightMotorSpeed = 0;

    XInputSetState(0, &vibration);

    // 振動タイマーのリセット
    isVibrating_ = false;
    vibrationTimer_ = 0.0f;
    vibrationDuration_ = 0.0f;
  }

} // namespace Tako

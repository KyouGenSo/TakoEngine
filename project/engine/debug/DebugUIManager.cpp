#include "DebugUIManager.h"
#include "FrameTimer.h"
#include "SrvManager.h"
#include "RtvManager.h"
#include "DsvManager.h"
#include "TextureManager.h"
#include "Object3dBasic.h"
#include "Light.h"
#include "Camera.h"
#include "Input.h"
#include "CollisionManager.h"
#include "PostEffectManager.h"
#include "WinApp.h"
#include "DX12Basic.h"
#include "ModelManager.h"
#include "ShadowRenderer.h"
#include "SceneManager.h"
#include "imgui_internal.h"
#include "LineRenderer.h"
#include "GPUParticle.h"
#include "Logger.h"
#include "EmitterManager.h"
#include "GlobalVariables.h"
#include "Audio.h"

#include <algorithm>
#include <iomanip>
#include <numbers>
#include <sstream>
#include <set>
#include <map>
#include <cstring>
#include <chrono>
#include <format>

namespace Tako {

  namespace {
    using Window = DebugUIManager::Window;

    // F12 / F10 で一括切り替えするメインウィンドウ
    constexpr Window kMainWindows[] = {
      Window::SceneHierarchy, Window::Inspector, Window::GameViewport, Window::Console, Window::Performance, Window::Assets
    };

    constexpr float  kMenuBarGap        = 6.0f;                               // メニューバーの区切り線の左右に足す余白(px)
    constexpr float  kMenuBarRightInset = 12.0f;                              // 右端の表示とウィンドウ端の間隔(px)
    constexpr ImVec4 kPausedColor       = ImVec4(0.25f, 0.50f, 0.95f, 1.0f);  // 一時停止中の強調色
    constexpr float  kIconButtonAspect  = 1.6f;                               // アイコンボタンの幅 / 高さ

    /// <summary>
    /// メニューバー用の区切り線（メニューバー内の Separator は縦線になる）
    /// </summary>
    void MenuBarSeparator() {
      ImGui::Dummy(ImVec2(kMenuBarGap, 0.0f));
      // 既定の Separator 色はメニューバー背景とほぼ同色で見えないため、控えめな文字色を使う
      ImGui::PushStyleColor(ImGuiCol_Separator, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
      ImGui::Separator();
      ImGui::PopStyleColor();
      ImGui::Dummy(ImVec2(kMenuBarGap, 0.0f));
    }

    /// <summary>
    /// MenuBarSeparator が占める幅（右寄せの幅計算用）
    /// </summary>
    float MenuBarSeparatorWidth() {
      // Dummy・縦線(1px)・Dummy の 3 アイテム分の幅と、それぞれの後ろに入る ItemSpacing
      return kMenuBarGap * 2.0f + 1.0f + ImGui::GetStyle().ItemSpacing.x * 3.0f;
    }

    enum class ToolbarIcon { Play, Pause, Step };

    /// <summary>
    /// 図形アイコン付きボタン（既定フォントに記号グリフが無いため DrawList で描く）
    /// </summary>
    bool IconButton(const char* id, ToolbarIcon icon, const char* tooltip, bool isActive) {
      const float height = ImGui::GetFrameHeight();
      if (isActive) {
        ImGui::PushStyleColor(ImGuiCol_Button, kPausedColor);
      }
      const bool pressed = ImGui::Button(id, ImVec2(height * kIconButtonAspect, height));
      if (isActive) {
        ImGui::PopStyleColor();
      }
      ImGui::SetItemTooltip("%s", tooltip);

      const ImVec2 rectMin = ImGui::GetItemRectMin();
      const ImVec2 rectMax = ImGui::GetItemRectMax();
      const ImVec2 center((rectMin.x + rectMax.x) * 0.5f, (rectMin.y + rectMax.y) * 0.5f);
      const float  half  = height * 0.25f;  // アイコンの半分の大きさ
      const ImU32  color = ImGui::GetColorU32(ImGuiCol_Text);
      ImDrawList*  drawList = ImGui::GetWindowDrawList();

      // 三角形は ImGui の AA 前提に合わせて時計回り（左上 → 右中 → 左下）で渡す
      switch (icon) {
      case ToolbarIcon::Play:
        drawList->AddTriangleFilled(ImVec2(center.x - half * 0.8f, center.y - half), ImVec2(center.x + half, center.y),
          ImVec2(center.x - half * 0.8f, center.y + half), color);
        break;
      case ToolbarIcon::Pause:
        drawList->AddRectFilled(ImVec2(center.x - half, center.y - half), ImVec2(center.x - half * 0.3f, center.y + half), color);
        drawList->AddRectFilled(ImVec2(center.x + half * 0.3f, center.y - half), ImVec2(center.x + half, center.y + half), color);
        break;
      case ToolbarIcon::Step:
        drawList->AddTriangleFilled(ImVec2(center.x - half, center.y - half), ImVec2(center.x + half * 0.4f, center.y),
          ImVec2(center.x - half, center.y + half), color);
        drawList->AddRectFilled(ImVec2(center.x + half * 0.5f, center.y - half), ImVec2(center.x + half, center.y + half), color);
        break;
      }
      return pressed;
    }
  }

  // シングルトンインスタンス
  std::unique_ptr<DebugUIManager> DebugUIManager::instance_ = nullptr;

  DebugUIManager::DebugUIManager(Token) {}
  DebugUIManager::~DebugUIManager() = default;

  DebugUIManager* DebugUIManager::GetInstance() {
    if (!instance_) {
      instance_ = std::make_unique<DebugUIManager>(Token{});
    }
    return instance_.get();
  }

  void DebugUIManager::Initialize() {
    // 起動時に開くウィンドウ（それ以外は windowVisibility_ の値初期化で false）
    WindowFlag(Window::GameViewport) = true;
    WindowFlag(Window::SceneHierarchy) = true;
    WindowFlag(Window::Inspector) = true;
    WindowFlag(Window::Console) = true;
    WindowFlag(Window::Assets) = true;

    primitiveEditor_.Initialize(&WindowFlag(Window::PrimitiveEditor));
    particleEditor_.Initialize(&WindowFlag(Window::ParticleEditor), &engineSettings_.GetParticleEditorGrid());
    debugViewport_.Initialize(&WindowFlag(Window::DebugViewport), &engineSettings_.GetDebugViewportGrid());
    engineSettings_.Initialize(&WindowFlag(Window::EngineSettings));
    assetBrowser_.Initialize(&WindowFlag(Window::Assets));

    // 初期ログ
    AddLog("DebugUIManager Initialized", LogType::Info);
    AddLog("TakoEngine Ready", LogType::Info);
  }

  void DebugUIManager::Finalize() {
    ClearLogs();
    primitiveEditor_.Finalize();
    particleEditor_.Finalize();
    debugViewport_.Finalize();
    instance_.reset();
  }

  void DebugUIManager::Update() {
    if (Input::GetInstance()->TriggerKey(DIK_F1)) {
      ToggleWindow(Window::DebugViewport);
    }
    if (Input::GetInstance()->TriggerKey(DIK_F2)) {
      ToggleWindow(Window::SceneHierarchy);
    }
    if (Input::GetInstance()->TriggerKey(DIK_F3)) {
      ToggleWindow(Window::Inspector);
    }
    if (Input::GetInstance()->TriggerKey(DIK_F4)) {
      ToggleWindow(Window::GameViewport);
    }
    if (Input::GetInstance()->TriggerKey(DIK_F5)) {
      ToggleWindow(Window::Console);
    }
    if (Input::GetInstance()->TriggerKey(DIK_F6)) {
      ToggleWindow(Window::Performance);
    }
    if (Input::GetInstance()->TriggerKey(DIK_F7)) {
      SetGamePaused(!FrameTimer::GetInstance()->IsPaused());
    }
    if (Input::GetInstance()->TriggerKey(DIK_F8)) {
      StepFrame();
    }
    if (Input::GetInstance()->TriggerKey(DIK_F12)) {
      for (Window window : kMainWindows) {
        ToggleWindow(window);
      }
    }
    if (Input::GetInstance()->TriggerKey(DIK_F10)) {
      for (Window window : kMainWindows) {
        WindowFlag(window) = true;
      }
    }

    // 表示状態に関わらず毎フレーム呼ぶ（非表示検知でプレビューを解放するため）
    primitiveEditor_.Update();
    particleEditor_.Update();
    debugViewport_.Update();
  }

  void DebugUIManager::Draw() {
#ifdef _DEBUG
    isPreviewInputCaptured_ = false;

    // メインメニューバー
    DrawMainMenuBar();

    // 各ウィンドウの描画
    if (WindowFlag(Window::SceneHierarchy)) DrawSceneHierarchy();
    if (WindowFlag(Window::Inspector)) DrawInspector();
    if (WindowFlag(Window::Console)) DrawConsole();
    if (WindowFlag(Window::Performance)) DrawPerformance();
    if (WindowFlag(Window::GameViewport)) DrawGameViewport();
    assetBrowser_.Draw();
    isPreviewInputCaptured_ |= debugViewport_.Draw();
    engineSettings_.Draw();
    if (WindowFlag(Window::InputDebug)) DrawInputDebug();
    if (WindowFlag(Window::CollisionDebug)) DrawCollisionDebug();
    isPreviewInputCaptured_ |= particleEditor_.Draw();
    isPreviewInputCaptured_ |= primitiveEditor_.Draw();
    if (WindowFlag(Window::ImGuiDemo)) ImGui::ShowDemoWindow(&WindowFlag(Window::ImGuiDemo));
    if (WindowFlag(Window::ImGuiMetrics)) ImGui::ShowMetricsWindow(&WindowFlag(Window::ImGuiMetrics));
    if (WindowFlag(Window::About)) DrawAbout();

    // GlobalVariables（グループがない場合は警告を出して閉じる）
    if (WindowFlag(Window::GlobalVariables)) {
      if (GlobalVariables::GetInstance()->HasGroups()) {
        GlobalVariables::GetInstance()->Update();
      }
      else {
        AddLog("GlobalVariables: No groups registered. Window will not open.", LogType::Warning);
        WindowFlag(Window::GlobalVariables) = false;
      }
    }

    // PostEffect は独自の描画を持つ
    if (WindowFlag(Window::PostEffect)) {
      PostEffectManager::GetInstance()->DrawImgui();
    }

    // ImGui 構築はゲーム更新より後なので、判定結果は次フレームの Input::Update で反映される
    Input::GetInstance()->SetBlocked(isPreviewInputCaptured_);
#endif
  }

  void DebugUIManager::DrawMainMenuBar() {
    if (ImGui::BeginMainMenuBar()) {
      if (ImGui::BeginMenu("File")) {
        SceneManager* sceneManager = SceneManager::GetInstance();
        const std::string& currentScene = sceneManager->GetCurrentSceneName();

        if (ImGui::BeginMenu("Scene")) {
          for (const std::string& name : sceneManager->GetSceneNames()) {
            if (ImGui::MenuItem(name.c_str(), nullptr, name == currentScene)) {
              RequestSceneChange(name);
            }
          }
          ImGui::EndMenu();
        }
        if (ImGui::MenuItem("Reload Current Scene", nullptr, false, !currentScene.empty())) {
          RequestSceneChange(currentScene);
        }

        ImGui::Separator();
        if (ImGui::MenuItem("Save All Global Variables")) {
          GlobalVariables::GetInstance()->SaveAllFiles();
          AddLog("GlobalVariables: all groups saved", LogType::Info);
        }
        if (ImGui::MenuItem("Reload Global Variables")) {
          GlobalVariables::GetInstance()->LoadFiles();
          AddLog("GlobalVariables: reloaded from files", LogType::Info);
        }

        ImGui::Separator();
        if (ImGui::MenuItem("Exit", "Alt+F4")) {
          if (pEndFlag_) *pEndFlag_ = true;
        }
        ImGui::EndMenu();
      }

      MenuBarSeparator();

      if (ImGui::BeginMenu("Edit")) {
        ImGui::MenuItem("Engine Settings...", nullptr, &WindowFlag(Window::EngineSettings));
        ImGui::EndMenu();
      }

      MenuBarSeparator();

      if (ImGui::BeginMenu("Window")) {
        ImGui::SeparatorText("Main Windows");
        ImGui::MenuItem("Debug Viewport", "F1", &WindowFlag(Window::DebugViewport));
        ImGui::MenuItem("Scene Hierarchy", "F2", &WindowFlag(Window::SceneHierarchy));
        ImGui::MenuItem("Inspector", "F3", &WindowFlag(Window::Inspector));
        ImGui::MenuItem("Game Viewport", "F4", &WindowFlag(Window::GameViewport));
        ImGui::MenuItem("Console", "F5", &WindowFlag(Window::Console));
        ImGui::MenuItem("Performance", "F6", &WindowFlag(Window::Performance));
        ImGui::MenuItem("Assets", nullptr, &WindowFlag(Window::Assets));
        ImGui::SeparatorText("Debug Windows");
        ImGui::MenuItem("Input Debug", nullptr, &WindowFlag(Window::InputDebug));
        ImGui::MenuItem("Collision Debug", nullptr, &WindowFlag(Window::CollisionDebug));
        ImGui::MenuItem("PostEffect Settings", nullptr, &WindowFlag(Window::PostEffect));
        ImGui::SeparatorText("Layout");
        if (ImGui::MenuItem("Show Main Windows", "F10")) {
          for (Window window : kMainWindows) {
            WindowFlag(window) = true;
          }
        }
        if (ImGui::MenuItem("Toggle Main Windows", "F12")) {
          for (Window window : kMainWindows) {
            ToggleWindow(window);
          }
        }
        ImGui::EndMenu();
      }

      MenuBarSeparator();

      if (ImGui::BeginMenu("Tools")) {
        ImGui::MenuItem("Particle Editor", nullptr, &WindowFlag(Window::ParticleEditor));
        ImGui::MenuItem("Primitive Editor", nullptr, &WindowFlag(Window::PrimitiveEditor));
        ImGui::MenuItem("Global Variables", nullptr, &WindowFlag(Window::GlobalVariables));
        ImGui::EndMenu();
      }

      MenuBarSeparator();

      if (ImGui::BeginMenu("Debug")) {
        bool paused = FrameTimer::GetInstance()->IsPaused();
        if (ImGui::MenuItem("Pause", "F7", &paused)) {
          SetGamePaused(paused);
        }
        if (ImGui::MenuItem("Step Frame", "F8")) {
          StepFrame();
        }
        ImGui::Separator();

        CollisionManager* collision = CollisionManager::GetInstance();
        bool colliderVisible = collision->IsDebugDrawEnabled();
        if (ImGui::MenuItem("Collider Visibility", nullptr, &colliderVisible)) {
          collision->SetDebugDrawEnabled(colliderVisible);
        }

        ShadowRenderer* shadow = ShadowRenderer::GetInstance();
        bool shadowEnabled = shadow->IsEnabled();
        if (ImGui::MenuItem("Shadows", nullptr, &shadowEnabled)) {
          shadow->SetEnabled(shadowEnabled);
        }

        if (ImGui::BeginMenu("Time Scale")) {
          FrameTimer* timer = FrameTimer::GetInstance();
          for (float preset : EngineSettingsWindow::kTimeScalePresets) {
            if (ImGui::MenuItem(std::format("x{}", preset).c_str(), nullptr, timer->GetTimeScale() == preset)) {
              timer->SetTimeScale(preset);
            }
          }
          ImGui::EndMenu();
        }

        bool fullScreen = WinApp::GetInstance()->IsFullScreen();
        if (ImGui::MenuItem("Fullscreen", "F11", &fullScreen)) {
          engineSettings_.SetFullScreen(fullScreen);
        }
        ImGui::EndMenu();
      }

      MenuBarSeparator();

      if (ImGui::BeginMenu("Help")) {
        ImGui::MenuItem("ImGui Demo", nullptr, &WindowFlag(Window::ImGuiDemo));
        ImGui::MenuItem("ImGui Metrics", nullptr, &WindowFlag(Window::ImGuiMetrics));
        ImGui::Separator();
        ImGui::MenuItem("About & Shortcuts", nullptr, &WindowFlag(Window::About));
        ImGui::EndMenu();
      }

      // 中央にエンジン名を表示
      ImGui::SetCursorPosX(ImGui::GetWindowWidth() / 2.0f - 40.0f);
      ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "TakoEngine");

      DrawFrameStats();

      ImGui::EndMainMenuBar();
    }
  }

  void DebugUIManager::DrawFrameStats() {
    constexpr const char* kPausedText = "PAUSED";

    const float       fps      = FrameTimer::GetInstance()->GetFPS();
    const bool        isPaused = FrameTimer::GetInstance()->IsPaused();
    const std::string fpsText  = std::format("FPS {:.1f}", fps);
    const std::string msText   = std::format("{:.1f} ms", fps > 0.0f ? 1000.0f / fps : 0.0f);

    // 右寄せのため描画するアイテムの合計幅を先に求める（各アイテムの後ろには ItemSpacing が入る）
    const float spacing = ImGui::GetStyle().ItemSpacing.x;
    float width = ImGui::CalcTextSize(fpsText.c_str()).x + spacing + MenuBarSeparatorWidth() + ImGui::CalcTextSize(msText.c_str()).x;
    if (isPaused) {
      width += ImGui::CalcTextSize(kPausedText).x + spacing + MenuBarSeparatorWidth();
    }
    ImGui::SetCursorPosX(ImGui::GetWindowWidth() - width - kMenuBarRightInset);

    if (isPaused) {
      ImGui::TextColored(kPausedColor, "%s", kPausedText);
      MenuBarSeparator();
    }

    ImVec4 fpsColor(1.0f, 0.3f, 0.3f, 1.0f);
    if (fps >= 55.0f) {
      fpsColor = ImVec4(0.2f, 1.0f, 0.2f, 1.0f);
    }
    else if (fps >= 30.0f) {
      fpsColor = ImVec4(1.0f, 0.8f, 0.2f, 1.0f);
    }
    ImGui::TextColored(fpsColor, "%s", fpsText.c_str());
    MenuBarSeparator();
    ImGui::TextUnformatted(msText.c_str());
  }

  void DebugUIManager::DrawPlaybackToolbar() {
    const bool  isPaused   = FrameTimer::GetInstance()->IsPaused();
    const float groupWidth = ImGui::GetFrameHeight() * kIconButtonAspect * 2.0f + ImGui::GetStyle().ItemSpacing.x;
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (ImGui::GetContentRegionAvail().x - groupWidth) * 0.5f);

    const bool isToggled = isPaused
      ? IconButton("##Resume", ToolbarIcon::Play, "Resume (F7)", true)
      : IconButton("##Pause", ToolbarIcon::Pause, "Pause (F7)", false);
    if (isToggled) {
      SetGamePaused(!isPaused);
    }

    ImGui::SameLine();
    if (IconButton("##Step", ToolbarIcon::Step, "Step (F8)", false)) {
      StepFrame();
    }
  }

  void DebugUIManager::SetGamePaused(bool paused) {
    FrameTimer* timer = FrameTimer::GetInstance();
    if (timer->IsPaused() == paused) {
      return;
    }
    timer->SetPaused(paused);
    Audio::GetInstance()->SetPaused(paused);
    AddLog(paused ? "Game paused" : "Game resumed", LogType::Info);
  }

  void DebugUIManager::StepFrame() {
    SetGamePaused(true);
    FrameTimer::GetInstance()->RequestStep();
  }

  void DebugUIManager::DrawSceneHierarchy() {
    ImGui::Begin("Scene Hierarchy", &WindowFlag(Window::SceneHierarchy));

    SceneManager* sceneManager = SceneManager::GetInstance();
    ImGui::Text("Current Scene: %s", sceneManager->GetCurrentSceneName().c_str());

    // シーン遷移 UI
    ImGui::PushItemWidth(120.0f);  // 入力ボックスの幅を設定
    ImGui::InputText("##SceneName", sceneNameBuffer_, sizeof(sceneNameBuffer_));
    ImGui::PopItemWidth();
    ImGui::SameLine();
    if (ImGui::Button("Change Scene")) {
      if (strlen(sceneNameBuffer_) > 0) {
        RequestSceneChange(sceneNameBuffer_);
        sceneNameBuffer_[0] = '\0';
      }
      else {
        AddLog("Scene name is empty", LogType::Warning);
      }
    }

    std::string availableScenes;
    for (const std::string& name : sceneManager->GetSceneNames()) {
      availableScenes += availableScenes.empty() ? name : ", " + name;
    }
    ImGui::TextDisabled("Available scenes: %s", availableScenes.c_str());

    ImGui::Separator();

    // ゲームオブジェクト一覧（選択可能）
    if (!gameObjects_.empty()) {
      ImGui::Text("Game Objects:");
      ImGui::Separator();

      for (int i = 0; i < static_cast<int>(gameObjects_.size()); i++) {
        ImGuiTreeNodeFlags nodeFlags = ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;

        // 選択されているオブジェクトはハイライト表示
        if (selectedObjectIndex_ == i) {
          nodeFlags |= ImGuiTreeNodeFlags_Selected;
        }

        // オブジェクト名を表示（クリック可能）
        ImGui::TreeNodeEx(reinterpret_cast<void*>(static_cast<intptr_t>(i)),
          nodeFlags, "%s", gameObjects_[i].name.c_str());

        // クリックされたら選択
        if (ImGui::IsItemClicked()) {
          selectedObjectIndex_ = i;
          selectedEngineObject_ = EngineObject::None;
        }
      }

      ImGui::Spacing();
      ImGui::Separator();
    }

    // エンジンオブジェクト
    ImGui::Text("Engine Objects:");
    ImGui::Separator();

    constexpr std::pair<EngineObject, const char*> kEngineObjects[] = {
      { EngineObject::MainCamera,       "Main Camera" },
      { EngineObject::DirectionalLight, "Directional Light" },
    };
    for (const auto& [object, name] : kEngineObjects) {
      if (ImGui::Selectable(name, selectedEngineObject_ == object)) {
        selectedEngineObject_ = object;
        selectedObjectIndex_ = -1;
      }
    }

    ImGui::End();
  }

  void DebugUIManager::RequestSceneChange(const std::string& sceneName) {
    AddLog("Scene change requested: " + sceneName, LogType::Info);
    SceneManager::GetInstance()->ChangeScene(sceneName);
  }

  void DebugUIManager::DrawInspector() {
    ImGui::Begin("Inspector", &WindowFlag(Window::Inspector));

    // 選択されたゲームオブジェクトがある場合
    if (selectedObjectIndex_ >= 0 && selectedObjectIndex_ < static_cast<int>(gameObjects_.size())) {
      // 選択されたオブジェクトの情報を表示
      const auto& selectedObject = gameObjects_[selectedObjectIndex_];
      ImGui::Text("Selected: %s", selectedObject.name.c_str());
      ImGui::Separator();

      // オブジェクトの DrawImGui 関数を呼び出す
      if (selectedObject.drawImGuiFunc) {
        selectedObject.drawImGuiFunc();
      }
    }
    else if (selectedEngineObject_ == EngineObject::MainCamera) {
      ImGui::Text("Selected: Main Camera");
      ImGui::Separator();
      DrawCameraInspector();
    }
    else if (selectedEngineObject_ == EngineObject::DirectionalLight) {
      ImGui::Text("Selected: Directional Light");
      ImGui::Separator();
      DrawLightInspector();
    }
    else {
      ImGui::Text("No object selected");
      ImGui::TextDisabled("Select an object from Scene Hierarchy");
    }

    ImGui::End();
  }

  void DebugUIManager::DrawCameraInspector() {
    Camera** cameraPtr = Object3dBasic::GetInstance()->GetCamera();
    if (!cameraPtr || !*cameraPtr) {
      ImGui::TextDisabled("No camera");
      return;
    }
    Camera* camera = *cameraPtr;

    Vector3 position = camera->GetTranslate();
    if (ImGui::DragFloat3("Position", &position.x, 0.1f)) {
      camera->SetTranslate(position);
    }
    Vector3 rotation = camera->GetRotate();
    if (ImGui::DragFloat3("Rotation", &rotation.x, 0.01f)) {
      camera->SetRotate(rotation);
    }

    float fov = camera->GetFovY() * 180.0f / std::numbers::pi_v<float>;
    if (ImGui::DragFloat("Field of View", &fov, 0.1f, 10.0f, 120.0f)) {
      camera->SetFovY(fov * std::numbers::pi_v<float> / 180.0f);
    }
    float nearClip = camera->GetNearClip();
    if (ImGui::DragFloat("Near Clip", &nearClip, 0.01f, 0.01f, 10.0f)) {
      camera->SetNearClip(nearClip);
    }
    float farClip = camera->GetFarClip();
    if (ImGui::DragFloat("Far Clip", &farClip, 1.0f, 10.0f, 10000.0f)) {
      camera->SetFarClip(farClip);
    }
  }

  void DebugUIManager::DrawLightInspector() {
    Light* light = Object3dBasic::GetInstance()->GetLight();
    if (!light) {
      ImGui::TextDisabled("No light");
      return;
    }

    const Light::DirectionalLight& dirLight = light->GetDirectionalLight();
    Vector3 dir = dirLight.direction;
    Vector4 color = dirLight.color;
    float intensity = dirLight.intensity;

    if (ImGui::DragFloat3("Direction", &dir.x, 0.01f)) {
      Object3dBasic::GetInstance()->SetDirectionalLightDirection(dir);
    }
    if (ImGui::ColorEdit4("Color", &color.x)) {
      Object3dBasic::GetInstance()->SetDirectionalLightColor(color);
    }
    if (ImGui::DragFloat("Intensity", &intensity, 0.01f, 0.0f, 10.0f)) {
      Object3dBasic::GetInstance()->SetDirectionalLightIntensity(intensity);
    }
  }

  void DebugUIManager::DrawConsole() {
    ImGui::Begin("Console", &WindowFlag(Window::Console));

    // 自動スクロールチェックボックス
    ImGui::Checkbox("Auto Scroll", &autoScroll_);
    // フィルタボタン
    if (ImGui::Button("Clear")) {
      ClearLogs();
    }
    ImGui::SameLine();
    ImGui::Checkbox("Info", &showInfo_);
    ImGui::SameLine();
    ImGui::Checkbox("Warning", &showWarning_);
    ImGui::SameLine();
    ImGui::Checkbox("Error", &showError_);

    ImGui::Separator();

    // ログ表示
    ImGui::BeginChild("ScrollingRegion", ImVec2(0, 0), false, ImGuiWindowFlags_HorizontalScrollbar);

    for (const auto& log : consoleLogs_) {
      if ((log.type == LogType::Info && !showInfo_) ||
        (log.type == LogType::Warning && !showWarning_) ||
        (log.type == LogType::Error && !showError_)) {
        continue;
      }

      ImVec4 color;
      const char* prefix = "";
      switch (log.type) {
      case LogType::Info:
        color = ImVec4(0.8f, 0.8f, 0.8f, 1.0f);
        prefix = "[INFO]";
        break;
      case LogType::Warning:
        color = ImVec4(1.0f, 1.0f, 0.0f, 1.0f);
        prefix = "[WARNING]";
        break;
      case LogType::Error:
        color = ImVec4(1.0f, 0.0f, 0.0f, 1.0f);
        prefix = "[ERROR]";
        break;
      }

      ImGui::TextColored(color, "%s %s %s", log.timestamp.c_str(), prefix, log.message.c_str());
    }

    // 自動スクロール
    if (autoScroll_) {
      ImGui::SetScrollHereY(1.0f);
    }

    ImGui::EndChild();
    ImGui::End();
  }

  void DebugUIManager::DrawPerformance() {
    ImGui::Begin("Performance", &WindowFlag(Window::Performance));

    float fps = FrameTimer::GetInstance()->GetFPS();
    float frameTime = 1000.0f / fps;

    ImGui::Text("Current FPS: %.1f", fps);
    ImGui::Text("Frame Time: %.2f ms", frameTime);

    // FPS グラフ
    fpsHistory_[fpsHistoryIndex_] = fps;
    fpsHistoryIndex_ = (fpsHistoryIndex_ + 1) % 100;

    ImGui::PlotLines("FPS History", fpsHistory_, 100, fpsHistoryIndex_,
      nullptr, 0.0f, 120.0f, ImVec2(0, 80));

    // 統計情報
    float minFPS = 120.0f, maxFPS = 0.0f, avgFPS = 0.0f;
    for (int i = 0; i < 100; i++) {
      if (fpsHistory_[i] > 0) {
        minFPS = (std::min)(minFPS, fpsHistory_[i]);
        maxFPS = (std::max)(maxFPS, fpsHistory_[i]);
        avgFPS += fpsHistory_[i];
      }
    }
    avgFPS /= 100.0f;

    ImGui::Text("Min: %.1f | Max: %.1f | Avg: %.1f", minFPS, maxFPS, avgFPS);

    if (ImGui::CollapsingHeader("Resources", ImGuiTreeNodeFlags_DefaultOpen)) {
      // index 0 は無効番兵のため使用可能数は最大数 - 1
      auto drawHeapUsage = [](const char* label, uint32_t allocated, uint32_t max) {
        const uint32_t usable = max - 1;
        ImGui::ProgressBar(static_cast<float>(allocated) / static_cast<float>(usable), ImVec2(-FLT_MIN, 0.0f),
          std::format("{}: {} / {}", label, allocated, usable).c_str());
      };
      drawHeapUsage("SRV", SrvManager::GetInstance()->GetAllocatedCount(), SrvManager::kMaxSRVCount);
      drawHeapUsage("RTV", RtvManager::GetInstance()->GetAllocatedCount(), RtvManager::kMaxRTVCount);
      drawHeapUsage("DSV", DsvManager::GetInstance()->GetAllocatedCount(), DsvManager::kMaxDSVCount);

      ImGui::Separator();
      ImGui::Text("Textures:  %zu", TextureManager::GetInstance()->GetLoadedTextureCount());
      ImGui::Text("Models:    %zu", ModelManager::GetInstance()->GetLoadedModelCount());
      ImGui::Text("Colliders: %zu", CollisionManager::GetInstance()->GetColliderCount());

      GPUParticle* particle = GPUParticle::GetInstance();
      ImGui::Text("Emitters:  %u", particle->GetEmitterCount());
      ImGui::Text("Particles: %u / %u", particle->GetActiveParticleCount(), GPUParticle::GetMaxParticleCount());
    }

    ImGui::End();
  }

  void DebugUIManager::DrawGameViewport() {
    // 毎フレームリセット（折りたたみ/非表示タブ時は hover 無しのまま）
    isGameViewportHovered_ = false;

    // Begin が false（折りたたみ等）の場合は中身を描かない。Begin/End は対で必ず呼ぶ
    if (ImGui::Begin("Game Viewport", &WindowFlag(Window::GameViewport))) {
      DrawPlaybackToolbar();

      // ウィンドウの利用可能サイズを取得（ツールバーの下の残り領域）
      ImVec2 availableSize = ImGui::GetContentRegionAvail();

      // クライアント領域のアスペクト比を計算
      float aspectRatio = 16.0f / 9.0f;

      // アスペクト比を維持したサイズを計算
      ImVec2 imageSize;
      float availableAspect = availableSize.x / availableSize.y;

      if (availableAspect > aspectRatio) {
        imageSize.y = availableSize.y;
        imageSize.x = imageSize.y * aspectRatio;
      }
      else {
        imageSize.x = availableSize.x;
        imageSize.y = imageSize.x / aspectRatio;
      }

      // 画像を中央に配置
      ImVec2 cursorPos = ImGui::GetCursorPos();
      cursorPos.x += (availableSize.x - imageSize.x) * 0.5f;
      cursorPos.y += (availableSize.y - imageSize.y) * 0.5f;
      ImGui::SetCursorPos(cursorPos);

      // ゲーム画面を表示
      uint32_t srvIndex = PostEffectManager::GetInstance()->GetFinalResultSrvIndex();
      D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle = SrvManager::GetInstance()->GetGPUDescriptorHandle(srvIndex);
      ImGui::Image((ImTextureID)gpuHandle.ptr, imageSize);

      // ゲーム画像の上にカーソルがあるか（レターボックス余白・タイトルバーは除外）
      isGameViewportHovered_ = ImGui::IsItemHovered();
    }

    ImGui::End();
  }

  bool DebugUIManager::IsCursorOverGameView() const {
    // GameViewport 表示中: ゲーム画像上にカーソルがあるか
    if (IsWindowVisible(Window::GameViewport)) {
      return isGameViewportHovered_;
    }
    // 非表示(フルスクリーン直描画)中: いずれの ImGui ウィンドウにもカーソルが無いか
    return !ImGui::GetIO().WantCaptureMouse;
  }

  void DebugUIManager::DrawInputDebug() {
    ImGui::Begin("Input Debug", &WindowFlag(Window::InputDebug));

    // ゲームパッド情報
    if (ImGui::CollapsingHeader("GamePad", ImGuiTreeNodeFlags_DefaultOpen)) {
      Input* input = Input::GetInstance();
      bool connected = input->IsConnect();

      if (connected) {
        ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "GamePad: Connected");

        // スティック値
        Vector2 leftStick = input->GetLeftStick();
        Vector2 rightStick = input->GetRightStick();

        ImGui::Text("Left Stick:");
        ImGui::SameLine(100);
        ImGui::ProgressBar((leftStick.x + 1.0f) * 0.5f, ImVec2(100, 0), "X");
        ImGui::SameLine();
        ImGui::ProgressBar((leftStick.y + 1.0f) * 0.5f, ImVec2(100, 0), "Y");

        ImGui::Text("Right Stick:");
        ImGui::SameLine(100);
        ImGui::ProgressBar((rightStick.x + 1.0f) * 0.5f, ImVec2(100, 0), "X");
        ImGui::SameLine();
        ImGui::ProgressBar((rightStick.y + 1.0f) * 0.5f, ImVec2(100, 0), "Y");

        // トリガー値
        float leftTrigger = input->GetLeftTrigger();
        float rightTrigger = input->GetRightTrigger();

        ImGui::Text("Triggers:");
        ImGui::SameLine(100);
        ImGui::ProgressBar(leftTrigger / 255.0f, ImVec2(100, 0), "L");
        ImGui::SameLine();
        ImGui::ProgressBar(rightTrigger / 255.0f, ImVec2(100, 0), "R");

        // ボタン状態
        ImGui::Text("Buttons:");
        const char* buttonNames[] = {
            "A", "B", "X", "Y",
            "Up", "Down", "Left", "Right",
            "LB", "RB", "L3", "R3",
            "Start", "Back"
        };

        for (int i = 0; i < GamepadButton::COUNT; i++) {
          if (i > 0 && i % 4 != 0) ImGui::SameLine();
          bool pressed = input->PushButton(GamepadButton::ALL[i]);
          if (pressed) {
            ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "[%s]", buttonNames[i]);
          }
          else {
            ImGui::Text("[%s]", buttonNames[i]);
          }
        }
      }
      else {
        ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "GamePad: Not Connected");
      }
    }

    // マウス情報
    if (ImGui::CollapsingHeader("Mouse")) {
      Vector2 mousePos = Input::GetInstance()->GetMousePos();
      ImGui::Text("Position: (%.0f, %.0f)", mousePos.x, mousePos.y);

      bool leftClick = Input::GetInstance()->PushMouse(0);
      bool rightClick = Input::GetInstance()->PushMouse(1);
      bool middleClick = Input::GetInstance()->PushMouse(2);

      ImGui::Text("Buttons: ");
      ImGui::SameLine();
      if (leftClick) ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "[L]");
      else ImGui::Text("[L]");
      ImGui::SameLine();
      if (rightClick) ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "[R]");
      else ImGui::Text("[R]");
      ImGui::SameLine();
      if (middleClick) ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "[M]");
      else ImGui::Text("[M]");
    }

    ImGui::End();
  }

  void DebugUIManager::AddLog(const std::string& message, LogType type) {
    LogEntry entry;
    entry.type = type;
    entry.message = message;
    entry.timestamp = GetCurrentTimestamp();

    Logger::Log(message + "\n"); // 外部ロガーにも出力

    consoleLogs_.push_back(entry);

    // 最大数を超えたら古いログを削除
    if (consoleLogs_.size() > static_cast<size_t>(maxConsoleLogs_)) {
      consoleLogs_.erase(consoleLogs_.begin());
    }
  }

  void DebugUIManager::ClearLogs() {
    consoleLogs_.clear();
  }

  void DebugUIManager::RegisterGameObject(const std::string& name, std::function<void()> drawImGuiFunc) {
    // 同じ名前のオブジェクトがあるか確認
    for (auto& obj : gameObjects_) {
      if (obj.name == name) {
        // 既存のオブジェクトを更新
        obj.drawImGuiFunc = drawImGuiFunc;
        return;
      }
    }
    // 新規登録
    gameObjects_.push_back({ name, drawImGuiFunc });
  }

  void DebugUIManager::UnregisterGameObject(const std::string& name) {
    gameObjects_.erase(
      std::remove_if(gameObjects_.begin(), gameObjects_.end(),
        [&name](const GameObjectDebugInfo& obj) { return obj.name == name; }),
      gameObjects_.end()
    );

    // 選択インデックスの調整
    if (selectedObjectIndex_ >= static_cast<int>(gameObjects_.size())) {
      selectedObjectIndex_ = -1;
    }
  }

  void DebugUIManager::ClearGameObjects() {
    gameObjects_.clear();
    selectedObjectIndex_ = -1;
  }

  std::string DebugUIManager::GetCurrentTimestamp() {
    auto localNow = std::chrono::current_zone()->to_local(std::chrono::system_clock::now());
    return std::format("{:%H:%M:%S}", std::chrono::floor<std::chrono::seconds>(localNow));
  }

  void DebugUIManager::DrawCollisionDebug() {
    ImGui::Begin("Collision Debug", &WindowFlag(Window::CollisionDebug));

    CollisionManager* collisionManager = CollisionManager::GetInstance();

    // ヘッダー
    ImGui::Text("Collision System");
    ImGui::Separator();

    // デバッグワイヤーフレーム表示の ON/OFF
    bool debugDraw = collisionManager->IsDebugDrawEnabled();
    if (ImGui::Checkbox("##ShowWireframes", &debugDraw)) {
      collisionManager->SetDebugDrawEnabled(debugDraw);
    }
    ImGui::SameLine();
    ImGui::Text(debugDraw ? "Debug Draw: ON" : "Debug Draw: OFF");

    ImGui::Separator();

    // 統計情報
    if (ImGui::CollapsingHeader("Statistics", ImGuiTreeNodeFlags_DefaultOpen)) {
      size_t totalColliders = collisionManager->GetColliderCount();

      // アクティブなコライダー数をカウント
      size_t activeCount = 0;
      size_t inactiveCount = 0;
      std::unordered_map<uint32_t, int> typeCountMap;
      const auto& colliders = collisionManager->GetColliders();

      for (const auto* collider : colliders) {
        if (collider) {
          if (collider->IsActive()) {
            activeCount++;
            uint32_t typeID = collider->GetTypeID();
            typeCountMap[typeID]++;
          }
          else {
            inactiveCount++;
          }
        }
      }

      // 総数と内訳
      ImGui::Text("Total Colliders: %zu", totalColliders);

      // アクティブ/非アクティブの比率表示
      if (totalColliders > 0) {
        float activeRatio = (float)activeCount / (float)totalColliders;
        ImGui::ProgressBar(activeRatio, ImVec2(-1, 0),
          ("Active: " + std::to_string(activeCount) + " / Inactive: " + std::to_string(inactiveCount)).c_str());
      }

      // Type ID 別の分布（汎用的な表示）
      if (!typeCountMap.empty()) {
        ImGui::Spacing();
        ImGui::Text("Type ID Distribution:");

        // Type ID でソートして表示
        std::map<uint32_t, int> sortedTypeMap(typeCountMap.begin(), typeCountMap.end());

        for (const auto& [typeID, count] : sortedTypeMap) {
          ImGui::Text("  Type %02u: %d collider%s",
            typeID, count, count > 1 ? "s" : "");
        }
      }
    }

    // コリジョンマスク情報
    if (ImGui::CollapsingHeader("Collision Masks")) {
      const auto& masks = collisionManager->GetCollisionMasks();

      if (masks.empty()) {
        ImGui::TextDisabled("No collision masks configured");
      }
      else {
        ImGui::Text("Active collision pairs:");
        ImGui::Spacing();

        // すべてのマスク情報を収集して整理
        std::set<std::pair<uint32_t, uint32_t>> collisionPairs;
        for (const auto& [typeA, typeBSet] : masks) {
          for (uint32_t typeB : typeBSet) {
            // 小さい番号を先にして重複を避ける
            if (typeA <= typeB) {
              collisionPairs.insert({ typeA, typeB });
            }
            else {
              collisionPairs.insert({ typeB, typeA });
            }
          }
        }

        // コリジョンペアを表示
        for (const auto& [typeA, typeB] : collisionPairs) {
          if (typeA == typeB) {
            ImGui::BulletText("Type %02u <-> Type %02u (self-collision)", typeA, typeB);
          }
          else {
            ImGui::BulletText("Type %02u <-> Type %02u", typeA, typeB);
          }
        }

        ImGui::Spacing();
        ImGui::Text("Total mask pairs: %zu", collisionPairs.size());
      }
    }

    // パフォーマンス情報
    if (ImGui::CollapsingHeader("Performance")) {
      size_t colliderCount = collisionManager->GetColliderCount();
      const auto& masks = collisionManager->GetCollisionMasks();

      // ブロードフェーズの計算量（最悪ケース）
      size_t maxPairs = colliderCount * (colliderCount - 1) / 2;
      ImGui::Text("Max possible pairs: %zu", maxPairs);

      // マスクによる最適化の効果を表示
      if (!masks.empty()) {
        ImGui::Text("Collision masks: Active");
        ImGui::TextDisabled("Reducing unnecessary checks");
      }
      else {
        ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f), "No masks configured");
        ImGui::TextDisabled("All types check against all types");
      }

      ImGui::Spacing();

      // パフォーマンスレベルのインジケーター
      ImVec4 performanceColor;
      const char* performanceText;

      if (maxPairs < 100) {
        performanceColor = ImVec4(0.2f, 1.0f, 0.2f, 1.0f);
        performanceText = "Excellent";
      }
      else if (maxPairs < 500) {
        performanceColor = ImVec4(0.7f, 1.0f, 0.2f, 1.0f);
        performanceText = "Good";
      }
      else if (maxPairs < 2000) {
        performanceColor = ImVec4(1.0f, 0.8f, 0.2f, 1.0f);
        performanceText = "Moderate";
      }
      else {
        performanceColor = ImVec4(1.0f, 0.3f, 0.3f, 1.0f);
        performanceText = "Heavy";
      }

      ImGui::Text("Performance Load: ");
      ImGui::SameLine();
      ImGui::TextColored(performanceColor, "%s", performanceText);
    }

    ImGui::End();
  }

  void DebugUIManager::DrawAbout() {
    if (ImGui::Begin("About", &WindowFlag(Window::About), ImGuiWindowFlags_AlwaysAutoResize)) {
      ImGui::Text("TakoEngine (DirectX 12)");
      ImGui::Text("ImGui: %s", IMGUI_VERSION);
      ImGui::Text("Resolution: %d x %d", WinApp::clientWidth, WinApp::clientHeight);

      ImGui::SeparatorText("Shortcuts");
      constexpr std::pair<const char*, const char*> kShortcuts[] = {
        { "F1",  "Debug Viewport" },
        { "F2",  "Scene Hierarchy" },
        { "F3",  "Inspector" },
        { "F4",  "Game Viewport" },
        { "F5",  "Console" },
        { "F6",  "Performance" },
        { "F7",  "Pause / Resume" },
        { "F8",  "Step Frame" },
        { "F10", "Show Main Windows" },
        { "F11", "Fullscreen" },
        { "F12", "Toggle Main Windows" },
      };
      if (ImGui::BeginTable("##Shortcuts", 2, ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV)) {
        for (const auto& [key, action] : kShortcuts) {
          ImGui::TableNextRow();
          ImGui::TableNextColumn();
          ImGui::TextUnformatted(key);
          ImGui::TableNextColumn();
          ImGui::TextUnformatted(action);
        }
        ImGui::EndTable();
      }
    }
    ImGui::End();
  }

} // namespace Tako
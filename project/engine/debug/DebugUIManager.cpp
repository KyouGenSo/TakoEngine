#include "DebugUIManager.h"
#include "FrameTimer.h"
#include "SrvManager.h"
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
#include "Draw2D.h"
#include "GPUParticle.h"
#include "Logger.h"
#include "EmitterManager.h"
#include "GlobalVariables.h"

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

  // シングルトンインスタンス
  std::unique_ptr<DebugUIManager> DebugUIManager::instance_ = nullptr;

  DebugUIManager* DebugUIManager::GetInstance() {
    if (!instance_) {
      instance_ = std::make_unique<DebugUIManager>(Token{});
    }
    return instance_.get();
  }

  void DebugUIManager::Initialize() {
    // デフォルトウィンドウの表示設定
    windowVisibility_["GameViewport"] = true;
    windowVisibility_["SceneHierarchy"] = true;
    windowVisibility_["Inspector"] = true;
    windowVisibility_["Console"] = false;
    windowVisibility_["Performance"] = false;
    windowVisibility_["EngineStatus"] = false;
    windowVisibility_["InputDebug"] = false;
    windowVisibility_["ShadowSettings"] = false;
    windowVisibility_["CollisionDebug"] = false;
    windowVisibility_["PostEffect"] = false;
    windowVisibility_["ParticleEditor"] = false;
    windowVisibility_["GlobalVariables"] = false;

    // 初期ログ
    AddLog("DebugUIManager Initialized", LogType::Info);
    AddLog("TakoEngine Ready", LogType::Info);
  }

  void DebugUIManager::Finalize() {
    ClearLogs();
    instance_.reset();
  }

  void DebugUIManager::Update() {
    if (Input::GetInstance()->TriggerKey(DIK_F2)) {
      windowVisibility_["SceneHierarchy"] = !windowVisibility_["SceneHierarchy"];
    }
    if (Input::GetInstance()->TriggerKey(DIK_F3)) {
      windowVisibility_["Inspector"] = !windowVisibility_["Inspector"];
    }
    if (Input::GetInstance()->TriggerKey(DIK_F4)) {
      windowVisibility_["GameViewport"] = !windowVisibility_["GameViewport"];
    }
    if (Input::GetInstance()->TriggerKey(DIK_F5)) {
      windowVisibility_["Console"] = !windowVisibility_["Console"];
    }
    if (Input::GetInstance()->TriggerKey(DIK_F6)) {
      windowVisibility_["Performance"] = !windowVisibility_["Performance"];
    }
    if (Input::GetInstance()->TriggerKey(DIK_F12)) {
      windowVisibility_["SceneHierarchy"] = !windowVisibility_["SceneHierarchy"];
      windowVisibility_["Inspector"] = !windowVisibility_["Inspector"];
      windowVisibility_["GameViewport"] = !windowVisibility_["GameViewport"];
      windowVisibility_["Console"] = !windowVisibility_["Console"];
      windowVisibility_["Performance"] = !windowVisibility_["Performance"];
    }
    if (Input::GetInstance()->TriggerKey(DIK_F10)) {
      windowVisibility_["SceneHierarchy"] = true;
      windowVisibility_["Inspector"] = true;
      windowVisibility_["GameViewport"] = true;
      windowVisibility_["Console"] = true;
      windowVisibility_["Performance"] = true;
    }
  }

  void DebugUIManager::Draw() {
#ifdef _DEBUG
    // メインメニューバー
    DrawMainMenuBar();

    // 各ウィンドウの描画
    if (windowVisibility_["SceneHierarchy"]) DrawSceneHierarchy();
    if (windowVisibility_["Inspector"]) DrawInspector();
    if (windowVisibility_["Console"]) DrawConsole();
    if (windowVisibility_["Performance"]) DrawPerformance();
    if (windowVisibility_["GameViewport"]) DrawGameViewport();
    if (windowVisibility_["EngineStatus"]) DrawEngineStatus();
    if (windowVisibility_["InputDebug"]) DrawInputDebug();
    if (windowVisibility_["ShadowSettings"]) DrawShadowSettings();
    if (windowVisibility_["CollisionDebug"]) DrawCollisionDebug();
    if (windowVisibility_["ParticleEditor"]) DrawParticleEditor();
    if (windowVisibility_["ParticleEditor"]) DrawParticleVisualization();

    // GlobalVariables（グループがない場合は警告を出して閉じる）
    if (windowVisibility_["GlobalVariables"]) {
      if (GlobalVariables::GetInstance()->HasGroups()) {
        GlobalVariables::GetInstance()->Update();
      }
      else {
        AddLog("GlobalVariables: No groups registered. Window will not open.", LogType::Warning);
        windowVisibility_["GlobalVariables"] = false;
      }
    }

    // PostEffect は独自の描画を持つ
    if (windowVisibility_["PostEffect"]) {
      PostEffectManager::GetInstance()->DrawImgui();
    }
#endif
  }

  void DebugUIManager::DrawMainMenuBar() {
    if (ImGui::BeginMainMenuBar()) {
      // General メニュー
      if (ImGui::BeginMenu("General")) {
        if (ImGui::MenuItem("Exit", "Alt+F4")) {
          if (pEndFlag_) *pEndFlag_ = true;
        }
        ImGui::EndMenu();
      }

      ImGui::Text(" | ");

      // View メニュー
      if (ImGui::BeginMenu("View")) {
        ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "Main Windows");
        ImGui::Separator();
        ImGui::MenuItem("Scene Hierarchy", "F2", &windowVisibility_["SceneHierarchy"]);
        ImGui::MenuItem("Inspector", "F3", &windowVisibility_["Inspector"]);
        ImGui::MenuItem("Game Viewport", "F4", &windowVisibility_["GameViewport"]);
        ImGui::MenuItem("Console", "F5", &windowVisibility_["Console"]);
        ImGui::MenuItem("Performance", "F6", &windowVisibility_["Performance"]);
        ImGui::Separator();
        ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "Debug Windows");
        ImGui::Separator();
        ImGui::MenuItem("Engine Status", nullptr, &windowVisibility_["EngineStatus"]);
        ImGui::MenuItem("Input Debug", nullptr, &windowVisibility_["InputDebug"]);
        ImGui::MenuItem("Shadow Settings", nullptr, &windowVisibility_["ShadowSettings"]);
        ImGui::MenuItem("Collision Debug", nullptr, &windowVisibility_["CollisionDebug"]);
        ImGui::MenuItem("PostEffect Settings", nullptr, &windowVisibility_["PostEffect"]);
        ImGui::EndMenu();
      }

      ImGui::Text(" | ");

      // Tools メニュー
      if (ImGui::BeginMenu("Tools")) {
        bool collisionDebug = CollisionManager::GetInstance()->IsDebugDrawEnabled();
        if (ImGui::MenuItem("Collider Visibility", nullptr, collisionDebug)) {
          CollisionManager::GetInstance()->SetDebugDrawEnabled(!collisionDebug);
        }

        // Debug Camera の切り替え
        if (pIsDebug_) {
          bool debugCamera = *pIsDebug_;
          if (ImGui::MenuItem("Debug Camera", "F1", debugCamera)) {
            *pIsDebug_ = !debugCamera;
            // 各コンポーネントのデバッグモードも同時に設定
            Object3dBasic::GetInstance()->SetDebug(*pIsDebug_);
            Draw2D::GetInstance()->SetDebug(*pIsDebug_);
            GPUParticle::GetInstance()->SetIsDebug(*pIsDebug_);
          }
        }

        ImGui::MenuItem("Particle Editor", nullptr, &windowVisibility_["ParticleEditor"]);
        ImGui::MenuItem("Global Variables", nullptr, &windowVisibility_["GlobalVariables"]);

        ImGui::EndMenu();
      }

      // 中央にエンジン名を表示
      ImGui::SetCursorPosX(ImGui::GetWindowWidth() / 2.0f - 40.0f);
      ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "TakoEngine");

      // 区切り線
      ImGui::SetCursorPosX(ImGui::GetWindowWidth() - 140);
      ImGui::Text("|");

      // FPS 表示（右端）
      ImGui::SetCursorPosX(ImGui::GetWindowWidth() - 120);
      float fps = FrameTimer::GetInstance()->GetFPS();
      if (fps >= 55.0f) {
        ImGui::TextColored(ImVec4(0.2f, 1.0f, 0.2f, 1.0f), "FPS: %.1f", fps);
      }
      else if (fps >= 30.0f) {
        ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f), "FPS: %.1f", fps);
      }
      else {
        ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "FPS: %.1f", fps);
      }

      ImGui::EndMainMenuBar();
    }
  }

  void DebugUIManager::DrawSceneHierarchy() {
    ImGui::Begin("Scene Hierarchy", &windowVisibility_["SceneHierarchy"]);

    ImGui::Text("Current Scene: %s", currentSceneName_.c_str());

    // シーン遷移 UI
    ImGui::PushItemWidth(120.0f);  // 入力ボックスの幅を設定
    ImGui::InputText("##SceneName", sceneNameBuffer_, sizeof(sceneNameBuffer_));
    ImGui::PopItemWidth();
    ImGui::SameLine();
    if (ImGui::Button("Change Scene")) {
      if (strlen(sceneNameBuffer_) > 0) {

        // ログに記録
        std::string logMsg = "Scene change requested: ";
        logMsg += sceneNameBuffer_;
        AddLog(logMsg, LogType::Info);

        // シーン遷移を実行
        SceneManager::GetInstance()->ChangeScene(sceneNameBuffer_);

        // 現在のシーン名を更新（成功したと仮定）
        currentSceneName_ = sceneNameBuffer_;

        // 入力ボックスをクリア
        sceneNameBuffer_[0] = '\0';
      }
      else {
        AddLog("Scene name is empty", LogType::Warning);
      }
    }

    // 利用可能なシーンのヒント表示
    ImGui::TextDisabled("Available scenes: title, game");

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
        }
      }

      ImGui::Spacing();
      ImGui::Separator();
    }

    // エンジンオブジェクト
    ImGui::Text("Engine Objects:");
    ImGui::Separator();

    // カメラ情報
    if (ImGui::TreeNode("Main Camera")) {
      Camera** cameraPtr = Object3dBasic::GetInstance()->GetCamera();
      if (cameraPtr && *cameraPtr) {
        Vector3 pos = (*cameraPtr)->GetTranslate();
        ImGui::Text("Position: (%.2f, %.2f, %.2f)", pos.x, pos.y, pos.z);
        Vector3 rot = (*cameraPtr)->GetRotate();
        ImGui::Text("Rotation: (%.2f, %.2f, %.2f)", rot.x, rot.y, rot.z);
      }
      ImGui::TreePop();
    }

    // ライト情報
    if (ImGui::TreeNode("Directional Light")) {
      Light* light = Object3dBasic::GetInstance()->GetLight();
      if (light) {
        const Light::DirectionalLight& dirLight = light->GetDirectionalLight();
        ImGui::Text("Intensity: %.2f", dirLight.intensity);
        Vector3 dir = dirLight.direction;
        ImGui::Text("Direction: (%.2f, %.2f, %.2f)", dir.x, dir.y, dir.z);
      }
      ImGui::TreePop();
    }

    ImGui::End();
  }

  void DebugUIManager::DrawInspector() {
    ImGui::Begin("Inspector", &windowVisibility_["Inspector"]);

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

      ImGui::Spacing();
      ImGui::Separator();
      ImGui::Spacing();
    }
    else {
      // 何も選択されていない場合
      ImGui::Text("No object selected");
      ImGui::TextDisabled("Select an object from Scene Hierarchy");
      ImGui::Separator();
    }

    // Resource Management
    if (ImGui::CollapsingHeader("Resource Management")) {
      uint32_t srvAllocated = SrvManager::GetInstance()->GetAllocatedCount();
      float srvUsageRate = (float)srvAllocated / (float)SrvManager::kMaxSRVCount;
      ImGui::Text("SRV Usage: %u / %u", srvAllocated, SrvManager::kMaxSRVCount);
      ImGui::ProgressBar(srvUsageRate, ImVec2(0.0f, 0.0f));
      ImGui::Text("Textures: ~%u", srvAllocated);
    }

    // Default Camera Settings
    if (ImGui::CollapsingHeader("Default Camera Settings")) {
      Camera** cameraPtr = Object3dBasic::GetInstance()->GetCamera();
      if (cameraPtr && *cameraPtr) {
        Camera* camera = *cameraPtr;
        Vector3 camPos = camera->GetTranslate();
        Vector3 camRot = camera->GetRotate();

        ImGui::DragFloat3("Camera Position", &camPos.x, 0.1f);
        ImGui::DragFloat3("Camera Rotation", &camRot.x, 0.01f);

        float fov = camera->GetFovY() * 180.0f / std::numbers::pi_v<float>;
        if (ImGui::DragFloat("Field of View", &fov, 0.1f, 10.0f, 120.0f)) {
          camera->SetFovY(fov * std::numbers::pi_v<float> / 180.0f);
        }
      }
    }

    // Light Settings
    if (ImGui::CollapsingHeader("Light Settings")) {
      Light* light = Object3dBasic::GetInstance()->GetLight();
      if (light) {
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
    }

    ImGui::End();
  }

  void DebugUIManager::DrawConsole() {
    ImGui::Begin("Console", &windowVisibility_["Console"]);

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
    ImGui::Begin("Performance", &windowVisibility_["Performance"]);

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

    ImGui::End();
  }

  void DebugUIManager::DrawGameViewport() {
    // 毎フレームリセット（折りたたみ/非表示タブ時は hover 無しのまま）
    isGameViewportHovered_ = false;

    // Begin が false（折りたたみ等）の場合は中身を描かない。Begin/End は対で必ず呼ぶ
    if (ImGui::Begin("Game Viewport", &windowVisibility_["GameViewport"])) {

      // ウィンドウの利用可能サイズを取得
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
    if (IsWindowVisible("GameViewport")) {
      return isGameViewportHovered_;
    }
    // 非表示(フルスクリーン直描画)中: いずれの ImGui ウィンドウにもカーソルが無いか
    return !ImGui::GetIO().WantCaptureMouse;
  }

  void DebugUIManager::DrawEngineStatus() {
    ImGui::Begin("Engine Status", &windowVisibility_["EngineStatus"]);

    // リソース管理タブ
    if (ImGui::CollapsingHeader("Resource Management")) {
      uint32_t srvAllocated = SrvManager::GetInstance()->GetAllocatedCount();
      float srvUsageRate = (float)srvAllocated / (float)SrvManager::kMaxSRVCount;
      ImGui::Text("SRV Usage: %u / %u", srvAllocated, SrvManager::kMaxSRVCount);
      ImGui::ProgressBar(srvUsageRate, ImVec2(0.0f, 0.0f));
      ImGui::Separator();
      ImGui::Text("Textures Loaded: %u", srvAllocated);
      ImGui::Separator();
      ImGui::Text("Models: (情報取得不可)");
    }

    ImGui::End();
  }

  void DebugUIManager::DrawInputDebug() {
    ImGui::Begin("Input Debug", &windowVisibility_["InputDebug"]);

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

  void DebugUIManager::SetWindowVisible(const std::string& windowName, bool visible) {
    windowVisibility_[windowName] = visible;
  }

  bool DebugUIManager::IsWindowVisible(const std::string& windowName) const {
    auto it = windowVisibility_.find(windowName);
    return it != windowVisibility_.end() ? it->second : false;
  }

  std::string DebugUIManager::GetCurrentTimestamp() {
    auto localNow = std::chrono::current_zone()->to_local(std::chrono::system_clock::now());
    return std::format("{:%H:%M:%S}", std::chrono::floor<std::chrono::seconds>(localNow));
  }

  void DebugUIManager::DrawShadowSettings() {

    ShadowRenderer* shadowRenderer = ShadowRenderer::GetInstance();

    ImGui::Begin("Shadow Settings", &windowVisibility_["ShadowSettings"]);

    shadowRenderer->DrawImGui();

    ImGui::End();
  }

  void DebugUIManager::DrawCollisionDebug() {
    ImGui::Begin("Collision Debug", &windowVisibility_["CollisionDebug"]);

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

} // namespace Tako
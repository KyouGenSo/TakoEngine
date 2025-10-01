#include "DebugUIManager.h"
#include "FrameTimer.h"
#include "SrvManager.h"
#include "Object3dBasic.h"
#include "Light.h"
#include "Camera.h"
#include "Input.h"
#include "CollisionManager.h"
#include "DebugCamera.h"
#include "PostEffectManager.h"
#include "WinApp.h"
#include "DX12Basic.h"
#include "TextureManager.h"
#include "ModelManager.h"
#include "ShadowRenderer.h"
#include "imgui_internal.h"
#include <algorithm>
#include <iomanip>
#include <sstream>
#include <set>
#include <map>

#include "Logger.h"

// シングルトンインスタンス
DebugUIManager* DebugUIManager::instance_ = nullptr;

DebugUIManager* DebugUIManager::GetInstance() {
    if (!instance_) {
        instance_ = new DebugUIManager();
    }
    return instance_;
}

void DebugUIManager::Initialize() {
    // デフォルトウィンドウの表示設定
    windowVisibility_["GameViewport"] = true;
    windowVisibility_["SceneHierarchy"] = true;
    windowVisibility_["Inspector"] = true;
    windowVisibility_["Console"] = true;
    windowVisibility_["Performance"] = true;
    windowVisibility_["EngineStatus"] = false;
    windowVisibility_["InputDebug"] = false;
    windowVisibility_["ShadowSettings"] = false;
    windowVisibility_["CollisionDebug"] = false;
    windowVisibility_["PostEffect"] = false;
    
    // 初期ログ
    AddLog("DebugUIManager Initialized", LogType::Info);
    AddLog("TakoEngine Ready", LogType::Info);
}

void DebugUIManager::Finalize() {
    ClearDebugInfo();
    ClearLogs();
    if (instance_) {
        delete instance_;
        instance_ = nullptr;
    }
}

void DebugUIManager::Update() {
    // 必要に応じて更新処理
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
    
    // PostEffectは独自の描画を持つ
    if (windowVisibility_["PostEffect"]) {
        PostEffectManager::GetInstance()->DrawImgui();
    }
#endif
}

void DebugUIManager::DrawMainMenuBar() {
    if (ImGui::BeginMainMenuBar()) {
        // Fileメニュー
        if (ImGui::BeginMenu("File")) {
            if (ImGui::MenuItem("New Scene", "Ctrl+N")) {}
            if (ImGui::MenuItem("Open Scene", "Ctrl+O")) {}
            if (ImGui::MenuItem("Save Scene", "Ctrl+S")) {}
            ImGui::Separator();
            if (ImGui::MenuItem("Exit", "Alt+F4")) { 
                if (pEndFlag_) *pEndFlag_ = true; 
            }
            ImGui::EndMenu();
        }
        
        ImGui::Text(" | ");
        
        // Viewメニュー
        if (ImGui::BeginMenu("View")) {
            ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "Main Windows");
            ImGui::Separator();
            ImGui::MenuItem("Scene Hierarchy", "F1", &windowVisibility_["SceneHierarchy"]);
            ImGui::MenuItem("Inspector", "F2", &windowVisibility_["Inspector"]);
            ImGui::MenuItem("Game Viewport", "F3", &windowVisibility_["GameViewport"]);
            ImGui::MenuItem("Console", "F4", &windowVisibility_["Console"]);
            ImGui::MenuItem("Performance", "F5", &windowVisibility_["Performance"]);
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
        
        // Toolsメニュー
        if (ImGui::BeginMenu("Tools")) {
            if (ImGui::MenuItem("Global Variables")) {}
            bool collisionDebug = CollisionManager::GetInstance()->IsDebugDrawEnabled();
            if (ImGui::MenuItem("Collision Debug", nullptr, collisionDebug)) {
                CollisionManager::GetInstance()->SetDebugDrawEnabled(!collisionDebug);
            }
            ImGui::EndMenu();
        }
        
        ImGui::Text(" | ");
        
        // Helpメニュー
        if (ImGui::BeginMenu("Help")) {
            if (ImGui::MenuItem("About TakoEngine")) {}
            if (ImGui::MenuItem("Documentation")) {}
            ImGui::EndMenu();
        }
        
        // 中央にエンジン名を表示
        ImGui::SetCursorPosX(ImGui::GetWindowWidth() / 2.0f - 40.0f);
        ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "TakoEngine");
        
        // 区切り線
        ImGui::SetCursorPosX(ImGui::GetWindowWidth() - 140);
        ImGui::Text("|");
        
        // FPS表示（右端）
        ImGui::SetCursorPosX(ImGui::GetWindowWidth() - 120);
        float fps = FrameTimer::GetInstance()->GetFPS();
        if (fps >= 55.0f) {
            ImGui::TextColored(ImVec4(0.2f, 1.0f, 0.2f, 1.0f), "FPS: %.1f", fps);
        } else if (fps >= 30.0f) {
            ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f), "FPS: %.1f", fps);
        } else {
            ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "FPS: %.1f", fps);
        }
        
        ImGui::EndMainMenuBar();
    }
}

void DebugUIManager::DrawSceneHierarchy() {
    ImGui::Begin("Scene Hierarchy", &windowVisibility_["SceneHierarchy"]);
    
    ImGui::Text("Current Scene: %s", currentSceneName_.c_str());
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
        
        // オブジェクトのDrawImGui関数を呼び出す
        if (selectedObject.drawImGuiFunc) {
            selectedObject.drawImGuiFunc();
        }
        
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();
    } else {
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
    
    // Rendering Settings
    if (ImGui::CollapsingHeader("Rendering Settings")) {
        Camera** cameraPtr = Object3dBasic::GetInstance()->GetCamera();
        if (cameraPtr && *cameraPtr) {
            Camera* camera = *cameraPtr;
            Vector3 camPos = camera->GetTranslate();
            Vector3 camRot = camera->GetRotate();
            
            ImGui::DragFloat3("Camera Position", &camPos.x, 0.1f);
            ImGui::DragFloat3("Camera Rotation", &camRot.x, 0.01f);
            
            float fov = camera->GetFovY() * 180.0f / 3.14159265f;
            if (ImGui::DragFloat("Field of View", &fov, 0.1f, 10.0f, 120.0f)) {
                camera->SetFovY(fov * 3.14159265f / 180.0f);
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
    if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY()) {
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
    
    // FPSグラフ
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
    ImGui::Begin("Game Viewport", &windowVisibility_["GameViewport"]);
    
    // ウィンドウの利用可能サイズを取得
    ImVec2 availableSize = ImGui::GetContentRegionAvail();
    
    // クライアント領域のアスペクト比を計算
    float aspectRatio = static_cast<float>(WinApp::clientWidth) / static_cast<float>(WinApp::clientHeight);
    
    // アスペクト比を維持したサイズを計算
    ImVec2 imageSize;
    float availableAspect = availableSize.x / availableSize.y;
    
    if (availableAspect > aspectRatio) {
        imageSize.y = availableSize.y;
        imageSize.x = imageSize.y * aspectRatio;
    } else {
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
    
    ImGui::End();
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
            XButtonIDs btns;
            const char* buttonNames[] = {
                "A", "B", "X", "Y", 
                "Up", "Down", "Left", "Right",
                "LB", "RB", "L3", "R3", 
                "Start", "Back"
            };
            
            for (int i = 0; i < 14; i++) {
                if (i > 0 && i % 4 != 0) ImGui::SameLine();
                bool pressed = input->PushButton(i);
                if (pressed) {
                    ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "[%s]", buttonNames[i]);
                } else {
                    ImGui::Text("[%s]", buttonNames[i]);
                }
            }
        } else {
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

void DebugUIManager::RegisterDebugInfo(const std::string& category, std::function<void()> callback) {
    debugInfoCallbacks_[category] = callback;
}

void DebugUIManager::UnregisterDebugInfo(const std::string& category) {
    debugInfoCallbacks_.erase(category);
}

void DebugUIManager::ClearDebugInfo() {
    debugInfoCallbacks_.clear();
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
    gameObjects_.push_back({name, drawImGuiFunc});
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
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    std::stringstream ss;
    
#ifdef _WIN32
    struct tm timeinfo;
    localtime_s(&timeinfo, &time_t);
    ss << std::put_time(&timeinfo, "%H:%M:%S");
#else
    ss << std::put_time(std::localtime(&time_t), "%H:%M:%S");
#endif
    
    return ss.str();
}

void DebugUIManager::DrawShadowSettings() {
    ImGui::Begin("Shadow Settings", &windowVisibility_["ShadowSettings"]);
    
    ShadowRenderer* shadowRenderer = ShadowRenderer::GetInstance();
    ShadowMap* shadowMap = shadowRenderer->GetShadowMap();
    
    // ヘッダー
    ImGui::Text("Shadow Mapping Configuration");
    ImGui::Separator();
    
    // シャドウのON/OFF（大きなトグルボタン）
    bool shadowEnabled = shadowRenderer->IsEnabled();
    if (ImGui::Checkbox("##EnableShadow", &shadowEnabled)) {
        shadowRenderer->SetEnabled(shadowEnabled);
    }
    ImGui::SameLine();
    ImGui::Text(shadowEnabled ? "Shadow: ON" : "Shadow: OFF");
    
    if (!shadowEnabled) {
        ImGui::TextDisabled("Enable shadows to configure settings");
        ImGui::End();
        return;
    }
    
    ImGui::Separator();
    
    // クイックプリセットボタン
    ImGui::Text("Quick Presets:");
    ImGui::SameLine();
    
    if (ImGui::Button("Low")) {
        shadowRenderer->SetShadowQuality(0);
    }
    ImGui::SameLine();
    if (ImGui::Button("Medium")) {
        shadowRenderer->SetShadowQuality(1);
    }
    ImGui::SameLine();
    if (ImGui::Button("High")) {
        shadowRenderer->SetShadowQuality(2);
    }
    ImGui::SameLine();
    if (ImGui::Button("Ultra")) {
        shadowRenderer->SetShadowQuality(3);
    }
    
    ImGui::Separator();
    
    // 詳細設定
    if (ImGui::CollapsingHeader("Quality Settings", ImGuiTreeNodeFlags_DefaultOpen)) {
        if (shadowMap) {
            // シャドウマップ解像度（static変数で状態を保持）
            static int shadowMapSize = shadowMap->GetShadowMapSize();
            ImGui::Text("Shadow Map Resolution:");
            ImGui::RadioButton("512x512", &shadowMapSize, 512);
            ImGui::SameLine();
            ImGui::RadioButton("1024x1024", &shadowMapSize, 1024);
            ImGui::RadioButton("2048x2048", &shadowMapSize, 2048);
            ImGui::SameLine();
            ImGui::RadioButton("4096x4096", &shadowMapSize, 4096);
            
            if (shadowMapSize != static_cast<int>(shadowMap->GetShadowMapSize())) {
                shadowRenderer->SetShadowMapSize(shadowMapSize);
            }
            
            // PCFカーネルサイズ
            static int kernelSize = shadowMap->GetPCFKernelSize();
            if (ImGui::SliderInt("PCF Kernel Size", &kernelSize, 0, 9, 
                kernelSize == 0 ? "No PCF" : "%dx%d")) {
                shadowRenderer->SetPCFKernelSize(kernelSize);
            }
            ImGui::TextDisabled("Higher values = softer shadows, lower performance");
        }
    }
    
    if (ImGui::CollapsingHeader("Shadow Parameters")) {
        // バイアス設定（static変数で状態を保持）
        static float bias = 0.0001f;
        if (ImGui::SliderFloat("Shadow Bias", &bias, 0.0001f, 0.01f, "%.5f")) {
            shadowRenderer->SetShadowBias(bias);
        }
        ImGui::TextDisabled("Reduces shadow acne");
        
        // 法線オフセットバイアス
        static float normalBias = 0.01f;
        if (ImGui::SliderFloat("Normal Offset Bias", &normalBias, 0.001f, 0.1f, "%.4f")) {
            shadowRenderer->SetNormalOffsetBias(normalBias);
        }
        ImGui::TextDisabled("Reduces shadow peter-panning");
        
        // 最大シャドウ距離
        float maxDistance = shadowRenderer->GetMaxShadowDistance();
        if (ImGui::SliderFloat("Max Shadow Distance", &maxDistance, 10.0f, 500.0f, "%.1f")) {
            shadowRenderer->SetMaxShadowDistance(maxDistance);
        }
        ImGui::TextDisabled("Objects beyond this distance won't cast shadows");
    }
    
    // パフォーマンス情報
    if (shadowMap) {
        ImGui::Separator();
        ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "Performance Info:");
        
        uint32_t mapSize = shadowMap->GetShadowMapSize();
        int kernelSize = shadowMap->GetPCFKernelSize();
        
        ImGui::Text("Shadow Map: %dx%d", mapSize, mapSize);
        ImGui::Text("PCF Kernel: %dx%d", kernelSize, kernelSize);
        
        // メモリ使用量
        float memoryMB = (mapSize * mapSize * 4) / (1024.0f * 1024.0f);
        ImGui::Text("Memory Usage: %.2f MB", memoryMB);
        
        // パフォーマンス影響の色表示
        int quality = -1;
        if (mapSize <= 512 && kernelSize <= 1) quality = 0;
        else if (mapSize <= 1024 && kernelSize <= 3) quality = 1;
        else if (mapSize <= 2048 && kernelSize <= 5) quality = 2;
        else quality = 3;
        
        const char* impactText[] = {"Minimal Impact", "Low Impact", "Medium Impact", "High Impact"};
        ImVec4 impactColor[] = {
            ImVec4(0.2f, 1.0f, 0.2f, 1.0f),
            ImVec4(0.7f, 1.0f, 0.2f, 1.0f),
            ImVec4(1.0f, 0.8f, 0.2f, 1.0f),
            ImVec4(1.0f, 0.3f, 0.3f, 1.0f)
        };
        ImGui::TextColored(impactColor[quality], "%s", impactText[quality]);
    }
    
    ImGui::End();
}

void DebugUIManager::DrawCollisionDebug() {
    ImGui::Begin("Collision Debug", &windowVisibility_["CollisionDebug"]);
    
    CollisionManager* collisionManager = CollisionManager::GetInstance();
    
    // ヘッダー
    ImGui::Text("Collision System");
    ImGui::Separator();
    
    // デバッグワイヤーフレーム表示のON/OFF
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
                } else {
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
        
        // Type ID別の分布（汎用的な表示）
        if (!typeCountMap.empty()) {
            ImGui::Spacing();
            ImGui::Text("Type ID Distribution:");
            
            // Type IDでソートして表示
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
        } else {
            ImGui::Text("Active collision pairs:");
            ImGui::Spacing();
            
            // すべてのマスク情報を収集して整理
            std::set<std::pair<uint32_t, uint32_t>> collisionPairs;
            for (const auto& [typeA, typeBSet] : masks) {
                for (uint32_t typeB : typeBSet) {
                    // 小さい番号を先にして重複を避ける
                    if (typeA <= typeB) {
                        collisionPairs.insert({typeA, typeB});
                    } else {
                        collisionPairs.insert({typeB, typeA});
                    }
                }
            }
            
            // コリジョンペアを表示
            for (const auto& [typeA, typeB] : collisionPairs) {
                if (typeA == typeB) {
                    ImGui::BulletText("Type %02u <-> Type %02u (self-collision)", typeA, typeB);
                } else {
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
        } else {
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
        } else if (maxPairs < 500) {
            performanceColor = ImVec4(0.7f, 1.0f, 0.2f, 1.0f);
            performanceText = "Good";
        } else if (maxPairs < 2000) {
            performanceColor = ImVec4(1.0f, 0.8f, 0.2f, 1.0f);
            performanceText = "Moderate";
        } else {
            performanceColor = ImVec4(1.0f, 0.3f, 0.3f, 1.0f);
            performanceText = "Heavy";
        }
        
        ImGui::Text("Performance Load: ");
        ImGui::SameLine();
        ImGui::TextColored(performanceColor, "%s", performanceText);
    }
    
    ImGui::End();
}
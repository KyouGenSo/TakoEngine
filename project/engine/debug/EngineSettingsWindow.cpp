#include "EngineSettingsWindow.h"
#include "DebugUIManager.h"
#include "ImGuiThemes.h"
#include "WinApp.h"
#include "DX12Basic.h"
#include "Object3dBasic.h"
#include "FrameTimer.h"
#include "ShadowRenderer.h"
#include "Audio.h"
#include "CollisionManager.h"

#include "imgui.h"
#include <json.hpp>

#include <algorithm>
#include <filesystem>
#include <format>
#include <fstream>
#include <iomanip>
#include <string>

namespace Tako {

  namespace {
    using json = nlohmann::json;

    const char* const kFilePath = "resources/Json/EngineSettings.json";

    const char* const kCategoryNames[] = { "Display", "Time", "Rendering", "Audio", "Physics", "Editor" };

    constexpr int kFPSOptions[] = { 0, 30, 60, 120, 144 };

    std::string FPSLabel(int fps) {
      return fps > 0 ? std::to_string(fps) : "Unlimited";
    }

    DX12Basic* GetDX12() {
      return Object3dBasic::GetInstance()->GetDX12Basic();
    }

    void Log(const std::string& message, DebugUIManager::LogType type = DebugUIManager::LogType::Info) {
      DebugUIManager::GetInstance()->AddLog(message, type);
    }
  }

  void EngineSettingsWindow::Draw() {
    if (!*isOpen_) {
      return;
    }

    if (ImGui::Begin("Engine Settings", isOpen_)) {
      // 下段の Save/Reload ボタン分の高さを残す
      const float footerHeight = ImGui::GetFrameHeightWithSpacing();

      ImGui::BeginChild("##Categories", ImVec2(120.0f, -footerHeight), ImGuiChildFlags_Borders);
      for (int i = 0; i < static_cast<int>(Category::Count); ++i) {
        if (ImGui::Selectable(kCategoryNames[i], static_cast<int>(selectedCategory_) == i)) {
          selectedCategory_ = static_cast<Category>(i);
        }
      }
      ImGui::EndChild();

      ImGui::SameLine();

      ImGui::BeginChild("##Content", ImVec2(0.0f, -footerHeight));
      switch (selectedCategory_) {
      case Category::Display:   DrawDisplay();   break;
      case Category::Time:      DrawTime();      break;
      case Category::Rendering: DrawRendering(); break;
      case Category::Audio:     DrawAudio();     break;
      case Category::Physics:   DrawPhysics();   break;
      case Category::Editor:    DrawEditor();    break;
      default: break;
      }
      ImGui::EndChild();

      if (ImGui::Button("Save")) {
        Save();
      }
      ImGui::SameLine();
      if (ImGui::Button("Reload")) {
        Load();
      }
      ImGui::SameLine();
      ImGui::TextDisabled("%s", kFilePath);
    }
    ImGui::End();
  }

  void EngineSettingsWindow::Load() {
    std::ifstream ifs(kFilePath);
    if (!ifs) {
      return;
    }

    // 手編集された JSON の型不一致などは例外になるため、そこまでの適用を残して中断する
    try {
      const json root = json::parse(ifs);
      const json empty = json::object();

      DX12Basic* dx12 = GetDX12();
      const json& display = root.contains("Display") ? root["Display"] : empty;
      SetFullScreen(display.value("Fullscreen", WinApp::GetInstance()->IsFullScreen()));
      dx12->SetVSync(display.value("VSync", dx12->IsVSync()));
      dx12->SetTargetFPS(display.value("TargetFPS", dx12->GetTargetFPS()));

      ShadowRenderer* shadow = ShadowRenderer::GetInstance();
      const json& rendering = root.contains("Rendering") ? root["Rendering"] : empty;
      shadow->SetEnabled(rendering.value("ShadowEnabled", shadow->IsEnabled()));
      shadow->SetShadowMapSize(rendering.value("ShadowMapSize", shadow->GetShadowMapSize()));
      shadow->SetPCFKernelSize(rendering.value("PCFKernelSize", shadow->GetPCFKernelSize()));
      shadow->SetMaxShadowDistance(rendering.value("MaxShadowDistance", shadow->GetMaxShadowDistance()));
      shadow->SetShadowBias(rendering.value("ShadowBias", shadow->GetShadowBias()));

      const json& audio = root.contains("Audio") ? root["Audio"] : empty;
      masterVolume_ = audio.value("MasterVolume", masterVolume_);
      isMuted_      = audio.value("Mute", isMuted_);
      ApplyMasterVolume();

      CollisionManager* collision = CollisionManager::GetInstance();
      const json& physics = root.contains("Physics") ? root["Physics"] : empty;
      collision->SetDebugDrawEnabled(physics.value("ColliderDebugDraw", collision->IsDebugDrawEnabled()));

      ImGuiIO& io = ImGui::GetIO();
      const json& editor = root.contains("Editor") ? root["Editor"] : empty;
      io.FontGlobalScale = editor.value("UIScale", io.FontGlobalScale);
      const int themeCount = static_cast<int>(GetImGuiThemes().size());
      themeIndex_ = std::clamp(editor.value("Theme", themeIndex_), 0, themeCount - 1);
      ApplyImGuiTheme(themeIndex_);

      Log(std::format("Engine settings loaded: {}", kFilePath));
    }
    catch (const json::exception& e) {
      Log(std::format("Engine settings load failed: {}", e.what()), DebugUIManager::LogType::Warning);
    }
  }

  void EngineSettingsWindow::Save() {
    DX12Basic*      dx12   = GetDX12();
    ShadowRenderer* shadow = ShadowRenderer::GetInstance();

    json root;
    root["Display"] = {
      { "Fullscreen", WinApp::GetInstance()->IsFullScreen() },
      { "VSync",      dx12->IsVSync() },
      { "TargetFPS",  dx12->GetTargetFPS() },
    };
    root["Rendering"] = {
      { "ShadowEnabled",     shadow->IsEnabled() },
      { "ShadowMapSize",     shadow->GetShadowMapSize() },
      { "PCFKernelSize",     shadow->GetPCFKernelSize() },
      { "MaxShadowDistance", shadow->GetMaxShadowDistance() },
      { "ShadowBias",        shadow->GetShadowBias() },
    };
    root["Audio"] = {
      { "MasterVolume", masterVolume_ },
      { "Mute",         isMuted_ },
    };
    root["Physics"] = {
      { "ColliderDebugDraw", CollisionManager::GetInstance()->IsDebugDrawEnabled() },
    };
    root["Editor"] = {
      { "UIScale", ImGui::GetIO().FontGlobalScale },
      { "Theme",   themeIndex_ },
    };

    std::filesystem::create_directories(std::filesystem::path(kFilePath).parent_path());
    std::ofstream ofs(kFilePath);
    if (!ofs) {
      Log(std::format("Engine settings save failed: {}", kFilePath), DebugUIManager::LogType::Error);
      return;
    }
    ofs << std::setw(4) << root << std::endl;

    Log(std::format("Engine settings saved: {}", kFilePath));
  }

  void EngineSettingsWindow::DrawDisplay() {
    ImGui::Text("Resolution: %d x %d", WinApp::clientWidth, WinApp::clientHeight);

    bool fullScreen = WinApp::GetInstance()->IsFullScreen();
    if (ImGui::Checkbox("Fullscreen (F11)", &fullScreen)) {
      SetFullScreen(fullScreen);
    }

    DX12Basic* dx12 = GetDX12();
    bool vsync = dx12->IsVSync();
    if (ImGui::Checkbox("VSync", &vsync)) {
      dx12->SetVSync(vsync);
    }

    const int targetFPS = dx12->GetTargetFPS();
    if (ImGui::BeginCombo("FPS Limit", FPSLabel(targetFPS).c_str())) {
      for (int fps : kFPSOptions) {
        if (ImGui::Selectable(FPSLabel(fps).c_str(), fps == targetFPS)) {
          dx12->SetTargetFPS(fps);
        }
      }
      ImGui::EndCombo();
    }
  }

  void EngineSettingsWindow::DrawTime() {
    FrameTimer* timer = FrameTimer::GetInstance();

    float timeScale = timer->GetTimeScale();
    if (ImGui::SliderFloat("Time Scale", &timeScale, 0.0f, 2.0f, "%.2f")) {
      timer->SetTimeScale(timeScale);
    }
    for (float preset : kTimeScalePresets) {
      if (ImGui::Button(std::format("x{}", preset).c_str())) {
        timer->SetTimeScale(preset);
      }
      ImGui::SameLine();
    }
    ImGui::NewLine();
    ImGui::TextDisabled("Affects only code using GetDeltaTime / GetTimeScale");

    ImGui::Separator();
    ImGui::Text("Delta Time: %.4f s (Unscaled: %.4f s)", timer->GetDeltaTime(), timer->GetUnscaledDeltaTime());
    ImGui::Text("Game Time:  %.1f s", timer->GetGameTime());
  }

  void EngineSettingsWindow::DrawRendering() {
    ImGui::SeparatorText("Shadows");
    ShadowRenderer::GetInstance()->DrawImGui();
  }

  void EngineSettingsWindow::DrawAudio() {
    if (ImGui::SliderFloat("Master Volume", &masterVolume_, 0.0f, 1.0f, "%.2f")) {
      ApplyMasterVolume();
    }
    if (ImGui::Checkbox("Mute", &isMuted_)) {
      ApplyMasterVolume();
    }
  }

  void EngineSettingsWindow::DrawPhysics() {
    CollisionManager* collision = CollisionManager::GetInstance();
    bool debugDraw = collision->IsDebugDrawEnabled();
    if (ImGui::Checkbox("Collider Debug Draw", &debugDraw)) {
      collision->SetDebugDrawEnabled(debugDraw);
    }
  }

  void EngineSettingsWindow::DrawEditor() {
    ImGui::SliderFloat("UI Scale", &ImGui::GetIO().FontGlobalScale, 0.5f, 2.0f, "%.2f");

    const auto getThemeName = [](void*, int index) { return GetImGuiThemes()[index].name; };
    const int  themeCount   = static_cast<int>(GetImGuiThemes().size());
    if (ImGui::Combo("Theme", &themeIndex_, getThemeName, nullptr, themeCount, 12)) {
      ApplyImGuiTheme(themeIndex_);
    }
  }

  void EngineSettingsWindow::SetFullScreen(bool fullScreen) {
    if (toggleFullScreen_ && WinApp::GetInstance()->IsFullScreen() != fullScreen) {
      toggleFullScreen_();
    }
  }

  void EngineSettingsWindow::ApplyMasterVolume() {
    Audio::GetInstance()->SetMasterVolume(isMuted_ ? 0.0f : masterVolume_);
  }

} // namespace Tako

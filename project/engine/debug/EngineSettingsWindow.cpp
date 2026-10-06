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
#include "LineRenderer.h"
#include "ProjectSettings.h"
#include "StringUtility.h"

#include "imgui.h"
#include <json.hpp>

#include <ShlObj.h>

#include <algorithm>
#include <filesystem>
#include <format>
#include <fstream>
#include <iomanip>
#include <string>

namespace Tako {

  namespace {
    using json = nlohmann::json;

    // %APPDATA%/TakoEngine/EditorSettings.json。ユーザー単位で複数プロジェクトから共有する。取得できなければ空
    const std::filesystem::path& EditorSettingsPath() {
      static const std::filesystem::path path = [] {
        PWSTR appData = nullptr;
        std::filesystem::path result;
        if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_RoamingAppData, 0, nullptr, &appData))) {
          result = std::filesystem::path(appData) / L"TakoEngine" / L"EditorSettings.json";
        }
        // 失敗時も解放が必要
        CoTaskMemFree(appData);
        return result;
      }();
      return path;
    }

    std::string EditorSettingsPathText() {
      return StringUtility::ConvertString(EditorSettingsPath().wstring());
    }

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

    json GridToJson(const GridSettings& grid) {
      return { { "Size", grid.size }, { "CellSize", grid.cellSize } };
    }

    void LoadGrid(const json& parent, const char* key, GridSettings& grid) {
      if (!parent.contains(key)) {
        return;
      }
      const json& node = parent[key];
      grid.size     = node.value("Size", grid.size);
      grid.cellSize = node.value("CellSize", grid.cellSize);
    }

    void DrawGridSettings(const char* label, GridSettings& grid) {
      constexpr float kMaxCells = static_cast<float>(LineRenderer::kGridMaxCellCount);
      ImGui::PushID(label);
      ImGui::TextUnformatted(label);
      // 互いの値から範囲を決め、DrawGrid のマス数上限で全長が勝手に縮まないようにする
      ImGui::DragFloat("Size", &grid.size, 1.0f, grid.cellSize * 2.0f, grid.cellSize * kMaxCells, "%.1f", ImGuiSliderFlags_AlwaysClamp);
      ImGui::DragFloat("Cell Size", &grid.cellSize, 0.01f, grid.size / kMaxCells, grid.size * 0.5f, "%.3f", ImGuiSliderFlags_AlwaysClamp);
      ImGui::PopID();
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
        const bool saved = ProjectSettings::Save(GetDX12());
        Log(std::format("Project settings {}: {}", saved ? "saved" : "save failed", ProjectSettings::kFilePath),
            saved ? DebugUIManager::LogType::Info : DebugUIManager::LogType::Error);
        SaveEditorSettings();
      }
      ImGui::SameLine();
      if (ImGui::Button("Reload")) {
        const bool loaded = ProjectSettings::Load(GetDX12());
        Log(std::format("Project settings {}: {}", loaded ? "loaded" : "load failed", ProjectSettings::kFilePath),
            loaded ? DebugUIManager::LogType::Info : DebugUIManager::LogType::Warning);
        LoadEditorSettings();
      }
      ImGui::SameLine();
      ImGui::TextDisabled("%s", ProjectSettings::kFilePath);
      if (ImGui::BeginItemTooltip()) {
        ImGui::Text("Editor settings: %s", EditorSettingsPathText().c_str());
        ImGui::EndTooltip();
      }
    }
    ImGui::End();
  }

  void EngineSettingsWindow::LoadEditorSettings() {
    std::ifstream ifs(EditorSettingsPath());
    if (!ifs) {
      return;
    }

    // 手編集された JSON の型不一致などは例外になるため、そこまでの適用を残して中断する
    try {
      const json root  = json::parse(ifs);
      const json empty = json::object();

      const json& display = root.contains("Display") ? root["Display"] : empty;
      SetFullScreen(display.value("Fullscreen", WinApp::GetInstance()->IsFullScreen()));

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
      // エンジンのバージョンでテーマ一覧が異なりうるため名前で照合し、見つからなければ現在のテーマを保つ
      const std::string themeName = editor.value("Theme", std::string());
      const auto        themes    = GetImGuiThemes();
      const auto        it        = std::ranges::find_if(themes, [&](const ImGuiTheme& theme) { return themeName == theme.name; });
      if (it != themes.end()) {
        themeIndex_ = static_cast<int>(it - themes.begin());
      }
      ApplyImGuiTheme(themeIndex_);
      LoadGrid(editor, "DebugViewportGrid", debugViewportGrid_);
      LoadGrid(editor, "ParticleEditorGrid", particleEditorGrid_);

      Log(std::format("Editor settings loaded: {}", EditorSettingsPathText()));
    }
    catch (const json::exception& e) {
      Log(std::format("Editor settings load failed: {}", e.what()), DebugUIManager::LogType::Warning);
    }
  }

  void EngineSettingsWindow::SaveEditorSettings() {
    json root;
    root["Display"] = {
      { "Fullscreen", WinApp::GetInstance()->IsFullScreen() },
    };
    root["Audio"] = {
      { "MasterVolume", masterVolume_ },
      { "Mute",         isMuted_ },
    };
    root["Physics"] = {
      { "ColliderDebugDraw", CollisionManager::GetInstance()->IsDebugDrawEnabled() },
    };
    root["Editor"] = {
      { "UIScale",            ImGui::GetIO().FontGlobalScale },
      { "Theme",              GetImGuiThemes()[themeIndex_].name },
      { "DebugViewportGrid",  GridToJson(debugViewportGrid_) },
      { "ParticleEditorGrid", GridToJson(particleEditorGrid_) },
    };

    const std::filesystem::path& path = EditorSettingsPath();
    std::error_code ec;
    std::filesystem::create_directories(path.parent_path(), ec);
    std::ofstream ofs(path);
    if (!ofs) {
      Log(std::format("Editor settings save failed: {}", EditorSettingsPathText()), DebugUIManager::LogType::Error);
      return;
    }
    ofs << std::setw(4) << root << std::endl;

    Log(std::format("Editor settings saved: {}", EditorSettingsPathText()));
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

    ImGui::SeparatorText("Grid");
    DrawGridSettings("Debug Viewport", debugViewportGrid_);
    DrawGridSettings("Particle Editor", particleEditorGrid_);
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

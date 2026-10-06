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
#include "PostEffectManager.h"
#include "ProjectSettings.h"
#include "SceneManager.h"
#include "Input.h"
#include "StringUtility.h"
#include "CodeGenerator.h"

#include "imgui.h"
#include <json.hpp>

#include <ShlObj.h>

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <format>
#include <fstream>
#include <iomanip>
#include <string>
#include <utility>
#include <vector>

namespace Tako {

  namespace {
    using json   = nlohmann::json;
    using Window = DebugUIManager::Window;

    // 起動時に開くか選べるウィンドウ（メニュー順）。名前は表示ラベルと保存キーを兼ねる。Engine Settings と Help メニュー系は対象外
    constexpr std::pair<Window, const char*> kStartupWindowOptions[] = {
      { Window::DebugViewport,   "Debug Viewport" },
      { Window::SceneHierarchy,  "Scene Hierarchy" },
      { Window::Inspector,       "Inspector" },
      { Window::GameViewport,    "Game Viewport" },
      { Window::Console,         "Console" },
      { Window::Performance,     "Performance" },
      { Window::Assets,          "Assets" },
      { Window::InputDebug,      "Input Debug" },
      { Window::CollisionDebug,  "Collision Debug" },
      { Window::PostEffect,      "PostEffect Settings" },
      { Window::ParticleEditor,  "Particle Editor" },
      { Window::PrimitiveEditor, "Primitive Editor" },
      { Window::GlobalVariables, "Global Variables" },
    };

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

    // Debug 実行時の作業ディレクトリはプロジェクトごとに異なるので、プロジェクトの識別に使う
    std::string ProjectKey() {
      std::error_code ec;
      return StringUtility::ConvertString(std::filesystem::current_path(ec).wstring());
    }

    const char* const kCategoryNames[] = { "Application", "Display", "Time", "Rendering", "Audio", "Physics", "Input", "Editor" };

    constexpr int kFPSOptions[] = { 0, 30, 60, 120, 144 };

    constexpr std::pair<int32_t, int32_t> kResolutionPresets[] = { { 1280, 720 }, { 1600, 900 }, { 1920, 1080 }, { 2560, 1440 } };

    // PostEffectManager::AntiAliasing / Input::Stick の並びと一致させること
    const char* const kAntiAliasingNames[] = { "None", "FXAA" };
    const char* const kStickNames[]        = { "None", "Left", "Right" };

    constexpr float kBindingLabelOffset = 140.0f;  ///< アクション/軸名の右に割当を並べ始める x 位置

    constexpr ImVec4 kErrorColor   = { 1.0f, 0.3f, 0.3f, 1.0f };
    constexpr ImVec4 kWarningColor = { 1.0f, 0.8f, 0.2f, 1.0f };

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

    bool InputString(const char* label, std::string& value, ImGuiInputTextFlags flags = 0, ImGuiInputTextCallback callback = nullptr) {
      std::array<char, 64> buffer{};
      value.copy(buffer.data(), buffer.size() - 1);
      if (!ImGui::InputText(label, buffer.data(), buffer.size(), flags, callback)) {
        return false;
      }
      value = buffer.data();
      return true;
    }

    // ImGuiInputTextFlags_CallbackCharFilter 用。識別子に使える文字（英数字と _）だけを通す
    int IdentifierCharFilter(ImGuiInputTextCallbackData* data) {
      const ImWchar c = data->EventChar;
      return c < 128 && (std::isalnum(c) || c == '_') ? 0 : 1;
    }

    // enum の列挙子にできるか（識別子で、他の層と重複しない）
    bool IsValidLayerName(const std::vector<std::string>& names, size_t index) {
      return CodeGenerator::IsIdentifier(names[index]) && std::ranges::count(names, names[index]) == 1;
    }

    using NameMap      = std::map<std::string, std::string, std::less<>>;
    using HeaderWriter = std::function<bool(const std::filesystem::path&, std::span<const std::string>)>;

    template<class Map>
    std::vector<std::string> Keys(const Map& map) {
      std::vector<std::string> keys;
      for (const auto& [key, value] : map) {
        keys.push_back(key);
      }
      return keys;
    }

    // 改名の追跡の初期状態（どの名前も前回 Save 時と同じ）
    NameMap IdentityOrigins(std::span<const std::string> names) {
      NameMap origins;
      for (const std::string& name : names) {
        origins.emplace(name, name);
      }
      return origins;
    }

    // 「今の名前 → 前回 Save 時の名前」から、変わったものを「旧名 → 新名」にする
    CodeGenerator::RenameMap OriginRenames(const std::optional<NameMap>& origins) {
      CodeGenerator::RenameMap renames;
      if (origins) {
        for (const auto& [current, saved] : *origins) {
          if (current != saved) {
            renames.emplace(saved, current);
          }
        }
      }
      return renames;
    }

    // 値を保ったまま map のキーを付け替える（from が無ければ何もしない）
    template<class Map>
    void RenameKey(Map& map, const std::string& from, const std::string& to) {
      if (auto node = map.extract(from)) {
        node.key() = to;
        map.insert(std::move(node));
      }
    }

    /// <summary>
    /// 生成先パスの入力欄。生成先があり、ファイル名が識別子ならその名前（enum / namespace 名）を返す
    /// </summary>
    std::optional<std::string> DrawGeneratedHeaderField(const char* label, std::string& header) {
      InputString(label, header);
      ImGui::SetItemTooltip("Path from the project folder. Written on Save; renamed names are also renamed in the game's .h/.cpp");
      if (header.empty()) {
        return std::nullopt;
      }
      std::string scope = std::filesystem::path(header).stem().string();
      if (!CodeGenerator::IsIdentifier(scope)) {
        ImGui::TextColored(kErrorColor, "File name must be a C++ identifier (used as the generated name)");
        return std::nullopt;
      }
      return scope;
    }

    /// <summary>
    /// 生成先が空なら何もしない。ファイル名と names を検証し、改名を使用箇所へ反映してから write でヘッダを書く。失敗はログに出す
    /// </summary>
    /// <returns>成功（生成しない場合を含む）なら改名で書き換えたソースファイル数。失敗なら nullopt で、呼び出し側は改名の基準を進めない</returns>
    std::optional<size_t> GenerateHeader(const std::string& header, std::span<const std::string> names, const CodeGenerator::RenameMap& renames, const HeaderWriter& write) {
      if (header.empty()) {
        return 0;
      }

      // 生成できない間は呼び出し側の改名の基準を残し、直した後の Save で改名をまとめて反映する
      const std::filesystem::path path  = header;
      const std::string           scope = path.stem().string();
      if (!CodeGenerator::IsIdentifier(scope)) {
        Log(std::format("{} not generated: the file name must be a C++ identifier", header), DebugUIManager::LogType::Error);
        return std::nullopt;
      }
      for (const std::string& name : names) {
        if (!CodeGenerator::IsIdentifier(name) || std::ranges::count(names, name) != 1) {
          Log(std::format("{} not generated: \"{}\" is not a unique C++ identifier", header, name), DebugUIManager::LogType::Error);
          return std::nullopt;
        }
      }

      size_t changedFiles = 0;
      if (!renames.empty()) {
        const std::filesystem::path root = std::filesystem::current_path();
        try {
          const std::vector<std::filesystem::path> files = CodeGenerator::RenameUsages(root, scope, renames);
          Log(std::format("Renamed {} name(s) of {} in {} file(s)", renames.size(), scope, files.size()));
          for (const std::filesystem::path& file : files) {
            Log(std::format("  {}", StringUtility::ConvertString(file.lexically_relative(root).wstring())));
          }
          changedFiles = files.size();
        }
        catch (const std::filesystem::filesystem_error& e) {
          Log(std::format("Rename of {} failed: {}", scope, e.what()), DebugUIManager::LogType::Error);
          return std::nullopt;
        }
      }

      if (!write(path, names)) {
        Log(std::format("Generate failed: {}", header), DebugUIManager::LogType::Error);
        return std::nullopt;
      }
      Log(std::format("Generated {} ({})", header, scope));
      return changedFiles;
    }

    /// <summary>
    /// SceneManager に登録されたシーン名から選ぶコンボ（noneLabel を渡すと先頭に空文字の選択肢を出す）
    /// </summary>
    bool SceneCombo(const char* label, std::string& scene, const char* noneLabel = nullptr) {
      bool changed = false;
      const char* preview = scene.empty() && noneLabel ? noneLabel : scene.c_str();
      if (ImGui::BeginCombo(label, preview)) {
        if (noneLabel && ImGui::Selectable(noneLabel, scene.empty())) {
          scene.clear();
          changed = true;
        }
        for (const std::string& name : SceneManager::GetInstance()->GetSceneNames()) {
          if (ImGui::Selectable(name.c_str(), name == scene)) {
            scene   = name;
            changed = true;
          }
        }
        ImGui::EndCombo();
      }
      return changed;
    }

    const char* KeyLabel(BYTE key) {
      const char* name = Input::FindName(Input::GetKeyTable(), key);
      return name ? name : "-";
    }

    const char* ButtonLabel(WORD button) {
      const char* name = Input::FindName(Input::GetButtonTable(), button);
      return name ? name : "?";
    }

    const char* MouseButtonLabel(int button) {
      const char* name = Input::FindName(Input::GetMouseButtonTable(), button);
      return name ? name : "?";
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
      case Category::Application: DrawApplication(); break;
      case Category::Display:     DrawDisplay();     break;
      case Category::Time:        DrawTime();        break;
      case Category::Rendering:   DrawRendering();   break;
      case Category::Audio:       DrawAudio();       break;
      case Category::Physics:     DrawPhysics();     break;
      case Category::Input:       DrawInput();       break;
      case Category::Editor:      DrawEditor();      break;
      default: break;
      }
      ImGui::EndChild();

      if (ImGui::Button("Save")) {
        const bool saved = ProjectSettings::Save(GetDX12());
        Log(std::format("Project settings {}: {}", saved ? "saved" : "save failed", ProjectSettings::kFilePath),
            saved ? DebugUIManager::LogType::Info : DebugUIManager::LogType::Error);
        SaveGeneratedHeaders();
        SaveEditorSettings();
      }
      ImGui::SameLine();
      if (ImGui::Button("Reload")) {
        // ウィンドウ設定は起動時の値なので、読み直してもタイトル以外は今のウィンドウに反映しない
        ProjectSettings::LoadWindow();
        WinApp::GetInstance()->SetWindowTitle(StringUtility::ConvertString(ProjectSettings::GetWindowSettings().productName));
        const bool loaded = ProjectSettings::Load(GetDX12());
        Log(std::format("Project settings {}: {}", loaded ? "loaded" : "load failed", ProjectSettings::kFilePath),
            loaded ? DebugUIManager::LogType::Info : DebugUIManager::LogType::Warning);
        layerOrigins_.reset();
        actionOrigins_.reset();
        axisOrigins_.reset();
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
      if (editor.contains("ViewportCamera")) {
        const json& camera = editor["ViewportCamera"];
        viewportCamera_.moveSpeed       = camera.value("MoveSpeed", viewportCamera_.moveSpeed);
        viewportCamera_.lookSensitivity = camera.value("Sensitivity", viewportCamera_.lookSensitivity);
        viewportCamera_.fovY            = camera.value("FovY", viewportCamera_.fovY);
      }
      if (editor.contains("StartupWindows")) {
        const auto names = editor["StartupWindows"].get<std::vector<std::string>>();
        for (const auto& [window, name] : kStartupWindowOptions) {
          startupWindows_[static_cast<size_t>(window)] = std::ranges::find(names, name) != names.end();
        }
      }

      SceneManager* sceneManager = SceneManager::GetInstance();
      if (editor.contains("PlayFromScene") && editor["PlayFromScene"].is_object()) {
        playFromSceneByProject_ = editor["PlayFromScene"].get<std::map<std::string, std::string>>();
      }
      const auto playFromScene = playFromSceneByProject_.find(ProjectKey());
      sceneManager->SetStartupSceneOverride(playFromScene != playFromSceneByProject_.end() ? playFromScene->second : std::string());
      // 個人設定なので他の人には見えない。戻し忘れに気付けるよう毎回知らせる
      if (!sceneManager->GetStartupSceneOverride().empty()) {
        Log(std::format("Play From Scene is set: {} (Engine Settings > Application)", sceneManager->GetStartupSceneOverride()), DebugUIManager::LogType::Warning);
      }

      Log(std::format("Editor settings loaded: {}", EditorSettingsPathText()));
    }
    catch (const json::exception& e) {
      Log(std::format("Editor settings load failed: {}", e.what()), DebugUIManager::LogType::Warning);
    }
  }

  void EngineSettingsWindow::SaveEditorSettings() {
    // 他プロジェクトの分は起動時に読んだまま残し、このプロジェクトの分だけ差し替える
    const std::string& playFromScene = SceneManager::GetInstance()->GetStartupSceneOverride();
    if (playFromScene.empty()) {
      playFromSceneByProject_.erase(ProjectKey());
    }
    else {
      playFromSceneByProject_[ProjectKey()] = playFromScene;
    }

    json startupWindows = json::array();
    for (const auto& [window, name] : kStartupWindowOptions) {
      if (startupWindows_[static_cast<size_t>(window)]) {
        startupWindows.push_back(name);
      }
    }

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
      { "ViewportCamera",     { { "MoveSpeed", viewportCamera_.moveSpeed }, { "Sensitivity", viewportCamera_.lookSensitivity }, { "FovY", viewportCamera_.fovY } } },
      { "StartupWindows",     startupWindows },
      { "PlayFromScene",      playFromSceneByProject_ },
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

  void EngineSettingsWindow::DrawApplication() {
    ProjectSettings::WindowSettings& window = ProjectSettings::GetWindowSettings();
    if (InputString("Product Name", window.productName)) {
      WinApp::GetInstance()->SetWindowTitle(StringUtility::ConvertString(window.productName));
    }

    SceneManager* sceneManager = SceneManager::GetInstance();
    std::string startupScene = sceneManager->GetStartupScene();
    if (SceneCombo("Startup Scene", startupScene)) {
      sceneManager->SetStartupScene(startupScene);
    }

    std::string playFromScene = sceneManager->GetStartupSceneOverride();
    if (SceneCombo("Play From Scene", playFromScene, "(Startup Scene)")) {
      sceneManager->SetStartupSceneOverride(playFromScene);
    }
    ImGui::SetItemTooltip("Editor only. Replaces Startup Scene on this PC from the next launch");
  }

  void EngineSettingsWindow::DrawDisplay() {
    WinApp* winApp = WinApp::GetInstance();
    ImGui::Text("Resolution: %d x %d", WinApp::clientWidth, WinApp::clientHeight);

    bool fullScreen = winApp->IsFullScreen();
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

    ImGui::SeparatorText("Window (Startup)");
    ProjectSettings::WindowSettings& window = ProjectSettings::GetWindowSettings();
    int size[2] = { window.width, window.height };
    if (ImGui::InputInt2("Size", size)) {
      window.width  = (std::max)(size[0], 1);
      window.height = (std::max)(size[1], 1);
    }
    ImGui::SameLine();
    if (ImGui::BeginCombo("##Preset", "Preset", ImGuiComboFlags_NoPreview)) {
      for (const auto& [width, height] : kResolutionPresets) {
        if (ImGui::Selectable(std::format("{} x {}", width, height).c_str(), width == window.width && height == window.height)) {
          window.width  = width;
          window.height = height;
        }
      }
      ImGui::EndCombo();
    }
    ImGui::BeginDisabled(winApp->IsFullScreen());
    if (ImGui::Button("Apply to Window")) {
      winApp->RequestClientSize(window.width, window.height);
    }
    ImGui::EndDisabled();
    ImGui::Checkbox("Start Fullscreen", &window.startFullscreen);
    ImGui::Checkbox("Resizable", &window.resizable);
    ImGui::SetItemTooltip("Applied from the next launch");
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

    float maxDeltaTime = timer->GetMaxDeltaTime();
    if (ImGui::SliderFloat("Max Delta Time", &maxDeltaTime, 1.0f / 60.0f, 1.0f, "%.3f s", ImGuiSliderFlags_AlwaysClamp)) {
      timer->SetMaxDeltaTime(maxDeltaTime);
    }
    ImGui::SetItemTooltip("Upper limit of delta time after a stall (breakpoint, window drag)");

    ImGui::Separator();
    ImGui::Text("Delta Time: %.4f s (Unscaled: %.4f s)", timer->GetDeltaTime(), timer->GetUnscaledDeltaTime());
    ImGui::Text("Game Time:  %.1f s", timer->GetGameTime());
  }

  void EngineSettingsWindow::DrawRendering() {
    DX12Basic*         dx12       = GetDX12();
    PostEffectManager* postEffect = PostEffectManager::GetInstance();

    ImGui::SeparatorText("Output");
    if (!isClearColorDirty_) {
      clearColorEdit_ = postEffect->GetClearColor();
    }
    isClearColorDirty_ |= ImGui::ColorEdit4("Clear Color", &clearColorEdit_.x);

    int antiAliasing = static_cast<int>(postEffect->GetAntiAliasing());
    if (ImGui::Combo("Anti-Aliasing", &antiAliasing, kAntiAliasingNames, IM_ARRAYSIZE(kAntiAliasingNames))) {
      postEffect->SetAntiAliasing(static_cast<PostEffectManager::AntiAliasing>(antiAliasing));
    }

    if (!isRenderScaleDirty_) {
      renderScaleEdit_ = dx12->GetRenderScale();
    }
    isRenderScaleDirty_ |= ImGui::SliderFloat("Render Scale", &renderScaleEdit_, DX12Basic::kMinRenderScale, 1.0f, "%.2f", ImGuiSliderFlags_AlwaysClamp);
    ImGui::TextDisabled("Internal: %u x %u (UI stays at window resolution)", dx12->GetSceneWidth(), dx12->GetSceneHeight());

    // RT を作り直す設定は、ドラッグやカラーピッカーの操作を終えてから 1 回だけ適用する
    if (!ImGui::IsAnyItemActive()) {
      if (isClearColorDirty_) {
        postEffect->SetClearColor(clearColorEdit_);
        isClearColorDirty_ = false;
      }
      if (isRenderScaleDirty_) {
        dx12->SetRenderScale(renderScaleEdit_);
        isRenderScaleDirty_ = false;
      }
    }

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

    DrawCollisionLayers();
  }

  void EngineSettingsWindow::DrawCollisionLayers() {
    CollisionManager*         collision = CollisionManager::GetInstance();
    std::vector<std::string>& names     = collision->GetLayerNames();

    // 名前の追加・削除・並べ替えに合わせて動かすので、ずれていたら（初回・Reload 後）今の名前を基準にする
    if (!layerOrigins_ || layerOrigins_->size() != names.size()) {
      layerOrigins_ = names;
    }

    ImGui::SeparatorText("Layers");
    if (const auto enumName = DrawGeneratedHeaderField("Enum Header", ProjectSettings::GetGeneratedHeaders().collisionLayers)) {
      ImGui::TextDisabled("Index = collider type ID. Saved as enum class %s", enumName->c_str());
    }
    else {
      ImGui::TextDisabled("Index = collider type ID (keep the game's ID order)");
    }

    std::optional<size_t> removeIndex;
    std::optional<size_t> swapIndex;  // この層と 1 つ下の層を入れ替える
    for (size_t i = 0; i < names.size(); ++i) {
      ImGui::PushID(static_cast<int>(i));
      ImGui::AlignTextToFramePadding();
      ImGui::Text("%2zu", i);
      ImGui::SameLine();
      ImGui::SetNextItemWidth(ImGui::GetFontSize() * 14.0f);
      InputString("##Name", names[i], ImGuiInputTextFlags_CallbackCharFilter, IdentifierCharFilter);
      ImGui::SameLine();
      ImGui::BeginDisabled(i == 0);
      if (ImGui::ArrowButton("##Up", ImGuiDir_Up)) {
        swapIndex = i - 1;
      }
      ImGui::EndDisabled();
      ImGui::SameLine();
      ImGui::BeginDisabled(i + 1 == names.size());
      if (ImGui::ArrowButton("##Down", ImGuiDir_Down)) {
        swapIndex = i;
      }
      ImGui::EndDisabled();
      ImGui::SameLine();
      if (ImGui::Button("x")) {
        removeIndex = i;
      }
      if (!IsValidLayerName(names, i)) {
        ImGui::SameLine();
        ImGui::TextColored(kErrorColor, "!");
        ImGui::SetItemTooltip("Must be a unique C++ identifier");
      }
      ImGui::PopID();
    }

    // ループ中に names を書き換えないよう、操作はまとめて後で適用する
    if (removeIndex) {
      collision->RemoveLayer(static_cast<uint32_t>(*removeIndex));
      layerOrigins_->erase(layerOrigins_->begin() + *removeIndex);
      isLayerIdChanged_ = true;
    }
    if (swapIndex) {
      collision->SwapLayers(static_cast<uint32_t>(*swapIndex), static_cast<uint32_t>(*swapIndex + 1));
      std::swap((*layerOrigins_)[*swapIndex], (*layerOrigins_)[*swapIndex + 1]);
      isLayerIdChanged_ = true;
    }

    ImGui::BeginDisabled(names.size() >= CollisionManager::kMaxLayers);
    if (ImGui::Button("Add Layer")) {
      names.push_back(std::format("Layer{}", names.size()));
      layerOrigins_->emplace_back();
    }
    ImGui::EndDisabled();
    if (isLayerIdChanged_) {
      ImGui::PushStyleColor(ImGuiCol_Text, kWarningColor);
      ImGui::TextWrapped("Layer IDs changed. Running colliders keep the old IDs until you Save, rebuild and restart");
      ImGui::PopStyleColor();
    }

    if (names.empty()) {
      return;
    }

    ImGui::SeparatorText("Collision Matrix");
    const int count = static_cast<int>(names.size());
    constexpr ImGuiTableFlags kTableFlags = ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_BordersInnerH | ImGuiTableFlags_HighlightHoveredColumn;
    if (ImGui::BeginTable("##CollisionMatrix", count + 1, kTableFlags)) {
      ImGui::TableSetupColumn("##Layer");
      for (const std::string& name : names) {
        ImGui::TableSetupColumn(name.c_str(), ImGuiTableColumnFlags_AngledHeader);
      }
      ImGui::TableAngledHeadersRow();

      for (int row = 0; row < count; ++row) {
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted(names[row].c_str());
        // 対称なので上三角だけ出す
        for (int column = row; column < count; ++column) {
          ImGui::TableSetColumnIndex(column + 1);
          ImGui::PushID(row * count + column);
          const uint32_t typeA      = static_cast<uint32_t>(row);
          const uint32_t typeB      = static_cast<uint32_t>(column);
          bool           canCollide = collision->CanCollide(typeA, typeB);
          if (ImGui::Checkbox("##Pair", &canCollide)) {
            collision->SetCollisionMask(typeA, typeB, canCollide);
          }
          ImGui::SetItemTooltip("%s x %s", names[row].c_str(), names[column].c_str());
          ImGui::PopID();
        }
      }
      ImGui::EndTable();
    }
  }

  void EngineSettingsWindow::SaveGeneratedHeaders() {
    const ProjectSettings::GeneratedHeaders& headers = ProjectSettings::GetGeneratedHeaders();

    const std::vector<std::string>& layers = CollisionManager::GetInstance()->GetLayerNames();
    CodeGenerator::RenameMap        layerRenames;
    if (layerOrigins_) {
      for (size_t i = 0; i < layers.size() && i < layerOrigins_->size(); ++i) {
        const std::string& origin = (*layerOrigins_)[i];
        if (!origin.empty() && origin != layers[i]) {
          layerRenames.emplace(origin, layers[i]);
        }
      }
    }
    const auto writeLayers = [](const std::filesystem::path& path, std::span<const std::string> names) {
      return CodeGenerator::WriteEnumHeader(path, names, "衝突層（Engine Settings > Physics > Layers。値はコライダーの型 ID）");
    };
    if (const auto changedFiles = GenerateHeader(headers.collisionLayers, layers, layerRenames, writeLayers)) {
      layerOrigins_ = layers;
      isRebuildRequired_ |= *changedFiles > 0;
    }

    Input*                         input   = Input::GetInstance();
    const std::vector<std::string> actions = Keys(input->GetActions());
    const auto writeActions = [](const std::filesystem::path& path, std::span<const std::string> names) {
      return CodeGenerator::WriteNameHeader(path, names, "入力アクション名（Engine Settings > Input > Actions。Input::TriggerAction などに渡す）");
    };
    if (const auto changedFiles = GenerateHeader(headers.inputActions, actions, OriginRenames(actionOrigins_), writeActions)) {
      actionOrigins_ = IdentityOrigins(actions);
      isRebuildRequired_ |= *changedFiles > 0;
    }

    const std::vector<std::string> axes = Keys(input->GetAxes());
    const auto writeAxes = [](const std::filesystem::path& path, std::span<const std::string> names) {
      return CodeGenerator::WriteNameHeader(path, names, "入力軸名（Engine Settings > Input > Axes。Input::GetAxis に渡す）");
    };
    if (const auto changedFiles = GenerateHeader(headers.inputAxes, axes, OriginRenames(axisOrigins_), writeAxes)) {
      axisOrigins_ = IdentityOrigins(axes);
      isRebuildRequired_ |= *changedFiles > 0;
    }
  }

  std::optional<std::string> EngineSettingsWindow::DrawRenamableName(const std::string& name, const std::function<bool(const std::string&)>& isTaken) {
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted(name.c_str());
    if (ImGui::IsItemClicked()) {
      renameBuffer_.fill('\0');
      name.copy(renameBuffer_.data(), renameBuffer_.size() - 1);
      ImGui::OpenPopup("##Rename");
    }
    ImGui::SetItemTooltip("Click to rename");
    if (!CodeGenerator::IsIdentifier(name)) {
      ImGui::SameLine();
      ImGui::TextColored(kErrorColor, "!");
      ImGui::SetItemTooltip("Not a C++ identifier: the name header cannot be generated");
    }

    std::optional<std::string> renamed;
    if (ImGui::BeginPopup("##Rename")) {
      if (ImGui::IsWindowAppearing()) {
        ImGui::SetKeyboardFocusHere();
      }
      const bool        isEntered = ImGui::InputText("##NewName", renameBuffer_.data(), renameBuffer_.size(),
                                                     ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_CallbackCharFilter, IdentifierCharFilter);
      const std::string newName   = renameBuffer_.data();
      const bool        isValid   = CodeGenerator::IsIdentifier(newName) && (newName == name || !isTaken(newName));
      if (!isValid) {
        ImGui::TextColored(kErrorColor, "Must be a unique C++ identifier");
      }
      if (isEntered && isValid) {
        if (newName != name) {
          renamed = newName;
        }
        ImGui::CloseCurrentPopup();
      }
      ImGui::EndPopup();
    }
    return renamed;
  }

  void EngineSettingsWindow::DrawInput() {
    Input* input = Input::GetInstance();
    UpdateBindingCapture();

    float leftDeadZone = input->GetLeftStickDeadZone();
    if (ImGui::SliderFloat("Left Stick Dead Zone", &leftDeadZone, 0.0f, 0.9f, "%.2f")) {
      input->SetLeftStickDeadZone(leftDeadZone);
    }
    float rightDeadZone = input->GetRightStickDeadZone();
    if (ImGui::SliderFloat("Right Stick Dead Zone", &rightDeadZone, 0.0f, 0.9f, "%.2f")) {
      input->SetRightStickDeadZone(rightDeadZone);
    }
    if (input->IsConnect()) {
      const Vector2 left  = input->GetLeftStick();
      const Vector2 right = input->GetRightStick();
      ImGui::TextDisabled("Left (%.2f, %.2f)  Right (%.2f, %.2f)", left.x, left.y, right.x, right.y);
    }

    Input::ActionMap&                  actions = input->GetActions();
    Input::AxisMap&                    axes    = input->GetAxes();
    ProjectSettings::GeneratedHeaders& headers = ProjectSettings::GetGeneratedHeaders();
    if (!actionOrigins_) {
      actionOrigins_ = IdentityOrigins(Keys(actions));
    }
    if (!axisOrigins_) {
      axisOrigins_ = IdentityOrigins(Keys(axes));
    }

    ImGui::SeparatorText("Actions");
    if (const auto scope = DrawGeneratedHeaderField("Name Header##Actions", headers.inputActions)) {
      ImGui::TextDisabled("Use as %s::<Name>", scope->c_str());
    }
    ImGui::TextDisabled("Click a name to rename, a binding to remove it");
    std::optional<std::string>                         removedAction;
    std::optional<std::pair<std::string, std::string>> renamedAction;  // 旧名, 新名
    // 軸と同名でも ID が衝突しないよう区切る
    ImGui::PushID("Actions");
    for (auto& [name, binding] : actions) {
      ImGui::PushID(name.c_str());
      if (auto newName = DrawRenamableName(name, [&actions](const std::string& other) { return actions.contains(other); })) {
        renamedAction.emplace(name, std::move(*newName));
      }
      ImGui::SameLine(kBindingLabelOffset);

      std::optional<size_t> removedKey;
      std::optional<size_t> removedButton;
      std::optional<size_t> removedMouse;
      for (size_t i = 0; i < binding.keys.size(); ++i) {
        ImGui::PushID(static_cast<int>(i));
        if (ImGui::SmallButton(KeyLabel(binding.keys[i]))) {
          removedKey = i;
        }
        ImGui::PopID();
        ImGui::SameLine();
      }
      for (size_t i = 0; i < binding.buttons.size(); ++i) {
        ImGui::PushID(static_cast<int>(binding.keys.size() + i));
        if (ImGui::SmallButton(std::format("Pad {}", ButtonLabel(binding.buttons[i])).c_str())) {
          removedButton = i;
        }
        ImGui::PopID();
        ImGui::SameLine();
      }
      for (size_t i = 0; i < binding.mouseButtons.size(); ++i) {
        ImGui::PushID(static_cast<int>(binding.keys.size() + binding.buttons.size() + i));
        if (ImGui::SmallButton(std::format("Mouse {}", MouseButtonLabel(binding.mouseButtons[i])).c_str())) {
          removedMouse = i;
        }
        ImGui::PopID();
        ImGui::SameLine();
      }
      if (removedKey) {
        binding.keys.erase(binding.keys.begin() + *removedKey);
      }
      if (removedButton) {
        binding.buttons.erase(binding.buttons.begin() + *removedButton);
      }
      if (removedMouse) {
        binding.mouseButtons.erase(binding.mouseButtons.begin() + *removedMouse);
      }

      DrawCaptureButton("+Key", name, CaptureSlot::ActionKey);
      ImGui::SameLine();
      DrawCaptureButton("+Pad", name, CaptureSlot::ActionButton);
      ImGui::SameLine();
      // ImGui 上のクリックはゲームへ渡さないので、押して登録する方式は使えない。一覧から選ぶ
      if (ImGui::SmallButton("+Mouse")) {
        ImGui::OpenPopup("##MouseButtons");
      }
      if (ImGui::BeginPopup("##MouseButtons")) {
        for (const auto& [button, buttonName] : Input::GetMouseButtonTable()) {
          const bool isBound = std::ranges::find(binding.mouseButtons, button) != binding.mouseButtons.end();
          if (ImGui::Selectable(buttonName, isBound, isBound ? ImGuiSelectableFlags_Disabled : ImGuiSelectableFlags_None)) {
            binding.mouseButtons.push_back(button);
          }
        }
        ImGui::EndPopup();
      }
      ImGui::SameLine();
      if (ImGui::SmallButton("x")) {
        removedAction = name;
      }
      ImGui::PopID();
    }
    ImGui::PopID();
    // ループ中に map を書き換えないよう、削除と改名はまとめて後で適用する
    // Input 側の API を通し、実行中のゲームが旧名で呼んでも落ちないようにする
    if (removedAction) {
      input->RemoveAction(*removedAction);
      actionOrigins_->erase(*removedAction);
    }
    if (renamedAction) {
      input->RenameAction(renamedAction->first, renamedAction->second);
      RenameKey(*actionOrigins_, renamedAction->first, renamedAction->second);
    }
    ImGui::SetNextItemWidth(160.0f);
    ImGui::InputText("##NewAction", newActionName_.data(), newActionName_.size(), ImGuiInputTextFlags_CallbackCharFilter, IdentifierCharFilter);
    ImGui::SameLine();
    ImGui::BeginDisabled(!CodeGenerator::IsIdentifier(newActionName_.data()));
    if (ImGui::Button("Add Action")) {
      actions.try_emplace(newActionName_.data());
      newActionName_.fill('\0');
    }
    ImGui::EndDisabled();

    ImGui::SeparatorText("Axes");
    if (const auto scope = DrawGeneratedHeaderField("Name Header##Axes", headers.inputAxes)) {
      ImGui::TextDisabled("Use as %s::<Name>", scope->c_str());
    }
    ImGui::TextDisabled("Click a name to rename, a direction to bind, right-click a direction to clear");
    std::optional<std::string>                         removedAxis;
    std::optional<std::pair<std::string, std::string>> renamedAxis;  // 旧名, 新名
    ImGui::PushID("Axes");
    for (auto& [name, axis] : axes) {
      ImGui::PushID(name.c_str());
      if (auto newName = DrawRenamableName(name, [&axes](const std::string& other) { return axes.contains(other); })) {
        renamedAxis.emplace(name, std::move(*newName));
      }
      ImGui::SameLine(kBindingLabelOffset);

      const std::pair<const char*, CaptureSlot> directions[] = {
        { "Up", CaptureSlot::AxisUp }, { "Down", CaptureSlot::AxisDown }, { "Left", CaptureSlot::AxisLeft }, { "Right", CaptureSlot::AxisRight },
      };
      BYTE* const keys[] = { &axis.up, &axis.down, &axis.left, &axis.right };
      for (size_t i = 0; i < std::size(directions); ++i) {
        const auto& [direction, slot] = directions[i];
        DrawCaptureButton(std::format("{}: {}", direction, KeyLabel(*keys[i])).c_str(), name, slot);
        if (ImGui::IsItemClicked(ImGuiMouseButton_Right)) {
          *keys[i] = 0;
        }
        ImGui::SameLine();
      }

      int stick = static_cast<int>(axis.stick);
      ImGui::SetNextItemWidth(80.0f);
      if (ImGui::Combo("##Stick", &stick, kStickNames, IM_ARRAYSIZE(kStickNames))) {
        axis.stick = static_cast<Input::Stick>(stick);
      }
      ImGui::SetItemTooltip("Stick added to the keys");
      ImGui::SameLine();
      if (ImGui::SmallButton("x")) {
        removedAxis = name;
      }
      ImGui::SameLine();
      const Vector2 value = input->GetAxis(name);
      ImGui::TextDisabled("(%.2f, %.2f)", value.x, value.y);
      ImGui::PopID();
    }
    ImGui::PopID();
    if (removedAxis) {
      input->RemoveAxis(*removedAxis);
      axisOrigins_->erase(*removedAxis);
    }
    if (renamedAxis) {
      input->RenameAxis(renamedAxis->first, renamedAxis->second);
      RenameKey(*axisOrigins_, renamedAxis->first, renamedAxis->second);
    }
    ImGui::SetNextItemWidth(160.0f);
    ImGui::InputText("##NewAxis", newAxisName_.data(), newAxisName_.size(), ImGuiInputTextFlags_CallbackCharFilter, IdentifierCharFilter);
    ImGui::SameLine();
    ImGui::BeginDisabled(!CodeGenerator::IsIdentifier(newAxisName_.data()));
    if (ImGui::Button("Add Axis")) {
      axes.try_emplace(newAxisName_.data());
      newAxisName_.fill('\0');
    }
    ImGui::EndDisabled();
  }

  void EngineSettingsWindow::UpdateBindingCapture() {
    if (!capture_) {
      return;
    }

    Input* input = Input::GetInstance();
    if (capture_->slot == CaptureSlot::ActionButton) {
      const auto buttonTable = Input::GetButtonTable();
      const auto pressed     = std::ranges::find_if(buttonTable, [input](const auto& entry) { return input->TriggerButton(entry.code); });
      if (pressed == buttonTable.end()) {
        return;
      }
      // 待ち受け中にアクションが削除されていたら何もしない
      if (const auto it = input->GetActions().find(capture_->name); it != input->GetActions().end() && std::ranges::find(it->second.buttons, pressed->code) == it->second.buttons.end()) {
        it->second.buttons.push_back(pressed->code);
      }
      capture_.reset();
      return;
    }

    const auto keyTable = Input::GetKeyTable();
    const auto pressed  = std::ranges::find_if(keyTable, [input](const auto& entry) { return input->TriggerKey(entry.code); });
    if (pressed == keyTable.end()) {
      return;
    }

    if (capture_->slot == CaptureSlot::ActionKey) {
      if (const auto it = input->GetActions().find(capture_->name); it != input->GetActions().end() && std::ranges::find(it->second.keys, pressed->code) == it->second.keys.end()) {
        it->second.keys.push_back(pressed->code);
      }
    }
    else if (const auto it = input->GetAxes().find(capture_->name); it != input->GetAxes().end()) {
      Input::AxisBinding& axis = it->second;
      switch (capture_->slot) {
      case CaptureSlot::AxisUp:    axis.up    = pressed->code; break;
      case CaptureSlot::AxisDown:  axis.down  = pressed->code; break;
      case CaptureSlot::AxisLeft:  axis.left  = pressed->code; break;
      case CaptureSlot::AxisRight: axis.right = pressed->code; break;
      default: break;
      }
    }
    capture_.reset();
  }

  void EngineSettingsWindow::DrawCaptureButton(const char* label, const std::string& name, CaptureSlot slot) {
    const bool isCapturing = capture_ && capture_->name == name && capture_->slot == slot;
    // ### 以降を ID にし、表示が "Press..." に変わっても同じボタンとして扱う
    if (ImGui::SmallButton(std::format("{}###{}", isCapturing ? "Press..." : label, static_cast<int>(slot)).c_str())) {
      if (isCapturing) {
        capture_.reset();
      }
      else {
        capture_ = BindingCapture{ name, slot };
      }
    }
  }

  void EngineSettingsWindow::DrawEditor() {
    ImGui::SliderFloat("UI Scale", &ImGui::GetIO().FontGlobalScale, 0.5f, 2.0f, "%.2f");

    const auto getThemeName = [](void*, int index) { return GetImGuiThemes()[index].name; };
    const int  themeCount   = static_cast<int>(GetImGuiThemes().size());
    if (ImGui::Combo("Theme", &themeIndex_, getThemeName, nullptr, themeCount, 12)) {
      ApplyImGuiTheme(themeIndex_);
    }

    ImGui::SeparatorText("Viewport Camera");
    ImGui::DragFloat("Default Move Speed", &viewportCamera_.moveSpeed, 0.1f, 0.05f, 500.0f, "%.2f", ImGuiSliderFlags_AlwaysClamp);
    ImGui::SetItemTooltip("Speed on first open and after Reset");
    ImGui::SliderFloat("Look Sensitivity", &viewportCamera_.lookSensitivity, 0.1f, 3.0f, "%.2f");
    ImGui::SliderAngle("FOV", &viewportCamera_.fovY, 10.0f, 120.0f);

    ImGui::SeparatorText("Grid");
    DrawGridSettings("Debug Viewport", debugViewportGrid_);
    DrawGridSettings("Particle Editor", particleEditorGrid_);

    ImGui::SeparatorText("Startup Windows");
    if (ImGui::Button("Use Current")) {
      for (const auto& [window, name] : kStartupWindowOptions) {
        startupWindows_[static_cast<size_t>(window)] = DebugUIManager::GetInstance()->IsWindowVisible(window);
      }
    }
    ImGui::SetItemTooltip("Copy the currently open windows");
    if (ImGui::BeginTable("##StartupWindows", 2)) {
      for (const auto& [window, name] : kStartupWindowOptions) {
        ImGui::TableNextColumn();
        ImGui::Checkbox(name, &startupWindows_[static_cast<size_t>(window)]);
      }
      ImGui::EndTable();
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

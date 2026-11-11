#include "DebugUIManager.h"
#include "FrameTimer.h"
#include "SrvManager.h"
#include "RtvManager.h"
#include "DsvManager.h"
#include "TextureManager.h"
#include "Renderer.h"
#include "Input.h"
#include "CollisionManager.h"
#include "PostEffectManager.h"
#include "RenderPipeline.h"
#include "WinApp.h"
#include "DX12Basic.h"
#include "ModelManager.h"
#include "ShadowRenderer.h"
#include "SceneManager.h"
#include "BaseScene.h"
#include "imgui_internal.h"
#include "LineRenderer.h"
#include "GPUParticle.h"
#include "Logger.h"
#include "EmitterManager.h"
#include "Audio.h"
#include "BehaviorTreeEditor.h"
#include "NewScript.h"
#include "UnsavedChangesModal.h"

#include <algorithm>
#include <iomanip>
#include <sstream>
#include <set>
#include <map>
#include <unordered_map>
#include <cstring>
#include <chrono>
#include <format>
#include <span>

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
    constexpr ImVec4 kWarningColor      = ImVec4(1.00f, 0.80f, 0.20f, 1.0f);  // 対応が必要な状態の表示色
    constexpr float  kIconButtonAspect  = 1.6f;                               // アイコンボタンの幅 / 高さ

    using ShortcutSection = std::pair<const char*, void (*)()>;  // 項目名と、その表を描く関数

    // About に並べる順
    constexpr ShortcutSection kShortcutSections[] = {
      { "General",                                            [] { DrawShortcutTable(DebugUIManager::Shortcuts()); } },
      { "All Editors",                                        [] { DrawShortcutTable(HistoryShortcuts()); } },
      { "3D Preview (Debug Viewport / Particle / Primitive)", [] { DrawShortcutTable(ViewportCameraController::Shortcuts()); } },
      { "UI Editor",                                          [] { DrawShortcutTable(UILayoutEditor::Shortcuts()); } },
      { "Camera Animation Editor",                            [] { DrawShortcutTable(CameraAnimationEditor::Shortcuts()); } },
      { "Animation Editor",                                   [] { DrawShortcutTable(AnimationEditor::Shortcuts()); } },
      { "Particle Editor",                                    [] { DrawShortcutTable(ParticleEditor::Shortcuts()); } },
      { "Primitive Editor",                                   [] { DrawShortcutTable(PrimitiveEditor::Shortcuts()); } },
      { "Material Editor",                                    [] { DrawShortcutTable(MaterialEditor::Shortcuts()); } },
      { "Scene Hierarchy / Inspector (Actors)",               [] { DrawShortcutTable(SceneEditor::Shortcuts()); } },
      { "Shader Editor",                                      [] { DrawShortcutTable(ShaderEditor::Shortcuts()); } },
      { "Render Pipeline",                                    [] { DrawShortcutTable(RenderPipelineWindow::Shortcuts()); } },
      { "PostEffect Editor",                                  [] { DrawShortcutTable(PostEffectManager::Shortcuts()); } },
      { "Engine Settings",                                    [] { DrawShortcutTable(EngineSettingsWindow::Shortcuts()); } },
      { "Behavior Tree Editor",                               [] { DrawShortcutTable(BehaviorTreeEditor::Shortcuts()); } },
      { "Assets",                                             [] { DrawShortcutTable(AssetBrowser::Shortcuts()); } },
    };

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

    enum class ToolbarIcon { Play, Pause, Step, Stop };

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
      case ToolbarIcon::Stop:
        drawList->AddRectFilled(ImVec2(center.x - half * 0.8f, center.y - half * 0.8f), ImVec2(center.x + half * 0.8f, center.y + half * 0.8f), color);
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
    RunUndoJournalSelfTest();
    UndoJournal::Current() = &undoJournal_;

    // 個人設定に起動時ウィンドウが保存されていない場合の既定（LoadEditorSettings で表示状態へ適用される）
    for (Window window : { Window::GameViewport, Window::SceneHierarchy, Window::Inspector, Window::Console, Window::Assets }) {
      startupWindows_[static_cast<size_t>(window)] = true;
    }

    const ViewportCameraSettings* cameraSettings = &engineSettings_.GetViewportCameraSettings();
    primitiveEditor_.Initialize(&WindowFlag(Window::PrimitiveEditor), cameraSettings);
    particleEditor_.Initialize(&WindowFlag(Window::ParticleEditor), &engineSettings_.GetParticleEditorGrid(), cameraSettings);
    materialEditor_.Initialize(&WindowFlag(Window::MaterialEditor));
    shaderEditor_.Initialize(&WindowFlag(Window::ShaderEditor));
    renderPipelineWindow_.Initialize(&WindowFlag(Window::RenderPipeline));
    debugViewport_.Initialize(&WindowFlag(Window::DebugViewport), &engineSettings_.GetDebugViewportGrid(), cameraSettings, &sceneEditor_);
    engineSettings_.Initialize(&WindowFlag(Window::EngineSettings), startupWindows_);
    assetBrowser_.Initialize(&WindowFlag(Window::Assets));
    uiLayoutEditor_.Initialize(&WindowFlag(Window::UIEditor));
    cameraAnimationEditor_.Initialize(&WindowFlag(Window::CameraAnimationEditor), &debugViewport_);
    animationEditor_.Initialize(&WindowFlag(Window::AnimationEditor), cameraSettings);
    behaviorTreeEditor_.Initialize({});
    RegisterAssetOpeners();

    // ×ボタン・Alt+F4 で未保存の変更があれば閉じずに確認する
    WinApp::GetInstance()->SetCloseHandler([this] { return ConfirmExit(); });

    // Logger::Log（VS の出力だけだった物）も Console に出す。末尾の改行は落とす
    Logger::SetSink([](const std::string& message) {
      const size_t end = message.find_last_not_of("\r\n");
      if (end != std::string::npos) {
        GetInstance()->PushLog(message.substr(0, end + 1), LogType::Info);
      }
    });

    // 初期ログ
    AddLog("DebugUIManager Initialized", LogType::Info);
    AddLog("TakoEngine Ready", LogType::Info);
  }

  void DebugUIManager::RegisterAssetOpeners() {
    // 開く先の窓を表示して前に出す（ドッキングで別のタブの裏にあることがある）
    auto opener = [this](Window window, const char* title, std::function<void(const std::string&)> open) {
      return [this, window, title, open = std::move(open)](const std::string& value) {
        WindowFlag(window) = true;
        focusRequest_      = title;
        open(value);
      };
    };
    SetAssetOpener("Material", opener(Window::MaterialEditor, "Material Editor", [this](const std::string& name) { materialEditor_.RequestOpen(name); }));
    SetAssetOpener("AnimationClip", opener(Window::AnimationEditor, "Animation Editor", [this](const std::string& name) { animationEditor_.RequestOpen(name); }));
    SetAssetOpener("CameraAnimation", opener(Window::CameraAnimationEditor, "Camera Animation Editor", [this](const std::string& name) { cameraAnimationEditor_.RequestOpen(name); }));
    SetAssetOpener("UILayout", opener(Window::UIEditor, "UI Editor", [this](const std::string& name) { uiLayoutEditor_.OpenLayout(name); }));
    SetAssetOpener("ParticlePreset", opener(Window::ParticleEditor, "Particle Editor", [this](const std::string& name) { particleEditor_.OpenPreset(name); }));
    SetAssetOpener("PostEffectProfile", opener(Window::PostEffect, "PostEffect Editor",[](const std::string& name) { PostEffectManager::GetInstance()->LoadProfile(name); }));
    SetAssetOpener("Prefab", opener(Window::SceneHierarchy, "Scene Hierarchy", [this](const std::string& name) { sceneEditor_.RequestSpawn(name); }));
    SetAssetOpener("Scene", [this](const std::string& name) { RequestSceneChange(name); });
    SetAssetOpener("BehaviorTree", [this](const std::string& name) {
      behaviorTreeEditor_.SetVisible(true);
      focusRequest_ = "Behavior Tree Editor";
      // 未保存の変更があれば、エディタが確認を出してから切り替える
      behaviorTreeEditor_.SwitchTree(name);
    });
  }

  void DebugUIManager::Finalize() {
    // この後に消える履歴（ゲームが持つエディタなど）が、破棄した順番に触らないようにする
    UndoJournal::Current() = nullptr;
    WinApp::GetInstance()->SetCloseHandler(nullptr);
    Logger::SetSink(nullptr);
    ClearLogs();
    primitiveEditor_.Finalize();
    particleEditor_.Finalize();
    debugViewport_.Finalize();
    animationEditor_.Finalize();
    behaviorTreeEditor_.Finalize();
    instance_.reset();
  }

  std::span<const ShortcutBinding<DebugUIManager>> DebugUIManager::Shortcuts() {
    static constexpr ShortcutBinding<DebugUIManager> kShortcuts[] = {
      { ImGuiKey_F1,  ImGuiInputFlags_None, "Debug Viewport",      [](DebugUIManager& manager) { manager.ToggleWindow(Window::DebugViewport); } },
      { ImGuiKey_F2,  ImGuiInputFlags_None, "Scene Hierarchy",     [](DebugUIManager& manager) { manager.ToggleWindow(Window::SceneHierarchy); } },
      { ImGuiKey_F3,  ImGuiInputFlags_None, "Inspector",           [](DebugUIManager& manager) { manager.ToggleWindow(Window::Inspector); } },
      { ImGuiKey_F4,  ImGuiInputFlags_None, "Game Viewport",       [](DebugUIManager& manager) { manager.ToggleWindow(Window::GameViewport); } },
      { ImGuiKey_F5,  ImGuiInputFlags_None, "Console",             [](DebugUIManager& manager) { manager.ToggleWindow(Window::Console); } },
      { ImGuiKey_F6,  ImGuiInputFlags_None, "Performance",         [](DebugUIManager& manager) { manager.ToggleWindow(Window::Performance); } },
      { ImGuiKey_F7,  ImGuiInputFlags_None, "Pause / Resume",      [](DebugUIManager& manager) { manager.SetGamePaused(!FrameTimer::GetInstance()->IsPaused()); } },
      { ImGuiKey_F8,  ImGuiInputFlags_None, "Step Frame",          [](DebugUIManager& manager) { manager.StepFrame(); } },
      { ImGuiKey_F9,  ImGuiInputFlags_None, "Play / Stop (Edit)",  [](DebugUIManager& manager) { manager.TogglePlay(); } },
      { ImGuiKey_F10, ImGuiInputFlags_None, "Show Main Windows",   [](DebugUIManager& manager) {
        for (Window window : kMainWindows) {
          manager.WindowFlag(window) = true;
        }
      } },
      // 処理は TakoFramework::Update（Release でも効くよう、この表ではなく Input で読む）
      { ImGuiKey_F11, ImGuiInputFlags_None, "Fullscreen" },
      { ImGuiKey_F12, ImGuiInputFlags_None, "Toggle Main Windows", [](DebugUIManager& manager) {
        for (Window window : kMainWindows) {
          manager.ToggleWindow(window);
        }
      } },
    };
    return kShortcuts;
  }

  void DebugUIManager::Update() {
    // 表示状態に関わらず毎フレーム呼ぶ（非表示検知でプレビューを解放するため）
    primitiveEditor_.Update();
    BaseScene* scene = SceneManager::GetInstance()->GetCurrentScene();
    particleEditor_.SetEmitterManager(scene ? &scene->GetEmitterManager() : nullptr);
    particleEditor_.SetForceFieldManager(scene ? &scene->GetForceFieldManager() : nullptr);
    particleEditor_.Update();
    debugViewport_.Update();
    uiLayoutEditor_.Update();
    cameraAnimationEditor_.Update();
    animationEditor_.Update();
    sceneEditor_.Update();
  }

  void DebugUIManager::Draw() {
#ifdef _DEBUG
    isPreviewInputCaptured_ = false;

    // どのウィンドウにフォーカスがあっても効かせる。文字の入力中は入力欄に任せる
    if (!ImGui::GetIO().WantTextInput) {
      ProcessShortcuts(Shortcuts(), *this, ShortcutScope::Global);
    }

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
    uiLayoutEditor_.Draw();
    cameraAnimationEditor_.Draw();
    isPreviewInputCaptured_ |= animationEditor_.Draw();
    behaviorTreeEditor_.Update();
    materialEditor_.Draw();
    shaderEditor_.Draw();
    renderPipelineWindow_.Draw();
    if (WindowFlag(Window::ImGuiDemo)) ImGui::ShowDemoWindow(&WindowFlag(Window::ImGuiDemo));
    if (WindowFlag(Window::ImGuiMetrics)) ImGui::ShowMetricsWindow(&WindowFlag(Window::ImGuiMetrics));
    if (WindowFlag(Window::About)) DrawAbout();

    // PostEffect は独自の描画を持つ
    if (WindowFlag(Window::PostEffect)) {
      PostEffectManager::GetInstance()->DrawImgui(&WindowFlag(Window::PostEffect));
    }

    DrawExitConfirmation();
    DrawNewScriptPopup();
    ApplyHistoryRequest();

    // 窓を初めて開いたフレームはまだ作られていないので、見つかるまで次のフレームでも試す
    if (focusRequest_ && ImGui::FindWindowByName(focusRequest_)) {
      ImGui::SetWindowFocus(focusRequest_);
      focusRequest_ = nullptr;
    }

    // ImGui 構築はゲーム更新より後なので、判定結果は次フレームの Input::Update で反映される
    Input* input = Input::GetInstance();
    input->SetBlocked(isPreviewInputCaptured_);
    // エディタのパネル操作がゲームに届かないよう、画面外のクリックと入力欄への文字入力・UI Editor の矢印キー移動を止める
    input->SetMouseButtonsBlocked(!IsCursorOverGameView());
    input->SetKeyboardBlocked(ImGui::GetIO().WantTextInput || uiLayoutEditor_.WantsKeyboard());
#endif
  }

  bool DebugUIManager::ConfirmExit() {
    unsavedChanges_.clear();
    uiLayoutEditor_.CollectUnsavedChanges(unsavedChanges_);
    cameraAnimationEditor_.CollectUnsavedChanges(unsavedChanges_);
    animationEditor_.CollectUnsavedChanges(unsavedChanges_);
    particleEditor_.CollectUnsavedChanges(unsavedChanges_);
    primitiveEditor_.CollectUnsavedChanges(unsavedChanges_);
    materialEditor_.CollectUnsavedChanges(unsavedChanges_);
    sceneEditor_.CollectUnsavedChanges(unsavedChanges_);
    shaderEditor_.CollectUnsavedChanges(unsavedChanges_);
    renderPipelineWindow_.CollectUnsavedChanges(unsavedChanges_);
    engineSettings_.CollectUnsavedChanges(unsavedChanges_);
    PostEffectManager::GetInstance()->CollectUnsavedChanges(unsavedChanges_);
    for (const BehaviorTreeEditor* editor : BehaviorTreeEditor::GetInstances()) {
      editor->CollectUnsavedChanges(unsavedChanges_);
    }

    if (unsavedChanges_.empty()) {
      return true;
    }
    isExitPopupRequested_ = true;
    return false;
  }

  void DebugUIManager::DrawExitConfirmation() {
    std::string message = "Unsaved changes will be lost:";
    for (const std::string& item : unsavedChanges_) {
      message += "\n  " + item;
    }
    if (DrawUnsavedChangesModal("Unsaved Changes##Exit", isExitPopupRequested_, message, "Exit") == UnsavedChoice::Proceed && pEndFlag_) {
      *pEndFlag_ = true;
    }
  }

  void DebugUIManager::ApplyHistoryRequest() {
    HistoryCommand command = std::exchange(historyRequest_, HistoryCommand::None);
    // 文字の入力中の Ctrl+Z は、入力欄の Undo に任せる
    if (command == HistoryCommand::None && !ImGui::GetIO().WantTextInput) {
      command = ReadHistoryShortcut();
    }
    // 操作の途中（EditGate::EndOperation と同じ判定）と、確認モーダルの間は戻さない
    if (command == HistoryCommand::None || ImGui::IsAnyItemActive() || ImGui::IsAnyMouseDown() || ImGui::GetTopMostPopupModal()) {
      return;
    }
    undoJournal_.Apply(command == HistoryCommand::Undo);
  }

  void DebugUIManager::DrawMainMenuBar() {
    MenuTarget& target = GetMenuTarget();
    // 前のフレームに相手が取り出さなかった操作は捨てる（相手が描かれなかったときに、後から走らないようにする）
    target.pending = ImGuiKey_None;

    // 直前にフォーカスのあったエディタへ、そのキーが押されたのと同じ操作を送る項目。相手の表に無ければ押せない
    const auto targetItem = [&target](const char* label, ImGuiKeyChord chord) {
      if (ImGui::MenuItem(label, ImGui::GetKeyChordName(chord), false, target.Has(chord))) {
        target.pending = chord;
      }
    };

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
        targetItem(std::format("Save{}###Save", target.name.empty() ? "" : std::format(" ({})", target.name)).c_str(), ImGuiMod_Ctrl | ImGuiKey_S);

        ImGui::Separator();
        ImGui::MenuItem("Engine Settings...", nullptr, &WindowFlag(Window::EngineSettings));

        ImGui::Separator();
        if (ImGui::MenuItem("Exit", "Alt+F4")) {
          if (ConfirmExit() && pEndFlag_) *pEndFlag_ = true;
        }
        ImGui::EndMenu();
      }

      MenuBarSeparator();

      if (ImGui::BeginMenu("Edit")) {
        // 全エディタで 1 本の履歴。括弧は、次に戻る手を積んだエディタ
        const auto historyItem = [this](const char* label, ImGuiKeyChord chord, bool isUndo) {
          const UndoJournal::Client* client  = undoJournal_.Peek(isUndo);
          const bool                 enabled = undoJournal_.CanApply(isUndo);
          const std::string          text    = std::format("{}{}###{}", label, client ? std::format(" ({})", client->GetName()) : "", label);
          if (ImGui::MenuItem(text.c_str(), ImGui::GetKeyChordName(chord), false, enabled)) {
            historyRequest_ = isUndo ? HistoryCommand::Undo : HistoryCommand::Redo;
          }
          if (client && !enabled && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
            ImGui::SetTooltip("%s cannot be restored now (stop playing first)", client->GetName());
          }
        };
        historyItem("Undo", ImGuiMod_Ctrl | ImGuiKey_Z, true);
        historyItem("Redo", ImGuiMod_Ctrl | ImGuiKey_Y, false);

        ImGui::Separator();
        ImGui::TextDisabled("%s", target.name.empty() ? "No editor focused" : target.name.c_str());
        targetItem("Cut", ImGuiMod_Ctrl | ImGuiKey_X);
        targetItem("Copy", ImGuiMod_Ctrl | ImGuiKey_C);
        targetItem("Paste", ImGuiMod_Ctrl | ImGuiKey_V);
        targetItem("Duplicate", ImGuiMod_Ctrl | ImGuiKey_D);
        targetItem("Delete", ImGuiKey_Delete);

        ImGui::Separator();
        targetItem("Select All", ImGuiMod_Ctrl | ImGuiKey_A);
        targetItem("Deselect All", ImGuiKey_Escape);
        ImGui::EndMenu();
      }

      MenuBarSeparator();

      if (ImGui::BeginMenu("View")) {
        ImGui::SeparatorText("Main Windows");
        ImGui::MenuItem("Debug Viewport", "F1", &WindowFlag(Window::DebugViewport));
        ImGui::MenuItem("Scene Hierarchy", "F2", &WindowFlag(Window::SceneHierarchy));
        ImGui::MenuItem("Inspector", "F3", &WindowFlag(Window::Inspector));
        ImGui::MenuItem("Game Viewport", "F4", &WindowFlag(Window::GameViewport));
        ImGui::MenuItem("Console", "F5", &WindowFlag(Window::Console));
        ImGui::MenuItem("Performance", "F6", &WindowFlag(Window::Performance));
        ImGui::MenuItem("Assets", nullptr, &WindowFlag(Window::Assets));
        ImGui::SeparatorText("Editors");
        ImGui::MenuItem("Particle Editor", nullptr, &WindowFlag(Window::ParticleEditor));
        ImGui::MenuItem("Primitive Editor", nullptr, &WindowFlag(Window::PrimitiveEditor));
        ImGui::MenuItem("Material Editor", nullptr, &WindowFlag(Window::MaterialEditor));
        ImGui::MenuItem("Shader Editor", nullptr, &WindowFlag(Window::ShaderEditor));
        ImGui::MenuItem("Render Pipeline", nullptr, &WindowFlag(Window::RenderPipeline));
        ImGui::MenuItem("PostEffect Editor", nullptr, &WindowFlag(Window::PostEffect));
        ImGui::MenuItem("UI Editor", nullptr, &WindowFlag(Window::UIEditor));
        ImGui::MenuItem("Camera Animation Editor", nullptr, &WindowFlag(Window::CameraAnimationEditor));
        ImGui::MenuItem("Animation Editor", nullptr, &WindowFlag(Window::AnimationEditor));
        if (ImGui::MenuItem("Behavior Tree Editor", nullptr, behaviorTreeEditor_.IsVisible())) {
          behaviorTreeEditor_.SetVisible(!behaviorTreeEditor_.IsVisible());
        }
        ImGui::SeparatorText("Debug Windows");
        ImGui::MenuItem("Input Debug", nullptr, &WindowFlag(Window::InputDebug));
        ImGui::MenuItem("Collision Debug", nullptr, &WindowFlag(Window::CollisionDebug));
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
    constexpr const char* kPausedText  = "PAUSED";
    constexpr const char* kRebuildText = "REBUILD REQUIRED";

    const float       fps               = FrameTimer::GetInstance()->GetFPS();
    const bool        isPaused          = FrameTimer::GetInstance()->IsPaused();
    const bool        isRebuildRequired = engineSettings_.IsRebuildRequired();
    const std::string fpsText           = std::format("FPS {:.1f}", fps);
    const std::string msText            = std::format("{:.1f} ms", fps > 0.0f ? 1000.0f / fps : 0.0f);

    // 右寄せのため描画するアイテムの合計幅を先に求める（各アイテムの後ろには ItemSpacing が入る）
    const float spacing = ImGui::GetStyle().ItemSpacing.x;
    float width = ImGui::CalcTextSize(fpsText.c_str()).x + spacing + MenuBarSeparatorWidth() + ImGui::CalcTextSize(msText.c_str()).x;
    if (isRebuildRequired) {
      width += ImGui::CalcTextSize(kRebuildText).x + spacing + MenuBarSeparatorWidth();
    }
    if (isPaused) {
      width += ImGui::CalcTextSize(kPausedText).x + spacing + MenuBarSeparatorWidth();
    }
    ImGui::SetCursorPosX(ImGui::GetWindowWidth() - width - kMenuBarRightInset);

    if (isRebuildRequired) {
      ImGui::TextColored(kWarningColor, "%s", kRebuildText);
      ImGui::SetItemTooltip("Save renamed names in the source files (listed in the Console).\nThe running game keeps the old names until you rebuild and restart");
      MenuBarSeparator();
    }
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
    const float groupWidth = ImGui::GetFrameHeight() * kIconButtonAspect * 3.0f + ImGui::GetStyle().ItemSpacing.x * 2.0f;
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (ImGui::GetContentRegionAvail().x - groupWidth) * 0.5f);

    // Edit 中は Play、Play 中は Stop。Stop は遷移中には押せない
    SceneManager* sceneManager = SceneManager::GetInstance();
    const bool    isPlaying    = sceneManager->IsPlaying();
    ImGui::BeginDisabled(isPlaying && !sceneManager->CanStop());
    const bool isPlayToggled = isPlaying
      ? IconButton("##Stop", ToolbarIcon::Stop, "Stop: back to Edit and restore the scene (F9)", false)
      : IconButton("##Play", ToolbarIcon::Play, "Play from Edit (F9)", true);
    ImGui::EndDisabled();
    if (isPlayToggled) {
      TogglePlay();
    }
    ImGui::SameLine();

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

  void DebugUIManager::TogglePlay() {
    SceneManager* sceneManager = SceneManager::GetInstance();
    if (sceneManager->IsPlaying()) {
      if (sceneManager->CanStop()) {
        sceneManager->RequestStop();
        AddLog("Stop: back to Edit", LogType::Info);
      }
    } else {
      sceneManager->RequestPlay();
      AddLog("Play", LogType::Info);
    }
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

    ImGui::TextDisabled("Mode: %s", sceneManager->IsPlaying() ? "Play" : "Edit (game update stopped)");

    sceneEditor_.DrawHierarchy();

    ImGui::End();
  }

  void DebugUIManager::RequestSceneChange(const std::string& sceneName) {
    AddLog("Scene change requested: " + sceneName, LogType::Info);
    SceneManager::GetInstance()->ChangeScene(sceneName);
  }

  void DebugUIManager::DrawInspector() {
    ImGui::Begin("Inspector", &WindowFlag(Window::Inspector));

    if (sceneEditor_.HasSelection()) {
      sceneEditor_.DrawInspector();
    }
    else {
      ImGui::Text("No object selected");
      ImGui::TextDisabled("Select an object from Scene Hierarchy");
    }

    ImGui::End();
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
      float aspectRatio = static_cast<float>(WinApp::clientWidth) / static_cast<float>((std::max)(WinApp::clientHeight, 1));

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
      uint32_t srvIndex = RenderPipeline::GetInstance()->GetFinalSrvIndex();
      D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle = SrvManager::GetInstance()->GetGPUDescriptorHandle(srvIndex);
      ImGui::Image((ImTextureID)gpuHandle.ptr, imageSize);
      const ImVec2 imageMin = ImGui::GetItemRectMin();
      const ImVec2 imageMax = ImGui::GetItemRectMax();

      // ゲーム画像の上にカーソルがあるか（レターボックス余白・タイトルバーは除外）
      const bool isImageHovered = ImGui::IsItemHovered();

      // UI Editor のギズモがマウスを使う間は、ゲームへクリックを渡さない
      const bool isGizmoCapturing = uiLayoutEditor_.DrawViewportOverlay(imageMin, imageMax);
      isGameViewportHovered_ = isImageHovered && !isGizmoCapturing;
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
    OutputDebugStringA((message + "\n").c_str());
    PushLog(message, type);
  }

  void DebugUIManager::PushLog(const std::string& message, LogType type) {
    LogEntry entry;
    entry.type = type;
    entry.message = message;
    entry.timestamp = GetCurrentTimestamp();

    consoleLogs_.push_back(entry);

    // 最大数を超えたら古いログを削除
    if (consoleLogs_.size() > static_cast<size_t>(maxConsoleLogs_)) {
      consoleLogs_.erase(consoleLogs_.begin());
    }
  }

  void DebugUIManager::ClearLogs() {
    consoleLogs_.clear();
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
          ImGui::Text("  %02u %s: %d collider%s",
            typeID, collisionManager->GetLayerLabel(typeID).c_str(), count, count > 1 ? "s" : "");
        }
      }
    }

    // コリジョンマスク情報
    if (ImGui::CollapsingHeader("Collision Masks")) {
      // マスクは対称なので typeA <= typeB の組だけ集める
      std::vector<std::pair<uint32_t, uint32_t>> collisionPairs;
      for (uint32_t typeA = 0; typeA < CollisionManager::kMaxLayers; ++typeA) {
        for (uint32_t typeB = typeA; typeB < CollisionManager::kMaxLayers; ++typeB) {
          if (collisionManager->CanCollide(typeA, typeB)) {
            collisionPairs.push_back({ typeA, typeB });
          }
        }
      }

      if (collisionPairs.empty()) {
        ImGui::TextDisabled("No collision masks configured");
      }
      else {
        ImGui::Text("Active collision pairs:");
        ImGui::Spacing();

        // コリジョンペアを表示
        for (const auto& [typeA, typeB] : collisionPairs) {
          const std::string labelA = collisionManager->GetLayerLabel(typeA);
          if (typeA == typeB) {
            ImGui::BulletText("%s (self-collision)", labelA.c_str());
          }
          else {
            ImGui::BulletText("%s <-> %s", labelA.c_str(), collisionManager->GetLayerLabel(typeB).c_str());
          }
        }

        ImGui::Spacing();
        ImGui::Text("Total mask pairs: %zu", collisionPairs.size());
      }
    }

    // パフォーマンス情報
    if (ImGui::CollapsingHeader("Performance")) {
      // 層ごとに分けて当たる層の組だけ判定するので、総当たり数ではなく実際に判定したペア数を出す
      const size_t testedPairs = collisionManager->GetTestedPairCount();
      ImGui::Text("Tested pairs: %zu", testedPairs);
      ImGui::Text("Check time: %.3f ms", collisionManager->GetLastCheckMs());

      const auto& masks = collisionManager->GetCollisionMasks();
      if (std::ranges::all_of(masks, [](uint32_t mask) { return mask == 0; })) {
        ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f), "No masks configured");
        ImGui::TextDisabled("No pairs are tested");
      }

      ImGui::Spacing();

      // パフォーマンスレベルのインジケーター
      ImVec4 performanceColor;
      const char* performanceText;

      if (testedPairs < 100) {
        performanceColor = ImVec4(0.2f, 1.0f, 0.2f, 1.0f);
        performanceText = "Excellent";
      }
      else if (testedPairs < 500) {
        performanceColor = ImVec4(0.7f, 1.0f, 0.2f, 1.0f);
        performanceText = "Good";
      }
      else if (testedPairs < 2000) {
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
      ImGui::TextDisabled("Editor shortcuts work in the focused window. Undo / Redo share one history across all editors.");
      for (const auto& [section, drawTable] : kShortcutSections) {
        const ImGuiTreeNodeFlags flags = section == kShortcutSections[0].first ? ImGuiTreeNodeFlags_DefaultOpen : ImGuiTreeNodeFlags_None;
        if (ImGui::CollapsingHeader(section, flags)) {
          ImGui::PushID(section);
          drawTable();
          ImGui::PopID();
        }
      }
    }
    ImGui::End();
  }

} // namespace Tako
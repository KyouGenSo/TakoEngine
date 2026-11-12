#include "ImGuiManager.h"
#include"imgui_impl_win32.h"
#include"imgui_impl_dx12.h"
#include"WinApp.h"
#include"DX12Basic.h"
#include"SrvManager.h"
#include "Logger.h"
#include "EnginePaths.h"
#include "ImGuiThemes.h"
#include "IconsFontAwesome6.h"
#include <cassert>
#include <filesystem>
#include <ShlObj.h>

namespace Tako {

  namespace {
    // ImGui のフォントの大きさは行の高さ（ascender - descender）。字面はフォントごとに 行の高さ / 比 になる
    constexpr float kFontSize          = 16.0f;              // 英字 Inter（比 1.21）
    constexpr float kJapaneseFontSize  = kFontSize * 1.14f;  // Noto Sans JP（比 1.45）。字面が英字の 0.95 倍になる大きさ
    constexpr float kIconFontSize      = kFontSize * 0.85f;  // Font Awesome（比 1.04）。字面が英字と同じになる大きさ
    constexpr float kIconAdvance       = kFontSize * 1.1f;   // アイコンの送り幅。揃えると、アイコンの後ろの文字の左端が揃う
    constexpr float kLargeIconFontSize = 48.0f;

    bool Exists(const std::string& path)
    {
      std::error_code ec;
      return std::filesystem::exists(path, ec);
    }

    /// <summary>
    /// 既定フォントに Windows の日本語フォントの字形を合成する。見つからなければ英字だけのまま
    /// </summary>
    void AddFallbackFonts(ImFontAtlas* fonts)
    {
      fonts->AddFontDefault();

      std::filesystem::path fontDir;
      PWSTR                 knownFolder = nullptr;
      if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_Fonts, 0, nullptr, &knownFolder))) {
        fontDir = knownFolder;
      }
      // 失敗時も解放が必要
      CoTaskMemFree(knownFolder);

      for (const wchar_t* name : { L"meiryo.ttc", L"YuGothM.ttc", L"msgothic.ttc" }) {
        const std::filesystem::path path = fontDir / name;
        std::error_code             ec;
        if (!std::filesystem::exists(path, ec)) {
          continue;
        }
        ImFontConfig config;
        config.MergeMode   = true;
        config.OversampleH = 1;  // 字形が約 3000 あるため、横の拡大を止めてアトラスを半分にする
        fonts->AddFontFromFileTTF(reinterpret_cast<const char*>(path.u8string().c_str()), 13.0f, &config, fonts->GetGlyphRangesJapanese());
        return;
      }
      Logger::Log("ImGui: no Japanese font found in the Fonts folder. Japanese text is shown as '?'\n");
    }

    /// <summary>
    /// エディタのフォントを読む。英字に日本語とアイコンを合成した 1 本と、大きいアイコンだけの 1 本
    /// </summary>
    /// <returns>大きいアイコンのフォント。フォントのファイルが揃っていなければ nullptr（文字は代わりのフォントで出て、アイコンは '?' になる）</returns>
    ImFont* AddEditorFonts(ImFontAtlas* fonts)
    {
      const std::string dir(EnginePaths::kEngineFonts);
      const std::string latin    = dir + "Inter-Regular.ttf";
      const std::string japanese = dir + "NotoSansJP-Regular.otf";
      const std::string icons    = dir + FONT_ICON_FILE_NAME_FAS;
      // AddFontFromFileTTF は無いファイルで assert するので、先に確かめる
      if (!Exists(latin) || !Exists(japanese) || !Exists(icons)) {
        Logger::Log("ImGui: font files are missing in " + dir + ". Using the default font (icons are shown as '?')\n");
        AddFallbackFonts(fonts);
        return nullptr;
      }

      fonts->AddFontFromFileTTF(latin.c_str(), kFontSize);

      ImFontConfig japaneseConfig;
      japaneseConfig.MergeMode   = true;
      japaneseConfig.OversampleH = 1;  // 字形が約 3000 あるため、横の拡大を止めてアトラスを半分にする
      fonts->AddFontFromFileTTF(japanese.c_str(), kJapaneseFontSize, &japaneseConfig, fonts->GetGlyphRangesJapanese());

      // 範囲の配列はアトラスを作るまで参照される
      static constexpr ImWchar kIconRange[] = { ICON_MIN_FA, ICON_MAX_16_FA, 0 };
      ImFontConfig iconConfig;
      iconConfig.MergeMode        = true;
      iconConfig.PixelSnapH       = true;
      iconConfig.GlyphMinAdvanceX = kIconAdvance;
      iconConfig.GlyphMaxAdvanceX = kIconAdvance;
      fonts->AddFontFromFileTTF(icons.c_str(), kIconFontSize, &iconConfig, kIconRange);

      // 大きい方は使う字だけを焼く（全部を焼くとアトラスが約 300 万画素増える）
      static ImVector<ImWchar> largeIconRanges;
      ImFontGlyphRangesBuilder builder;
      builder.AddText(ImGuiManager::kLargeIcons);
      builder.BuildRanges(&largeIconRanges);
      return fonts->AddFontFromFileTTF(icons.c_str(), kLargeIconFontSize, nullptr, largeIconRanges.Data);
    }
  }

  void ImGuiManager::Initialize(WinApp* winApp, DX12Basic* dx12, bool isDocking)
  {

    winApp_ = winApp;

    dx12_ = dx12;

    isDocking_ = isDocking;

    ImGui::CreateContext();

    // スケールの調整（DPI スケーリングに対応）
    ImGuiIO& io = ImGui::GetIO();
    io.FontGlobalScale = 1.0f;
    largeIconFont_ = AddEditorFonts(io.Fonts);

    // 個人設定を読むまでの見た目（EngineSettingsWindow::LoadEditorSettings が保存済みのテーマに替える）
    ApplyImGuiTheme(0);

    ImGui_ImplWin32_Init(winApp_->GetHWnd());

    // フォント用の SRV インデックスを確保
    fontSrvIndex_ = SrvManager::GetInstance()->Allocate();

    // DX12用の初期化
    InitializeForDX12();

    // Docking の設定
    SetDocking(isDocking_);

  }

  void ImGuiManager::InitializeForDX12()
  {
    // SrvManager のディスクリプタヒープを取得
    ID3D12DescriptorHeap* srvHeap = SrvManager::GetInstance()->GetDescriptorHeap();

    // フォント用の CPU/GPU ハンドルを取得
    D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle = SrvManager::GetInstance()->GetCPUDescriptorHandle(fontSrvIndex_);
    D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle = SrvManager::GetInstance()->GetGPUDescriptorHandle(fontSrvIndex_);

    ImGui_ImplDX12_Init(
      dx12_->GetDevice(),
      static_cast<int>(dx12_->GetSwapChainBufferCount()),
      DXGI_FORMAT_R8G8B8A8_UNORM,
      srvHeap,
      cpuHandle,
      gpuHandle);
  }

  void ImGuiManager::Begin()
  {
    ImGui_ImplDX12_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();

    if (isDocking_) {
      SetDockSpaceViewPort();
    }
  }

  void ImGuiManager::Draw()
  {
    ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), dx12_->GetCommandList());
  }

  void ImGuiManager::End()
  {
    ImGui::Render();
  }

  void ImGuiManager::Shutdown()
  {
    ImGui_ImplDX12_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();

    // フォント用の SRV インデックスを解放
    if (fontSrvIndex_ != 0) {
      SrvManager::GetInstance()->Free(fontSrvIndex_);
      fontSrvIndex_ = 0;
    }
  }

  void ImGuiManager::OnWindowResize()
  {
    // イベント処理を強制的に更新
    ImGuiIO& io = ImGui::GetIO();
    io.DisplaySize = ImVec2(
      static_cast<float>(WinApp::clientWidth),
      static_cast<float>(WinApp::clientHeight)
    );

    // Docking の再設定
    SetDocking(isDocking_);
  }

  void ImGuiManager::SetDocking(bool isDocking)
  {
    if (isDocking) {
      ImGuiIO& io = ImGui::GetIO();
      io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    }
    else {
      ImGuiIO& io = ImGui::GetIO();
      io.ConfigFlags &= ~ImGuiConfigFlags_DockingEnable;
    }
  }

  void ImGuiManager::SetDockSpaceViewPort()
  {
    ImGuiID dockspace_id = ImGui::GetID("MyDockSpace");
    ImGui::DockSpaceOverViewport(dockspace_id, ImGui::GetMainViewport(), ImGuiDockNodeFlags_PassthruCentralNode);
  }

} // namespace Tako

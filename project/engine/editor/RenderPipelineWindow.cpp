#include "RenderPipelineWindow.h"

#ifdef TAKO_EDITOR
#include "EditorUI.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <format>
#include <iterator>

#include "DebugUIManager.h"
#include "DX12Basic.h"
#include "ImGuiManager.h"
#include "ImGuiThemes.h"
#include "MaterialLibrary.h"
#include "Object3d.h"
#include "Renderer.h"

namespace Tako {

  namespace {

    // 各位置の描画先と深度（Event のツールチップ）
    constexpr const char* kRenderEventTargets[] = {
      "Scene color / scene depth (just cleared). The shadow map is the previous frame's",
      "Scene color / scene depth with opaque objects",
      "Scene color / scene depth, after decals",
      "Scene color / scene depth, after transparent objects, particles and lines",
      "Scene color / scene depth, before post effects",
      "Final result (window size) / UI depth (the scene depth unless the render scale is below 1)",
      "Final result / UI depth, before UI",
      "Final result / UI depth, after UI",
    };
    static_assert(std::size(kRenderEventTargets) == static_cast<size_t>(RenderEvent::Count));

  } // anonymous namespace

  std::span<const ShortcutBinding<RenderPipelineWindow>> RenderPipelineWindow::Shortcuts()
  {
    static constexpr ShortcutBinding<RenderPipelineWindow> kShortcuts[] = {
      { ImGuiMod_Ctrl | ImGuiKey_S, ImGuiInputFlags_None, "Save the render passes", [](RenderPipelineWindow& window) { window.Save(); } },
    };
    return kShortcuts;
  }

  void RenderPipelineWindow::Draw()
  {
    if (!*isOpen_) {
      return;
    }

    ImGui::SetNextWindowSize(ImVec2(640.0f, 560.0f), ImGuiCond_FirstUseEver);
    if (ImGui::Begin(WindowTitle::kRenderPipeline, isOpen_)) {
      // UI を描く区間の前後で保存内容を比べて編集を検出する
      tracker_.BeginSection();

      if (ImGui::BeginTabBar("##RenderPipeline")) {
        if (ImGui::BeginTabItem("Passes")) {
          DrawPassesTab();
          ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Frame")) {
          DrawFrameTab();
          ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
      }

      tracker_.EndSection();
      ProcessShortcuts(Shortcuts(), *this);
    }
    ImGui::End();
  }

  void RenderPipelineWindow::CollectUnsavedChanges(std::vector<std::string>& out) const
  {
    if (tracker_.IsEdited()) {
      out.push_back("Render Pipeline (RenderPasses.json)");
    }
  }

  bool RenderPipelineWindow::Save()
  {
    const bool saved = RenderPipeline::GetInstance()->SaveObjectPasses();
    LogToConsole(saved ? "Render passes saved" : "Render passes save failed", saved ? DebugUIManager::LogType::Info : DebugUIManager::LogType::Error);
    if (saved) {
      tracker_.MarkSaved();
    }
    return saved;
  }

  void RenderPipelineWindow::DrawPassesTab()
  {
    if (UI::IconButton(ICON_FA_FLOPPY_DISK, "Save (Ctrl+S)")) {
      Save();
    }
    ImGui::SameLine();
    ImGui::AlignTextToFramePadding();
    ImGui::Text(tracker_.IsEdited() ? "RenderPasses.json *" : "RenderPasses.json");
    ImGui::TextDisabled("Each pass draws the named objects again with the material at the event (Replace: only there)");

    // 選べる名前は、生存中の Object3d の名前（MeshComponent は Actor の名前を付ける）
    std::vector<std::string> objectNames;
    for (const Object3d* object : Object3d::GetLiveObjects()) {
      if (!object->GetName().empty()) {
        objectNames.push_back(object->GetName());
      }
    }
    std::ranges::sort(objectNames);
    objectNames.erase(std::ranges::unique(objectNames).begin(), objectNames.end());
    // ポストエフェクトのマテリアルは描画物に使えないので出さない
    std::vector<std::string> materials = MaterialLibrary::GetInstance()->GetMaterialFileNames();
    std::erase_if(materials, [](const std::string& name) { return MaterialLibrary::GetInstance()->IsPostEffectMaterial(name); });

    RenderPipeline*                  pipeline = RenderPipeline::GetInstance();
    std::vector<ObjectPassSettings>& passes   = pipeline->GetObjectPasses();
    bool                             changed  = false;
    for (size_t i = 0; i < passes.size(); ++i) {
      ImGui::PushID(static_cast<int>(i));
      ImGui::SeparatorText(passes[i].name.empty() ? "(unnamed)" : passes[i].name.c_str());
      changed |= DrawPass(passes[i], objectNames, materials);

      // 同じ位置のパスは上から順に描く
      ImGui::BeginDisabled(i == 0);
      if (ImGui::SmallButton(ICON_FA_ARROW_UP "##Up")) {
        std::swap(passes[i], passes[i - 1]);
        changed = true;
      }
      ImGui::EndDisabled();
      ImGui::SameLine();
      ImGui::BeginDisabled(i + 1 == passes.size());
      if (ImGui::SmallButton(ICON_FA_ARROW_DOWN "##Down")) {
        std::swap(passes[i], passes[i + 1]);
        changed = true;
      }
      ImGui::EndDisabled();
      ImGui::SameLine();
      const bool remove = ImGui::SmallButton(ICON_FA_TRASH_CAN "##Delete");
      ImGui::SetItemTooltip("Delete this pass");
      ImGui::PopID();
      if (remove) {
        passes.erase(passes.begin() + static_cast<std::ptrdiff_t>(i));
        changed = true;
        break;
      }
    }

    ImGui::Separator();
    if (ImGui::Button(ICON_FA_PLUS "  Add Pass")) {
      passes.push_back({ .name = std::format("Pass{}", passes.size() + 1) });
      changed = true;
    }
    if (changed) {
      pipeline->RefreshObjectPasses();
    }
  }

  bool RenderPipelineWindow::DrawPass(ObjectPassSettings& pass, const std::vector<std::string>& objectNames, const std::vector<std::string>& materials)
  {
    bool changed = false;

    changed |= ImGui::Checkbox("##Enabled", &pass.enabled);
    ImGui::SetItemTooltip("Enabled");
    ImGui::SameLine();
    std::array<char, 64> name{};
    pass.name.copy(name.data(), name.size() - 1);
    if (ImGui::InputText("Name", name.data(), name.size())) {
      pass.name = name.data();
      changed = true;
    }

    int event = static_cast<int>(pass.event);
    if (UI::Combo("Event", &event, kRenderEventNames, static_cast<int>(std::size(kRenderEventNames)))) {
      pass.event = static_cast<RenderEvent>(event);
      changed = true;
    }
    ImGui::SetItemTooltip("%s", kRenderEventTargets[event]);

    int queue = static_cast<int>(pass.queue);
    if (UI::Combo("Queue", &queue, kQueueFilterNames, static_cast<int>(std::size(kQueueFilterNames)))) {
      pass.queue = static_cast<QueueFilter>(queue);
      changed = true;
    }
    ImGui::SetItemTooltip("Opaque = render queue below 3000, Transparent = 3000 and above (the object's own material)");

    changed |= UI::Checkbox("Replace", &pass.replace);
    ImGui::SetItemTooltip("Draw the objects only in this pass (removed from the normal opaque / transparent draw).\n"
                          "Shadows and the object mask stay as they are. Needs Objects (ignored when Objects is (all))");

    if (UI::BeginCombo("Material", pass.material.empty() ? "(object's own)" : pass.material.c_str())) {
      if (ImGui::Selectable("(object's own)", pass.material.empty())) {
        pass.material.clear();
        changed = true;
      }
      for (const std::string& material : materials) {
        if (ImGui::Selectable(material.c_str(), material == pass.material)) {
          pass.material = material;
          changed = true;
        }
      }
      ImGui::EndCombo();
    }
    ImGui::SetItemTooltip("The material's render state (blend, depth test / func, cull) is used as is");

    // 名前の札（クリックで外す）と、足す名前の一覧
    ImGui::TextUnformatted("Objects");
    ImGui::SameLine();
    if (pass.objects.empty()) {
      ImGui::TextDisabled("(all)");
      ImGui::SameLine();
    }
    for (size_t i = 0; i < pass.objects.size(); ++i) {
      if (ImGui::SmallButton(std::format("{} x##Object{}", pass.objects[i], i).c_str())) {
        pass.objects.erase(pass.objects.begin() + static_cast<std::ptrdiff_t>(i));
        changed = true;
        break;
      }
      ImGui::SameLine();
    }
    ImGui::SetNextItemWidth(160.0f);
    if (ImGui::BeginCombo("##AddObject", "+ add")) {
      for (const std::string& objectName : objectNames) {
        if (std::ranges::find(pass.objects, objectName) == pass.objects.end() && ImGui::Selectable(objectName.c_str())) {
          pass.objects.push_back(objectName);
          changed = true;
        }
      }
      ImGui::EndCombo();
    }
    return changed;
  }

  void RenderPipelineWindow::DrawFrameTab() const
  {
    FrameProfiler&    profiler = Renderer::GetInstance()->GetDX12Basic()->GetProfiler();
    const FrameTimes& times    = profiler.GetFrameTimes();

    bool isMeasuring = profiler.IsEnabled();
    if (ImGui::Checkbox("Measure", &isMeasuring)) {
      profiler.SetEnabled(isMeasuring);
    }
    ImGui::SetItemTooltip("Measure the time of each step. Not saved: it is on whenever the editor starts");
    // 止めている間は、出す値が無い
    if (!profiler.IsEnabled()) {
      return;
    }

    ImGui::PushTextWrapPos(0.0f);
    ImGui::TextDisabled("Averages per frame over the last 0.5 s. Gray rows are the events where passes can be added");
#ifdef _DEBUG
    ImGui::TextColored(GetEditorColors().warning, "Debug build: the debug layer and unoptimized shaders make these times longer. Measure in Develop");
#endif
    ImGui::PopTextWrapPos();
    if (ImGui::BeginTable("Steps##RenderPipeline", 4, ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_ScrollY)) {
      ImGui::TableSetupScrollFreeze(0, 1);
      ImGui::TableSetupColumn("Step", ImGuiTableColumnFlags_WidthStretch);
      ImGui::TableSetupColumn("Draws", ImGuiTableColumnFlags_WidthFixed, 60.0f);
      ImGui::TableSetupColumn("CPU ms", ImGuiTableColumnFlags_WidthFixed, 70.0f);
      ImGui::TableSetupColumn("GPU ms", ImGuiTableColumnFlags_WidthFixed, 70.0f);
      ImGui::TableHeadersRow();
      for (const ProfileSample& sample : profiler.GetSamples()) {
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        // Indent(0) は既定の幅で字下げするので、深さ 0 では呼ばない
        const float indent = ImGui::GetStyle().IndentSpacing * static_cast<float>(sample.depth);
        if (indent > 0.0f) {
          ImGui::Indent(indent);
        }
        if (sample.isEvent) {
          ImGui::TextDisabled("%s", sample.label.c_str());
        }
        else {
          ImGui::TextUnformatted(sample.label.c_str());
        }
        if (indent > 0.0f) {
          ImGui::Unindent(indent);
        }
        if (sample.isEvent) {
          continue;
        }
        ImGui::TableNextColumn();
        // 1 フレームおきにしか通らない区間は、平均が半端になる。そのときだけ小数を出す
        if (std::abs(sample.count - std::round(sample.count)) < 0.05f) {
          ImGui::Text("%.0f", sample.count);
        }
        else {
          ImGui::Text("%.1f", sample.count);
        }
        ImGui::TableNextColumn();
        ImGui::Text("%.3f", sample.cpuMs);
        ImGui::TableNextColumn();
        if (sample.hasGpuTime) {
          ImGui::Text("%.3f", sample.gpuMs);
        }
      }

      // どの区間にも入っていない GPU 時間（区間の間のクリアやバリアなど）
      ImGui::TableNextRow();
      ImGui::TableNextColumn();
      ImGui::TextDisabled("Other");
      ImGui::TableNextColumn();
      ImGui::TableNextColumn();
      ImGui::TableNextColumn();
      ImGui::Text("%.3f", times.otherGpuMs);

      ImGui::TableNextRow();
      ImGui::TableNextColumn();
      ImGui::TextUnformatted("Frame");
      ImGui::TableNextColumn();
      ImGui::TableNextColumn();
      ImGui::Text("%.3f", times.updateMs + times.recordMs);
      ImGui::TableNextColumn();
      ImGui::Text("%.3f", times.gpuMs);
      ImGui::EndTable();
    }
  }

} // namespace Tako

#endif // TAKO_EDITOR

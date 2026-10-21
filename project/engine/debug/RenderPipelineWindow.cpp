#include "RenderPipelineWindow.h"

#ifdef _DEBUG

#include <algorithm>
#include <array>
#include <format>
#include <iterator>

#include "DebugUIManager.h"
#include "ImGuiManager.h"
#include "MaterialLibrary.h"
#include "Object3d.h"
#include "ObjectSettings.h"

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
    if (ImGui::Begin("Render Pipeline", isOpen_)) {
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
    if (ImGui::Button("Save")) {
      Save();
    }
    ImGui::SameLine();
    tracker_.DrawButtons();
    ImGui::SameLine();
    ImGui::TextDisabled(tracker_.IsEdited() ? "RenderPasses.json *" : "RenderPasses.json");
    ImGui::TextDisabled("Each pass redraws the named objects with the material at the event (after the passes added in code)");

    // 選べる名前は、設定のある名前と生存中の Object3d の名前
    std::vector<std::string> objectNames = ObjectSettings::GetInstance()->GetNames();
    for (const Object3d* object : Object3d::GetLiveObjects()) {
      if (!object->GetName().empty()) {
        objectNames.push_back(object->GetName());
      }
    }
    std::ranges::sort(objectNames);
    objectNames.erase(std::ranges::unique(objectNames).begin(), objectNames.end());
    const std::vector<std::string> materials = MaterialLibrary::GetInstance()->GetMaterialFileNames();

    RenderPipeline*                  pipeline = RenderPipeline::GetInstance();
    std::vector<ObjectPassSettings>& passes   = pipeline->GetObjectPasses();
    bool                             changed  = false;
    for (size_t i = 0; i < passes.size(); ++i) {
      ImGui::PushID(static_cast<int>(i));
      ImGui::SeparatorText(passes[i].name.empty() ? "(unnamed)" : passes[i].name.c_str());
      changed |= DrawPass(passes[i], objectNames, materials);

      // 同じ位置のパスは上から順に描く
      ImGui::BeginDisabled(i == 0);
      if (ImGui::SmallButton("Up")) {
        std::swap(passes[i], passes[i - 1]);
        changed = true;
      }
      ImGui::EndDisabled();
      ImGui::SameLine();
      ImGui::BeginDisabled(i + 1 == passes.size());
      if (ImGui::SmallButton("Down")) {
        std::swap(passes[i], passes[i + 1]);
        changed = true;
      }
      ImGui::EndDisabled();
      ImGui::SameLine();
      const bool remove = ImGui::SmallButton("Delete");
      ImGui::PopID();
      if (remove) {
        passes.erase(passes.begin() + static_cast<std::ptrdiff_t>(i));
        changed = true;
        break;
      }
    }

    ImGui::Separator();
    if (ImGui::Button("Add Pass")) {
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
    if (ImGui::Combo("Event", &event, kRenderEventNames, static_cast<int>(std::size(kRenderEventNames)))) {
      pass.event = static_cast<RenderEvent>(event);
      changed = true;
    }
    ImGui::SetItemTooltip("%s", kRenderEventTargets[event]);

    int queue = static_cast<int>(pass.queue);
    if (ImGui::Combo("Queue", &queue, kQueueFilterNames, static_cast<int>(std::size(kQueueFilterNames)))) {
      pass.queue = static_cast<QueueFilter>(queue);
      changed = true;
    }
    ImGui::SetItemTooltip("Opaque = render queue below 3000, Transparent = 3000 and above (the object's own material)");

    if (ImGui::BeginCombo("Material", pass.material.empty() ? "(object's own)" : pass.material.c_str())) {
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
    ImGui::TextDisabled("Steps of the previous frame. Gray rows are the events where passes can be added");
    if (ImGui::BeginTable("Steps##RenderPipeline", 2, ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_ScrollY)) {
      ImGui::TableSetupScrollFreeze(0, 1);
      ImGui::TableSetupColumn("Step", ImGuiTableColumnFlags_WidthStretch);
      ImGui::TableSetupColumn("Draws", ImGuiTableColumnFlags_WidthFixed, 60.0f);
      ImGui::TableHeadersRow();
      for (const FrameStep& step : RenderPipeline::GetInstance()->GetFrameSteps()) {
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        if (step.depth > 0) {
          ImGui::Indent();
        }
        if (step.isEvent) {
          ImGui::TextDisabled("%s", step.label.c_str());
        }
        else {
          ImGui::TextUnformatted(step.label.c_str());
        }
        if (step.depth > 0) {
          ImGui::Unindent();
        }
        ImGui::TableNextColumn();
        if (!step.isEvent) {
          ImGui::Text("%u", step.drawCalls);
        }
      }
      ImGui::EndTable();
    }
  }

} // namespace Tako

#endif // _DEBUG

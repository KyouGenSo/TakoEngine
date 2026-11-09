#include "ScriptComponent.h"
#include "JsonPropertyVisitor.h"
#include "Logger.h"
#include "Script.h"
#include "TypeRegistry.h"

#ifdef _DEBUG
#include "AssetBrowser.h"
#include "ImGuiPropertyVisitor.h"
#include "imgui.h"
#endif

namespace Tako {

  ScriptComponent::ScriptComponent() = default;

  ScriptComponent::~ScriptComponent() = default;

  void ScriptComponent::BeginPlay()
  {
    hasBegunPlay_ = true;
    if (instance_) { instance_->BeginPlay(); }
  }

  void ScriptComponent::Tick(float deltaTime)
  {
    if (instance_) { instance_->Tick(deltaTime); }
  }

  void ScriptComponent::EndPlay()
  {
    if (instance_) { instance_->EndPlay(); }
    hasBegunPlay_ = false;
  }

  void ScriptComponent::Draw()
  {
    if (builtScript_ != script_) {
      RebuildInstance(instance_ ? WriteProperties(*instance_) : missingValues_);
    }
    if (instance_) { instance_->Draw(); }
  }

  void ScriptComponent::OnCollisionEnter(Actor* other)
  {
    if (instance_) { instance_->OnCollisionEnter(other); }
  }

  void ScriptComponent::OnCollisionStay(Actor* other)
  {
    if (instance_) { instance_->OnCollisionStay(other); }
  }

  void ScriptComponent::OnCollisionExit(Actor* other)
  {
    if (instance_) { instance_->OnCollisionExit(other); }
  }

  void ScriptComponent::PostLoad(const nlohmann::json& raw)
  {
    RebuildInstance(raw);
  }

  void ScriptComponent::PostSave(nlohmann::json& out)
  {
    const nlohmann::json values = instance_ ? WriteProperties(*instance_) : missingValues_;
    if (!values.is_object()) { return; }
    for (const auto& [key, value] : values.items()) {
      // 予約中の差し替え先の名前を、古いスクリプトの値で上書きしない
      if (key != "script") { out[key] = value; }
    }
  }

  void ScriptComponent::RebuildInstance(const nlohmann::json& values)
  {
    builtScript_ = script_;
    if (instance_ && script_ == instance_->GetType().name) {
      ReadProperties(*instance_, values);
      return;
    }

    if (instance_ && hasBegunPlay_) { instance_->EndPlay(); }
    instance_.reset();
    missingValues_ = nullptr;
    if (script_.empty()) { return; }

    std::unique_ptr<TObject> object = TypeRegistry::GetInstance()->Create(script_);
    if (!object || !object->IsA<Script>()) {
      Logger::Log("ScriptComponent: script \"" + script_ + "\" was not found. Its values are kept and saved as they are\n");
      missingValues_ = values;
      return;
    }
    instance_.reset(static_cast<Script*>(object.release()));
    instance_->owner_ = GetOwner();
    instance_->name_  = GetName();
    ReadProperties(*instance_, values);
    if (hasBegunPlay_) { instance_->BeginPlay(); }
  }

#ifdef _DEBUG
  bool ScriptComponent::DrawCustomImGui()
  {
    std::string selected = script_;
    const bool  isOpen   = ImGui::BeginCombo("Script", script_.empty() ? "None" : script_.c_str());
    if (isOpen) {
      if (ImGui::Selectable("None", script_.empty())) { selected.clear(); }
      for (const TypeInfo* type : TypeRegistry::GetInstance()->GetCreatableTypes(Script::StaticType())) {
        if (ImGui::Selectable(type->name, script_ == type->name)) { selected = type->name; }
      }
      ImGui::EndCombo();
    } else {
      // 一覧を開いている間は直前のアイテムが一覧の中のものになるので、閉じているときだけ受け付ける
      ImGui::SetItemTooltip("Drop a script from the Assets window (Script folder)");
      if (std::optional<std::string> dropped = AcceptScriptDrop()) { selected = *dropped; }
    }
    ImGui::SameLine();
    ImGui::BeginDisabled(script_.empty());
    if (ImGui::SmallButton("Open")) { OpenScriptFile(script_); }
    ImGui::EndDisabled();

    const bool isChanged = selected != script_;
    script_              = selected;

    if (instance_ && builtScript_ == script_) {
      ImGui::SeparatorText(instance_->GetType().name);
      return DrawProperties(*instance_) || isChanged;
    }
    if (!missingValues_.is_null() && builtScript_ == script_) {
      ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.2f, 1.0f), "Script \"%s\" was not found (values are kept)", script_.c_str());
    }
    return isChanged;
  }
#endif

} // namespace Tako

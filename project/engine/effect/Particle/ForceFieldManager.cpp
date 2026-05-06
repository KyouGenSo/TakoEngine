#include "ForceFieldManager.h"
#include "GPUParticle.h"
#include <filesystem>
#include <fstream>
#include <iomanip>

#ifdef _DEBUG
#include "DebugUIManager.h"
#endif

namespace Tako {

  namespace {
    constexpr const char* kSceneDirectory = "resources/Json/ForceFieldPresets/";
    constexpr const char* kPresetDirectory = "resources/Json/ParticlePresets/ForceFieldPresets/";
    constexpr const char* kSceneJsonKey = "forceFields";
  }

  ForceFieldManager::ForceFieldManager(GPUParticle* particleSystem)
    : particleSystem_(particleSystem)
  {
  }

  //========================================
  // ファイル単位 API
  //========================================

  void ForceFieldManager::SaveScenePreset(const std::string& filename)
  {
    using json = nlohmann::json;
    json root;

    if (!std::filesystem::exists(kSceneDirectory)) {
      std::filesystem::create_directories(kSceneDirectory);
    }

    SerializeAllToJSON(root);

    const std::string filepath = std::string(kSceneDirectory) + filename + ".json";
    std::ofstream ofs(filepath);
    if (ofs.is_open()) {
      ofs << std::setw(2) << root << std::endl;
      ofs.close();
#ifdef _DEBUG
      DebugUIManager::GetInstance()->AddLog(
        "Saved force fields to: " + filepath, DebugUIManager::LogType::Info);
#endif
    }
    else {
#ifdef _DEBUG
      DebugUIManager::GetInstance()->AddLog(
        "Failed to save force fields to: " + filepath, DebugUIManager::LogType::Error);
#endif
    }
  }

  void ForceFieldManager::LoadScenePreset(const std::string& filename)
  {
    using json = nlohmann::json;

    const std::string filepath = std::string(kSceneDirectory) + filename + ".json";
    std::ifstream ifs(filepath);
    if (!ifs.is_open()) {
#ifdef _DEBUG
      DebugUIManager::GetInstance()->AddLog(
        "Failed to load force fields from: " + filepath, DebugUIManager::LogType::Error);
#endif
      return;
    }

    json root;
    try {
      ifs >> root;
    }
    catch (const json::exception& e) {
#ifdef _DEBUG
      DebugUIManager::GetInstance()->AddLog(
        std::string("ForceField scene JSON parse error: ") + e.what(), DebugUIManager::LogType::Error);
#endif
      return;
    }
    ifs.close();

    DeserializeAllFromJSON(root);

#ifdef _DEBUG
    DebugUIManager::GetInstance()->AddLog(
      "Loaded force fields from: " + filepath, DebugUIManager::LogType::Info);
#endif
  }

  void ForceFieldManager::SavePreset(const std::string& presetName, uint32_t forceFieldIndex)
  {
    using json = nlohmann::json;

    const auto& fields = particleSystem_->GetForceFields();
    if (forceFieldIndex >= fields.size()) {
#ifdef _DEBUG
      DebugUIManager::GetInstance()->AddLog(
        "SavePreset: ForceField index out of range: " + std::to_string(forceFieldIndex),
        DebugUIManager::LogType::Warning);
#endif
      return;
    }

    json preset;
    SerializeForceFieldToJSON(fields[forceFieldIndex], preset);

    if (!std::filesystem::exists(kPresetDirectory)) {
      std::filesystem::create_directories(kPresetDirectory);
    }

    const std::string filepath = std::string(kPresetDirectory) + presetName + ".json";
    std::ofstream ofs(filepath);
    if (ofs.is_open()) {
      ofs << std::setw(2) << preset << std::endl;
      ofs.close();
#ifdef _DEBUG
      DebugUIManager::GetInstance()->AddLog(
        "Saved force field preset: " + presetName, DebugUIManager::LogType::Info);
#endif
    }
    else {
#ifdef _DEBUG
      DebugUIManager::GetInstance()->AddLog(
        "Failed to save force field preset: " + filepath, DebugUIManager::LogType::Error);
#endif
    }
  }

  void ForceFieldManager::LoadPreset(const std::string& presetName)
  {
    using json = nlohmann::json;

    const std::string filepath = std::string(kPresetDirectory) + presetName + ".json";
    std::ifstream ifs(filepath);
    if (!ifs.is_open()) {
#ifdef _DEBUG
      DebugUIManager::GetInstance()->AddLog(
        "Failed to load force field preset: " + presetName, DebugUIManager::LogType::Error);
#endif
      return;
    }

    json preset;
    try {
      ifs >> preset;
    }
    catch (const json::exception& e) {
#ifdef _DEBUG
      DebugUIManager::GetInstance()->AddLog(
        std::string("Force field preset JSON parse error: ") + e.what(), DebugUIManager::LogType::Error);
#endif
      return;
    }
    ifs.close();

    ForceFieldData field{};
    if (!DeserializeForceFieldFromJSON(preset, field)) {
#ifdef _DEBUG
      DebugUIManager::GetInstance()->AddLog(
        "Force field preset is malformed: " + presetName, DebugUIManager::LogType::Error);
#endif
      return;
    }

    const int32_t addedIndex = particleSystem_->AddForceField(field);
    if (addedIndex < 0) {
#ifdef _DEBUG
      DebugUIManager::GetInstance()->AddLog(
        "AddForceField failed (max " + std::to_string(GPUParticle::kMaxForceFields) + " reached)",
        DebugUIManager::LogType::Warning);
#endif
      return;
    }

#ifdef _DEBUG
    DebugUIManager::GetInstance()->AddLog(
      "Loaded force field preset '" + presetName + "' (index " + std::to_string(addedIndex) + ")",
      DebugUIManager::LogType::Info);
#endif
  }

  //========================================
  // JSON object 単位 API（シーン統合保存連携）
  //========================================

  void ForceFieldManager::SerializeAllToJSON(nlohmann::json& root) const
  {
    using json = nlohmann::json;

    json arr = json::array();
    const auto& fields = particleSystem_->GetForceFields();
    for (const auto& field : fields) {
      json fieldJson;
      SerializeForceFieldToJSON(field, fieldJson);
      arr.push_back(fieldJson);
    }
    root[kSceneJsonKey] = arr;
  }

  void ForceFieldManager::DeserializeAllFromJSON(const nlohmann::json& root)
  {
    using json = nlohmann::json;

    try {
      if (!root.contains(kSceneJsonKey) || !root[kSceneJsonKey].is_array()) {
        // 後方互換: forceFields キー不在のシーンプリセットはスキップ
        return;
      }

      for (const auto& fieldJson : root[kSceneJsonKey]) {
        ForceFieldData field{};
        if (!DeserializeForceFieldFromJSON(fieldJson, field)) {
          continue;
        }
        const int32_t addedIndex = particleSystem_->AddForceField(field);
        if (addedIndex < 0) {
#ifdef _DEBUG
          DebugUIManager::GetInstance()->AddLog(
            "AddForceField failed during scene load (max " + std::to_string(GPUParticle::kMaxForceFields) + " reached)",
            DebugUIManager::LogType::Warning);
#endif
          break;
        }
      }
    }
    catch (const json::exception& e) {
#ifdef _DEBUG
      DebugUIManager::GetInstance()->AddLog(
        std::string("ForceField scene deserialization error: ") + e.what(), DebugUIManager::LogType::Error);
#endif
    }
  }

  //========================================
  // JSON 変換ヘルパー
  //========================================

  void ForceFieldManager::SerializeForceFieldToJSON(const ForceFieldData& field, nlohmann::json& json) const
  {
    json["type"] = field.type;
    json["position"] = { field.position.x, field.position.y, field.position.z };
    json["direction"] = { field.direction.x, field.direction.y, field.direction.z };
    json["strength"] = field.strength;
    json["radius"] = field.radius;
    json["falloff"] = field.falloff;
    // pad[2] は GPU アライメント専用なので JSON 対象外
  }

  bool ForceFieldManager::DeserializeForceFieldFromJSON(const nlohmann::json& json, ForceFieldData& outField) const
  {
    using nlohJson = nlohmann::json;

    // pad はあらかじめゼロ初期化しておく（途中で false return しても未初期化が残らないように）
    outField.pad[0] = 0.0f;
    outField.pad[1] = 0.0f;

    try {
      // 構造の最低限チェック
      if (!json.is_object()) return false;
      if (!json.contains("type") || !json.contains("position")) return false;
      if (!json["type"].is_number()) return false;
      if (!json["position"].is_array() || json["position"].size() < 3) return false;
      if (!json["position"][0].is_number()
          || !json["position"][1].is_number()
          || !json["position"][2].is_number()) {
        return false;
      }

      outField.type = json["type"].get<uint32_t>();
      outField.position = {
        json["position"][0].get<float>(),
        json["position"][1].get<float>(),
        json["position"][2].get<float>()
      };

      // direction はオプショナル（Attract/Repel では未使用）。デフォルト (0,0,0)
      const bool dirOk = json.contains("direction")
        && json["direction"].is_array()
        && json["direction"].size() >= 3
        && json["direction"][0].is_number()
        && json["direction"][1].is_number()
        && json["direction"][2].is_number();
      if (dirOk) {
        outField.direction = {
          json["direction"][0].get<float>(),
          json["direction"][1].get<float>(),
          json["direction"][2].get<float>()
        };
      }
      else {
        outField.direction = { 0.0f, 0.0f, 0.0f };
      }

      outField.strength = (json.contains("strength") && json["strength"].is_number())
        ? json["strength"].get<float>() : 0.0f;
      outField.radius = (json.contains("radius") && json["radius"].is_number())
        ? json["radius"].get<float>() : 0.0f;
      outField.falloff = (json.contains("falloff") && json["falloff"].is_number())
        ? json["falloff"].get<float>() : 1.0f;

      return true;
    }
    catch (const nlohJson::exception&) {
      // 型不一致・out_of_range 等の予期しない例外を吸収（境界での防御）
      return false;
    }
  }

} // namespace Tako

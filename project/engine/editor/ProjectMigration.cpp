#ifdef TAKO_EDITOR

#include "ProjectMigration.h"
#include "Logger.h"
#include "Project.h"
#include "SceneFile.h"
#include "StringUtility.h"

#include <json.hpp>

#include <ctime>
#include <exception>
#include <format>
#include <fstream>
#include <stdexcept>
#include <vector>

namespace Tako::ProjectMigration {

  namespace {

    namespace fs = std::filesystem;

    struct Step {
      int         toVersion;                       ///< この手順を終えた後の形式の番号
      const char* description;
      void (*apply)(const fs::path& gameProject);  ///< 失敗は例外で知らせる
    };

    /// <summary>
    /// json の中の全部の ColliderComponent（シーン・プレハブ・置いた物の差分のどこにあっても）に、isTrigger が無ければ true を入れる
    /// </summary>
    /// <returns>1 つでも入れたら true</returns>
    bool MarkCollidersAsTriggers(nlohmann::json& json)
    {
      bool isChanged = false;
      if (json.is_object()) {
        const auto type = json.find("type");
        if (type != json.end() && type->is_string() && type->get_ref<const std::string&>() == "ColliderComponent") {
          nlohmann::json& properties = json["properties"];
          if (!properties.contains("isTrigger")) {
            properties["isTrigger"] = true;
            isChanged               = true;
          }
        }
      }
      if (json.is_structured()) {
        for (nlohmann::json& child : json) {
          isChanged |= MarkCollidersAsTriggers(child);
        }
      }
      return isChanged;
    }

    /// <summary>
    /// 形式 1 までのコライダーは、どれも重なりを知らせるだけだった。新しい既定（押し合う実体）にならないよう、保存済みの物に Trigger の印を付ける
    /// </summary>
    void MigrateCollidersToTriggers(const fs::path& gameProject)
    {
      for (const char* folder : { "Scenes", "Prefabs" }) {
        const fs::path directory = gameProject / "resources" / "Json" / folder;
        if (!fs::is_directory(directory)) {
          continue;
        }
        for (const fs::directory_entry& entry : fs::recursive_directory_iterator(directory)) {
          if (!entry.is_regular_file() || entry.path().extension() != ".json") {
            continue;
          }
          std::ifstream  input(entry.path());
          nlohmann::json json = nlohmann::json::parse(input, nullptr, false);
          input.close();
          // 読めないファイルは触らない（開いたときに、読む側が知らせる）
          if (json.is_discarded() || !MarkCollidersAsTriggers(json)) {
            continue;
          }
          std::ofstream output(entry.path());
          output << SceneFile::ToText(json);
          if (!output) {
            throw std::runtime_error("Could not write " + StringUtility::ConvertString(entry.path().wstring()));
          }
        }
      }
    }

    // 保存の形を変えたら、Project::kFormatVersion を 1 つ上げて、古い形から直す手順をここへ足す（番号の小さい順）。
    const std::vector<Step> kSteps = {
      { 2, "Scenes / Prefabs: mark the existing colliders as triggers", MigrateCollidersToTriggers },
    };

  } // namespace

  std::string Run(const fs::path& root, int fromVersion)
  {
    // フォルダー名には、この PC の時刻を付ける
    const std::time_t now = std::time(nullptr);
    std::tm           local{};
    char              stamp[32]{};
    localtime_s(&local, &now);
    std::strftime(stamp, sizeof(stamp), "%Y%m%d-%H%M%S", &local);

    const fs::path    gameProject = root / "GameProject";
    const fs::path    backup      = root / "Generated" / "Backup" / std::format("format-{}-{}", fromVersion, stamp);
    const std::string backupText  = StringUtility::ConvertString(backup.wstring());

    std::error_code error;
    fs::create_directories(backup, error);
    fs::copy(gameProject / "resources" / "Json", backup / "Json", fs::copy_options::recursive, error);
    if (error) {
      return "Could not save a backup to " + backupText;
    }

    try {
      for (const Step& step : kSteps) {
        if (step.toVersion > fromVersion && step.toVersion <= Project::kFormatVersion) {
          Logger::Log(std::format("ProjectMigration: format {}: {}\n", step.toVersion, step.description));
          step.apply(gameProject);
        }
      }
    }
    catch (const std::exception& exception) {
      return std::format("{}\nThe files before the conversion are in {}", exception.what(), backupText);
    }
    return {};
  }

} // namespace Tako::ProjectMigration

#endif // TAKO_EDITOR

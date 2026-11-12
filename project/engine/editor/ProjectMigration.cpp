#ifdef _DEBUG

#include "ProjectMigration.h"
#include "Logger.h"
#include "Project.h"
#include "StringUtility.h"

#include <ctime>
#include <exception>
#include <format>
#include <vector>

namespace Tako::ProjectMigration {

  namespace {

    namespace fs = std::filesystem;

    struct Step {
      int         toVersion;                       ///< この手順を終えた後の形式の番号
      const char* description;
      void (*apply)(const fs::path& gameProject);  ///< 失敗は例外で知らせる
    };

    // 保存の形を変えたら、Project::kFormatVersion を 1 つ上げて、古い形から直す手順をここへ足す（番号の小さい順）。
    // 例: { 2, "Scenes: rename \"tags\" to \"labels\"", RenameTagsToLabels },
    const std::vector<Step> kSteps = {};

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

#endif // _DEBUG

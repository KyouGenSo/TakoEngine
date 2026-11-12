#ifdef _DEBUG

#include "UpdateChecker.h"
#include "ChildProcess.h"
#include "DebugUIManager.h"
#include "Project.h"

#include <json.hpp>

#include <Windows.h>
#include <chrono>
#include <format>
#include <fstream>

namespace Tako {

  namespace {

    namespace fs = std::filesystem;

    using nlohmann::json;

    // リリースの置き場所。エンジンを別のリポジトリで配るなら、ここと UpdateEngine.ps1 を変える
    constexpr const wchar_t* kLatestReleaseUrl = L"https://api.github.com/repos/KyouGenSo/TakoEngine/releases/latest";

    fs::path CacheFile()
    {
      return Project::GetUserSettingsFolder() / "UpdateCheck.json";
    }

    std::string Today()
    {
      return std::format("{:%Y-%m-%d}", std::chrono::floor<std::chrono::days>(std::chrono::system_clock::now()));
    }

  } // namespace

  UpdateChecker::~UpdateChecker()
  {
    if (process_) {
      CloseHandle(process_);
      CloseHandle(outputRead_);
    }
  }

  void UpdateChecker::Start()
  {
    if (process_) {
      return;
    }
    std::ifstream file(CacheFile());
    const json    cache = file ? json::parse(file, nullptr, false) : json();
    if (cache.is_object() && cache.value("day", std::string()) == Today()) {
      SetLatest(cache.value("latest", std::string()));
      return;
    }
    // curl は Windows 10 以降に入っている。-s: 進み具合を出さない。-m 8: 8 秒で諦める
    if (!ChildProcess::Spawn(std::format(L"curl.exe -s -m 8 {}", kLatestReleaseUrl), process_, outputRead_)) {
      process_ = nullptr;
    }
  }

  void UpdateChecker::Update()
  {
    if (!process_) {
      return;
    }
    ChildProcess::ReadAvailable(outputRead_, output_);
    if (WaitForSingleObject(process_, 0) != WAIT_OBJECT_0) {
      return;
    }
    ChildProcess::ReadAvailable(outputRead_, output_);
    CloseHandle(process_);
    CloseHandle(outputRead_);
    process_    = nullptr;
    outputRead_ = nullptr;

    // リリースが無い・リポジトリが見えない・ネットワークが無いときは tag_name が無い。何も出さず、今日はもう聞かない
    const json        reply = json::parse(output_, nullptr, false);
    const std::string tag   = reply.is_object() && reply.contains("tag_name") && reply["tag_name"].is_string() ? reply["tag_name"].get<std::string>() : std::string();
    output_.clear();

    std::error_code error;
    fs::create_directories(CacheFile().parent_path(), error);
    std::ofstream file(CacheFile());
    file << json{ { "day", Today() }, { "latest", tag } }.dump(2) << "\n";

    SetLatest(tag);
  }

  void UpdateChecker::SetLatest(const std::string& tag)
  {
    const std::string latest  = tag.starts_with('v') ? tag.substr(1) : tag;
    const std::string current = Project::GetEngineVersion();
    if (latest.empty() || current.empty() || Project::CompareVersions(latest, current) <= 0) {
      return;
    }
    newVersion_ = latest;
    LogToConsole(std::format("TakoEngine {} is available (this is {}). Close the editor and run UpdateEngine.bat in the project folder", latest, current),
                 DebugUIManager::LogType::Warning);
  }

} // namespace Tako

#endif // _DEBUG

#ifdef _DEBUG

#include "SceneRecovery.h"
#include "Project.h"
#include "StringUtility.h"

#include <filesystem>
#include <fstream>
#include <iterator>

namespace Tako::SceneRecovery {

  namespace {

    namespace fs = std::filesystem;

    constexpr std::string_view kSuffix = ".scene.json";

    fs::path Folder()
    {
      return Project::GetRoot() / "Generated" / "Recovery";
    }

    fs::path FileOf(const std::string& sceneName)
    {
      return Folder() / StringUtility::ConvertString(sceneName + std::string(kSuffix));
    }

  } // namespace

  void Write(const std::string& sceneName, const std::string& content)
  {
    std::error_code error;
    fs::create_directories(Folder(), error);
    // 書いている途中で落ちても前の物が残るよう、別の名前で書いてから置き換える
    const fs::path target    = FileOf(sceneName);
    fs::path       temporary = target;
    temporary += ".tmp";
    {
      std::ofstream file(temporary, std::ios::binary);
      file << content;
      if (!file.good()) {
        return;
      }
    }
    fs::rename(temporary, target, error);
  }

  void Remove(const std::string& sceneName)
  {
    std::error_code error;
    fs::remove(FileOf(sceneName), error);
  }

  std::vector<Entry> ReadAll()
  {
    std::vector<Entry> entries;
    std::error_code    error;
    for (const fs::directory_entry& item : fs::directory_iterator(Folder(), error)) {
      const std::string name = StringUtility::ConvertString(item.path().filename().wstring());
      if (!item.is_regular_file(error) || !name.ends_with(kSuffix)) {
        continue;
      }
      std::ifstream file(item.path(), std::ios::binary);
      entries.push_back({ name.substr(0, name.size() - kSuffix.size()), std::string(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()) });
    }
    return entries;
  }

} // namespace Tako::SceneRecovery

#endif // _DEBUG

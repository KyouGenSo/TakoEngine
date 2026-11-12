#include "GameModule.h"
#include "Logger.h"
#include "TypeRegistry.h"

#ifdef TAKO_SHARED
#include "GameSettings.h"
#include "Project.h"
#include "StringUtility.h"

#include <Windows.h>
#include <algorithm>
#include <iterator>
#include <string>
#include <vector>
#endif

namespace Tako::GameModule {

#ifdef TAKO_SHARED

  namespace {

    namespace fs = std::filesystem;

    HMODULE                  library    = nullptr;
    std::vector<std::string> typeNames;             ///< この DLL が登録した型（外すときに TypeRegistry から消す）
    fs::file_time_type       loadedTime;            ///< 読んだ Game.dll の更新時刻
    int                      generation = 0;

    std::string ToUtf8(const fs::path& path)
    {
      return StringUtility::ConvertString(path.wstring());
    }

    fs::path HotReloadFolder()
    {
      return Project::GetGameModulePath().parent_path() / "HotReload";
    }

    /// <summary>
    /// Game.dll と Game.pdb を新しいフォルダーへ写す。読むのは写しなので、ビルドは元のファイルを書き換えられる
    /// </summary>
    /// <returns>写した DLL。元が無いか、リンカがまだ書いていれば空</returns>
    fs::path Stage()
    {
      const fs::path           source = Project::GetGameModulePath();
      std::error_code          error;
      const fs::file_time_type time   = fs::last_write_time(source, error);
      if (error) {
        return {};
      }
      // プロセス ID を入れて、同じプロジェクトを開いた別のエディタとぶつからないようにする
      const fs::path folder = HotReloadFolder() / (std::to_string(GetCurrentProcessId()) + "_" + std::to_string(++generation));
      fs::create_directories(folder, error);
      const fs::path copy = folder / source.filename();
      fs::copy_file(source, copy, fs::copy_options::overwrite_existing, error);
      if (error) {
        return {};
      }
      // PDB は無くても動く。ゲームのリンクの /PDBALTPATH により、デバッガーは DLL の隣の PDB を使う
      fs::path pdb = source;
      pdb.replace_extension(".pdb");
      fs::copy_file(pdb, folder / pdb.filename(), fs::copy_options::overwrite_existing, error);
      loadedTime = time;
      return copy;
    }

    bool LoadStaged(const fs::path& copy)
    {
      TypeRegistry*                  registry = TypeRegistry::GetInstance();
      const std::vector<std::string> before   = registry->GetTypeNames();
      library = LoadLibraryW(copy.c_str());
      if (!library) {
        Logger::Log("GameModule: could not load " + ToUtf8(copy) + " (error " + std::to_string(GetLastError()) + ")\n");
        return false;
      }
      // TakoHeaderTool がゲームの TakoReflection.gen.cpp に出す入口
      const FARPROC registerTypes = GetProcAddress(library, "TakoRegisterGameTypes");
      if (!registerTypes) {
        Logger::Log("GameModule: TakoRegisterGameTypes was not found in " + ToUtf8(copy) + "\n");
        FreeLibrary(library);
        library = nullptr;
        return false;
      }
      reinterpret_cast<void (*)()>(registerTypes)();
      const std::vector<std::string> after = registry->GetTypeNames();
      typeNames.clear();
      std::ranges::set_difference(after, before, std::back_inserter(typeNames));
      return true;
    }

  } // namespace

  bool Load()
  {
    // 前の起動の写しを消す（デバッガーが掴んでいる物は残る）
    std::error_code error;
    fs::remove_all(HotReloadFolder(), error);

    const fs::path copy = Stage();
    if (copy.empty()) {
      Logger::Log("GameModule: " + ToUtf8(Project::GetGameModulePath()) + " could not be read. Build the game project\n");
      return false;
    }
    return LoadStaged(copy);
  }

  void Unload()
  {
    if (!library) {
      return;
    }
    TypeRegistry* registry = TypeRegistry::GetInstance();
    for (const std::string& name : typeNames) {
      const TypeInfo* type = registry->Find(name);
      if (type && type->IsChildOf(GameSettings::StaticType())) {
        GameSettings::Release(*type);
      }
    }
    for (const std::string& name : typeNames) {
      registry->Unregister(name);
    }
    typeNames.clear();
    // ponytail: ゲームがエンジンの全体の表（RegisterPostEffect・AddRenderPass・TransitionManager::SetCurrentEffect）に入れた物は追っていない。
    // 入れるスクリプトを書いたら、EndPlay で外させるか、ここで外す
    FreeLibrary(library);
    library = nullptr;
  }

  bool Reload(const std::function<void()>& beforeUnload)
  {
    const fs::path copy = Stage();
    if (copy.empty()) {
      return false;
    }
    beforeUnload();
    Unload();
    LoadStaged(copy);
    return true;
  }

  bool IsOutdated()
  {
    std::error_code          error;
    const fs::file_time_type time = fs::last_write_time(Project::GetGameModulePath(), error);
    return !error && time != loadedTime;
  }

#else

  bool Load()
  {
    RegisterTypes_Game();
    return true;
  }

  void Unload() {}

  bool Reload(const std::function<void()>&)
  {
    return false;
  }

  bool IsOutdated()
  {
    return false;
  }

#endif

} // namespace Tako::GameModule

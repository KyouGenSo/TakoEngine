#include "AssetBrowser.h"
#include "AnimationClip.h"
#include "CameraAnimation.h"
#include "DebugUIManager.h"
#include "Font.h"
#include "ImGuiManager.h"
#include "TextureManager.h"
#include "UILayout.h"
#include "WinApp.h"
#include "EnginePaths.h"
#include "StringUtility.h"
#include "NewScript.h"
#include "Script.h"
#include "TypeRegistry.h"

#include "imgui_internal.h"

#include <shellapi.h>
#include <wrl/client.h>

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <format>
#include <map>
#include <string_view>
#include <system_error>

namespace Tako {

  namespace {
    namespace fs = std::filesystem;

    using LogType = DebugUIManager::LogType;

    constexpr const char* kGameRoot   = "resources";
    constexpr const char* kScriptRoot = "Script";  // C++ スクリプトの置き場所（作業ディレクトリ＝ゲームのプロジェクトのフォルダからの相対）

    constexpr float kTreeInitialWidth = 200.0f;
    constexpr float kMinThumbnailSize = 32.0f;
    constexpr float kMaxThumbnailSize = 256.0f;
    constexpr float kIconPaddingRatio = 0.12f;                           // アイコン周囲の余白（サムネイルサイズ比）
    constexpr float kLargeIconRatio   = 0.8f;                            // 種類のアイコンの大きさ（余白を除いた枠の高さ比）
    constexpr ImU32 kFolderColor      = IM_COL32(214, 172, 92, 255);

    // ponytail: 1 フレームの読み込み枚数上限。大きい画像が多く引っかかるなら非同期読み込みへ
    constexpr int kMaxThumbnailLoadsPerFrame = 4;

    /// <summary>
    /// 種類のファイルを探すフォルダ（作業ディレクトリから、末尾 /）。keepDir なら値はフォルダ込みのパス、でなければフォルダからの相対
    /// </summary>
    struct AssetRoot {
      std::string_view dir;
      bool             keepDir = false;
    };

    struct AssetKind {
      std::string_view              name;
      std::vector<AssetRoot>        roots;
      std::vector<std::string_view> extensions;             ///< ファイル名の末尾と比べる（".mat.json" のように 2 段のものもある）
      bool                          keepExtension = false;  ///< 値に拡張子を残す（ファイル名で読む種類）
    };

    // TextureManager は小文字 ".dds" 以外を WIC で読むので、拡張子は大文字小文字を区別して比べる
    const std::vector<std::string_view> kImageExtensions = { ".png", ".jpg", ".jpeg", ".bmp", ".dds" };

    // フォルダは TakoFramework が TextureManager・Audio に渡すもの、ModelBasic・MaterialLibrary・Prefab・BaseScene・EmitterManager・PostEffectManager が読むものと同じ
    const AssetKind kAssetKinds[] = {
      { "Texture",           { { "resources/Texture/" }, { "EngineResources/", true } }, kImageExtensions, true },
      { "Cubemap",           { { "resources/Texture/" }, { "EngineResources/", true } }, { ".dds" }, true },
      { "Model",             { { "resources/Model/" }, { "EngineResources/Model/", true } }, { ".gltf", ".glb", ".obj", ".fbx" }, true },
      { "Material",          { { "resources/Materials/" } }, { ".mat.json" } },
      { "Scene",             { { "resources/Json/Scenes/" } }, { ".scene.json" } },
      { "Prefab",            { { "resources/Json/Prefabs/" } }, { ".prefab.json" } },
      { "ParticlePreset",    { { "resources/Json/ParticlePresets/Presets/" } }, { ".json" } },
      { "AnimationClip",     { { AnimationClip::kDirectory } }, { ".json" } },
      { "CameraAnimation",   { { CameraAnimation::kDirectory } }, { ".json" } },
      { "UILayout",          { { UILayout::kDirectory } }, { ".json" } },
      { "PostEffectProfile", { { "resources/Json/PostEffects/" } }, { ".json" } },
      { "BehaviorTree",      { { "resources/Json/BT/" } }, { ".json" } },
      { "Sound",             { { "resources/Sound/" } }, { ".wav", ".mp3" }, true },
      { "Font",              { { Font::kDirectory }, { EnginePaths::kEngineFonts, true } }, { ".ttf", ".otf" }, true },
      { "Script",            { { "Script/" } }, { ".h", ".cpp" } },
    };

    const AssetKind* FindAssetKind(std::string_view name) {
      const auto it = std::ranges::find(kAssetKinds, name, &AssetKind::name);
      return it != std::end(kAssetKinds) ? &*it : nullptr;
    }

    std::map<std::string, std::function<void(const std::string&)>, std::less<>>& AssetOpeners() {
      static std::map<std::string, std::function<void(const std::string&)>, std::less<>> openers;
      return openers;
    }

    /// <summary>
    /// fileName の末尾に合う拡張子。合わなければ空
    /// </summary>
    std::string_view MatchExtension(const AssetKind& kind, std::string_view fileName) {
      for (std::string_view extension : kind.extensions) {
        if (fileName.size() > extension.size() && fileName.ends_with(extension)) {
          return extension;
        }
      }
      return {};
    }

    constexpr const char* kRenamePopup = "Rename##AssetBrowser";
    constexpr const char* kDeletePopup = "Delete##AssetBrowser";

    constexpr size_t      kMaxDeleteListLines = 10;  // 確認モーダルに並べるパスの上限（超えた分は件数だけ出す）
    constexpr const char* kReadOnlyWarning    = "Assets: EngineResources is a build-time copy and cannot be modified";

    /// <summary>
    /// imgui.ini に保存する状態。ini は AssetBrowser の破棄後（DestroyContext 時）にも書き出されるため外に持つ
    /// </summary>
    struct SavedState {
      std::string currentDir;
      float       thumbnailSize;
    };
    SavedState savedState;

    std::string ToUtf8(const fs::path& path) {
      return StringUtility::ConvertString(path.generic_wstring());
    }

    fs::path FromUtf8(const std::string& text) {
      return fs::path(StringUtility::ConvertString(text));
    }

    bool LessIgnoreCase(const std::string& a, const std::string& b) {
      return _stricmp(a.c_str(), b.c_str()) < 0;
    }

    bool IsSameOrAncestor(const fs::path& ancestor, const fs::path& path) {
      return std::mismatch(ancestor.begin(), ancestor.end(), path.begin(), path.end()).first == ancestor.end();
    }

    const fs::path& EngineRootPath() {
      // kEngineRoot は末尾 "/" 付きのため、要素比較用に除く
      static const fs::path path = fs::path(EnginePaths::kEngineRoot).parent_path();
      return path;
    }

    /// <summary>
    /// EngineResources 配下か（ビルド時に robocopy で作られるコピーのため変更しても元に反映されない）
    /// </summary>
    bool IsEngineCopy(const fs::path& path) {
      return IsSameOrAncestor(EngineRootPath(), path);
    }

    /// <summary>
    /// 既に存在する場合は "name (n).ext" の空き番号を探す
    /// </summary>
    fs::path MakeUniquePath(const fs::path& path) {
      std::error_code ec;
      fs::path        candidate = path;
      for (int n = 1; fs::exists(candidate, ec); ++n) {
        candidate = path.parent_path() / std::format(L"{} ({}){}", path.stem().wstring(), n, path.extension().wstring());
      }
      return candidate;
    }

    /// <summary>
    /// from 配下のパスを to 配下へ付け替える（リネーム後の表示中フォルダ・選択の追従用）
    /// </summary>
    fs::path Rebase(const fs::path& path, const fs::path& from, const fs::path& to) {
      return path == from ? to : to / path.lexically_relative(from);
    }

    fs::path AbsolutePath(const fs::path& path) {
      std::error_code ec;
      fs::path        absolute = fs::absolute(path, ec);
      return absolute.make_preferred();
    }

    /// <summary>
    /// 親フォルダをエクスプローラーで開き、path を選択状態にする
    /// </summary>
    void ShowInExplorer(const fs::path& path) {
      const std::wstring arguments = L"/select,\"" + AbsolutePath(path).wstring() + L"\"";
      ShellExecuteW(nullptr, L"open", L"explorer.exe", arguments.c_str(), nullptr, SW_SHOWNORMAL);
    }

    void MoveToRecycleBin(const std::vector<fs::path>& paths) {
      // pFrom は null 区切り・二重 null 終端のリスト
      std::wstring from;
      for (const fs::path& path : paths) {
        from += AbsolutePath(path).wstring();
        from.push_back(L'\0');
      }

      SHFILEOPSTRUCTW operation{};
      operation.wFunc  = FO_DELETE;
      operation.pFrom  = from.c_str();
      operation.fFlags = FOF_ALLOWUNDO | FOF_NOCONFIRMATION | FOF_SILENT | FOF_NOERRORUI;
      SHFileOperationW(&operation);
    }

    /// <summary>
    /// 大きいアイコン 1 字を [min, max] の中央に描く（icon は ImGuiManager::kLargeIcons にある字）
    /// </summary>
    void DrawLargeIcon(ImDrawList* drawList, const char* icon, ImU32 color, const ImVec2& min, const ImVec2& max) {
      ImFont*      font = ImGuiManager::GetLargeIconFont();
      const float  size = (max.y - min.y) * kLargeIconRatio;
      const ImVec2 textSize = font->CalcTextSizeA(size, FLT_MAX, 0.0f, icon);
      drawList->AddText(font, size, ImVec2((min.x + max.x - textSize.x) * 0.5f, (min.y + max.y - textSize.y) * 0.5f), color, icon);
    }

    /// <summary>
    /// 縦横比を保って [min, max] の中央に収める
    /// </summary>
    void DrawFitImage(ImDrawList* drawList, ImTextureID texture, float textureWidth, float textureHeight, const ImVec2& min, const ImVec2& max) {
      const float  scale = (std::min)((max.x - min.x) / textureWidth, (max.y - min.y) / textureHeight);
      const ImVec2 half(textureWidth * scale * 0.5f, textureHeight * scale * 0.5f);
      const ImVec2 center((min.x + max.x) * 0.5f, (min.y + max.y) * 0.5f);
      drawList->AddImage(texture, ImVec2(center.x - half.x, center.y - half.y), ImVec2(center.x + half.x, center.y + half.y));
    }

    bool HasExtension(const fs::path& path, std::initializer_list<std::wstring_view> extensions) {
      const std::wstring extension = path.extension().wstring();
      return std::ranges::any_of(extensions, [&](std::wstring_view candidate) { return _wcsicmp(extension.c_str(), candidate.data()) == 0; });
    }

    bool IsSourceFile(const fs::path& path) {
      return HasExtension(path, { L".h", L".hpp", L".cpp", L".hlsl", L".hlsli" });
    }

    // 種類ごとのアイコン（ImGuiManager::kLargeIcons にある字）。上から順に比べ、最初に合った種類を使う
    constexpr std::pair<std::string_view, const char*> kKindIcons[] = {
      { "Model",             ICON_FA_CUBE },
      { "Material",          ICON_FA_PALETTE },
      { "Scene",             ICON_FA_MOUNTAIN_SUN },
      { "Prefab",            ICON_FA_CUBES },
      { "ParticlePreset",    ICON_FA_WAND_MAGIC_SPARKLES },
      { "AnimationClip",     ICON_FA_PERSON_RUNNING },
      { "CameraAnimation",   ICON_FA_VIDEO },
      { "UILayout",          ICON_FA_TABLE_COLUMNS },
      { "PostEffectProfile", ICON_FA_FILM },
      { "BehaviorTree",      ICON_FA_DIAGRAM_PROJECT },
      { "Sound",             ICON_FA_MUSIC },
      { "Font",              ICON_FA_FONT },
      { "Texture",           ICON_FA_IMAGE },
    };

    /// <summary>
    /// ファイルの種類を表すアイコン。アセットの種類に合わなければ拡張子で決める
    /// </summary>
    const char* FileIcon(const fs::path& path) {
      for (const auto& [kind, icon] : kKindIcons) {
        if (ToAssetValue(kind, path)) {
          return icon;
        }
      }
      if (IsSourceFile(path)) {
        // 一覧では名前が途中で切れるので、同じ名前のヘッダーとソースをアイコンで見分ける
        return HasExtension(path, { L".h", L".hpp", L".hlsli" }) ? ICON_FA_FILE_LINES : ICON_FA_FILE_CODE;
      }
      if (HasExtension(path, { L".ttf", L".otf" })) {
        return ICON_FA_FONT;
      }
      return HasExtension(path, { L".json", L".txt", L".ini" }) ? ICON_FA_FILE_LINES : ICON_FA_FILE;
    }

    /// <summary>
    /// 開く処理が登録された種類のファイルなら、それで開く
    /// </summary>
    /// <returns>そういう種類でなければ false</returns>
    bool OpenInRegisteredEditor(const fs::path& path) {
      for (const AssetKind& kind : kAssetKinds) {
        const auto opener = AssetOpeners().find(kind.name);
        if (opener == AssetOpeners().end()) {
          continue;
        }
        if (const std::optional<std::string> value = ToAssetValue(kind.name, path)) {
          opener->second(*value);
          return true;
        }
      }
      return false;
    }

    /// <summary>
    /// ファイル名（拡張子なし）と同じ名前の、作れる Script の型。.h / .cpp 以外か、見つからなければ nullptr
    /// </summary>
    const TypeInfo* FindScriptType(const fs::path& path) {
      if (!HasExtension(path, { L".h", L".cpp" })) {
        return nullptr;
      }
      const TypeInfo* type = TypeRegistry::GetInstance()->Find(ToUtf8(path.stem()));
      return type && type->create && type->IsChildOf(Script::StaticType()) ? type : nullptr;
    }

    /// <summary>
    /// IDispatch のメンバーを名前で呼ぶ（引数は 1 つまで）
    /// </summary>
    bool InvokeByName(IDispatch* target, const wchar_t* name, WORD flags, VARIANT* result, VARIANT* argument = nullptr) {
      DISPID   id      = 0;
      LPOLESTR names[] = { const_cast<LPOLESTR>(name) };
      if (FAILED(target->GetIDsOfNames(IID_NULL, names, 1, LOCALE_USER_DEFAULT, &id))) {
        return false;
      }
      DISPPARAMS parameters{ argument, nullptr, argument ? 1u : 0u, 0u };
      return SUCCEEDED(target->Invoke(id, IID_NULL, LOCALE_USER_DEFAULT, flags, &parameters, result, nullptr, nullptr));
    }

    Microsoft::WRL::ComPtr<IDispatch> GetDispatchByName(IDispatch* target, const wchar_t* name, VARIANT* argument = nullptr) {
      VARIANT result;
      VariantInit(&result);
      Microsoft::WRL::ComPtr<IDispatch> out;
      if (InvokeByName(target, name, DISPATCH_METHOD | DISPATCH_PROPERTYGET, &result, argument) && result.vt == VT_DISPATCH && result.pdispVal) {
        out.Attach(result.pdispVal);
        return out;
      }
      VariantClear(&result);
      return out;
    }

    /// <summary>
    /// 実行中の VS（ROT の "!VisualStudio.DTE."）のうち、path をプロジェクトに含むものの DTE で開く。プロジェクトの一員として開くので IntelliSense が効く
    /// </summary>
    /// <returns>そういう VS が無ければ false</returns>
    bool OpenInVisualStudio(const fs::path& path) {
      using Microsoft::WRL::ComPtr;
      ComPtr<IRunningObjectTable> table;
      ComPtr<IEnumMoniker>        monikers;
      ComPtr<IBindCtx>            context;
      if (FAILED(GetRunningObjectTable(0, &table)) || FAILED(table->EnumRunning(&monikers)) || FAILED(CreateBindCtx(0, &context))) {
        return false;
      }

      const std::wstring file = AbsolutePath(path).wstring();
      ComPtr<IMoniker>   moniker;
      while (monikers->Next(1, &moniker, nullptr) == S_OK) {
        LPOLESTR   displayName = nullptr;
        const bool isDte       = SUCCEEDED(moniker->GetDisplayName(context.Get(), nullptr, &displayName)) && std::wstring_view(displayName).starts_with(L"!VisualStudio.DTE.");
        CoTaskMemFree(displayName);
        ComPtr<IUnknown>  object;
        ComPtr<IDispatch> dte;
        if (!isDte || FAILED(table->GetObject(moniker.Get(), &object)) || FAILED(object.As(&dte))) {
          continue;
        }

        VARIANT fileArgument;
        fileArgument.vt      = VT_BSTR;
        fileArgument.bstrVal = SysAllocString(file.c_str());
        bool                    isOpened = false;
        const ComPtr<IDispatch> solution = GetDispatchByName(dte.Get(), L"Solution");
        if (solution && GetDispatchByName(solution.Get(), L"FindProjectItem", &fileArgument)) {
          const ComPtr<IDispatch> operations = GetDispatchByName(dte.Get(), L"ItemOperations");
          VARIANT                 window;
          VariantInit(&window);
          isOpened = operations && InvokeByName(operations.Get(), L"OpenFile", DISPATCH_METHOD, &window, &fileArgument);
          VariantClear(&window);
          if (const ComPtr<IDispatch> mainWindow = isOpened ? GetDispatchByName(dte.Get(), L"MainWindow") : nullptr) {
            // 前面にいるこのプロセスが許可しないと、VS は自分を前に出せない
            AllowSetForegroundWindow(ASFW_ANY);
            InvokeByName(mainWindow.Get(), L"Activate", DISPATCH_METHOD, nullptr);
          }
        }
        VariantClear(&fileArgument);
        if (isOpened) {
          return true;
        }
      }
      return false;
    }

    void* SettingsReadOpen(ImGuiContext*, ImGuiSettingsHandler*, const char*) {
      return &savedState;
    }

    void SettingsReadLine(ImGuiContext*, ImGuiSettingsHandler*, void*, const char* line) {
      constexpr std::string_view kCurrentDirKey = "CurrentDir=";
      const std::string_view     text(line);
      float                      size = 0.0f;
      if (text.starts_with(kCurrentDirKey)) {
        savedState.currentDir = text.substr(kCurrentDirKey.size());
      }
      else if (sscanf_s(line, "ThumbnailSize=%f", &size) == 1) {
        savedState.thumbnailSize = std::clamp(size, kMinThumbnailSize, kMaxThumbnailSize);
      }
    }

    void SettingsWriteAll(ImGuiContext*, ImGuiSettingsHandler* handler, ImGuiTextBuffer* buffer) {
      buffer->appendf("[%s][State]\n", handler->TypeName);
      buffer->appendf("CurrentDir=%s\n", savedState.currentDir.c_str());
      buffer->appendf("ThumbnailSize=%.0f\n\n", savedState.thumbnailSize);
    }
  }

  void OpenWithShell(const std::filesystem::path& path) {
    const HINSTANCE result = ShellExecuteW(nullptr, L"open", AbsolutePath(path).c_str(), nullptr, nullptr, SW_SHOWNORMAL);
    // 戻り値は互換のため HINSTANCE 型だが、32 以下はエラーコード
    const INT_PTR code = reinterpret_cast<INT_PTR>(result);
    if (code <= 32) {
      LogToConsole(std::format("Failed to open {} (ShellExecute error {})", ToUtf8(path), code), LogType::Warning);
    }
  }

  void OpenSourceFile(const std::filesystem::path& path) {
    if (!OpenInVisualStudio(path)) {
      OpenWithShell(path);
    }
  }

  void OpenScriptFile(std::string_view typeName) {
    const fs::path path = ToAssetPath("Script", typeName);
    if (path.empty()) {
      LogToConsole(std::format("Script file not found: {}/**/{}.h", kScriptRoot, typeName), LogType::Warning);
      return;
    }
    OpenSourceFile(path);
  }

  std::optional<std::string> ToAssetValue(std::string_view kindName, const std::filesystem::path& path) {
    const AssetKind* kind = FindAssetKind(kindName);
    if (!kind) {
      return std::nullopt;
    }
    const std::string      generic   = ToUtf8(path);
    const std::string_view extension = MatchExtension(*kind, generic);
    if (extension.empty()) {
      return std::nullopt;
    }
    // スクリプトはフォルダに関係なく、ファイル名と同じ名前の型
    if (kind->name == "Script") {
      const TypeInfo* type = FindScriptType(path);
      return type ? std::optional<std::string>(type->name) : std::nullopt;
    }
    for (const AssetRoot& root : kind->roots) {
      if (!generic.starts_with(root.dir)) {
        continue;
      }
      std::string value = root.keepDir ? generic : generic.substr(root.dir.size());
      if (!kind->keepExtension) {
        value.resize(value.size() - extension.size());
      }
      return value;
    }
    return std::nullopt;
  }

  std::filesystem::path ToAssetPath(std::string_view kindName, std::string_view value) {
    const AssetKind* kind = FindAssetKind(kindName);
    if (!kind || value.empty()) {
      return {};
    }
    std::error_code ec;
    if (kind->name == "Script") {
      const std::wstring fileName = StringUtility::ConvertString(std::string(value)) + L".h";
      for (const fs::directory_entry& item : fs::recursive_directory_iterator(kScriptRoot, ec)) {
        if (item.path().filename() == fileName) {
          return item.path();
        }
      }
      return {};
    }
    for (const AssetRoot& root : kind->roots) {
      if (root.keepDir) {
        if (value.starts_with(root.dir) && fs::exists(FromUtf8(std::string(value)), ec)) {
          return FromUtf8(std::string(value));
        }
        continue;
      }
      const std::string base = std::string(root.dir) + std::string(value);
      if (kind->keepExtension) {
        if (fs::exists(FromUtf8(base), ec)) {
          return FromUtf8(base);
        }
        continue;
      }
      for (std::string_view extension : kind->extensions) {
        if (const fs::path candidate = FromUtf8(base + std::string(extension)); fs::exists(candidate, ec)) {
          return candidate;
        }
      }
    }
    return {};
  }

  std::vector<std::string> ListAssets(std::string_view kindName) {
    std::vector<std::string> values;
    const AssetKind*         kind = FindAssetKind(kindName);
    if (!kind) {
      return values;
    }
    for (const AssetRoot& root : kind->roots) {
      std::error_code ec;
      for (fs::recursive_directory_iterator it(FromUtf8(std::string(root.dir)), ec), end; !ec && it != end; it.increment(ec)) {
        if (!it->is_regular_file(ec)) {
          continue;
        }
        if (std::optional<std::string> value = ToAssetValue(kindName, it->path())) {
          values.push_back(std::move(*value));
        }
      }
    }
    // スクリプトは .h と .cpp が同じ値になる
    std::ranges::sort(values, LessIgnoreCase);
    const auto [first, last] = std::ranges::unique(values);
    values.erase(first, last);
    return values;
  }

  std::optional<std::string> AcceptAssetPayload(std::string_view kind) {
    // 先に種類を確かめ、違う種類では Accept を呼ばない（呼ぶとドロップ先が光る）
    const ImGuiPayload* dragging = ImGui::GetDragDropPayload();
    if (!dragging || !dragging->IsDataType(AssetBrowser::kAssetPayload)) {
      return std::nullopt;
    }
    std::optional<std::string> value = ToAssetValue(kind, FromUtf8(static_cast<const char*>(dragging->Data)));
    if (!value || !ImGui::AcceptDragDropPayload(AssetBrowser::kAssetPayload)) {
      return std::nullopt;
    }
    return value;
  }

  std::optional<std::string> AcceptAssetDrop(std::string_view kind) {
    if (!ImGui::BeginDragDropTarget()) {
      return std::nullopt;
    }
    std::optional<std::string> value = AcceptAssetPayload(kind);
    ImGui::EndDragDropTarget();
    return value;
  }

  void SetAssetOpener(std::string_view kind, std::function<void(const std::string& value)> opener) {
    AssetOpeners().insert_or_assign(std::string(kind), std::move(opener));
  }

  void OpenAsset(std::string_view kind, const std::string& value) {
    if (value.empty()) {
      return;
    }
    if (const auto it = AssetOpeners().find(kind); it != AssetOpeners().end()) {
      it->second(value);
      return;
    }
    const fs::path path = ToAssetPath(kind, value);
    if (path.empty()) {
      LogToConsole(std::format("{} not found: {}", kind, value), LogType::Warning);
      return;
    }
    if (IsSourceFile(path)) {
      OpenSourceFile(path);
    }
    else {
      OpenWithShell(path);
    }
  }

  AssetBrowser::~AssetBrowser() {
    CloseWatches();
  }

  void AssetBrowser::Initialize(bool* isOpen) {
    isOpen_ = isOpen;

    // ini に保存が無い場合の既定値（ini は最初の NewFrame で読まれて上書きされる）
    savedState.thumbnailSize = thumbnailSize_;

    ImGuiSettingsHandler handler;
    handler.TypeName   = "AssetBrowser";
    handler.TypeHash   = ImHashStr(handler.TypeName);
    handler.ReadOpenFn = SettingsReadOpen;
    handler.ReadLineFn = SettingsReadLine;
    handler.WriteAllFn = SettingsWriteAll;
    ImGui::AddSettingsHandler(&handler);

    DragAcceptFiles(WinApp::GetInstance()->GetHWnd(), TRUE);
  }

  std::span<const ShortcutBinding<AssetBrowser>> AssetBrowser::Shortcuts() {
    // 確認の有無は RequestDelete が Shift で決める
    constexpr auto deleteSelection = [](AssetBrowser& browser) {
      if (std::vector<fs::path> targets = browser.SelectedPaths(); !targets.empty()) {
        browser.RequestDelete(std::move(targets));
      }
    };
    // 修飾キーまで一致した組み合わせだけが反応するので、Shift 併用は別の行にする
    static constexpr ShortcutBinding<AssetBrowser> kShortcuts[] = {
      { ImGuiKey_Delete,                  ImGuiInputFlags_None, "Move to the Recycle Bin (asks first)",   deleteSelection },
      { ImGuiMod_Shift | ImGuiKey_Delete, ImGuiInputFlags_None, "Move to the Recycle Bin without asking", deleteSelection },
      { .description = "Open a folder, or open a file with its default app", .keyLabel = "Double-click" },
    };
    return kShortcuts;
  }

  void AssetBrowser::Draw() {
    // ドロップ先の判定には直近フレームのウィンドウ矩形を使う（閉じていれば空なので何も取り込まない）
    const WinApp::DroppedFiles& dropped = WinApp::GetInstance()->GetDroppedFiles();
    if (!dropped.paths.empty() && ImRect(windowMin_, windowMax_).Contains(ImVec2(dropped.position.x, dropped.position.y))) {
      ImportFiles(dropped.paths);
    }
    windowMin_ = windowMax_ = ImVec2();

    if (!*isOpen_) {
      return;
    }

    // 閉じている間に溜まった通知もここで拾う
    for (void* handle : watchHandles_) {
      if (WaitForSingleObject(handle, 0) == WAIT_OBJECT_0) {
        needsRefresh_ = true;
      }
    }
    if (needsRefresh_) {
      Refresh();
    }
    if (!pendingDir_.empty()) {
      SetCurrentDir(std::move(pendingDir_));
      pendingDir_.clear();
      ScanEntries();
    }

    if (ImGui::Begin(WindowTitle::kAssets, isOpen_)) {
      windowMin_ = ImGui::GetWindowPos();
      windowMax_ = ImVec2(windowMin_.x + ImGui::GetWindowWidth(), windowMin_.y + ImGui::GetWindowHeight());

      DrawToolbar();

      ImGui::BeginChild("##Tree", ImVec2(kTreeInitialWidth, 0.0f), ImGuiChildFlags_Borders | ImGuiChildFlags_ResizeX);
      for (const Folder& root : roots_) {
        ImGui::SetNextItemOpen(true, ImGuiCond_FirstUseEver);
        DrawFolderTree(root);
      }
      revealInTree_ = false;
      ImGui::EndChild();

      ImGui::SameLine();

      ImGui::BeginGroup();
      // 下段の Size スライダー分の高さを残す
      const float footerHeight = ImGui::GetFrameHeightWithSpacing();
      ImGui::BeginChild("##Grid", ImVec2(0.0f, -footerHeight));
      DrawGrid();
      DrawBackgroundContextMenu();
      ImGui::EndChild();
      ImGui::SetNextItemWidth(ImGui::GetFontSize() * 10.0f);
      if (ImGui::SliderFloat("##Size", &thumbnailSize_, kMinThumbnailSize, kMaxThumbnailSize, "Size %.0f")) {
        savedState.thumbnailSize = thumbnailSize_;
        ImGui::MarkIniSettingsDirty();
      }
      ImGui::EndGroup();

      DrawPopups();
    }
    ImGui::End();
  }

  AssetBrowser::Folder AssetBrowser::BuildFolder(const std::filesystem::path& path) {
    Folder folder{ path, ToUtf8(path.filename()), {} };

    std::error_code ec;
    for (const fs::directory_entry& item : fs::directory_iterator(path, ec)) {
      if (item.is_directory(ec)) {
        folder.children.push_back(BuildFolder(item.path()));
      }
    }
    std::ranges::sort(folder.children, LessIgnoreCase, &Folder::name);
    return folder;
  }

  void AssetBrowser::Refresh() {
    needsRefresh_ = false;

    CloseWatches();
    roots_.clear();
    std::error_code ec;
    for (const fs::path& root : { fs::path(kGameRoot), fs::path(kScriptRoot), EngineRootPath() }) {
      if (!fs::is_directory(root, ec)) {
        continue;
      }
      roots_.push_back(BuildFolder(root));

      // 追加・削除・リネームのみ監視する（内容の更新はサムネイルのホットリロード扱いで対象外）
      const HANDLE handle = FindFirstChangeNotificationW(root.c_str(), TRUE, FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_DIR_NAME);
      if (handle != INVALID_HANDLE_VALUE) {
        watchHandles_.push_back(handle);
      }
    }

    // 初回は imgui.ini の値を復元する（ini は最初の NewFrame で読まれ、最初の Draw より前）
    fs::path dir = currentDir_;
    if (dir.empty()) {
      dir            = FromUtf8(savedState.currentDir);
      thumbnailSize_ = savedState.thumbnailSize;
    }
    while (!dir.empty() && !fs::is_directory(dir, ec)) {
      dir = dir.parent_path();
    }
    if (dir.empty() && !roots_.empty()) {
      dir = roots_.front().path;
    }
    if (dir != currentDir_) {
      SetCurrentDir(std::move(dir));
    }
    ScanEntries();
  }

  void AssetBrowser::ScanEntries() {
    // 再走査をまたいで選択を保つ（フォルダが変われば一致するパスが無く、全て解除される）
    std::vector<fs::path> selected = SelectedPaths();
    std::ranges::sort(selected);
    entries_.clear();

    std::error_code ec;
    for (const fs::directory_entry& item : fs::directory_iterator(currentDir_, ec)) {
      const bool isDirectory = item.is_directory(ec);
      const bool isSelected  = std::ranges::binary_search(selected, item.path());
      entries_.push_back({ item.path(), ToUtf8(item.path().filename()), isDirectory ? std::string() : ToAssetValue("Texture", item.path()).value_or(std::string()),
        isDirectory ? ICON_FA_FOLDER : FileIcon(item.path()), isDirectory, isSelected });
    }
    std::ranges::sort(entries_, [](const Entry& a, const Entry& b) {
      if (a.isDirectory != b.isDirectory) {
        return a.isDirectory;
      }
      return LessIgnoreCase(a.name, b.name);
    });
  }

  void AssetBrowser::CloseWatches() {
    for (void* handle : watchHandles_) {
      FindCloseChangeNotification(handle);
    }
    watchHandles_.clear();
  }

  void AssetBrowser::SetCurrentDir(std::filesystem::path dir) {
    currentDir_   = std::move(dir);
    revealInTree_ = true;

    savedState.currentDir = ToUtf8(currentDir_);
    ImGui::MarkIniSettingsDirty();
  }

  void AssetBrowser::ImportFiles(const std::vector<std::filesystem::path>& sources) {
    if (IsEngineCopy(currentDir_)) {
      LogToConsole(kReadOnlyWarning, LogType::Warning);
      return;
    }

    std::error_code ec;
    const fs::path  destinationDir = fs::weakly_canonical(currentDir_, ec);
    for (const fs::path& source : sources) {
      // フォルダを自身の配下へコピーすると再帰が終わらない
      if (IsSameOrAncestor(fs::weakly_canonical(source, ec), destinationDir)) {
        LogToConsole(std::format("Assets: Cannot copy {} into itself", ToUtf8(source)), LogType::Warning);
        continue;
      }

      const fs::path destination = MakeUniquePath(currentDir_ / source.filename());
      fs::copy(source, destination, fs::copy_options::recursive, ec);
      if (ec) {
        LogToConsole(std::format("Assets: Failed to import {} (error {})", ToUtf8(source), ec.value()), LogType::Error);
      }
      else {
        LogToConsole(std::format("Assets: Imported {}", ToUtf8(destination)));
      }
    }
    needsRefresh_ = true;
  }

  void AssetBrowser::DrawToolbar() {
    if (UI::IconButton(ICON_FA_ROTATE, "Refresh")) {
      needsRefresh_ = true;
    }

    // パンくずは枠の無いボタンにする（ホバーしたときだけ背景が出る）
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
    fs::path partial;
    int      depth = 0;
    for (const fs::path& part : currentDir_) {
      partial /= part;
      ImGui::SameLine();
      if (depth > 0) {
        ImGui::AlignTextToFramePadding();
        ImGui::TextDisabled(ICON_FA_CHEVRON_RIGHT);
        ImGui::SameLine();
      }
      ImGui::PushID(depth++);
      if (ImGui::Button(ToUtf8(part).c_str())) {
        pendingDir_ = partial;
      }
      ImGui::PopID();
    }
    ImGui::PopStyleColor();

    // 検索欄を右端に寄せる
    ImGui::SameLine();
    const float filterWidth = ImGui::GetFontSize() * 14.0f;
    UI::AlignRight(filterWidth);
    ImGui::SetNextItemWidth(filterWidth);
    if (ImGui::InputTextWithHint("##Filter", ICON_FA_MAGNIFYING_GLASS "  Search", filter_.InputBuf, IM_ARRAYSIZE(filter_.InputBuf))) {
      filter_.Build();
      // 検索で隠れた項目が選択に残ると、見えないまま削除対象に入る
      for (Entry& entry : entries_) {
        entry.isSelected = entry.isSelected && filter_.PassFilter(entry.name.c_str());
      }
    }
    ImGui::SetItemTooltip("Search (\"a,b\" = OR, \"-a\" = exclude)");
  }

  void AssetBrowser::DrawFolderTree(const Folder& folder) {
    const bool isCurrent = folder.path == currentDir_;

    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick | ImGuiTreeNodeFlags_SpanAvailWidth;
    if (folder.children.empty()) {
      flags |= ImGuiTreeNodeFlags_Leaf;
    }
    if (isCurrent) {
      flags |= ImGuiTreeNodeFlags_Selected;
    }

    // グリッドやパンくずで移動したとき、表示中フォルダの祖先を開いて見える位置へスクロールする
    if (revealInTree_ && !isCurrent && IsSameOrAncestor(folder.path, currentDir_)) {
      ImGui::SetNextItemOpen(true);
    }
    const bool isNodeOpen = ImGui::TreeNodeEx(folder.name.c_str(), flags, ICON_FA_FOLDER "  %s", folder.name.c_str());
    if (revealInTree_ && isCurrent && !ImGui::IsItemVisible()) {
      ImGui::SetScrollHereY();
    }

    if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen()) {
      pendingDir_ = folder.path;
    }
    DrawItemContextMenu(folder.path, true, {}, false);

    if (isNodeOpen) {
      for (const Folder& child : folder.children) {
        DrawFolderTree(child);
      }
      ImGui::TreePop();
    }
  }

  void AssetBrowser::DrawGrid() {
    if (entries_.empty()) {
      ImGui::TextDisabled("(empty)");
      return;
    }

    const float spacing = ImGui::GetStyle().ItemSpacing.x;
    const int   columns = (std::max)(1, static_cast<int>((ImGui::GetContentRegionAvail().x + spacing) / (thumbnailSize_ + spacing)));

    // ponytail: 範囲選択の起点は ImGui が添字で覚えるため、再走査後は同じ位置の別項目が起点になる。気になるなら再走査時に起点を捨てる
    const int itemCount = static_cast<int>(entries_.size());
    ApplySelectionRequests(ImGui::BeginMultiSelect(ImGuiMultiSelectFlags_ClearOnEscape | ImGuiMultiSelectFlags_ClearOnClickVoid, -1, itemCount));

    int loadBudget = kMaxThumbnailLoadsPerFrame;
    int column     = 0;
    for (int i = 0; i < itemCount; ++i) {
      const Entry& entry = entries_[i];
      if (!filter_.PassFilter(entry.name.c_str())) {
        continue;
      }
      if (column > 0) {
        ImGui::SameLine();
      }
      // 位置でなく名前を ID にする（再走査で並びが変わっても、開いているメニューが別のファイルへ付け替わらない）
      ImGui::PushID(entry.name.c_str());
      ImGui::SetNextItemSelectionUserData(i);
      DrawItem(entry, loadBudget);
      ImGui::PopID();
      column = (column + 1) % columns;
    }

    ProcessShortcuts(Shortcuts(), *this);

    ApplySelectionRequests(ImGui::EndMultiSelect());
  }

  void AssetBrowser::ApplySelectionRequests(const ImGuiMultiSelectIO* io) {
    const int lastIndex = static_cast<int>(entries_.size()) - 1;
    for (const ImGuiSelectionRequest& request : io->Requests) {
      // SetAll は範囲を持たないので全件に読み替える
      const bool isSetAll = request.Type == ImGuiSelectionRequestType_SetAll;
      // ImGui が覚えている起点の添字は項目数が減った後も残り得るので、範囲内へ収める
      const int first = isSetAll ? 0 : (std::max)(0, static_cast<int>(request.RangeFirstItem));
      const int last  = isSetAll ? lastIndex : (std::min)(lastIndex, static_cast<int>(request.RangeLastItem));
      for (int i = first; i <= last; ++i) {
        // 検索で隠れている項目は選択させない（見えないまま削除対象に入るのを防ぐ）
        entries_[i].isSelected = request.Selected && filter_.PassFilter(entries_[i].name.c_str());
      }
    }
  }

  std::vector<std::filesystem::path> AssetBrowser::SelectedPaths() const {
    std::vector<fs::path> paths;
    for (const Entry& entry : entries_) {
      if (entry.isSelected) {
        paths.push_back(entry.path);
      }
    }
    return paths;
  }

  void AssetBrowser::DrawItem(const Entry& entry, int& loadBudget) {
    const ImVec2 pos         = ImGui::GetCursorScreenPos();
    const float  labelHeight = ImGui::GetTextLineHeightWithSpacing();

    if (ImGui::Selectable("##Item", entry.isSelected, ImGuiSelectableFlags_AllowDoubleClick, ImVec2(thumbnailSize_, thumbnailSize_ + labelHeight))) {
      if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
        if (entry.isDirectory) {
          pendingDir_ = entry.path;
        }
        else if (!OpenInRegisteredEditor(entry.path)) {
          if (IsSourceFile(entry.path)) {
            OpenSourceFile(entry.path);
          }
          else {
            OpenWithShell(entry.path);
          }
        }
      }
    }
    // ファイルはどれもドラッグでき、受け取る側が種類を見て選ぶ。ツールチップ（ウィンドウ）の End で直前のアイテムは Selectable に戻る
    if (!entry.isDirectory && ImGui::BeginDragDropSource()) {
      const std::string path = ToUtf8(entry.path);
      ImGui::SetDragDropPayload(kAssetPayload, path.c_str(), path.size() + 1);
      ImGui::TextUnformatted(entry.name.c_str());
      ImGui::EndDragDropSource();
    }

    // ポップアップの End で直前のアイテムは Selectable に戻るので、以降の判定はそのまま使える
    DrawItemContextMenu(entry.path, entry.isDirectory, entry.textureKey, entry.isSelected);

    // スクロール外の項目は読み込みも描画もしない
    if (!ImGui::IsItemVisible()) {
      return;
    }

    TextureManager*             textureManager = TextureManager::GetInstance();
    const DirectX::TexMetadata* metadata       = nullptr;
    if (!entry.textureKey.empty()) {
      if (!textureManager->IsLoaded(entry.textureKey) && loadBudget > 0) {
        textureManager->LoadTexture(entry.textureKey);
        --loadBudget;
      }
      if (textureManager->IsLoaded(entry.textureKey)) {
        // ImGui のシェーダーは Texture2D 前提のため cubemap などはアイコン表示にする
        const DirectX::TexMetadata& candidate = textureManager->GetMetaData(entry.textureKey);
        if (candidate.dimension == DirectX::TEX_DIMENSION_TEXTURE2D && !candidate.IsCubemap()) {
          metadata = &candidate;
        }
      }
    }

    if (ImGui::BeginItemTooltip()) {
      ImGui::TextUnformatted(ToUtf8(entry.path).c_str());
      if (metadata) {
        ImGui::TextDisabled("%zu x %zu", metadata->width, metadata->height);
      }
      ImGui::EndTooltip();
    }

    ImDrawList*  drawList = ImGui::GetWindowDrawList();
    const float  padding  = thumbnailSize_ * kIconPaddingRatio;
    const ImVec2 iconMin(pos.x + padding, pos.y + padding);
    const ImVec2 iconMax(pos.x + thumbnailSize_ - padding, pos.y + thumbnailSize_ - padding);
    if (metadata) {
      const ImTextureID texture = static_cast<ImTextureID>(textureManager->GetSRVGPUHandle(entry.textureKey).ptr);
      DrawFitImage(drawList, texture, static_cast<float>(metadata->width), static_cast<float>(metadata->height), iconMin, iconMax);
    }
    else {
      DrawLargeIcon(drawList, entry.icon, entry.isDirectory ? kFolderColor : ImGui::GetColorU32(ImGuiCol_Text, 0.75f), iconMin, iconMax);
    }

    // 収まる名前は中央寄せ、はみ出す名前は末尾を "..." で省略
    const ImVec2 textSize  = ImGui::CalcTextSize(entry.name.c_str());
    const float  labelMaxX = pos.x + thumbnailSize_;
    const ImVec2 labelMin(pos.x + (std::max)(0.0f, (thumbnailSize_ - textSize.x) * 0.5f), pos.y + thumbnailSize_);
    const ImVec2 labelMax(labelMaxX, labelMin.y + labelHeight);
    ImGui::RenderTextEllipsis(drawList, labelMin, labelMax, labelMaxX, labelMaxX, entry.name.c_str(), nullptr, &textSize);
  }

  void AssetBrowser::DrawItemContextMenu(const std::filesystem::path& path, bool isDirectory, const std::string& textureKey, bool isSelected) {
    if (!ImGui::BeginPopupContextItem()) {
      return;
    }

    if (ImGui::MenuItem(ICON_FA_ARROW_UP_RIGHT_FROM_SQUARE "  Open")) {
      if (isDirectory) {
        pendingDir_ = path;
      }
      else if (IsSourceFile(path)) {
        OpenSourceFile(path);
      }
      else {
        OpenWithShell(path);
      }
    }
    if (ImGui::MenuItem(ICON_FA_FOLDER_OPEN "  Show in Explorer")) {
      ShowInExplorer(path);
    }

    ImGui::Separator();
    if (ImGui::MenuItem(ICON_FA_COPY "  Copy Path")) {
      ImGui::SetClipboardText(ToUtf8(path).c_str());
    }
    if (!textureKey.empty() && ImGui::MenuItem(ICON_FA_KEY "  Copy Texture Key")) {
      ImGui::SetClipboardText(textureKey.c_str());
    }

    ImGui::Separator();
    const bool isEngineCopy = IsEngineCopy(path);
    // ルート（親を持たない）は名前変更・削除させない
    const bool canModify = !isEngineCopy && path.has_parent_path();
    if (isDirectory && ImGui::MenuItem(ICON_FA_FOLDER_PLUS "  New Folder", nullptr, false, !isEngineCopy)) {
      CreateFolder(path);
    }
    if (ImGui::MenuItem(ICON_FA_PEN "  Rename", nullptr, false, canModify)) {
      RequestRename(path);
    }
    // 選択中の項目から開いたメニューは、選択全体を削除対象にする
    std::vector<fs::path> targets     = isSelected ? SelectedPaths() : std::vector<fs::path>{ path };
    const std::string     deleteLabel = targets.size() > 1 ? std::format(ICON_FA_TRASH_CAN "  Delete {} items###Delete", targets.size()) : ICON_FA_TRASH_CAN "  Delete###Delete";
    if (ImGui::MenuItem(deleteLabel.c_str(), nullptr, false, canModify)) {
      RequestDelete(std::move(targets));
    }
    if (isEngineCopy) {
      ImGui::TextDisabled("EngineResources is read-only (build-time copy)");
    }

    ImGui::EndPopup();
  }

  void AssetBrowser::DrawBackgroundContextMenu() {
    if (!ImGui::BeginPopupContextWindow("##GridContext", ImGuiPopupFlags_MouseButtonRight | ImGuiPopupFlags_NoOpenOverItems)) {
      return;
    }

    if (ImGui::MenuItem(ICON_FA_FOLDER_PLUS "  New Folder", nullptr, false, !IsEngineCopy(currentDir_))) {
      CreateFolder(currentDir_);
    }
    if (IsSameOrAncestor(kScriptRoot, currentDir_) && ImGui::MenuItem(ICON_FA_FILE_CIRCLE_PLUS "  New Script...")) {
      RequestNewScriptPopup(currentDir_.generic_string());
    }
    if (ImGui::MenuItem(ICON_FA_FOLDER_OPEN "  Show in Explorer")) {
      OpenWithShell(currentDir_);
    }
    if (ImGui::MenuItem(ICON_FA_COPY "  Copy Path")) {
      ImGui::SetClipboardText(ToUtf8(currentDir_).c_str());
    }
    ImGui::Separator();
    if (ImGui::MenuItem(ICON_FA_ROTATE "  Refresh")) {
      needsRefresh_ = true;
    }

    ImGui::EndPopup();
  }

  void AssetBrowser::DrawPopups() {
    if (openRenamePopup_) {
      ImGui::OpenPopup(kRenamePopup);
      openRenamePopup_ = false;
    }
    if (openDeletePopup_) {
      ImGui::OpenPopup(kDeletePopup);
      openDeletePopup_ = false;
    }

    if (ImGui::BeginPopupModal(kRenamePopup, nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
      if (ImGui::IsWindowAppearing()) {
        ImGui::SetKeyboardFocusHere();
      }
      const bool entered = ImGui::InputText("##Name", renameBuffer_, sizeof(renameBuffer_), ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll);
      const bool ok      = ImGui::Button("OK");
      ImGui::SameLine();
      const bool cancel = ImGui::Button("Cancel") || ImGui::IsKeyPressed(ImGuiKey_Escape);
      if (entered || ok) {
        ApplyRename();
      }
      if (entered || ok || cancel) {
        ImGui::CloseCurrentPopup();
      }
      ImGui::EndPopup();
    }

    if (ImGui::BeginPopupModal(kDeletePopup, nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
      ImGui::Text("Move %zu item(s) to Recycle Bin?", deleteTargets_.size());
      for (size_t i = 0; i < (std::min)(deleteTargets_.size(), kMaxDeleteListLines); ++i) {
        ImGui::TextDisabled("%s", ToUtf8(deleteTargets_[i]).c_str());
      }
      if (deleteTargets_.size() > kMaxDeleteListLines) {
        ImGui::TextDisabled("... and %zu more", deleteTargets_.size() - kMaxDeleteListLines);
      }
      if (ImGui::Button(ICON_FA_TRASH_CAN "  Delete")) {
        ApplyDelete();
        ImGui::CloseCurrentPopup();
      }
      ImGui::SameLine();
      if (ImGui::Button("Cancel") || ImGui::IsKeyPressed(ImGuiKey_Escape)) {
        ImGui::CloseCurrentPopup();
      }
      ImGui::EndPopup();
    }
  }

  void AssetBrowser::RequestRename(const std::filesystem::path& path) {
    renameTarget_ = path;
    strncpy_s(renameBuffer_, ToUtf8(path.filename()).c_str(), _TRUNCATE);
    openRenamePopup_ = true;
  }

  void AssetBrowser::ApplyRename() {
    const std::string newName = renameBuffer_;
    if (newName.empty() || newName.find_first_of("/\\") != std::string::npos) {
      LogToConsole(std::format("Assets: Invalid name \"{}\"", newName), LogType::Warning);
      return;
    }

    const fs::path  destination = renameTarget_.parent_path() / FromUtf8(newName);
    std::error_code ec;
    // 大文字小文字だけの変更は同じ実体を指すので許可する
    if (fs::exists(destination, ec) && !fs::equivalent(renameTarget_, destination, ec)) {
      LogToConsole(std::format("Assets: {} already exists", ToUtf8(destination)), LogType::Warning);
      return;
    }
    fs::rename(renameTarget_, destination, ec);
    if (ec) {
      LogToConsole(std::format("Assets: Failed to rename {} (error {})", ToUtf8(renameTarget_), ec.value()), LogType::Error);
      return;
    }

    // 次の ScanEntries は選択をパスで引き継ぐので、名前が変わった分を先に付け替えておく
    for (Entry& entry : entries_) {
      if (IsSameOrAncestor(renameTarget_, entry.path)) {
        entry.path = Rebase(entry.path, renameTarget_, destination);
      }
    }
    if (IsSameOrAncestor(renameTarget_, currentDir_)) {
      SetCurrentDir(Rebase(currentDir_, renameTarget_, destination));
    }
    needsRefresh_ = true;
  }

  void AssetBrowser::RequestDelete(std::vector<std::filesystem::path> targets) {
    // メニューの Delete は無効表示で防げるが、Delete キーには無効表示が無い
    if (std::ranges::any_of(targets, IsEngineCopy)) {
      LogToConsole(kReadOnlyWarning, LogType::Warning);
      return;
    }

    deleteTargets_ = std::move(targets);
    if (ImGui::GetIO().KeyShift) {
      ApplyDelete();
    }
    else {
      openDeletePopup_ = true;
    }
  }

  void AssetBrowser::ApplyDelete() {
    MoveToRecycleBin(deleteTargets_);

    // 一括操作は一部だけ成功し得るので、成否は対象ごとに実体の有無で判定する
    std::error_code ec;
    for (const fs::path& target : deleteTargets_) {
      if (fs::exists(target, ec)) {
        LogToConsole(std::format("Assets: Failed to delete {}", ToUtf8(target)), LogType::Error);
      }
      else {
        LogToConsole(std::format("Assets: Moved {} to Recycle Bin", ToUtf8(target)));
      }
    }
    needsRefresh_ = true;
  }

  void AssetBrowser::CreateFolder(const std::filesystem::path& parent) {
    const fs::path  folder = MakeUniquePath(parent / "New Folder");
    std::error_code ec;
    if (!fs::create_directory(folder, ec)) {
      LogToConsole(std::format("Assets: Failed to create {} (error {})", ToUtf8(folder), ec.value()), LogType::Error);
      return;
    }
    needsRefresh_ = true;
    RequestRename(folder);
  }

} // namespace Tako

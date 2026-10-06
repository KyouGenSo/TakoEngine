#include "AssetBrowser.h"
#include "DebugUIManager.h"
#include "TextureManager.h"
#include "WinApp.h"
#include "EnginePaths.h"
#include "StringUtility.h"

#include "imgui_internal.h"

#include <shellapi.h>

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <format>
#include <string_view>
#include <system_error>

namespace Tako {

  namespace {
    namespace fs = std::filesystem;

    using LogType = DebugUIManager::LogType;

    constexpr const char* kGameRoot = "resources";

    constexpr float kTreeInitialWidth = 200.0f;
    constexpr float kMinThumbnailSize = 32.0f;
    constexpr float kMaxThumbnailSize = 256.0f;
    constexpr float kIconPaddingRatio = 0.12f;                           // アイコン周囲の余白（サムネイルサイズ比）
    constexpr ImU32 kFolderColor      = IM_COL32(214, 172, 92, 255);

    // ponytail: 1 フレームの読み込み枚数上限。大きい画像が多く引っかかるなら非同期読み込みへ
    constexpr int kMaxThumbnailLoadsPerFrame = 4;

    // TextureManager は小文字 ".dds" 以外を WIC で読み、失敗時 assert するため大文字拡張子は含めない
    constexpr std::string_view kImageExtensions[] = { ".png", ".jpg", ".jpeg", ".bmp", ".dds" };

    constexpr const char* kRenamePopup = "Rename##AssetBrowser";
    constexpr const char* kDeletePopup = "Delete##AssetBrowser";

    /// <summary>
    /// imgui.ini に保存する状態。ini は AssetBrowser の破棄後（DestroyContext 時）にも書き出されるため外に持つ
    /// </summary>
    struct SavedState {
      std::string currentDir;
      float       thumbnailSize;
    };
    SavedState savedState;

    void Log(const std::string& message, LogType type = LogType::Info) {
      DebugUIManager::GetInstance()->AddLog(message, type);
    }

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
    /// TextureManager::LoadTexture のパス解決規則に合わせたキー（サムネイル対象外は空）
    /// </summary>
    std::string ToTextureKey(const fs::path& path) {
      const std::string extension = ToUtf8(path.extension());
      if (std::ranges::find(kImageExtensions, std::string_view(extension)) == std::end(kImageExtensions)) {
        return {};
      }

      const std::string  genericPath = ToUtf8(path);
      const std::string& textureDir  = TextureManager::GetInstance()->GetDirectoryPath();
      if (genericPath.starts_with(textureDir)) {
        return genericPath.substr(textureDir.size());
      }
      if (genericPath.starts_with(EnginePaths::kEngineRoot)) {
        return genericPath;
      }
      return {};
    }

    std::string ExtensionLabel(const std::string& fileName) {
      const size_t dot = fileName.rfind('.');
      if (dot == std::string::npos) {
        return {};
      }
      std::string label = fileName.substr(dot + 1);
      std::ranges::transform(label, label.begin(), [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
      return label;
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
    /// OS の関連付けアプリで開く（フォルダはエクスプローラー）
    /// </summary>
    void OpenWithShell(const fs::path& path) {
      const HINSTANCE result = ShellExecuteW(nullptr, L"open", AbsolutePath(path).c_str(), nullptr, nullptr, SW_SHOWNORMAL);
      // 戻り値は互換のため HINSTANCE 型だが、32 以下はエラーコード
      const INT_PTR code = reinterpret_cast<INT_PTR>(result);
      if (code <= 32) {
        Log(std::format("Assets: Failed to open {} (ShellExecute error {})", ToUtf8(path), code), LogType::Warning);
      }
    }

    /// <summary>
    /// 親フォルダをエクスプローラーで開き、path を選択状態にする
    /// </summary>
    void ShowInExplorer(const fs::path& path) {
      const std::wstring arguments = L"/select,\"" + AbsolutePath(path).wstring() + L"\"";
      ShellExecuteW(nullptr, L"open", L"explorer.exe", arguments.c_str(), nullptr, SW_SHOWNORMAL);
    }

    bool MoveToRecycleBin(const fs::path& path) {
      // pFrom は二重 null 終端のリスト
      std::wstring from = AbsolutePath(path).wstring();
      from.push_back(L'\0');

      SHFILEOPSTRUCTW operation{};
      operation.wFunc  = FO_DELETE;
      operation.pFrom  = from.c_str();
      operation.fFlags = FOF_ALLOWUNDO | FOF_NOCONFIRMATION | FOF_SILENT | FOF_NOERRORUI;
      return SHFileOperationW(&operation) == 0 && !operation.fAnyOperationsAborted;
    }

    void DrawFolderIcon(ImDrawList* drawList, const ImVec2& min, const ImVec2& max) {
      const float width    = max.x - min.x;
      const float height   = max.y - min.y;
      const float rounding = width * 0.06f;
      // タブと本体を同色で重ね、フォルダのシルエットにする
      drawList->AddRectFilled(ImVec2(min.x, min.y + height * 0.12f), ImVec2(min.x + width * 0.45f, min.y + height * 0.32f), kFolderColor, rounding);
      drawList->AddRectFilled(ImVec2(min.x, min.y + height * 0.22f), ImVec2(max.x, max.y - height * 0.08f), kFolderColor, rounding);
    }

    /// <summary>
    /// 縦長の用紙形に拡張子ラベルを載せたファイルアイコン
    /// </summary>
    void DrawFileIcon(ImDrawList* drawList, const ImVec2& min, const ImVec2& max, const std::string& label) {
      const float  width    = max.x - min.x;
      const float  rounding = width * 0.06f;
      const ImVec2 pageMin(min.x + width * 0.15f, min.y);
      const ImVec2 pageMax(max.x - width * 0.15f, max.y);
      drawList->AddRectFilled(pageMin, pageMax, ImGui::GetColorU32(ImGuiCol_Button), rounding);
      drawList->AddRect(pageMin, pageMax, ImGui::GetColorU32(ImGuiCol_Border), rounding);
      ImGui::RenderTextClipped(pageMin, pageMax, label.c_str(), nullptr, nullptr, ImVec2(0.5f, 0.5f));
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

    if (ImGui::Begin("Assets", isOpen_)) {
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
      if (ImGui::SliderFloat("Size", &thumbnailSize_, kMinThumbnailSize, kMaxThumbnailSize, "%.0f")) {
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
    for (const fs::path& root : { fs::path(kGameRoot), EngineRootPath() }) {
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
    entries_.clear();

    std::error_code ec;
    for (const fs::directory_entry& item : fs::directory_iterator(currentDir_, ec)) {
      const bool isDirectory = item.is_directory(ec);
      entries_.push_back({ item.path(), ToUtf8(item.path().filename()), isDirectory ? std::string() : ToTextureKey(item.path()), isDirectory });
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
      Log("Assets: EngineResources is a build-time copy and cannot be modified", LogType::Warning);
      return;
    }

    std::error_code ec;
    const fs::path  destinationDir = fs::weakly_canonical(currentDir_, ec);
    for (const fs::path& source : sources) {
      // フォルダを自身の配下へコピーすると再帰が終わらない
      if (IsSameOrAncestor(fs::weakly_canonical(source, ec), destinationDir)) {
        Log(std::format("Assets: Cannot copy {} into itself", ToUtf8(source)), LogType::Warning);
        continue;
      }

      const fs::path destination = MakeUniquePath(currentDir_ / source.filename());
      fs::copy(source, destination, fs::copy_options::recursive, ec);
      if (ec) {
        Log(std::format("Assets: Failed to import {} (error {})", ToUtf8(source), ec.value()), LogType::Error);
      }
      else {
        Log(std::format("Assets: Imported {}", ToUtf8(destination)));
      }
    }
    needsRefresh_ = true;
  }

  void AssetBrowser::DrawToolbar() {
    if (ImGui::Button("Refresh")) {
      needsRefresh_ = true;
    }

    fs::path partial;
    int      depth = 0;
    for (const fs::path& part : currentDir_) {
      partial /= part;
      ImGui::SameLine();
      if (depth > 0) {
        ImGui::TextDisabled(">");
        ImGui::SameLine();
      }
      ImGui::PushID(depth++);
      if (ImGui::Button(ToUtf8(part).c_str())) {
        pendingDir_ = partial;
      }
      ImGui::PopID();
    }

    // 検索欄を右端に寄せる
    ImGui::SameLine();
    const float filterWidth = ImGui::GetFontSize() * 12.0f;
    const float space       = ImGui::GetContentRegionAvail().x - filterWidth;
    if (space > 0.0f) {
      ImGui::SetCursorPosX(ImGui::GetCursorPosX() + space);
    }
    filter_.Draw("##Filter", filterWidth);
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
    const bool isNodeOpen = ImGui::TreeNodeEx(folder.name.c_str(), flags);
    if (revealInTree_ && isCurrent && !ImGui::IsItemVisible()) {
      ImGui::SetScrollHereY();
    }

    if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen()) {
      pendingDir_ = folder.path;
    }
    DrawItemContextMenu(folder.path, true, {});

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

    int loadBudget = kMaxThumbnailLoadsPerFrame;
    int column     = 0;
    for (int i = 0; i < static_cast<int>(entries_.size()); ++i) {
      const Entry& entry = entries_[i];
      if (!filter_.PassFilter(entry.name.c_str())) {
        continue;
      }
      if (column > 0) {
        ImGui::SameLine();
      }
      ImGui::PushID(i);
      DrawItem(entry, loadBudget);
      ImGui::PopID();
      column = (column + 1) % columns;
    }
  }

  void AssetBrowser::DrawItem(const Entry& entry, int& loadBudget) {
    const ImVec2 pos         = ImGui::GetCursorScreenPos();
    const float  labelHeight = ImGui::GetTextLineHeightWithSpacing();

    if (ImGui::Selectable("##Item", entry.path == selectedPath_, ImGuiSelectableFlags_AllowDoubleClick, ImVec2(thumbnailSize_, thumbnailSize_ + labelHeight))) {
      selectedPath_ = entry.path;
      if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
        if (entry.isDirectory) {
          pendingDir_ = entry.path;
        }
        else {
          OpenWithShell(entry.path);
        }
      }
    }
    // ポップアップの End で直前のアイテムは Selectable に戻るので、以降の判定はそのまま使える
    DrawItemContextMenu(entry.path, entry.isDirectory, entry.textureKey);

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
    else if (entry.isDirectory) {
      DrawFolderIcon(drawList, iconMin, iconMax);
    }
    else {
      DrawFileIcon(drawList, iconMin, iconMax, ExtensionLabel(entry.name));
    }

    // 収まる名前は中央寄せ、はみ出す名前は末尾を "..." で省略
    const ImVec2 textSize  = ImGui::CalcTextSize(entry.name.c_str());
    const float  labelMaxX = pos.x + thumbnailSize_;
    const ImVec2 labelMin(pos.x + (std::max)(0.0f, (thumbnailSize_ - textSize.x) * 0.5f), pos.y + thumbnailSize_);
    const ImVec2 labelMax(labelMaxX, labelMin.y + labelHeight);
    ImGui::RenderTextEllipsis(drawList, labelMin, labelMax, labelMaxX, labelMaxX, entry.name.c_str(), nullptr, &textSize);
  }

  void AssetBrowser::DrawItemContextMenu(const std::filesystem::path& path, bool isDirectory, const std::string& textureKey) {
    if (!ImGui::BeginPopupContextItem()) {
      return;
    }

    if (ImGui::MenuItem("Open")) {
      if (isDirectory) {
        pendingDir_ = path;
      }
      else {
        OpenWithShell(path);
      }
    }
    if (ImGui::MenuItem("Show in Explorer")) {
      ShowInExplorer(path);
    }

    ImGui::Separator();
    if (ImGui::MenuItem("Copy Path")) {
      ImGui::SetClipboardText(ToUtf8(path).c_str());
    }
    if (!textureKey.empty() && ImGui::MenuItem("Copy Texture Key")) {
      ImGui::SetClipboardText(textureKey.c_str());
    }

    ImGui::Separator();
    const bool isEngineCopy = IsEngineCopy(path);
    // ルート（親を持たない）は名前変更・削除させない
    const bool canModify = !isEngineCopy && path.has_parent_path();
    if (isDirectory && ImGui::MenuItem("New Folder", nullptr, false, !isEngineCopy)) {
      CreateFolder(path);
    }
    if (ImGui::MenuItem("Rename", nullptr, false, canModify)) {
      RequestRename(path);
    }
    if (ImGui::MenuItem("Delete", nullptr, false, canModify)) {
      deleteTarget_    = path;
      openDeletePopup_ = true;
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

    if (ImGui::MenuItem("New Folder", nullptr, false, !IsEngineCopy(currentDir_))) {
      CreateFolder(currentDir_);
    }
    if (ImGui::MenuItem("Show in Explorer")) {
      OpenWithShell(currentDir_);
    }
    if (ImGui::MenuItem("Copy Path")) {
      ImGui::SetClipboardText(ToUtf8(currentDir_).c_str());
    }
    ImGui::Separator();
    if (ImGui::MenuItem("Refresh")) {
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
      ImGui::TextUnformatted("Move to Recycle Bin?");
      ImGui::TextDisabled("%s", ToUtf8(deleteTarget_).c_str());
      if (ImGui::Button("Delete")) {
        if (MoveToRecycleBin(deleteTarget_)) {
          Log(std::format("Assets: Moved {} to Recycle Bin", ToUtf8(deleteTarget_)));
        }
        else {
          Log(std::format("Assets: Failed to delete {}", ToUtf8(deleteTarget_)), LogType::Error);
        }
        needsRefresh_ = true;
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
      Log(std::format("Assets: Invalid name \"{}\"", newName), LogType::Warning);
      return;
    }

    const fs::path  destination = renameTarget_.parent_path() / FromUtf8(newName);
    std::error_code ec;
    // 大文字小文字だけの変更は同じ実体を指すので許可する
    if (fs::exists(destination, ec) && !fs::equivalent(renameTarget_, destination, ec)) {
      Log(std::format("Assets: {} already exists", ToUtf8(destination)), LogType::Warning);
      return;
    }
    fs::rename(renameTarget_, destination, ec);
    if (ec) {
      Log(std::format("Assets: Failed to rename {} (error {})", ToUtf8(renameTarget_), ec.value()), LogType::Error);
      return;
    }

    if (IsSameOrAncestor(renameTarget_, selectedPath_)) {
      selectedPath_ = Rebase(selectedPath_, renameTarget_, destination);
    }
    if (IsSameOrAncestor(renameTarget_, currentDir_)) {
      SetCurrentDir(Rebase(currentDir_, renameTarget_, destination));
    }
    needsRefresh_ = true;
  }

  void AssetBrowser::CreateFolder(const std::filesystem::path& parent) {
    const fs::path  folder = MakeUniquePath(parent / "New Folder");
    std::error_code ec;
    if (!fs::create_directory(folder, ec)) {
      Log(std::format("Assets: Failed to create {} (error {})", ToUtf8(folder), ec.value()), LogType::Error);
      return;
    }
    needsRefresh_ = true;
    RequestRename(folder);
  }

} // namespace Tako

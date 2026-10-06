#include "AssetBrowser.h"
#include "TextureManager.h"
#include "EnginePaths.h"
#include "StringUtility.h"

#include "imgui_internal.h"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <string_view>
#include <system_error>

namespace Tako {

  namespace {
    namespace fs = std::filesystem;

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

    std::string ToUtf8(const fs::path& path) {
      return StringUtility::ConvertString(path.generic_wstring());
    }

    bool LessIgnoreCase(const std::string& a, const std::string& b) {
      return _stricmp(a.c_str(), b.c_str()) < 0;
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
  }

  void AssetBrowser::Draw() {
    if (!*isOpen_) {
      return;
    }

    if (needsRefresh_) {
      Refresh();
    }
    if (!pendingDir_.empty()) {
      currentDir_ = std::move(pendingDir_);
      pendingDir_.clear();
      ScanEntries();
    }

    if (ImGui::Begin("Assets", isOpen_)) {
      DrawToolbar();

      ImGui::BeginChild("##Tree", ImVec2(kTreeInitialWidth, 0.0f), ImGuiChildFlags_Borders | ImGuiChildFlags_ResizeX);
      for (const Folder& root : roots_) {
        ImGui::SetNextItemOpen(true, ImGuiCond_FirstUseEver);
        DrawFolderTree(root);
      }
      ImGui::EndChild();

      ImGui::SameLine();

      ImGui::BeginGroup();
      // 下段の Size スライダー分の高さを残す
      const float footerHeight = ImGui::GetFrameHeightWithSpacing();
      ImGui::BeginChild("##Grid", ImVec2(0.0f, -footerHeight));
      DrawGrid();
      ImGui::EndChild();
      ImGui::SetNextItemWidth(ImGui::GetFontSize() * 10.0f);
      ImGui::SliderFloat("Size", &thumbnailSize_, kMinThumbnailSize, kMaxThumbnailSize, "%.0f");
      ImGui::EndGroup();
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

    roots_.clear();
    std::error_code ec;
    for (const fs::path& root : { fs::path(kGameRoot), fs::path(EnginePaths::kEngineRoot).parent_path() }) {
      if (fs::is_directory(root, ec)) {
        roots_.push_back(BuildFolder(root));
      }
    }

    if (!roots_.empty() && !fs::is_directory(currentDir_, ec)) {
      currentDir_ = roots_.front().path;
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
    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick | ImGuiTreeNodeFlags_SpanAvailWidth;
    if (folder.children.empty()) {
      flags |= ImGuiTreeNodeFlags_Leaf;
    }
    if (folder.path == currentDir_) {
      flags |= ImGuiTreeNodeFlags_Selected;
    }

    const bool isNodeOpen = ImGui::TreeNodeEx(folder.name.c_str(), flags);
    if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen()) {
      pendingDir_ = folder.path;
    }
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
      if (entry.isDirectory && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
        pendingDir_ = entry.path;
      }
    }
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

} // namespace Tako

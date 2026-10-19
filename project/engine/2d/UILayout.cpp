#include "UILayout.h"
#include "Sprite.h"
#include "TextureManager.h"
#include "WinApp.h"
#include "Logger.h"
#include "StringUtility.h"
#ifdef _DEBUG
#include "DebugUIManager.h"
#endif

#include <json.hpp>

#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <filesystem>
#include <format>
#include <fstream>
#include <iterator>
#include <map>
#include <numbers>
#include <type_traits>
#include <utility>

namespace Tako {

  using json = nlohmann::json;

  // ADL で見つかるよう型と同じ名前空間に置き、static で他翻訳単位と衝突させない
  static void to_json(json& j, const Vector2& v) { j = { v.x, v.y }; }
  static void from_json(const json& j, Vector2& v) { v = { j.at(0).get<float>(), j.at(1).get<float>() }; }
  static void to_json(json& j, const Vector4& v) { j = { v.x, v.y, v.z, v.w }; }
  static void from_json(const json& j, Vector4& v) { v = { j.at(0).get<float>(), j.at(1).get<float>(), j.at(2).get<float>(), j.at(3).get<float>() }; }
  static void to_json(json& j, const UISlice& s) { j = { { "border", s.border }, { "visible", s.regionVisible }, { "textures", s.regionTextures } }; }
  static void from_json(const json& j, UISlice& s) {
    const UISlice defaults;
    s.border         = j.value("border", defaults.border);
    s.regionVisible  = j.value("visible", defaults.regionVisible);
    s.regionTextures = j.value("textures", defaults.regionTextures);
  }

  static_assert(UISlice::kRegionCount == Sprite::kRegionCount);

  namespace {

    constexpr float kDegToRad = std::numbers::pi_v<float> / 180.0f;

    const UISlice kNoSlice;  ///< 枠なし。9-slice を使わない要素と、共有の登録が無いテクスチャに返す

    Vector2 Mul(const Vector2& a, const Vector2& b) { return { a.x * b.x, a.y * b.y }; }

    std::string FilePath(const std::string& name) { return std::string(UILayout::kDirectory) + name + ".json"; }

    void LogWarning(const std::string& message) {
      Logger::Log(message + "\n");
#ifdef _DEBUG
      DebugUIManager::GetInstance()->AddLog(message, DebugUIManager::LogType::Warning);
#endif
    }

#ifdef _DEBUG
    std::vector<UILayout*>& Instances() {
      static std::vector<UILayout*> instances;
      return instances;
    }
#endif

    /// <summary>
    /// テクスチャキー → 共有の 9-slice。最初の参照時にファイルから読む。std::map なので保存順が安定する
    /// </summary>
    std::map<std::string, UISlice>& TextureSlices() {
      static std::map<std::string, UISlice> slices;
      static bool                           isLoaded = false;
      if (!isLoaded) {
        isLoaded = true;
        UILayout::LoadTextureSlices();
      }
      return slices;
    }

    /// <summary>
    /// 未読み込みなら読み込む。JSON の誤記で LoadTexture の assert に落ちないよう、先にファイルの有無を確かめる
    /// </summary>
    /// <returns>読み込み済みになれば true。空のキーやファイルが無い場合は false</returns>
    bool EnsureTextureLoaded(const std::string& texture) {
      if (texture.empty()) {
        return false;
      }
      TextureManager* textureManager = TextureManager::GetInstance();
      if (textureManager->IsLoaded(texture)) {
        return true;
      }
      const std::string path = textureManager->GetFilePath(texture);
      if (!std::filesystem::exists(StringUtility::ConvertString(path))) {
        LogWarning(std::format("UILayout: texture not found: {}", path));
        return false;
      }
      textureManager->LoadTexture(texture);
      return true;
    }

    /// <summary>
    /// 親矩形上のアンカー範囲（左上と幅）を求める
    /// </summary>
    void AnchorRange(const UIElement& element, Vector2& anchorMin, Vector2& anchorSpan) {
      const UIElement& parent = *element.parent;
      anchorMin  = parent.rectMin + Mul(parent.rectSize, element.anchorMin);
      anchorSpan = Mul(parent.rectSize, element.anchorMax - element.anchorMin);
    }

    /// <summary>
    /// スプライトを描く大きさ。preserveAspect なら切り出し範囲の縦横比を保って矩形に収める（pivot の位置は矩形と同じ）
    /// </summary>
    Vector2 DrawSize(const UIElement& element) {
      const Vector2& rect = element.rectSize;
      const Vector2& uv   = element.uvSize;
      if (!element.preserveAspect || element.isSliced || uv.x <= 0.0f || uv.y <= 0.0f) {
        return rect;
      }
      return uv * (std::min)(rect.x / uv.x, rect.y / uv.y);
    }

    std::vector<std::unique_ptr<UIElement>>::iterator FindChild(UIElement& parent, const UIElement& element) {
      return std::ranges::find_if(parent.children, [&](const std::unique_ptr<UIElement>& child) { return child.get() == &element; });
    }

    const UIElement* FindIn(const UIElement& node, const std::string& name) {
      for (const std::unique_ptr<UIElement>& child : node.children) {
        if (child->name == name) {
          return child.get();
        }
        if (const UIElement* found = FindIn(*child, name)) {
          return found;
        }
      }
      return nullptr;
    }

    /// <summary>
    /// node の子孫（node 自身は除く）に name が何個あるか
    /// </summary>
    int CountName(const UIElement& node, const std::string& name) {
      int count = 0;
      for (const std::unique_ptr<UIElement>& child : node.children) {
        count += (child->name == name ? 1 : 0) + CountName(*child, name);
      }
      return count;
    }

    void CollectSubtree(UIElement& node, std::vector<UIElement*>& out) {
      out.push_back(&node);
      for (std::unique_ptr<UIElement>& child : node.children) {
        CollectSubtree(*child, out);
      }
    }

    void DrawElement(const UIElement& element) {
      if (!element.visible) {
        return;
      }
      if (element.sprite) {
        element.sprite->Draw();
      }
      for (const std::unique_ptr<UIElement>& child : element.children) {
        DrawElement(*child);
      }
    }

    /// <summary>
    /// 保存する項目（子は除く）を field(キー, 値) へ順に渡す。WriteElement と HashElement が同じ項目を見るためのもの
    /// </summary>
    template <class Field>
    void VisitSavedFields(const UIElement& element, Field&& field) {
      field("name", element.name);
      field("texture", element.texture);
      field("anchorMin", element.anchorMin);
      field("anchorMax", element.anchorMax);
      field("pivot", element.pivot);
      field("position", element.position);
      field("size", element.size);
      field("rotation", element.rotation);
      field("color", element.color);
      field("flip", std::array<bool, 2>{ element.flipX, element.flipY });
      field("uvLeftTop", element.uvLeftTop);
      field("uvSize", element.uvSize);
      field("sliced", element.isSliced);
      field("preserveAspect", element.preserveAspect);
      field("sliceOverride", element.isSliceOverridden);
      field("visible", element.visible);
      field("locked", element.locked);
      // 共有の 9-slice を使う要素には書かない（共有側の値と紛れないように）
      if (element.isSliceOverridden) {
        field("slice", element.slice);
      }
    }

    json WriteElement(const UIElement& element) {
      json node;
      VisitSavedFields(element, [&node](const char* key, const auto& value) { node[key] = value; });

      json& children = node["children"] = json::array();
      for (const std::unique_ptr<UIElement>& child : element.children) {
        children.push_back(WriteElement(*child));
      }
      return node;
    }

    // ハッシュは FNV-1a（64 bit）
    constexpr uint64_t kHashBasis = 14695981039346656037ull;
    constexpr uint64_t kHashPrime = 1099511628211ull;

    void HashBytes(uint64_t& hash, const void* data, size_t size) {
      const auto* bytes = static_cast<const unsigned char*>(data);
      for (size_t i = 0; i < size; ++i) {
        hash = (hash ^ bytes[i]) * kHashPrime;
      }
    }

    // float・bool・Vector2 など詰め物の無い型はバイト列のまま混ぜる
    template <class T>
      requires std::is_trivially_copyable_v<T>
    void HashValue(uint64_t& hash, const T& value) {
      HashBytes(hash, &value, sizeof(value));
    }

    // 長さも混ぜ、隣の文字列との区切りを変えた内容を区別する
    void HashValue(uint64_t& hash, const std::string& value) {
      HashValue(hash, value.size());
      HashBytes(hash, value.data(), value.size());
    }

    void HashValue(uint64_t& hash, const UISlice& slice) {
      HashValue(hash, slice.border);
      HashValue(hash, slice.regionVisible);
      for (const std::string& texture : slice.regionTextures) {
        HashValue(hash, texture);
      }
    }

    void HashElement(uint64_t& hash, const UIElement& element) {
      VisitSavedFields(element, [&hash](const char*, const auto& value) { HashValue(hash, value); });
      HashValue(hash, element.children.size());
      for (const std::unique_ptr<UIElement>& child : element.children) {
        HashElement(hash, *child);
      }
    }

    /// <summary>
    /// node を parent の末尾の子として読み込む（子孫も再帰的に読む）。欠けたキーは UIElement の既定値になる
    /// </summary>
    void ReadElement(UILayout& layout, const json& node, UIElement& parent) {
      UIElement& element = *parent.children.emplace_back(std::make_unique<UIElement>());
      element.parent            = &parent;
      element.name              = node.value("name", std::string("Element"));
      element.anchorMin         = node.value("anchorMin", element.anchorMin);
      element.anchorMax         = node.value("anchorMax", element.anchorMax);
      element.pivot             = node.value("pivot", element.pivot);
      element.position          = node.value("position", element.position);
      element.size              = node.value("size", element.size);
      element.rotation          = node.value("rotation", element.rotation);
      element.color             = node.value("color", element.color);
      element.isSliced          = node.value("sliced", element.isSliced);
      element.preserveAspect    = node.value("preserveAspect", element.preserveAspect);
      element.isSliceOverridden = node.value("sliceOverride", element.isSliceOverridden);
      element.slice             = node.value("slice", element.slice);
      element.visible           = node.value("visible", element.visible);
      element.locked            = node.value("locked", element.locked);

      const std::array<bool, 2> flip = node.value("flip", std::array<bool, 2>{});
      element.flipX = flip[0];
      element.flipY = flip[1];

      // uvSize が無ければ SetTexture が設定したテクスチャ全体のまま使う
      layout.SetTexture(element, node.value("texture", std::string()));
      if (node.contains("uvSize")) {
        element.uvLeftTop = node.value("uvLeftTop", element.uvLeftTop);
        element.uvSize    = node.value("uvSize", element.uvSize);
      }

      if (node.contains("children")) {
        for (const json& child : node["children"]) {
          ReadElement(layout, child, element);
        }
      }
    }

  } // namespace

  UIElement::UIElement() = default;
  UIElement::~UIElement() = default;

  UILayout::UILayout() {
#ifdef _DEBUG
    Instances().push_back(this);
#endif
  }

  UILayout::~UILayout() {
#ifdef _DEBUG
    std::erase(Instances(), this);
#endif
  }

#ifdef _DEBUG
  const std::vector<UILayout*>& UILayout::GetInstances() {
    return Instances();
  }
#endif

  void UILayout::Create(const std::string& name) {
    name_ = name;
    root_.children.clear();
    referenceResolution_ = kDefaultReferenceResolution;
    Update();
  }

  void UILayout::Load(const std::string& name) {
    Create(name);

    const std::string path = FilePath(name);
    std::ifstream     ifs(path);
    if (!ifs) {
      LogWarning(std::format("UILayout: {} not found. Starting with an empty layout.", path));
      return;
    }
    Deserialize(std::string(std::istreambuf_iterator<char>(ifs), {}));
  }

  void UILayout::Deserialize(const std::string& text) {
    root_.children.clear();
    referenceResolution_ = kDefaultReferenceResolution;

    try {
      const json root = json::parse(text);
      referenceResolution_ = root.value("referenceResolution", kDefaultReferenceResolution);
      if (referenceResolution_.x <= 0.0f || referenceResolution_.y <= 0.0f) {
        LogWarning(std::format("UILayout '{}': invalid referenceResolution. Using default.", name_));
        referenceResolution_ = kDefaultReferenceResolution;
      }
      if (root.contains("elements")) {
        for (const json& node : root["elements"]) {
          ReadElement(*this, node, root_);
        }
      }
    }
    catch (const json::exception& e) {
      LogWarning(std::format("UILayout '{}': failed to parse: {}", name_, e.what()));
      root_.children.clear();
    }

    // 読み込み直後の Draw やエディタの当たり判定で矩形が使えるようにする
    Update();
  }

  bool UILayout::Save() const {
    std::error_code ec;
    std::filesystem::create_directories(kDirectory, ec);

    const std::string path = FilePath(name_);
    std::ofstream     ofs(path);
    if (!ofs) {
      LogWarning(std::format("UILayout: cannot open {} for writing.", path));
      return false;
    }
    ofs << Serialize();
    return static_cast<bool>(ofs);
  }

  void UILayout::LoadTextureSlices() {
    // ファイルが無いのは共有の 9-slice をまだ作っていないだけなので警告しない
    std::ifstream ifs(kTextureSlicePath);
    if (!ifs) {
      TextureSlices().clear();
      return;
    }
    DeserializeTextureSlices(std::string(std::istreambuf_iterator<char>(ifs), {}));
  }

  void UILayout::DeserializeTextureSlices(const std::string& text) {
    std::map<std::string, UISlice>& slices = TextureSlices();
    slices.clear();
    try {
      const json root = json::parse(text);
      for (const auto& [texture, node] : root.items()) {
        slices[texture] = node.get<UISlice>();
      }
    }
    catch (const json::exception& e) {
      LogWarning(std::format("UILayout: failed to parse the texture 9-slice settings: {}", e.what()));
      slices.clear();
    }
  }

  bool UILayout::SaveTextureSlices() {
    std::ofstream ofs(kTextureSlicePath);
    if (!ofs) {
      LogWarning(std::format("UILayout: cannot open {} for writing.", kTextureSlicePath));
      return false;
    }
    ofs << SerializeTextureSlices();
    return static_cast<bool>(ofs);
  }

  std::string UILayout::SerializeTextureSlices() {
    json root = json::object();
    for (const auto& [texture, slice] : TextureSlices()) {
      if (slice != UISlice{}) {
        root[texture] = slice;
      }
    }
    return root.dump(2);
  }

  uint64_t UILayout::HashTextureSlices() {
    uint64_t hash = kHashBasis;
    for (const auto& [texture, slice] : TextureSlices()) {
      if (slice != kNoSlice) {
        HashValue(hash, texture);
        HashValue(hash, slice);
      }
    }
    return hash;
  }

  void UILayout::SetTextureSlice(const std::string& texture, const UISlice& slice) {
    if (!texture.empty()) {
      TextureSlices()[texture] = slice;
    }
  }

  const UISlice& UILayout::GetTextureSlice(const std::string& texture) {
    const std::map<std::string, UISlice>& slices = TextureSlices();
    const auto it = slices.find(texture);
    return it != slices.end() ? it->second : kNoSlice;
  }

  const UISlice& UILayout::GetEffectiveSlice(const UIElement& element) {
    if (!element.isSliced) {
      return kNoSlice;
    }
    return element.isSliceOverridden ? element.slice : GetTextureSlice(element.texture);
  }

  std::string UILayout::Serialize() const {
    json root;
    root["referenceResolution"] = referenceResolution_;
    json& elements = root["elements"] = json::array();
    for (const std::unique_ptr<UIElement>& child : root_.children) {
      elements.push_back(WriteElement(*child));
    }
    return root.dump(2);
  }

  uint64_t UILayout::Hash() const {
    uint64_t hash = kHashBasis;
    HashValue(hash, referenceResolution_);
    HashValue(hash, root_.children.size());
    for (const std::unique_ptr<UIElement>& child : root_.children) {
      HashElement(hash, *child);
    }
    return hash;
  }

  void UILayout::Update() {
    const Vector2 client = { static_cast<float>(WinApp::clientWidth), static_cast<float>(WinApp::clientHeight) };

    // 基準解像度の画面が収まる倍率（Unity CanvasScaler の Expand と同じ）
    scale_ = (std::min)(client.x / referenceResolution_.x, client.y / referenceResolution_.y);

    root_.rectMin  = {};
    root_.rectSize = client;
    for (std::unique_ptr<UIElement>& child : root_.children) {
      UpdateElement(*child);
    }
  }

  void UILayout::Draw() {
    for (const std::unique_ptr<UIElement>& child : root_.children) {
      DrawElement(*child);
    }
  }

  void UILayout::Draw(const std::string& name) {
    if (const UIElement* element = Find(name)) {
      DrawElement(*element);
    }
  }

  UIElement* UILayout::Find(const std::string& name) {
    // 探索は const 版に任せる。this が非 const なので、結果の const を外してよい
    return const_cast<UIElement*>(std::as_const(*this).Find(name));
  }

  const UIElement* UILayout::Find(const std::string& name) const {
    // ponytail: 毎回 DFS。数百要素を超えるレイアウトが出てきたら名前→要素のマップを持つ
    return FindIn(root_, name);
  }

  void UILayout::SetVisible(const std::string& name, bool visible) {
    if (UIElement* element = Find(name)) {
      element->visible = visible;
    }
  }

  void UILayout::SetTexture(const std::string& name, const std::string& texture) {
    if (UIElement* element = Find(name)) {
      SetTexture(*element, texture);
    }
  }

  void UILayout::SetPosition(const std::string& name, const Vector2& position) {
    if (UIElement* element = Find(name)) {
      element->position = position;
    }
  }

  void UILayout::SetSize(const std::string& name, const Vector2& size) {
    if (UIElement* element = Find(name)) {
      element->size = size;
    }
  }

  void UILayout::SetRotation(const std::string& name, float rotation) {
    if (UIElement* element = Find(name)) {
      element->rotation = rotation;
    }
  }

  void UILayout::SetColor(const std::string& name, const Vector4& color) {
    if (UIElement* element = Find(name)) {
      element->color = color;
    }
  }

  void UILayout::SetAlpha(const std::string& name, float alpha) {
    if (UIElement* element = Find(name)) {
      element->color.w = alpha;
    }
  }

  void UILayout::SetAnchor(const std::string& name, const Vector2& anchorMin, const Vector2& anchorMax) {
    if (UIElement* element = Find(name)) {
      element->anchorMin = anchorMin;
      element->anchorMax = anchorMax;
    }
  }

  void UILayout::SetAnchor(const std::string& name, const Vector2& anchor) {
    SetAnchor(name, anchor, anchor);
  }

  void UILayout::SetPivot(const std::string& name, const Vector2& pivot) {
    if (UIElement* element = Find(name)) {
      element->pivot = pivot;
    }
  }

  void UILayout::SetUV(const std::string& name, const Vector2& leftTop, const Vector2& size) {
    if (UIElement* element = Find(name)) {
      element->uvLeftTop = leftTop;
      element->uvSize    = size;
    }
  }

  void UILayout::SetSliceBorder(const std::string& name, const Vector4& sliceBorder) {
    if (UIElement* element = Find(name)) {
      element->slice.border      = sliceBorder;
      element->isSliced          = true;
      element->isSliceOverridden = true;
    }
  }

  void UILayout::SetFlip(const std::string& name, bool flipX, bool flipY) {
    if (UIElement* element = Find(name)) {
      element->flipX = flipX;
      element->flipY = flipY;
    }
  }

  bool UILayout::IsVisible(const std::string& name) const {
    const UIElement* element = Find(name);
    return element ? element->visible : false;
  }

  std::string UILayout::GetTexture(const std::string& name) const {
    const UIElement* element = Find(name);
    return element ? element->texture : std::string();
  }

  Vector2 UILayout::GetPosition(const std::string& name) const {
    const UIElement* element = Find(name);
    return element ? element->position : Vector2{};
  }

  Vector2 UILayout::GetSize(const std::string& name) const {
    const UIElement* element = Find(name);
    return element ? element->size : Vector2{};
  }

  float UILayout::GetRotation(const std::string& name) const {
    const UIElement* element = Find(name);
    return element ? element->rotation : 0.0f;
  }

  Vector4 UILayout::GetColor(const std::string& name) const {
    const UIElement* element = Find(name);
    return element ? element->color : Vector4{};
  }

  float UILayout::GetAlpha(const std::string& name) const {
    const UIElement* element = Find(name);
    return element ? element->color.w : 0.0f;
  }

  Vector2 UILayout::GetAnchorMin(const std::string& name) const {
    const UIElement* element = Find(name);
    return element ? element->anchorMin : Vector2{};
  }

  Vector2 UILayout::GetAnchorMax(const std::string& name) const {
    const UIElement* element = Find(name);
    return element ? element->anchorMax : Vector2{};
  }

  Vector2 UILayout::GetPivot(const std::string& name) const {
    const UIElement* element = Find(name);
    return element ? element->pivot : Vector2{};
  }

  Vector2 UILayout::GetUVLeftTop(const std::string& name) const {
    const UIElement* element = Find(name);
    return element ? element->uvLeftTop : Vector2{};
  }

  Vector2 UILayout::GetUVSize(const std::string& name) const {
    const UIElement* element = Find(name);
    return element ? element->uvSize : Vector2{};
  }

  Vector4 UILayout::GetSliceBorder(const std::string& name) const {
    const UIElement* element = Find(name);
    return element ? GetEffectiveSlice(*element).border : Vector4{};
  }

  bool UILayout::IsFlipX(const std::string& name) const {
    const UIElement* element = Find(name);
    return element ? element->flipX : false;
  }

  bool UILayout::IsFlipY(const std::string& name) const {
    const UIElement* element = Find(name);
    return element ? element->flipY : false;
  }

  Vector2 UILayout::GetScreenRectMin(const std::string& name) const {
    const UIElement* element = Find(name);
    return element ? element->rectMin : Vector2{};
  }

  Vector2 UILayout::GetScreenRectSize(const std::string& name) const {
    const UIElement* element = Find(name);
    return element ? element->rectSize : Vector2{};
  }

  void UILayout::SetScreenRect(UIElement& element, const Vector2& rectMin, const Vector2& rectSize) const {
    assert(element.parent && "UILayout::SetScreenRect: root has no parent rect");
    if (!element.parent || scale_ <= 0.0f) {
      return;
    }

    Vector2 anchorMin{};
    Vector2 anchorSpan{};
    AnchorRange(element, anchorMin, anchorSpan);
    element.size     = (rectSize - anchorSpan) / scale_;
    element.position = (rectMin + Mul(rectSize, element.pivot) - anchorMin - Mul(anchorSpan, element.pivot)) / scale_;

    ComputeRect(element);

#ifdef _DEBUG
    // 逆算の検算: 順方向に計算し直して指定の矩形へ戻ること
    constexpr float kTolerance = 0.01f;
    assert(std::abs(element.rectMin.x - rectMin.x) < kTolerance && std::abs(element.rectMin.y - rectMin.y) < kTolerance);
    assert(std::abs(element.rectSize.x - rectSize.x) < kTolerance && std::abs(element.rectSize.y - rectSize.y) < kTolerance);
#endif
  }

  void UILayout::SetTexture(UIElement& element, const std::string& texture) {
    element.texture   = texture;
    element.uvLeftTop = {};
    element.uvSize    = {};
    // 読み込めなくてもキーは残し、保存で消えないようにする
    if (!EnsureTextureLoaded(texture)) {
      element.sprite.reset();
      return;
    }

    if (element.sprite) {
      element.sprite->SetTexture(texture);
    }
    else {
      element.sprite = std::make_unique<Sprite>();
      element.sprite->Initialize(texture);
    }
    element.uvSize = element.sprite->GetTexCutSize();
  }

  UIElement* UILayout::AddChild(UIElement& parent, const std::string& name) {
    std::string uniqueName = MakeUniqueName(name);

    UIElement& element = *parent.children.emplace_back(std::make_unique<UIElement>());
    element.parent = &parent;
    element.name   = std::move(uniqueName);
    ComputeRect(element);
    return &element;
  }

  UIElement* UILayout::Duplicate(const UIElement& element) {
    assert(element.parent && "UILayout::Duplicate: root cannot be duplicated");
    if (!element.parent) {
      return nullptr;
    }
    UIElement&   parent = *element.parent;
    const size_t index  = static_cast<size_t>(FindChild(parent, element) - parent.children.begin());
    return InsertElements(parent, SerializeElements({ &element }), index + 1).front();
  }

  std::string UILayout::SerializeElements(const std::vector<const UIElement*>& elements) {
    json nodes = json::array();
    for (const UIElement* element : elements) {
      nodes.push_back(WriteElement(*element));
    }
    return nodes.dump();
  }

  std::vector<UIElement*> UILayout::InsertElements(UIElement& parent, const std::string& serialized, size_t index) {
    // 末尾に読み込んでから index の位置へ移す
    std::vector<UIElement*> inserted;
    index = (std::min)(index, parent.children.size());
    for (const json& node : json::parse(serialized)) {
      ReadElement(*this, node, parent);
      std::unique_ptr<UIElement> element = std::move(parent.children.back());
      parent.children.pop_back();
      inserted.push_back(element.get());
      parent.children.insert(parent.children.begin() + static_cast<std::ptrdiff_t>(index++), std::move(element));
    }

    // 前順に見て、既にある名前と重なった（出現数 2 以上）ものだけ改名する
    for (UIElement* element : inserted) {
      std::vector<UIElement*> nodes;
      CollectSubtree(*element, nodes);
      for (UIElement* node : nodes) {
        if (CountName(root_, node->name) > 1) {
          node->name = MakeUniqueName(node->name);
        }
      }
      UpdateElement(*element);
    }
    return inserted;
  }

  void UILayout::Remove(UIElement& element) {
    assert(element.parent && "UILayout::Remove: root cannot be removed");
    if (!element.parent) {
      return;
    }
    UIElement& parent = *element.parent;
    auto       it     = FindChild(parent, element);
    assert(it != parent.children.end());
    if (it != parent.children.end()) {
      parent.children.erase(it);
    }
  }

  bool UILayout::MoveTo(UIElement& element, UIElement& newParent, size_t index) {
    assert(element.parent && "UILayout::MoveTo: root cannot be moved");
    if (!element.parent) {
      return false;
    }
    for (const UIElement* node = &newParent; node; node = node->parent) {
      if (node == &element) {
        return false;
      }
    }

    const Vector2 rectMin  = element.rectMin;
    const Vector2 rectSize = element.rectSize;

    UIElement& oldParent = *element.parent;
    auto       it        = FindChild(oldParent, element);
    std::unique_ptr<UIElement> moving = std::move(*it);
    oldParent.children.erase(it);

    index = (std::min)(index, newParent.children.size());
    newParent.children.insert(newParent.children.begin() + static_cast<std::ptrdiff_t>(index), std::move(moving));
    element.parent = &newParent;

    // 親が変わっても見た目の位置とサイズを保つ
    SetScreenRect(element, rectMin, rectSize);
    return true;
  }

  std::string UILayout::MakeUniqueName(const std::string& base) {
    if (!Find(base)) {
      return base;
    }
    for (int number = 1;; ++number) {
      std::string candidate = std::format("{}_{}", base, number);
      if (!Find(candidate)) {
        return candidate;
      }
    }
  }

  void UILayout::UpdateElement(UIElement& element) {
    ComputeRect(element);

    if (element.sprite) {
      Sprite& sprite = *element.sprite;

      // Sprite の反転はアンカーポイント中心の鏡映なので、反転した軸はアンカーを 1-pivot にして矩形を保つ
      const Vector2 spriteAnchor = {
        element.flipX ? 1.0f - element.pivot.x : element.pivot.x,
        element.flipY ? 1.0f - element.pivot.y : element.pivot.y,
      };
      sprite.SetAnchorPoint(spriteAnchor);
      sprite.SetPos(element.rectMin + Mul(element.rectSize, element.pivot));
      sprite.SetSize(DrawSize(element));
      sprite.SetRotation(element.rotation * kDegToRad);
      sprite.SetColor(element.color);
      sprite.SetIsFlipX(element.flipX);
      sprite.SetIsFlipY(element.flipY);
      sprite.SetTexLeftTop(element.uvLeftTop);
      sprite.SetTexCutSize(element.uvSize);

      const UISlice& slice = GetEffectiveSlice(element);
      sprite.SetSlice(slice.border, scale_);
      for (int region = 0; region < UISlice::kRegionCount; ++region) {
        sprite.SetRegionVisible(region, slice.regionVisible[region]);
        // 読み込みはキーが変わったときだけ（無いファイルの警告を毎フレーム出さない）
        const std::string& texture = slice.regionTextures[region];
        if (texture != sprite.GetRegionTexture(region)) {
          EnsureTextureLoaded(texture);
          sprite.SetRegionTexture(region, texture);
        }
      }
      sprite.Update();
    }

    for (std::unique_ptr<UIElement>& child : element.children) {
      UpdateElement(*child);
    }
  }

  void UILayout::ComputeRect(UIElement& element) const {
    Vector2 anchorMin{};
    Vector2 anchorSpan{};
    AnchorRange(element, anchorMin, anchorSpan);

    element.rectSize = anchorSpan + element.size * scale_;
    const Vector2 pivotPos = anchorMin + Mul(anchorSpan, element.pivot) + element.position * scale_;
    element.rectMin = pivotPos - Mul(element.rectSize, element.pivot);
  }

} // namespace Tako

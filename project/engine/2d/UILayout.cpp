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
#include <numbers>

namespace Tako {

  using json = nlohmann::json;

  // ADL で見つかるよう型と同じ名前空間に置き、static で他翻訳単位と衝突させない
  static void to_json(json& j, const Vector2& v) { j = { v.x, v.y }; }
  static void from_json(const json& j, Vector2& v) { v = { j.at(0).get<float>(), j.at(1).get<float>() }; }
  static void to_json(json& j, const Vector4& v) { j = { v.x, v.y, v.z, v.w }; }
  static void from_json(const json& j, Vector4& v) { v = { j.at(0).get<float>(), j.at(1).get<float>(), j.at(2).get<float>(), j.at(3).get<float>() }; }

  namespace {

    constexpr float kDegToRad = std::numbers::pi_v<float> / 180.0f;

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
    /// 親矩形上のアンカー範囲（左上と幅）を求める
    /// </summary>
    void AnchorRange(const UIElement& element, Vector2& anchorMin, Vector2& anchorSpan) {
      const UIElement& parent = *element.parent;
      anchorMin  = parent.rectMin + Mul(parent.rectSize, element.anchorMin);
      anchorSpan = Mul(parent.rectSize, element.anchorMax - element.anchorMin);
    }

    std::vector<std::unique_ptr<UIElement>>::iterator FindChild(UIElement& parent, const UIElement& element) {
      return std::ranges::find_if(parent.children, [&](const std::unique_ptr<UIElement>& child) { return child.get() == &element; });
    }

    UIElement* FindIn(UIElement& node, const std::string& name) {
      for (std::unique_ptr<UIElement>& child : node.children) {
        if (child->name == name) {
          return child.get();
        }
        if (UIElement* found = FindIn(*child, name)) {
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

    json WriteElement(const UIElement& element) {
      json node;
      node["name"]        = element.name;
      node["texture"]     = element.texture;
      node["anchorMin"]   = element.anchorMin;
      node["anchorMax"]   = element.anchorMax;
      node["pivot"]       = element.pivot;
      node["position"]    = element.position;
      node["size"]        = element.size;
      node["rotation"]    = element.rotation;
      node["color"]       = element.color;
      node["flip"]        = { element.flipX, element.flipY };
      node["uvLeftTop"]   = element.uvLeftTop;
      node["uvSize"]      = element.uvSize;
      node["sliceBorder"] = element.sliceBorder;
      node["visible"]     = element.visible;
      node["locked"]      = element.locked;

      json& children = node["children"] = json::array();
      for (const std::unique_ptr<UIElement>& child : element.children) {
        children.push_back(WriteElement(*child));
      }
      return node;
    }

    /// <summary>
    /// node を parent の末尾の子として読み込む（子孫も再帰的に読む）。欠けたキーは UIElement の既定値になる
    /// </summary>
    void ReadElement(UILayout& layout, const json& node, UIElement& parent) {
      UIElement& element = *parent.children.emplace_back(std::make_unique<UIElement>());
      element.parent      = &parent;
      element.name        = node.value("name", std::string("Element"));
      element.anchorMin   = node.value("anchorMin", element.anchorMin);
      element.anchorMax   = node.value("anchorMax", element.anchorMax);
      element.pivot       = node.value("pivot", element.pivot);
      element.position    = node.value("position", element.position);
      element.size        = node.value("size", element.size);
      element.rotation    = node.value("rotation", element.rotation);
      element.color       = node.value("color", element.color);
      element.sliceBorder = node.value("sliceBorder", element.sliceBorder);
      element.visible     = node.value("visible", element.visible);
      element.locked      = node.value("locked", element.locked);

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

    try {
      const json root = json::parse(ifs);
      referenceResolution_ = root.value("referenceResolution", kDefaultReferenceResolution);
      if (referenceResolution_.x <= 0.0f || referenceResolution_.y <= 0.0f) {
        LogWarning(std::format("UILayout: invalid referenceResolution in {}. Using default.", path));
        referenceResolution_ = kDefaultReferenceResolution;
      }
      if (root.contains("elements")) {
        for (const json& node : root["elements"]) {
          ReadElement(*this, node, root_);
        }
      }
    }
    catch (const json::exception& e) {
      LogWarning(std::format("UILayout: failed to parse {}: {}", path, e.what()));
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

  std::string UILayout::Serialize() const {
    json root;
    root["referenceResolution"] = referenceResolution_;
    json& elements = root["elements"] = json::array();
    for (const std::unique_ptr<UIElement>& child : root_.children) {
      elements.push_back(WriteElement(*child));
    }
    return root.dump(2);
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

  UIElement* UILayout::Find(const std::string& name) {
    // ponytail: 毎回 DFS。数百要素を超えるレイアウトが出てきたら名前→要素のマップを持つ
    return FindIn(root_, name);
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
    if (texture.empty()) {
      element.sprite.reset();
      return;
    }

    TextureManager* textureManager = TextureManager::GetInstance();
    if (!textureManager->IsLoaded(texture)) {
      // JSON の誤記で LoadTexture の assert に落ちないよう、先に存在を確かめる。キーは残して保存で消えないようにする
      const std::string path = textureManager->GetFilePath(texture);
      if (!std::filesystem::exists(StringUtility::ConvertString(path))) {
        LogWarning(std::format("UILayout: texture not found: {}", path));
        element.sprite.reset();
        return;
      }
      textureManager->LoadTexture(texture);
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

    // 保存形式を経由して子孫ごと複製し、元の直後へ並べ替える
    ReadElement(*this, WriteElement(element), parent);
    std::unique_ptr<UIElement> copy = std::move(parent.children.back());
    parent.children.pop_back();
    UIElement* result = copy.get();
    parent.children.insert(parent.children.begin() + static_cast<std::ptrdiff_t>(index + 1), std::move(copy));

    // 前順に見て、元と同名（出現数 2 以上）のものだけ改名する
    std::vector<UIElement*> copied;
    CollectSubtree(*result, copied);
    for (UIElement* node : copied) {
      if (CountName(root_, node->name) > 1) {
        node->name = MakeUniqueName(node->name);
      }
    }

    UpdateElement(*result);
    return result;
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
      sprite.SetSize(element.rectSize);
      sprite.SetRotation(element.rotation * kDegToRad);
      sprite.SetColor(element.color);
      sprite.SetIsFlipX(element.flipX);
      sprite.SetIsFlipY(element.flipY);
      sprite.SetTexLeftTop(element.uvLeftTop);
      sprite.SetTexCutSize(element.uvSize);
      sprite.SetSlice(element.sliceBorder, scale_);
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

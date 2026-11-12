#pragma once

#ifdef _DEBUG

#include "imgui.h"
#include "imgui_internal.h"
#include "IconsFontAwesome6.h"
#include "ImGuiThemes.h"

#include <algorithm>
#include <string>
#include <utility>

// タブに出す名前。### の後ろが ID なので、アイコンを足しても imgui.ini と ImGui::SetWindowFocus の指定は name のまま
#define TAKO_WINDOW_TITLE(icon, name) icon "  " name "###" name

namespace Tako::WindowTitle {

  inline constexpr const char* kSceneHierarchy        = TAKO_WINDOW_TITLE(ICON_FA_SITEMAP, "Scene Hierarchy");
  inline constexpr const char* kInspector             = TAKO_WINDOW_TITLE(ICON_FA_CIRCLE_INFO, "Inspector");
  inline constexpr const char* kConsole               = TAKO_WINDOW_TITLE(ICON_FA_TERMINAL, "Console");
  inline constexpr const char* kPerformance           = TAKO_WINDOW_TITLE(ICON_FA_GAUGE_HIGH, "Performance");
  inline constexpr const char* kAssets                = TAKO_WINDOW_TITLE(ICON_FA_FOLDER_OPEN, "Assets");
  inline constexpr const char* kGameViewport          = TAKO_WINDOW_TITLE(ICON_FA_GAMEPAD, "Game Viewport");
  inline constexpr const char* kDebugViewport         = TAKO_WINDOW_TITLE(ICON_FA_VIDEO, "Debug Viewport");
  inline constexpr const char* kEngineSettings        = TAKO_WINDOW_TITLE(ICON_FA_GEAR, "Engine Settings");
  inline constexpr const char* kInputDebug            = TAKO_WINDOW_TITLE(ICON_FA_KEYBOARD, "Input Debug");
  inline constexpr const char* kCollisionDebug        = TAKO_WINDOW_TITLE(ICON_FA_VECTOR_SQUARE, "Collision Debug");
  inline constexpr const char* kParticleEditor        = TAKO_WINDOW_TITLE(ICON_FA_BURST, "Particle Editor");
  inline constexpr const char* kPrimitiveEditor       = TAKO_WINDOW_TITLE(ICON_FA_SHAPES, "Primitive Editor");
  inline constexpr const char* kMaterialEditor        = TAKO_WINDOW_TITLE(ICON_FA_PALETTE, "Material Editor");
  inline constexpr const char* kShaderEditor          = TAKO_WINDOW_TITLE(ICON_FA_CODE, "Shader Editor");
  inline constexpr const char* kRenderPipeline        = TAKO_WINDOW_TITLE(ICON_FA_LAYER_GROUP, "Render Pipeline");
  inline constexpr const char* kPostEffectEditor      = TAKO_WINDOW_TITLE(ICON_FA_WAND_MAGIC_SPARKLES, "PostEffect Editor");
  inline constexpr const char* kUIEditor              = TAKO_WINDOW_TITLE(ICON_FA_OBJECT_GROUP, "UI Editor");
  inline constexpr const char* kCameraAnimationEditor = TAKO_WINDOW_TITLE(ICON_FA_CLAPPERBOARD, "Camera Animation Editor");
  inline constexpr const char* kAnimationEditor       = TAKO_WINDOW_TITLE(ICON_FA_PERSON_RUNNING, "Animation Editor");
  inline constexpr const char* kBehaviorTreeEditor    = TAKO_WINDOW_TITLE(ICON_FA_DIAGRAM_PROJECT, "Behavior Tree Editor");
  inline constexpr const char* kAbout                 = TAKO_WINDOW_TITLE(ICON_FA_CIRCLE_QUESTION, "About");

} // namespace Tako::WindowTitle

/// <summary>
/// エディタの各ウィンドウで共通に使う部品（左ラベルの欄、アイコンのボタン）
/// </summary>
namespace Tako::UI {

  inline constexpr float kLabelRatio = 0.36f;   // ラベルの列の幅 / ウィンドウの幅
  inline constexpr float kLabelMin   = 96.0f;   // ラベルの列の最小幅(px)
  inline constexpr float kLabelMax   = 260.0f;  // ラベルの列の最大幅(px)

  /// <summary>
  /// ラベルを左の列に描き、次の欄を右の列いっぱいに置く。欄の左端は字下げがあっても揃う
  /// </summary>
  /// <param name="label">ImGui のラベル。## より後ろは出さない。出す文字が無ければ何もしない（呼び出し側が決めた幅のまま、ImGui の欄と同じになる）</param>
  inline void Label(const char* label) {
    const char* end = ImGui::FindRenderedTextEnd(label);
    if (end == label) {
      return;
    }
    const ImGuiStyle& style = ImGui::GetStyle();
    const ImRect&     work  = ImGui::GetCurrentWindow()->WorkRect;
    const ImVec2      pos   = ImGui::GetCursorScreenPos();
    // 字下げが深くても、ラベルに最小幅の半分は残す
    const float fieldX    = (std::max)(work.Min.x + std::clamp(work.GetWidth() * kLabelRatio, kLabelMin, kLabelMax), pos.x + kLabelMin * 0.5f);
    const float labelMaxX = fieldX - style.ItemInnerSpacing.x;
    // ラベルはアイテムにしない（長いラベルがウィンドウの中身の幅を広げないように、欄の手前で "..." にする）
    ImGui::RenderTextEllipsis(ImGui::GetWindowDrawList(), ImVec2(pos.x, pos.y + style.FramePadding.y), ImVec2(labelMaxX, pos.y + ImGui::GetFrameHeight()),
      labelMaxX, labelMaxX, label, end, nullptr);
    ImGui::SetCursorScreenPos(ImVec2(fieldX, pos.y));
    ImGui::SetNextItemWidth(-FLT_MIN);
  }

  /// <summary>
  /// Label に加えて、次の欄の右にアイコンのボタンを buttonCount 個置く幅を残す。欄の後に SameLineInner と IconButton を続ける
  /// </summary>
  inline void LabelWithButtons(const char* label, int buttonCount) {
    Label(label);
    ImGui::SetNextItemWidth(-(ImGui::GetFrameHeight() + ImGui::GetStyle().ItemInnerSpacing.x) * static_cast<float>(buttonCount));
  }

  /// <summary>
  /// 欄のすぐ右に次のアイテムを置く（欄と、それに付くボタンの間隔）
  /// </summary>
  inline void SameLineInner() {
    ImGui::SameLine(0.0f, ImGui::GetStyle().ItemInnerSpacing.x);
  }

  /// <summary>
  /// ラベルを隠した ImGui のラベル（"##" + label）。ID は元のラベルごとに分かれる
  /// </summary>
  /// <returns>次に HiddenLabel を呼ぶまで有効（ImGui はウィジェットの呼び出しの中で読み終える）</returns>
  inline const char* HiddenLabel(const char* label) {
    static std::string buffer;
    buffer.assign("##").append(label);
    return buffer.c_str();
  }

// ImGui::name と同じ引数で、ラベルだけ左の列に出す欄。既定引数と多重定義は ImGui 側のものがそのまま使われる
// 引数は転送されるので、型は ImGui の宣言どおりに書く（float の引数に 1 と書くと、変換の警告でビルドが止まる）
#define TAKO_UI_FIELD(name)                                                  \
  template <class... Args>                                                   \
  auto name(const char* label, Args&&... args) {                             \
    Label(label);                                                            \
    return ImGui::name(HiddenLabel(label), std::forward<Args>(args)...);     \
  }

  TAKO_UI_FIELD(DragFloat)
  TAKO_UI_FIELD(DragFloat2)
  TAKO_UI_FIELD(DragFloat3)
  TAKO_UI_FIELD(DragFloat4)
  TAKO_UI_FIELD(DragFloatRange2)
  TAKO_UI_FIELD(DragInt)
  TAKO_UI_FIELD(DragInt2)
  TAKO_UI_FIELD(DragInt3)
  TAKO_UI_FIELD(DragInt4)
  TAKO_UI_FIELD(DragScalar)
  TAKO_UI_FIELD(DragScalarN)
  TAKO_UI_FIELD(SliderFloat)
  TAKO_UI_FIELD(SliderFloat2)
  TAKO_UI_FIELD(SliderFloat3)
  TAKO_UI_FIELD(SliderFloat4)
  TAKO_UI_FIELD(SliderInt)
  TAKO_UI_FIELD(SliderAngle)
  TAKO_UI_FIELD(InputText)
  TAKO_UI_FIELD(InputTextWithHint)
  TAKO_UI_FIELD(InputInt)
  TAKO_UI_FIELD(InputInt2)
  TAKO_UI_FIELD(InputFloat)
  TAKO_UI_FIELD(InputFloat2)
  TAKO_UI_FIELD(InputFloat3)
  TAKO_UI_FIELD(InputFloat4)
  TAKO_UI_FIELD(ColorEdit3)
  TAKO_UI_FIELD(ColorEdit4)
  TAKO_UI_FIELD(Combo)
  TAKO_UI_FIELD(BeginCombo)
  TAKO_UI_FIELD(Checkbox)
  TAKO_UI_FIELD(CheckboxFlags)

#undef TAKO_UI_FIELD

  /// <summary>
  /// 「名前：値」の行。値は欄と同じ列に出す
  /// </summary>
  template <class... Args>
  void Value(const char* label, const char* format, Args&&... args) {
    Label(label);
    ImGui::AlignTextToFramePadding();
    ImGui::Text(format, std::forward<Args>(args)...);
  }

  /// <summary>
  /// アイコンだけの正方形のボタン（ツールバー用）。同じウィンドウに同じアイコンを 2 つ置くなら icon に "##id" を付ける
  /// </summary>
  /// <param name="isActive">切り替えの ON として色を変える</param>
  inline bool IconButton(const char* icon, const char* tooltip, bool isActive = false) {
    if (isActive) {
      ImGui::PushStyleColor(ImGuiCol_Button, GetEditorColors().toggleOn);
    }
    const float size = ImGui::GetFrameHeight();
    ImGui::PushID(icon);
    const bool pressed = ImGui::Button("##IconButton", ImVec2(size, size));
    ImGui::PopID();
    if (isActive) {
      ImGui::PopStyleColor();
    }
    // 押せない間も出す（押せない理由を tooltip に書けるように）
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_ForTooltip | ImGuiHoveredFlags_AllowWhenDisabled)) {
      ImGui::SetTooltip("%s", tooltip);
    }

    // Button のラベルにすると、字送りの幅で揃えられて字面が左右に寄る（余白より広い字は左端に置かれ、字送りの中の位置も端数が切り捨てられる）。
    // 字面（実際に描かれる範囲）の中心をボタンの中心に合わせて、自分で描く
    unsigned int codepoint = 0;
    ImTextCharFromUtf8(&codepoint, icon, nullptr);
    if (const ImFontGlyph* glyph = ImGui::GetFont()->FindGlyph(static_cast<ImWchar>(codepoint))) {
      const float  scale = ImGui::GetFontSize() / ImGui::GetFont()->FontSize;
      const ImVec2 min   = ImGui::GetItemRectMin();
      const float  x     = min.x + ImFloor((size - (glyph->X0 + glyph->X1) * scale) * 0.5f + 0.5f);
      ImGui::GetWindowDrawList()->AddText(ImVec2(x, min.y + ImGui::GetStyle().FramePadding.y), ImGui::GetColorU32(ImGuiCol_Text), icon, ImGui::FindRenderedTextEnd(icon));
    }
    return pressed;
  }

  /// <summary>
  /// 押すたびに value を反転するアイコンのボタン
  /// </summary>
  /// <returns>反転したフレームに true</returns>
  inline bool IconToggle(const char* icon, const char* tooltip, bool* value) {
    const bool pressed = IconButton(icon, tooltip, *value);
    if (pressed) {
      *value = !*value;
    }
    return pressed;
  }

  /// <summary>
  /// 取り消しにくい操作（削除など）のボタン。エラーの色で出す
  /// </summary>
  inline bool DangerButton(const char* label, const ImVec2& size = ImVec2(0.0f, 0.0f)) {
    const ImVec4& error = GetEditorColors().error;
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(error.x, error.y, error.z, 0.45f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(error.x, error.y, error.z, 0.65f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(error.x, error.y, error.z, 0.85f));
    const bool pressed = ImGui::Button(label, size);
    ImGui::PopStyleColor(3);
    return pressed;
  }

  /// <summary>
  /// テーマの色 a に b を t の割合で混ぜた色。自前で描く面（タイムラインなど）の、テーマに無い中間の色に使う
  /// </summary>
  inline ImU32 MixColor(ImGuiCol a, ImGuiCol b, float t) {
    return ImGui::GetColorU32(ImLerp(ImGui::GetStyleColorVec4(a), ImGui::GetStyleColorVec4(b), t));
  }

  /// <summary>
  /// 同じ行の続きに縦の区切り線を置く（ツールバーの仲間分け）
  /// </summary>
  inline void ToolbarSeparator() {
    ImGui::SameLine();
    ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
    ImGui::SameLine();
  }

  /// <summary>
  /// 次のアイテム（幅 width）を行の右端に寄せる。入らなければ今の位置のまま
  /// </summary>
  inline void AlignRight(float width) {
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (std::max)(0.0f, ImGui::GetContentRegionAvail().x - width));
  }

} // namespace Tako::UI

#endif // _DEBUG

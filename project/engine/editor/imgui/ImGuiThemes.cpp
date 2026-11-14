#include "ImGuiThemes.h"
#include "ColorFunc.h"

#include "imgui_internal.h"

#include <algorithm>
#include <cassert>
#include <cstdint>

namespace Tako {

  namespace {

    struct Palette {
      uint32_t mantle;    ///< タイトルバー・メニューバー・ポップアップ
      uint32_t base;      ///< ウィンドウ背景
      uint32_t surface0;  ///< ウィジェットの通常時
      uint32_t surface1;  ///< ホバー時・枠線
      uint32_t surface2;  ///< 押下時
      uint32_t text;
      uint32_t accent;    ///< チェックマーク・スライダー・選択の強調
      uint32_t accent2;   ///< グラフ・ドロップ先
    };

    EditorColors editorColors;

    /// <summary>
    /// 全テーマ共通の形（余白・間隔・角丸・枠線）。既定値の style に設定する
    /// </summary>
    void ApplyEditorShape(ImGuiStyle& style) {
      style.WindowPadding            = ImVec2(8.0f, 8.0f);
      style.FramePadding             = ImVec2(6.0f, 4.0f);
      style.CellPadding              = ImVec2(6.0f, 3.0f);
      style.ItemSpacing              = ImVec2(8.0f, 5.0f);
      style.ItemInnerSpacing         = ImVec2(6.0f, 4.0f);
      style.IndentSpacing            = 16.0f;
      style.ScrollbarSize            = 12.0f;
      style.GrabMinSize              = 10.0f;
      style.WindowBorderSize         = 1.0f;
      style.ChildBorderSize          = 1.0f;
      style.PopupBorderSize          = 1.0f;
      style.FrameBorderSize          = 0.0f;
      style.TabBorderSize            = 0.0f;
      style.TabBarOverlineSize       = 2.0f;
      style.DockingSeparatorSize     = 2.0f;
      style.SeparatorTextBorderSize  = 1.0f;
      style.SeparatorTextPadding     = ImVec2(8.0f, 4.0f);
      style.WindowRounding           = 4.0f;
      style.ChildRounding            = 4.0f;
      style.PopupRounding            = 4.0f;
      style.FrameRounding            = 3.0f;
      style.GrabRounding             = 3.0f;
      style.TabRounding              = 4.0f;
      style.ScrollbarRounding        = 6.0f;
      style.WindowTitleAlign         = ImVec2(0.0f, 0.5f);
      style.WindowMenuButtonPosition = ImGuiDir_None;  // タブの左の三角（折りたたみ）は出さない
      style.ColorButtonPosition      = ImGuiDir_Right;
      style.DisabledAlpha            = 0.5f;
    }

    /// <summary>
    /// 状態の色を、背景の明るさに合う組から選ぶ
    /// </summary>
    EditorColors MakeEditorColors(const ImGuiStyle& style) {
      const ImVec4& background  = style.Colors[ImGuiCol_WindowBg];
      const bool    isLight     = background.x * 0.299f + background.y * 0.587f + background.z * 0.114f > 0.5f;
      EditorColors  colors      = isLight
        ? EditorColors{ ImVec4(0.10f, 0.50f, 0.22f, 1.0f), ImVec4(0.60f, 0.40f, 0.00f, 1.0f), ImVec4(0.81f, 0.13f, 0.18f, 1.0f), ImVec4(0.04f, 0.41f, 0.85f, 1.0f), {} }
        : EditorColors{ ImVec4(0.35f, 0.85f, 0.45f, 1.0f), ImVec4(1.00f, 0.78f, 0.25f, 1.0f), ImVec4(1.00f, 0.42f, 0.42f, 1.0f), ImVec4(0.40f, 0.72f, 1.00f, 1.0f), {} };
      // 強調色が白や灰のテーマでも見えるよう、ボタンの色と文字の色の間を取る
      colors.toggleOn = ImLerp(style.Colors[ImGuiCol_Button], style.Colors[ImGuiCol_Text], 0.3f);
      colors.toggleOn.w = 1.0f;
      return colors;
    }

    void ApplyPalette(ImGuiStyle& style, const Palette& palette) {
      static_assert(ImGuiCol_COUNT == 58, "ImGuiCol が増減したら割り当てを見直す");

      const Color   mantle   = Color::FromHex(palette.mantle);
      const Color   base     = Color::FromHex(palette.base);
      const Color   surface0 = Color::FromHex(palette.surface0);
      const Color   surface1 = Color::FromHex(palette.surface1);
      const Color   surface2 = Color::FromHex(palette.surface2);
      const Color   text     = Color::FromHex(palette.text);
      const Color   accent   = Color::FromHex(palette.accent);
      const Color   accent2  = Color::FromHex(palette.accent2);

      Vector4 colors[ImGuiCol_COUNT] = {};
      colors[ImGuiCol_Text]                      = text;
      colors[ImGuiCol_TextDisabled]              = Vector4::Lerp(text, base, 0.45f);
      colors[ImGuiCol_WindowBg]                  = base;
      colors[ImGuiCol_ChildBg]                   = Color::kClear;
      colors[ImGuiCol_PopupBg]                   = mantle;
      colors[ImGuiCol_Border]                    = surface1;
      colors[ImGuiCol_BorderShadow]              = Color::kClear;
      colors[ImGuiCol_FrameBg]                   = surface0;
      colors[ImGuiCol_FrameBgHovered]            = surface1;
      colors[ImGuiCol_FrameBgActive]             = surface2;
      colors[ImGuiCol_TitleBg]                   = mantle;
      colors[ImGuiCol_TitleBgActive]             = surface0;
      colors[ImGuiCol_TitleBgCollapsed]          = mantle;
      colors[ImGuiCol_MenuBarBg]                 = mantle;
      colors[ImGuiCol_ScrollbarBg]               = mantle;
      colors[ImGuiCol_ScrollbarGrab]             = surface1;
      colors[ImGuiCol_ScrollbarGrabHovered]      = surface2;
      colors[ImGuiCol_ScrollbarGrabActive]       = accent;
      colors[ImGuiCol_CheckMark]                 = accent;
      colors[ImGuiCol_SliderGrab]                = accent;
      colors[ImGuiCol_SliderGrabActive]          = Vector4::Lerp(accent, text, 0.3f);
      colors[ImGuiCol_Button]                    = surface0;
      colors[ImGuiCol_ButtonHovered]             = surface1;
      colors[ImGuiCol_ButtonActive]              = surface2;
      // 選択中の行が背景に埋もれないよう、ウィジェットの通常時より 1 段明るくする
      colors[ImGuiCol_Header]                    = surface1;
      colors[ImGuiCol_HeaderHovered]             = Vector4::Lerp(surface1, surface2, 0.5f);
      colors[ImGuiCol_HeaderActive]              = surface2;
      colors[ImGuiCol_Separator]                 = surface1;
      colors[ImGuiCol_SeparatorHovered]          = accent.WithAlpha(0.78f);
      colors[ImGuiCol_SeparatorActive]           = accent;
      colors[ImGuiCol_ResizeGrip]                = accent.WithAlpha(0.2f);
      colors[ImGuiCol_ResizeGripHovered]         = accent.WithAlpha(0.67f);
      colors[ImGuiCol_ResizeGripActive]          = accent.WithAlpha(0.95f);
      colors[ImGuiCol_TabHovered]                = surface1;
      colors[ImGuiCol_Tab]                       = surface0;
      colors[ImGuiCol_TabSelected]               = surface1;
      colors[ImGuiCol_TabSelectedOverline]       = accent;
      colors[ImGuiCol_TabDimmed]                 = mantle;
      colors[ImGuiCol_TabDimmedSelected]         = surface0;
      colors[ImGuiCol_TabDimmedSelectedOverline] = Color::kClear;
      colors[ImGuiCol_DockingPreview]            = accent.WithAlpha(0.7f);
      colors[ImGuiCol_DockingEmptyBg]            = mantle;
      colors[ImGuiCol_PlotLines]                 = accent;
      colors[ImGuiCol_PlotLinesHovered]          = accent2;
      colors[ImGuiCol_PlotHistogram]             = accent2;
      colors[ImGuiCol_PlotHistogramHovered]      = Vector4::Lerp(accent2, text, 0.3f);
      colors[ImGuiCol_TableHeaderBg]             = surface0;
      colors[ImGuiCol_TableBorderStrong]         = surface1;
      colors[ImGuiCol_TableBorderLight]          = surface0;
      colors[ImGuiCol_TableRowBg]                = Color::kClear;
      colors[ImGuiCol_TableRowBgAlt]             = text.WithAlpha(0.04f);
      colors[ImGuiCol_TextLink]                  = accent;
      colors[ImGuiCol_TextSelectedBg]            = accent.WithAlpha(0.35f);
      colors[ImGuiCol_DragDropTarget]            = accent2.WithAlpha(0.9f);
      colors[ImGuiCol_NavCursor]                 = accent;
      colors[ImGuiCol_NavWindowingHighlight]     = text.WithAlpha(0.7f);
      colors[ImGuiCol_NavWindowingDimBg]         = text.WithAlpha(0.2f);
      colors[ImGuiCol_ModalWindowDimBg]          = text.WithAlpha(0.35f);

      for (int i = 0; i < ImGuiCol_COUNT; ++i) {
        style.Colors[i] = ImVec4(colors[i].x, colors[i].y, colors[i].z, colors[i].w);
      }
    }

    // Monochrome 以外の配色は各テーマ公式のパレット値
    constexpr Palette kMonochrome      = { 0x0f0f0f, 0x171717, 0x262626, 0x363636, 0x4a4a4a, 0xe6e6e6, 0xffffff, 0xa6a6a6 };
    constexpr Palette kCatppuccinMocha = { 0x181825, 0x1e1e2e, 0x313244, 0x45475a, 0x585b70, 0xcdd6f4, 0xcba6f7, 0xfab387 };
    constexpr Palette kCatppuccinLatte = { 0xe6e9ef, 0xeff1f5, 0xccd0da, 0xbcc0cc, 0xacb0be, 0x4c4f69, 0x8839ef, 0xfe640b };
    constexpr Palette kNord            = { 0x2e3440, 0x2e3440, 0x3b4252, 0x434c5e, 0x4c566a, 0xd8dee9, 0x88c0d0, 0xebcb8b };
    constexpr Palette kDracula         = { 0x21222c, 0x282a36, 0x343746, 0x44475a, 0x6272a4, 0xf8f8f2, 0xbd93f9, 0xff79c6 };
    constexpr Palette kGruvbox         = { 0x1d2021, 0x282828, 0x3c3836, 0x504945, 0x665c54, 0xebdbb2, 0xfabd2f, 0xfe8019 };
    constexpr Palette kTokyoNight      = { 0x16161e, 0x1a1b26, 0x292e42, 0x3b4261, 0x545c7e, 0xc0caf5, 0x7aa2f7, 0xff9e64 };
    constexpr Palette kOneDark         = { 0x21252b, 0x282c34, 0x2c313a, 0x3e4452, 0x4e5666, 0xabb2bf, 0x61afef, 0xe5c07b };
    constexpr Palette kMonokai         = { 0x1e1f1c, 0x272822, 0x34352f, 0x3e3d32, 0x75715e, 0xf8f8f2, 0xf92672, 0xa6e22e };

    // 以下は ImThemes (https://github.com/Patitotective/ImThemes) 収録テーマ。MIT License, Copyright (c) 2022 Patitotective
    // 形（余白・角丸など）の行は ApplyImGuiTheme が捨て、配色だけを使う
    // >>> gen_imgui_themes.py の生成範囲（手で編集しない）

    // Unreal（作者: dev0-1）
    void StyleUnreal(ImGuiStyle& style) {
      style.GrabMinSize = 10.0f;

      style.Colors[ImGuiCol_Text]                  = ImVec4(1.000f, 1.000f, 1.000f, 1.000f);
      style.Colors[ImGuiCol_TextDisabled]          = ImVec4(0.498f, 0.498f, 0.498f, 1.000f);
      style.Colors[ImGuiCol_WindowBg]              = ImVec4(0.059f, 0.059f, 0.059f, 0.940f);
      style.Colors[ImGuiCol_ChildBg]               = ImVec4(1.000f, 1.000f, 1.000f, 0.000f);
      style.Colors[ImGuiCol_PopupBg]               = ImVec4(0.078f, 0.078f, 0.078f, 0.940f);
      style.Colors[ImGuiCol_Border]                = ImVec4(0.427f, 0.427f, 0.498f, 0.500f);
      style.Colors[ImGuiCol_BorderShadow]          = ImVec4(0.000f, 0.000f, 0.000f, 0.000f);
      style.Colors[ImGuiCol_FrameBg]               = ImVec4(0.200f, 0.208f, 0.220f, 0.540f);
      style.Colors[ImGuiCol_FrameBgHovered]        = ImVec4(0.400f, 0.400f, 0.400f, 0.400f);
      style.Colors[ImGuiCol_FrameBgActive]         = ImVec4(0.176f, 0.176f, 0.176f, 0.670f);
      style.Colors[ImGuiCol_TitleBg]               = ImVec4(0.039f, 0.039f, 0.039f, 1.000f);
      style.Colors[ImGuiCol_TitleBgActive]         = ImVec4(0.286f, 0.286f, 0.286f, 1.000f);
      style.Colors[ImGuiCol_TitleBgCollapsed]      = ImVec4(0.000f, 0.000f, 0.000f, 0.510f);
      style.Colors[ImGuiCol_MenuBarBg]             = ImVec4(0.137f, 0.137f, 0.137f, 1.000f);
      style.Colors[ImGuiCol_ScrollbarBg]           = ImVec4(0.020f, 0.020f, 0.020f, 0.530f);
      style.Colors[ImGuiCol_ScrollbarGrab]         = ImVec4(0.310f, 0.310f, 0.310f, 1.000f);
      style.Colors[ImGuiCol_ScrollbarGrabHovered]  = ImVec4(0.408f, 0.408f, 0.408f, 1.000f);
      style.Colors[ImGuiCol_ScrollbarGrabActive]   = ImVec4(0.510f, 0.510f, 0.510f, 1.000f);
      style.Colors[ImGuiCol_CheckMark]             = ImVec4(0.937f, 0.937f, 0.937f, 1.000f);
      style.Colors[ImGuiCol_SliderGrab]            = ImVec4(0.510f, 0.510f, 0.510f, 1.000f);
      style.Colors[ImGuiCol_SliderGrabActive]      = ImVec4(0.859f, 0.859f, 0.859f, 1.000f);
      style.Colors[ImGuiCol_Button]                = ImVec4(0.439f, 0.439f, 0.439f, 0.400f);
      style.Colors[ImGuiCol_ButtonHovered]         = ImVec4(0.459f, 0.467f, 0.478f, 1.000f);
      style.Colors[ImGuiCol_ButtonActive]          = ImVec4(0.420f, 0.420f, 0.420f, 1.000f);
      style.Colors[ImGuiCol_Header]                = ImVec4(0.698f, 0.698f, 0.698f, 0.310f);
      style.Colors[ImGuiCol_HeaderHovered]         = ImVec4(0.698f, 0.698f, 0.698f, 0.800f);
      style.Colors[ImGuiCol_HeaderActive]          = ImVec4(0.478f, 0.498f, 0.518f, 1.000f);
      style.Colors[ImGuiCol_Separator]             = ImVec4(0.427f, 0.427f, 0.498f, 0.500f);
      style.Colors[ImGuiCol_SeparatorHovered]      = ImVec4(0.718f, 0.718f, 0.718f, 0.780f);
      style.Colors[ImGuiCol_SeparatorActive]       = ImVec4(0.510f, 0.510f, 0.510f, 1.000f);
      style.Colors[ImGuiCol_ResizeGrip]            = ImVec4(0.910f, 0.910f, 0.910f, 0.250f);
      style.Colors[ImGuiCol_ResizeGripHovered]     = ImVec4(0.808f, 0.808f, 0.808f, 0.670f);
      style.Colors[ImGuiCol_ResizeGripActive]      = ImVec4(0.459f, 0.459f, 0.459f, 0.950f);
      style.Colors[ImGuiCol_Tab]                   = ImVec4(0.176f, 0.349f, 0.576f, 0.862f);
      style.Colors[ImGuiCol_TabHovered]            = ImVec4(0.259f, 0.588f, 0.976f, 0.800f);
      style.Colors[ImGuiCol_TabSelected]           = ImVec4(0.196f, 0.408f, 0.678f, 1.000f);
      style.Colors[ImGuiCol_TabDimmed]             = ImVec4(0.067f, 0.102f, 0.145f, 0.972f);
      style.Colors[ImGuiCol_TabDimmedSelected]     = ImVec4(0.133f, 0.259f, 0.424f, 1.000f);
      style.Colors[ImGuiCol_PlotLines]             = ImVec4(0.608f, 0.608f, 0.608f, 1.000f);
      style.Colors[ImGuiCol_PlotLinesHovered]      = ImVec4(1.000f, 0.427f, 0.349f, 1.000f);
      style.Colors[ImGuiCol_PlotHistogram]         = ImVec4(0.729f, 0.600f, 0.149f, 1.000f);
      style.Colors[ImGuiCol_PlotHistogramHovered]  = ImVec4(1.000f, 0.600f, 0.000f, 1.000f);
      style.Colors[ImGuiCol_TableHeaderBg]         = ImVec4(0.188f, 0.188f, 0.200f, 1.000f);
      style.Colors[ImGuiCol_TableBorderStrong]     = ImVec4(0.310f, 0.310f, 0.349f, 1.000f);
      style.Colors[ImGuiCol_TableBorderLight]      = ImVec4(0.227f, 0.227f, 0.247f, 1.000f);
      style.Colors[ImGuiCol_TableRowBg]            = ImVec4(0.000f, 0.000f, 0.000f, 0.000f);
      style.Colors[ImGuiCol_TableRowBgAlt]         = ImVec4(1.000f, 1.000f, 1.000f, 0.060f);
      style.Colors[ImGuiCol_TextSelectedBg]        = ImVec4(0.867f, 0.867f, 0.867f, 0.350f);
      style.Colors[ImGuiCol_DragDropTarget]        = ImVec4(1.000f, 1.000f, 0.000f, 0.900f);
      style.Colors[ImGuiCol_NavCursor]             = ImVec4(0.600f, 0.600f, 0.600f, 1.000f);
      style.Colors[ImGuiCol_NavWindowingHighlight] = ImVec4(1.000f, 1.000f, 1.000f, 0.700f);
      style.Colors[ImGuiCol_NavWindowingDimBg]     = ImVec4(0.800f, 0.800f, 0.800f, 0.200f);
      style.Colors[ImGuiCol_ModalWindowDimBg]      = ImVec4(0.800f, 0.800f, 0.800f, 0.350f);
    }

    // Visual Studio（作者: MomoDeve）
    void StyleVisualStudio(ImGuiStyle& style) {
      style.ScrollbarRounding = 0.0f;
      style.GrabMinSize       = 10.0f;
      style.TabRounding       = 0.0f;

      style.Colors[ImGuiCol_Text]                  = ImVec4(1.000f, 1.000f, 1.000f, 1.000f);
      style.Colors[ImGuiCol_TextDisabled]          = ImVec4(0.592f, 0.592f, 0.592f, 1.000f);
      style.Colors[ImGuiCol_WindowBg]              = ImVec4(0.145f, 0.145f, 0.149f, 1.000f);
      style.Colors[ImGuiCol_ChildBg]               = ImVec4(0.145f, 0.145f, 0.149f, 1.000f);
      style.Colors[ImGuiCol_PopupBg]               = ImVec4(0.145f, 0.145f, 0.149f, 1.000f);
      style.Colors[ImGuiCol_Border]                = ImVec4(0.306f, 0.306f, 0.306f, 1.000f);
      style.Colors[ImGuiCol_BorderShadow]          = ImVec4(0.306f, 0.306f, 0.306f, 1.000f);
      style.Colors[ImGuiCol_FrameBg]               = ImVec4(0.200f, 0.200f, 0.216f, 1.000f);
      style.Colors[ImGuiCol_FrameBgHovered]        = ImVec4(0.114f, 0.592f, 0.925f, 1.000f);
      style.Colors[ImGuiCol_FrameBgActive]         = ImVec4(0.000f, 0.467f, 0.784f, 1.000f);
      style.Colors[ImGuiCol_TitleBg]               = ImVec4(0.145f, 0.145f, 0.149f, 1.000f);
      style.Colors[ImGuiCol_TitleBgActive]         = ImVec4(0.145f, 0.145f, 0.149f, 1.000f);
      style.Colors[ImGuiCol_TitleBgCollapsed]      = ImVec4(0.145f, 0.145f, 0.149f, 1.000f);
      style.Colors[ImGuiCol_MenuBarBg]             = ImVec4(0.200f, 0.200f, 0.216f, 1.000f);
      style.Colors[ImGuiCol_ScrollbarBg]           = ImVec4(0.200f, 0.200f, 0.216f, 1.000f);
      style.Colors[ImGuiCol_ScrollbarGrab]         = ImVec4(0.322f, 0.322f, 0.333f, 1.000f);
      style.Colors[ImGuiCol_ScrollbarGrabHovered]  = ImVec4(0.353f, 0.353f, 0.373f, 1.000f);
      style.Colors[ImGuiCol_ScrollbarGrabActive]   = ImVec4(0.353f, 0.353f, 0.373f, 1.000f);
      style.Colors[ImGuiCol_CheckMark]             = ImVec4(0.000f, 0.467f, 0.784f, 1.000f);
      style.Colors[ImGuiCol_SliderGrab]            = ImVec4(0.114f, 0.592f, 0.925f, 1.000f);
      style.Colors[ImGuiCol_SliderGrabActive]      = ImVec4(0.000f, 0.467f, 0.784f, 1.000f);
      style.Colors[ImGuiCol_Button]                = ImVec4(0.200f, 0.200f, 0.216f, 1.000f);
      style.Colors[ImGuiCol_ButtonHovered]         = ImVec4(0.114f, 0.592f, 0.925f, 1.000f);
      style.Colors[ImGuiCol_ButtonActive]          = ImVec4(0.114f, 0.592f, 0.925f, 1.000f);
      style.Colors[ImGuiCol_Header]                = ImVec4(0.200f, 0.200f, 0.216f, 1.000f);
      style.Colors[ImGuiCol_HeaderHovered]         = ImVec4(0.114f, 0.592f, 0.925f, 1.000f);
      style.Colors[ImGuiCol_HeaderActive]          = ImVec4(0.000f, 0.467f, 0.784f, 1.000f);
      style.Colors[ImGuiCol_Separator]             = ImVec4(0.306f, 0.306f, 0.306f, 1.000f);
      style.Colors[ImGuiCol_SeparatorHovered]      = ImVec4(0.306f, 0.306f, 0.306f, 1.000f);
      style.Colors[ImGuiCol_SeparatorActive]       = ImVec4(0.306f, 0.306f, 0.306f, 1.000f);
      style.Colors[ImGuiCol_ResizeGrip]            = ImVec4(0.145f, 0.145f, 0.149f, 1.000f);
      style.Colors[ImGuiCol_ResizeGripHovered]     = ImVec4(0.200f, 0.200f, 0.216f, 1.000f);
      style.Colors[ImGuiCol_ResizeGripActive]      = ImVec4(0.322f, 0.322f, 0.333f, 1.000f);
      style.Colors[ImGuiCol_Tab]                   = ImVec4(0.145f, 0.145f, 0.149f, 1.000f);
      style.Colors[ImGuiCol_TabHovered]            = ImVec4(0.114f, 0.592f, 0.925f, 1.000f);
      style.Colors[ImGuiCol_TabSelected]           = ImVec4(0.000f, 0.467f, 0.784f, 1.000f);
      style.Colors[ImGuiCol_TabDimmed]             = ImVec4(0.145f, 0.145f, 0.149f, 1.000f);
      style.Colors[ImGuiCol_TabDimmedSelected]     = ImVec4(0.000f, 0.467f, 0.784f, 1.000f);
      style.Colors[ImGuiCol_PlotLines]             = ImVec4(0.000f, 0.467f, 0.784f, 1.000f);
      style.Colors[ImGuiCol_PlotLinesHovered]      = ImVec4(0.114f, 0.592f, 0.925f, 1.000f);
      style.Colors[ImGuiCol_PlotHistogram]         = ImVec4(0.000f, 0.467f, 0.784f, 1.000f);
      style.Colors[ImGuiCol_PlotHistogramHovered]  = ImVec4(0.114f, 0.592f, 0.925f, 1.000f);
      style.Colors[ImGuiCol_TableHeaderBg]         = ImVec4(0.188f, 0.188f, 0.200f, 1.000f);
      style.Colors[ImGuiCol_TableBorderStrong]     = ImVec4(0.310f, 0.310f, 0.349f, 1.000f);
      style.Colors[ImGuiCol_TableBorderLight]      = ImVec4(0.227f, 0.227f, 0.247f, 1.000f);
      style.Colors[ImGuiCol_TableRowBg]            = ImVec4(0.000f, 0.000f, 0.000f, 0.000f);
      style.Colors[ImGuiCol_TableRowBgAlt]         = ImVec4(1.000f, 1.000f, 1.000f, 0.060f);
      style.Colors[ImGuiCol_TextSelectedBg]        = ImVec4(0.000f, 0.467f, 0.784f, 1.000f);
      style.Colors[ImGuiCol_DragDropTarget]        = ImVec4(0.145f, 0.145f, 0.149f, 1.000f);
      style.Colors[ImGuiCol_NavCursor]             = ImVec4(0.145f, 0.145f, 0.149f, 1.000f);
      style.Colors[ImGuiCol_NavWindowingHighlight] = ImVec4(1.000f, 1.000f, 1.000f, 0.700f);
      style.Colors[ImGuiCol_NavWindowingDimBg]     = ImVec4(0.800f, 0.800f, 0.800f, 0.200f);
      style.Colors[ImGuiCol_ModalWindowDimBg]      = ImVec4(0.145f, 0.145f, 0.149f, 1.000f);
    }

    // Photoshop（作者: Derydoca）
    void StylePhotoshop(ImGuiStyle& style) {
      style.WindowRounding    = 4.0f;
      style.ChildRounding     = 4.0f;
      style.PopupRounding     = 2.0f;
      style.FrameRounding     = 2.0f;
      style.FrameBorderSize   = 1.0f;
      style.ScrollbarSize     = 13.0f;
      style.ScrollbarRounding = 12.0f;
      style.GrabMinSize       = 7.0f;
      style.TabRounding       = 0.0f;
      style.TabBorderSize     = 1.0f;

      style.Colors[ImGuiCol_Text]                  = ImVec4(1.000f, 1.000f, 1.000f, 1.000f);
      style.Colors[ImGuiCol_TextDisabled]          = ImVec4(0.498f, 0.498f, 0.498f, 1.000f);
      style.Colors[ImGuiCol_WindowBg]              = ImVec4(0.176f, 0.176f, 0.176f, 1.000f);
      style.Colors[ImGuiCol_ChildBg]               = ImVec4(0.278f, 0.278f, 0.278f, 0.000f);
      style.Colors[ImGuiCol_PopupBg]               = ImVec4(0.310f, 0.310f, 0.310f, 1.000f);
      style.Colors[ImGuiCol_Border]                = ImVec4(0.263f, 0.263f, 0.263f, 1.000f);
      style.Colors[ImGuiCol_BorderShadow]          = ImVec4(0.000f, 0.000f, 0.000f, 0.000f);
      style.Colors[ImGuiCol_FrameBg]               = ImVec4(0.157f, 0.157f, 0.157f, 1.000f);
      style.Colors[ImGuiCol_FrameBgHovered]        = ImVec4(0.200f, 0.200f, 0.200f, 1.000f);
      style.Colors[ImGuiCol_FrameBgActive]         = ImVec4(0.278f, 0.278f, 0.278f, 1.000f);
      style.Colors[ImGuiCol_TitleBg]               = ImVec4(0.145f, 0.145f, 0.145f, 1.000f);
      style.Colors[ImGuiCol_TitleBgActive]         = ImVec4(0.145f, 0.145f, 0.145f, 1.000f);
      style.Colors[ImGuiCol_TitleBgCollapsed]      = ImVec4(0.145f, 0.145f, 0.145f, 1.000f);
      style.Colors[ImGuiCol_MenuBarBg]             = ImVec4(0.192f, 0.192f, 0.192f, 1.000f);
      style.Colors[ImGuiCol_ScrollbarBg]           = ImVec4(0.157f, 0.157f, 0.157f, 1.000f);
      style.Colors[ImGuiCol_ScrollbarGrab]         = ImVec4(0.275f, 0.275f, 0.275f, 1.000f);
      style.Colors[ImGuiCol_ScrollbarGrabHovered]  = ImVec4(0.298f, 0.298f, 0.298f, 1.000f);
      style.Colors[ImGuiCol_ScrollbarGrabActive]   = ImVec4(1.000f, 0.388f, 0.000f, 1.000f);
      style.Colors[ImGuiCol_CheckMark]             = ImVec4(1.000f, 1.000f, 1.000f, 1.000f);
      style.Colors[ImGuiCol_SliderGrab]            = ImVec4(0.388f, 0.388f, 0.388f, 1.000f);
      style.Colors[ImGuiCol_SliderGrabActive]      = ImVec4(1.000f, 0.388f, 0.000f, 1.000f);
      style.Colors[ImGuiCol_Button]                = ImVec4(1.000f, 1.000f, 1.000f, 0.000f);
      style.Colors[ImGuiCol_ButtonHovered]         = ImVec4(1.000f, 1.000f, 1.000f, 0.156f);
      style.Colors[ImGuiCol_ButtonActive]          = ImVec4(1.000f, 1.000f, 1.000f, 0.391f);
      style.Colors[ImGuiCol_Header]                = ImVec4(0.310f, 0.310f, 0.310f, 1.000f);
      style.Colors[ImGuiCol_HeaderHovered]         = ImVec4(0.467f, 0.467f, 0.467f, 1.000f);
      style.Colors[ImGuiCol_HeaderActive]          = ImVec4(0.467f, 0.467f, 0.467f, 1.000f);
      style.Colors[ImGuiCol_Separator]             = ImVec4(0.263f, 0.263f, 0.263f, 1.000f);
      style.Colors[ImGuiCol_SeparatorHovered]      = ImVec4(0.388f, 0.388f, 0.388f, 1.000f);
      style.Colors[ImGuiCol_SeparatorActive]       = ImVec4(1.000f, 0.388f, 0.000f, 1.000f);
      style.Colors[ImGuiCol_ResizeGrip]            = ImVec4(1.000f, 1.000f, 1.000f, 0.250f);
      style.Colors[ImGuiCol_ResizeGripHovered]     = ImVec4(1.000f, 1.000f, 1.000f, 0.670f);
      style.Colors[ImGuiCol_ResizeGripActive]      = ImVec4(1.000f, 0.388f, 0.000f, 1.000f);
      style.Colors[ImGuiCol_Tab]                   = ImVec4(0.094f, 0.094f, 0.094f, 1.000f);
      style.Colors[ImGuiCol_TabHovered]            = ImVec4(0.349f, 0.349f, 0.349f, 1.000f);
      style.Colors[ImGuiCol_TabSelected]           = ImVec4(0.192f, 0.192f, 0.192f, 1.000f);
      style.Colors[ImGuiCol_TabDimmed]             = ImVec4(0.094f, 0.094f, 0.094f, 1.000f);
      style.Colors[ImGuiCol_TabDimmedSelected]     = ImVec4(0.192f, 0.192f, 0.192f, 1.000f);
      style.Colors[ImGuiCol_PlotLines]             = ImVec4(0.467f, 0.467f, 0.467f, 1.000f);
      style.Colors[ImGuiCol_PlotLinesHovered]      = ImVec4(1.000f, 0.388f, 0.000f, 1.000f);
      style.Colors[ImGuiCol_PlotHistogram]         = ImVec4(0.584f, 0.584f, 0.584f, 1.000f);
      style.Colors[ImGuiCol_PlotHistogramHovered]  = ImVec4(1.000f, 0.388f, 0.000f, 1.000f);
      style.Colors[ImGuiCol_TableHeaderBg]         = ImVec4(0.188f, 0.188f, 0.200f, 1.000f);
      style.Colors[ImGuiCol_TableBorderStrong]     = ImVec4(0.310f, 0.310f, 0.349f, 1.000f);
      style.Colors[ImGuiCol_TableBorderLight]      = ImVec4(0.227f, 0.227f, 0.247f, 1.000f);
      style.Colors[ImGuiCol_TableRowBg]            = ImVec4(0.000f, 0.000f, 0.000f, 0.000f);
      style.Colors[ImGuiCol_TableRowBgAlt]         = ImVec4(1.000f, 1.000f, 1.000f, 0.060f);
      style.Colors[ImGuiCol_TextSelectedBg]        = ImVec4(1.000f, 1.000f, 1.000f, 0.156f);
      style.Colors[ImGuiCol_DragDropTarget]        = ImVec4(1.000f, 0.388f, 0.000f, 1.000f);
      style.Colors[ImGuiCol_NavCursor]             = ImVec4(1.000f, 0.388f, 0.000f, 1.000f);
      style.Colors[ImGuiCol_NavWindowingHighlight] = ImVec4(1.000f, 0.388f, 0.000f, 1.000f);
      style.Colors[ImGuiCol_NavWindowingDimBg]     = ImVec4(0.000f, 0.000f, 0.000f, 0.586f);
      style.Colors[ImGuiCol_ModalWindowDimBg]      = ImVec4(0.000f, 0.000f, 0.000f, 0.586f);
    }

    // Darcula（作者: ice1000）
    void StyleDarcula(ImGuiStyle& style) {
      style.WindowRounding    = 5.3f;
      style.FrameRounding     = 2.3f;
      style.FrameBorderSize   = 1.0f;
      style.ItemSpacing       = ImVec2(8.0f, 6.5f);
      style.ScrollbarRounding = 5.0f;
      style.GrabMinSize       = 10.0f;
      style.GrabRounding      = 2.3f;

      style.Colors[ImGuiCol_Text]                  = ImVec4(0.733f, 0.733f, 0.733f, 1.000f);
      style.Colors[ImGuiCol_TextDisabled]          = ImVec4(0.345f, 0.345f, 0.345f, 1.000f);
      style.Colors[ImGuiCol_WindowBg]              = ImVec4(0.235f, 0.247f, 0.255f, 0.940f);
      style.Colors[ImGuiCol_ChildBg]               = ImVec4(0.235f, 0.247f, 0.255f, 0.000f);
      style.Colors[ImGuiCol_PopupBg]               = ImVec4(0.235f, 0.247f, 0.255f, 0.940f);
      style.Colors[ImGuiCol_Border]                = ImVec4(0.333f, 0.333f, 0.333f, 0.500f);
      style.Colors[ImGuiCol_BorderShadow]          = ImVec4(0.157f, 0.157f, 0.157f, 0.000f);
      style.Colors[ImGuiCol_FrameBg]               = ImVec4(0.169f, 0.169f, 0.169f, 0.540f);
      style.Colors[ImGuiCol_FrameBgHovered]        = ImVec4(0.451f, 0.675f, 0.996f, 0.670f);
      style.Colors[ImGuiCol_FrameBgActive]         = ImVec4(0.471f, 0.471f, 0.471f, 0.670f);
      style.Colors[ImGuiCol_TitleBg]               = ImVec4(0.039f, 0.039f, 0.039f, 1.000f);
      style.Colors[ImGuiCol_TitleBgActive]         = ImVec4(0.000f, 0.000f, 0.000f, 0.510f);
      style.Colors[ImGuiCol_TitleBgCollapsed]      = ImVec4(0.157f, 0.286f, 0.478f, 1.000f);
      style.Colors[ImGuiCol_MenuBarBg]             = ImVec4(0.271f, 0.286f, 0.290f, 0.800f);
      style.Colors[ImGuiCol_ScrollbarBg]           = ImVec4(0.271f, 0.286f, 0.290f, 0.600f);
      style.Colors[ImGuiCol_ScrollbarGrab]         = ImVec4(0.220f, 0.310f, 0.420f, 0.510f);
      style.Colors[ImGuiCol_ScrollbarGrabHovered]  = ImVec4(0.220f, 0.310f, 0.420f, 1.000f);
      style.Colors[ImGuiCol_ScrollbarGrabActive]   = ImVec4(0.137f, 0.192f, 0.263f, 0.910f);
      style.Colors[ImGuiCol_CheckMark]             = ImVec4(0.898f, 0.898f, 0.898f, 0.830f);
      style.Colors[ImGuiCol_SliderGrab]            = ImVec4(0.698f, 0.698f, 0.698f, 0.620f);
      style.Colors[ImGuiCol_SliderGrabActive]      = ImVec4(0.298f, 0.298f, 0.298f, 0.840f);
      style.Colors[ImGuiCol_Button]                = ImVec4(0.333f, 0.353f, 0.361f, 0.490f);
      style.Colors[ImGuiCol_ButtonHovered]         = ImVec4(0.220f, 0.310f, 0.420f, 1.000f);
      style.Colors[ImGuiCol_ButtonActive]          = ImVec4(0.137f, 0.192f, 0.263f, 1.000f);
      style.Colors[ImGuiCol_Header]                = ImVec4(0.333f, 0.353f, 0.361f, 0.530f);
      style.Colors[ImGuiCol_HeaderHovered]         = ImVec4(0.451f, 0.675f, 0.996f, 0.670f);
      style.Colors[ImGuiCol_HeaderActive]          = ImVec4(0.471f, 0.471f, 0.471f, 0.670f);
      style.Colors[ImGuiCol_Separator]             = ImVec4(0.314f, 0.314f, 0.314f, 1.000f);
      style.Colors[ImGuiCol_SeparatorHovered]      = ImVec4(0.314f, 0.314f, 0.314f, 1.000f);
      style.Colors[ImGuiCol_SeparatorActive]       = ImVec4(0.314f, 0.314f, 0.314f, 1.000f);
      style.Colors[ImGuiCol_ResizeGrip]            = ImVec4(1.000f, 1.000f, 1.000f, 0.850f);
      style.Colors[ImGuiCol_ResizeGripHovered]     = ImVec4(1.000f, 1.000f, 1.000f, 0.600f);
      style.Colors[ImGuiCol_ResizeGripActive]      = ImVec4(1.000f, 1.000f, 1.000f, 0.900f);
      style.Colors[ImGuiCol_Tab]                   = ImVec4(0.176f, 0.349f, 0.576f, 0.862f);
      style.Colors[ImGuiCol_TabHovered]            = ImVec4(0.259f, 0.588f, 0.976f, 0.800f);
      style.Colors[ImGuiCol_TabSelected]           = ImVec4(0.196f, 0.408f, 0.678f, 1.000f);
      style.Colors[ImGuiCol_TabDimmed]             = ImVec4(0.067f, 0.102f, 0.145f, 0.972f);
      style.Colors[ImGuiCol_TabDimmedSelected]     = ImVec4(0.133f, 0.259f, 0.424f, 1.000f);
      style.Colors[ImGuiCol_PlotLines]             = ImVec4(0.608f, 0.608f, 0.608f, 1.000f);
      style.Colors[ImGuiCol_PlotLinesHovered]      = ImVec4(1.000f, 0.427f, 0.349f, 1.000f);
      style.Colors[ImGuiCol_PlotHistogram]         = ImVec4(0.898f, 0.698f, 0.000f, 1.000f);
      style.Colors[ImGuiCol_PlotHistogramHovered]  = ImVec4(1.000f, 0.600f, 0.000f, 1.000f);
      style.Colors[ImGuiCol_TableHeaderBg]         = ImVec4(0.188f, 0.188f, 0.200f, 1.000f);
      style.Colors[ImGuiCol_TableBorderStrong]     = ImVec4(0.310f, 0.310f, 0.349f, 1.000f);
      style.Colors[ImGuiCol_TableBorderLight]      = ImVec4(0.227f, 0.227f, 0.247f, 1.000f);
      style.Colors[ImGuiCol_TableRowBg]            = ImVec4(0.000f, 0.000f, 0.000f, 0.000f);
      style.Colors[ImGuiCol_TableRowBgAlt]         = ImVec4(1.000f, 1.000f, 1.000f, 0.060f);
      style.Colors[ImGuiCol_TextSelectedBg]        = ImVec4(0.184f, 0.396f, 0.792f, 0.900f);
      style.Colors[ImGuiCol_DragDropTarget]        = ImVec4(1.000f, 1.000f, 0.000f, 0.900f);
      style.Colors[ImGuiCol_NavCursor]             = ImVec4(0.259f, 0.588f, 0.976f, 1.000f);
      style.Colors[ImGuiCol_NavWindowingHighlight] = ImVec4(1.000f, 1.000f, 1.000f, 0.700f);
      style.Colors[ImGuiCol_NavWindowingDimBg]     = ImVec4(0.800f, 0.800f, 0.800f, 0.200f);
      style.Colors[ImGuiCol_ModalWindowDimBg]      = ImVec4(0.800f, 0.800f, 0.800f, 0.350f);
    }

    // Material Flat（作者: ImJC1C）
    void StyleMaterialFlat(ImGuiStyle& style) {
      style.DisabledAlpha       = 0.5f;
      style.ScrollbarRounding   = 0.0f;
      style.GrabMinSize         = 10.0f;
      style.TabRounding         = 0.0f;
      style.ColorButtonPosition = ImGuiDir_Left;

      style.Colors[ImGuiCol_Text]                  = ImVec4(0.831f, 0.847f, 0.878f, 1.000f);
      style.Colors[ImGuiCol_TextDisabled]          = ImVec4(0.831f, 0.847f, 0.878f, 0.502f);
      style.Colors[ImGuiCol_WindowBg]              = ImVec4(0.173f, 0.192f, 0.235f, 1.000f);
      style.Colors[ImGuiCol_ChildBg]               = ImVec4(0.000f, 0.000f, 0.000f, 0.159f);
      style.Colors[ImGuiCol_PopupBg]               = ImVec4(0.173f, 0.192f, 0.235f, 1.000f);
      style.Colors[ImGuiCol_Border]                = ImVec4(0.204f, 0.231f, 0.282f, 1.000f);
      style.Colors[ImGuiCol_BorderShadow]          = ImVec4(0.000f, 0.000f, 0.000f, 0.000f);
      style.Colors[ImGuiCol_FrameBg]               = ImVec4(0.106f, 0.114f, 0.137f, 0.502f);
      style.Colors[ImGuiCol_FrameBgHovered]        = ImVec4(0.310f, 0.624f, 0.933f, 0.251f);
      style.Colors[ImGuiCol_FrameBgActive]         = ImVec4(0.310f, 0.624f, 0.933f, 1.000f);
      style.Colors[ImGuiCol_TitleBg]               = ImVec4(0.106f, 0.114f, 0.137f, 1.000f);
      style.Colors[ImGuiCol_TitleBgActive]         = ImVec4(0.106f, 0.114f, 0.137f, 1.000f);
      style.Colors[ImGuiCol_TitleBgCollapsed]      = ImVec4(0.106f, 0.114f, 0.137f, 1.000f);
      style.Colors[ImGuiCol_MenuBarBg]             = ImVec4(0.106f, 0.114f, 0.137f, 1.000f);
      style.Colors[ImGuiCol_ScrollbarBg]           = ImVec4(0.020f, 0.020f, 0.020f, 0.000f);
      style.Colors[ImGuiCol_ScrollbarGrab]         = ImVec4(0.533f, 0.533f, 0.533f, 1.000f);
      style.Colors[ImGuiCol_ScrollbarGrabHovered]  = ImVec4(0.333f, 0.333f, 0.333f, 1.000f);
      style.Colors[ImGuiCol_ScrollbarGrabActive]   = ImVec4(0.600f, 0.600f, 0.600f, 1.000f);
      style.Colors[ImGuiCol_CheckMark]             = ImVec4(0.310f, 0.624f, 0.933f, 1.000f);
      style.Colors[ImGuiCol_SliderGrab]            = ImVec4(0.239f, 0.522f, 0.878f, 1.000f);
      style.Colors[ImGuiCol_SliderGrabActive]      = ImVec4(0.259f, 0.588f, 0.980f, 1.000f);
      style.Colors[ImGuiCol_Button]                = ImVec4(0.153f, 0.173f, 0.212f, 0.502f);
      style.Colors[ImGuiCol_ButtonHovered]         = ImVec4(0.153f, 0.173f, 0.212f, 1.000f);
      style.Colors[ImGuiCol_ButtonActive]          = ImVec4(0.310f, 0.624f, 0.933f, 1.000f);
      style.Colors[ImGuiCol_Header]                = ImVec4(0.153f, 0.173f, 0.212f, 1.000f);
      style.Colors[ImGuiCol_HeaderHovered]         = ImVec4(0.310f, 0.624f, 0.933f, 0.251f);
      style.Colors[ImGuiCol_HeaderActive]          = ImVec4(0.310f, 0.624f, 0.933f, 1.000f);
      style.Colors[ImGuiCol_Separator]             = ImVec4(0.427f, 0.427f, 0.498f, 0.500f);
      style.Colors[ImGuiCol_SeparatorHovered]      = ImVec4(0.098f, 0.400f, 0.749f, 0.780f);
      style.Colors[ImGuiCol_SeparatorActive]       = ImVec4(0.098f, 0.400f, 0.749f, 1.000f);
      style.Colors[ImGuiCol_ResizeGrip]            = ImVec4(0.106f, 0.114f, 0.137f, 1.000f);
      style.Colors[ImGuiCol_ResizeGripHovered]     = ImVec4(0.310f, 0.624f, 0.933f, 0.251f);
      style.Colors[ImGuiCol_ResizeGripActive]      = ImVec4(0.310f, 0.624f, 0.933f, 1.000f);
      style.Colors[ImGuiCol_Tab]                   = ImVec4(0.153f, 0.173f, 0.212f, 1.000f);
      style.Colors[ImGuiCol_TabHovered]            = ImVec4(0.310f, 0.624f, 0.933f, 0.251f);
      style.Colors[ImGuiCol_TabSelected]           = ImVec4(0.310f, 0.624f, 0.933f, 1.000f);
      style.Colors[ImGuiCol_TabDimmed]             = ImVec4(0.153f, 0.173f, 0.212f, 1.000f);
      style.Colors[ImGuiCol_TabDimmedSelected]     = ImVec4(0.310f, 0.624f, 0.933f, 1.000f);
      style.Colors[ImGuiCol_PlotLines]             = ImVec4(0.608f, 0.608f, 0.608f, 1.000f);
      style.Colors[ImGuiCol_PlotLinesHovered]      = ImVec4(1.000f, 0.427f, 0.349f, 1.000f);
      style.Colors[ImGuiCol_PlotHistogram]         = ImVec4(0.898f, 0.698f, 0.000f, 1.000f);
      style.Colors[ImGuiCol_PlotHistogramHovered]  = ImVec4(1.000f, 0.600f, 0.000f, 1.000f);
      style.Colors[ImGuiCol_TableHeaderBg]         = ImVec4(0.106f, 0.114f, 0.137f, 1.000f);
      style.Colors[ImGuiCol_TableBorderStrong]     = ImVec4(0.204f, 0.231f, 0.282f, 1.000f);
      style.Colors[ImGuiCol_TableBorderLight]      = ImVec4(0.204f, 0.231f, 0.282f, 0.502f);
      style.Colors[ImGuiCol_TableRowBg]            = ImVec4(0.000f, 0.000f, 0.000f, 0.000f);
      style.Colors[ImGuiCol_TableRowBgAlt]         = ImVec4(1.000f, 1.000f, 1.000f, 0.039f);
      style.Colors[ImGuiCol_TextSelectedBg]        = ImVec4(0.204f, 0.231f, 0.282f, 1.000f);
      style.Colors[ImGuiCol_DragDropTarget]        = ImVec4(1.000f, 1.000f, 0.000f, 0.900f);
      style.Colors[ImGuiCol_NavCursor]             = ImVec4(0.259f, 0.588f, 0.976f, 1.000f);
      style.Colors[ImGuiCol_NavWindowingHighlight] = ImVec4(0.204f, 0.231f, 0.282f, 0.753f);
      style.Colors[ImGuiCol_NavWindowingDimBg]     = ImVec4(0.106f, 0.114f, 0.137f, 0.753f);
      style.Colors[ImGuiCol_ModalWindowDimBg]      = ImVec4(0.106f, 0.114f, 0.137f, 0.753f);
    }

    // Cherry（作者: r-lyeh）
    void StyleCherry(ImGuiStyle& style) {
      style.WindowPadding     = ImVec2(6.0f, 3.0f);
      style.WindowTitleAlign  = ImVec2(0.5f, 0.5f);
      style.FramePadding      = ImVec2(5.0f, 1.0f);
      style.FrameRounding     = 3.0f;
      style.FrameBorderSize   = 1.0f;
      style.ItemSpacing       = ImVec2(7.0f, 1.0f);
      style.ItemInnerSpacing  = ImVec2(1.0f, 1.0f);
      style.IndentSpacing     = 6.0f;
      style.ScrollbarSize     = 13.0f;
      style.ScrollbarRounding = 16.0f;
      style.GrabMinSize       = 20.0f;
      style.GrabRounding      = 2.0f;
      style.TabBorderSize     = 1.0f;

      style.Colors[ImGuiCol_Text]                  = ImVec4(0.859f, 0.929f, 0.886f, 0.880f);
      style.Colors[ImGuiCol_TextDisabled]          = ImVec4(0.859f, 0.929f, 0.886f, 0.280f);
      style.Colors[ImGuiCol_WindowBg]              = ImVec4(0.129f, 0.137f, 0.169f, 1.000f);
      style.Colors[ImGuiCol_ChildBg]               = ImVec4(0.000f, 0.000f, 0.000f, 0.000f);
      style.Colors[ImGuiCol_PopupBg]               = ImVec4(0.200f, 0.220f, 0.267f, 0.900f);
      style.Colors[ImGuiCol_Border]                = ImVec4(0.537f, 0.478f, 0.255f, 0.162f);
      style.Colors[ImGuiCol_BorderShadow]          = ImVec4(0.000f, 0.000f, 0.000f, 0.000f);
      style.Colors[ImGuiCol_FrameBg]               = ImVec4(0.200f, 0.220f, 0.267f, 1.000f);
      style.Colors[ImGuiCol_FrameBgHovered]        = ImVec4(0.455f, 0.196f, 0.298f, 0.780f);
      style.Colors[ImGuiCol_FrameBgActive]         = ImVec4(0.455f, 0.196f, 0.298f, 1.000f);
      style.Colors[ImGuiCol_TitleBg]               = ImVec4(0.231f, 0.200f, 0.271f, 1.000f);
      style.Colors[ImGuiCol_TitleBgActive]         = ImVec4(0.502f, 0.075f, 0.255f, 1.000f);
      style.Colors[ImGuiCol_TitleBgCollapsed]      = ImVec4(0.200f, 0.220f, 0.267f, 0.750f);
      style.Colors[ImGuiCol_MenuBarBg]             = ImVec4(0.200f, 0.220f, 0.267f, 0.470f);
      style.Colors[ImGuiCol_ScrollbarBg]           = ImVec4(0.200f, 0.220f, 0.267f, 1.000f);
      style.Colors[ImGuiCol_ScrollbarGrab]         = ImVec4(0.086f, 0.149f, 0.157f, 1.000f);
      style.Colors[ImGuiCol_ScrollbarGrabHovered]  = ImVec4(0.455f, 0.196f, 0.298f, 0.780f);
      style.Colors[ImGuiCol_ScrollbarGrabActive]   = ImVec4(0.455f, 0.196f, 0.298f, 1.000f);
      style.Colors[ImGuiCol_CheckMark]             = ImVec4(0.710f, 0.220f, 0.267f, 1.000f);
      style.Colors[ImGuiCol_SliderGrab]            = ImVec4(0.467f, 0.769f, 0.827f, 0.140f);
      style.Colors[ImGuiCol_SliderGrabActive]      = ImVec4(0.710f, 0.220f, 0.267f, 1.000f);
      style.Colors[ImGuiCol_Button]                = ImVec4(0.467f, 0.769f, 0.827f, 0.140f);
      style.Colors[ImGuiCol_ButtonHovered]         = ImVec4(0.455f, 0.196f, 0.298f, 0.860f);
      style.Colors[ImGuiCol_ButtonActive]          = ImVec4(0.455f, 0.196f, 0.298f, 1.000f);
      style.Colors[ImGuiCol_Header]                = ImVec4(0.455f, 0.196f, 0.298f, 0.760f);
      style.Colors[ImGuiCol_HeaderHovered]         = ImVec4(0.455f, 0.196f, 0.298f, 0.860f);
      style.Colors[ImGuiCol_HeaderActive]          = ImVec4(0.502f, 0.075f, 0.255f, 1.000f);
      style.Colors[ImGuiCol_Separator]             = ImVec4(0.427f, 0.427f, 0.498f, 0.500f);
      style.Colors[ImGuiCol_SeparatorHovered]      = ImVec4(0.098f, 0.400f, 0.749f, 0.780f);
      style.Colors[ImGuiCol_SeparatorActive]       = ImVec4(0.098f, 0.400f, 0.749f, 1.000f);
      style.Colors[ImGuiCol_ResizeGrip]            = ImVec4(0.467f, 0.769f, 0.827f, 0.040f);
      style.Colors[ImGuiCol_ResizeGripHovered]     = ImVec4(0.455f, 0.196f, 0.298f, 0.780f);
      style.Colors[ImGuiCol_ResizeGripActive]      = ImVec4(0.455f, 0.196f, 0.298f, 1.000f);
      style.Colors[ImGuiCol_Tab]                   = ImVec4(0.176f, 0.349f, 0.576f, 0.862f);
      style.Colors[ImGuiCol_TabHovered]            = ImVec4(0.259f, 0.588f, 0.976f, 0.800f);
      style.Colors[ImGuiCol_TabSelected]           = ImVec4(0.196f, 0.408f, 0.678f, 1.000f);
      style.Colors[ImGuiCol_TabDimmed]             = ImVec4(0.067f, 0.102f, 0.145f, 0.972f);
      style.Colors[ImGuiCol_TabDimmedSelected]     = ImVec4(0.133f, 0.259f, 0.424f, 1.000f);
      style.Colors[ImGuiCol_PlotLines]             = ImVec4(0.859f, 0.929f, 0.886f, 0.630f);
      style.Colors[ImGuiCol_PlotLinesHovered]      = ImVec4(0.455f, 0.196f, 0.298f, 1.000f);
      style.Colors[ImGuiCol_PlotHistogram]         = ImVec4(0.859f, 0.929f, 0.886f, 0.630f);
      style.Colors[ImGuiCol_PlotHistogramHovered]  = ImVec4(0.455f, 0.196f, 0.298f, 1.000f);
      style.Colors[ImGuiCol_TableHeaderBg]         = ImVec4(0.188f, 0.188f, 0.200f, 1.000f);
      style.Colors[ImGuiCol_TableBorderStrong]     = ImVec4(0.310f, 0.310f, 0.349f, 1.000f);
      style.Colors[ImGuiCol_TableBorderLight]      = ImVec4(0.227f, 0.227f, 0.247f, 1.000f);
      style.Colors[ImGuiCol_TableRowBg]            = ImVec4(0.000f, 0.000f, 0.000f, 0.000f);
      style.Colors[ImGuiCol_TableRowBgAlt]         = ImVec4(1.000f, 1.000f, 1.000f, 0.060f);
      style.Colors[ImGuiCol_TextSelectedBg]        = ImVec4(0.455f, 0.196f, 0.298f, 0.430f);
      style.Colors[ImGuiCol_DragDropTarget]        = ImVec4(1.000f, 1.000f, 0.000f, 0.900f);
      style.Colors[ImGuiCol_NavCursor]             = ImVec4(0.259f, 0.588f, 0.976f, 1.000f);
      style.Colors[ImGuiCol_NavWindowingHighlight] = ImVec4(1.000f, 1.000f, 1.000f, 0.700f);
      style.Colors[ImGuiCol_NavWindowingDimBg]     = ImVec4(0.800f, 0.800f, 0.800f, 0.200f);
      style.Colors[ImGuiCol_ModalWindowDimBg]      = ImVec4(0.800f, 0.800f, 0.800f, 0.350f);
    }

    // Soft Cherry（作者: Patitotective）
    void StyleSoftCherry(ImGuiStyle& style) {
      style.DisabledAlpha     = 0.4f;
      style.WindowPadding     = ImVec2(10.0f, 10.0f);
      style.WindowRounding    = 4.0f;
      style.WindowBorderSize  = 0.0f;
      style.WindowMinSize     = ImVec2(50.0f, 50.0f);
      style.WindowTitleAlign  = ImVec2(0.5f, 0.5f);
      style.PopupRounding     = 1.0f;
      style.FramePadding      = ImVec2(5.0f, 3.0f);
      style.FrameRounding     = 3.0f;
      style.ItemSpacing       = ImVec2(6.0f, 6.0f);
      style.ItemInnerSpacing  = ImVec2(3.0f, 2.0f);
      style.CellPadding       = ImVec2(3.0f, 3.0f);
      style.IndentSpacing     = 6.0f;
      style.ScrollbarSize     = 13.0f;
      style.ScrollbarRounding = 16.0f;
      style.GrabMinSize       = 20.0f;
      style.GrabRounding      = 4.0f;
      style.TabBorderSize     = 1.0f;

      style.Colors[ImGuiCol_Text]                  = ImVec4(0.859f, 0.929f, 0.886f, 1.000f);
      style.Colors[ImGuiCol_TextDisabled]          = ImVec4(0.522f, 0.549f, 0.533f, 1.000f);
      style.Colors[ImGuiCol_WindowBg]              = ImVec4(0.129f, 0.137f, 0.169f, 1.000f);
      style.Colors[ImGuiCol_ChildBg]               = ImVec4(0.149f, 0.157f, 0.188f, 1.000f);
      style.Colors[ImGuiCol_PopupBg]               = ImVec4(0.200f, 0.220f, 0.267f, 1.000f);
      style.Colors[ImGuiCol_Border]                = ImVec4(0.137f, 0.114f, 0.133f, 1.000f);
      style.Colors[ImGuiCol_BorderShadow]          = ImVec4(0.000f, 0.000f, 0.000f, 1.000f);
      style.Colors[ImGuiCol_FrameBg]               = ImVec4(0.169f, 0.184f, 0.231f, 1.000f);
      style.Colors[ImGuiCol_FrameBgHovered]        = ImVec4(0.455f, 0.196f, 0.298f, 1.000f);
      style.Colors[ImGuiCol_FrameBgActive]         = ImVec4(0.455f, 0.196f, 0.298f, 1.000f);
      style.Colors[ImGuiCol_TitleBg]               = ImVec4(0.231f, 0.200f, 0.271f, 1.000f);
      style.Colors[ImGuiCol_TitleBgActive]         = ImVec4(0.502f, 0.075f, 0.255f, 1.000f);
      style.Colors[ImGuiCol_TitleBgCollapsed]      = ImVec4(0.200f, 0.220f, 0.267f, 1.000f);
      style.Colors[ImGuiCol_MenuBarBg]             = ImVec4(0.200f, 0.220f, 0.267f, 1.000f);
      style.Colors[ImGuiCol_ScrollbarBg]           = ImVec4(0.239f, 0.239f, 0.220f, 1.000f);
      style.Colors[ImGuiCol_ScrollbarGrab]         = ImVec4(0.388f, 0.388f, 0.373f, 1.000f);
      style.Colors[ImGuiCol_ScrollbarGrabHovered]  = ImVec4(0.694f, 0.694f, 0.686f, 1.000f);
      style.Colors[ImGuiCol_ScrollbarGrabActive]   = ImVec4(0.694f, 0.694f, 0.686f, 1.000f);
      style.Colors[ImGuiCol_CheckMark]             = ImVec4(0.659f, 0.137f, 0.176f, 1.000f);
      style.Colors[ImGuiCol_SliderGrab]            = ImVec4(0.651f, 0.149f, 0.345f, 1.000f);
      style.Colors[ImGuiCol_SliderGrabActive]      = ImVec4(0.710f, 0.220f, 0.267f, 1.000f);
      style.Colors[ImGuiCol_Button]                = ImVec4(0.651f, 0.149f, 0.345f, 1.000f);
      style.Colors[ImGuiCol_ButtonHovered]         = ImVec4(0.455f, 0.196f, 0.298f, 1.000f);
      style.Colors[ImGuiCol_ButtonActive]          = ImVec4(0.455f, 0.196f, 0.298f, 1.000f);
      style.Colors[ImGuiCol_Header]                = ImVec4(0.455f, 0.196f, 0.298f, 1.000f);
      style.Colors[ImGuiCol_HeaderHovered]         = ImVec4(0.651f, 0.149f, 0.345f, 1.000f);
      style.Colors[ImGuiCol_HeaderActive]          = ImVec4(0.502f, 0.075f, 0.255f, 1.000f);
      style.Colors[ImGuiCol_Separator]             = ImVec4(0.427f, 0.427f, 0.498f, 1.000f);
      style.Colors[ImGuiCol_SeparatorHovered]      = ImVec4(0.098f, 0.400f, 0.749f, 1.000f);
      style.Colors[ImGuiCol_SeparatorActive]       = ImVec4(0.098f, 0.400f, 0.749f, 1.000f);
      style.Colors[ImGuiCol_ResizeGrip]            = ImVec4(0.651f, 0.149f, 0.345f, 1.000f);
      style.Colors[ImGuiCol_ResizeGripHovered]     = ImVec4(0.455f, 0.196f, 0.298f, 1.000f);
      style.Colors[ImGuiCol_ResizeGripActive]      = ImVec4(0.455f, 0.196f, 0.298f, 1.000f);
      style.Colors[ImGuiCol_Tab]                   = ImVec4(0.176f, 0.349f, 0.576f, 1.000f);
      style.Colors[ImGuiCol_TabHovered]            = ImVec4(0.259f, 0.588f, 0.976f, 1.000f);
      style.Colors[ImGuiCol_TabSelected]           = ImVec4(0.196f, 0.408f, 0.678f, 1.000f);
      style.Colors[ImGuiCol_TabDimmed]             = ImVec4(0.067f, 0.102f, 0.145f, 1.000f);
      style.Colors[ImGuiCol_TabDimmedSelected]     = ImVec4(0.133f, 0.259f, 0.424f, 1.000f);
      style.Colors[ImGuiCol_PlotLines]             = ImVec4(0.859f, 0.929f, 0.886f, 1.000f);
      style.Colors[ImGuiCol_PlotLinesHovered]      = ImVec4(0.455f, 0.196f, 0.298f, 1.000f);
      style.Colors[ImGuiCol_PlotHistogram]         = ImVec4(0.310f, 0.776f, 0.196f, 1.000f);
      style.Colors[ImGuiCol_PlotHistogramHovered]  = ImVec4(0.455f, 0.196f, 0.298f, 1.000f);
      style.Colors[ImGuiCol_TableHeaderBg]         = ImVec4(0.188f, 0.188f, 0.200f, 1.000f);
      style.Colors[ImGuiCol_TableBorderStrong]     = ImVec4(0.310f, 0.310f, 0.349f, 1.000f);
      style.Colors[ImGuiCol_TableBorderLight]      = ImVec4(0.227f, 0.227f, 0.247f, 1.000f);
      style.Colors[ImGuiCol_TableRowBg]            = ImVec4(0.000f, 0.000f, 0.000f, 1.000f);
      style.Colors[ImGuiCol_TableRowBgAlt]         = ImVec4(1.000f, 1.000f, 1.000f, 1.000f);
      style.Colors[ImGuiCol_TextSelectedBg]        = ImVec4(0.384f, 0.627f, 0.918f, 1.000f);
      style.Colors[ImGuiCol_DragDropTarget]        = ImVec4(1.000f, 1.000f, 0.000f, 1.000f);
      style.Colors[ImGuiCol_NavCursor]             = ImVec4(0.259f, 0.588f, 0.976f, 1.000f);
      style.Colors[ImGuiCol_NavWindowingHighlight] = ImVec4(1.000f, 1.000f, 1.000f, 1.000f);
      style.Colors[ImGuiCol_NavWindowingDimBg]     = ImVec4(0.800f, 0.800f, 0.800f, 1.000f);
      style.Colors[ImGuiCol_ModalWindowDimBg]      = ImVec4(0.800f, 0.800f, 0.800f, 0.300f);
    }

    // Comfy（作者: Giuseppe）
    void StyleComfy(ImGuiStyle& style) {
      style.DisabledAlpha            = 0.1f;
      style.WindowRounding           = 10.0f;
      style.WindowBorderSize         = 0.0f;
      style.WindowMinSize            = ImVec2(30.0f, 30.0f);
      style.WindowTitleAlign         = ImVec2(0.5f, 0.5f);
      style.WindowMenuButtonPosition = ImGuiDir_Right;
      style.ChildRounding            = 5.0f;
      style.PopupRounding            = 10.0f;
      style.PopupBorderSize          = 0.0f;
      style.FramePadding             = ImVec2(5.0f, 3.5f);
      style.FrameRounding            = 5.0f;
      style.ItemSpacing              = ImVec2(5.0f, 4.0f);
      style.ItemInnerSpacing         = ImVec2(5.0f, 5.0f);
      style.IndentSpacing            = 5.0f;
      style.ColumnsMinSpacing        = 5.0f;
      style.ScrollbarSize            = 15.0f;
      style.GrabMinSize              = 15.0f;
      style.GrabRounding             = 5.0f;
      style.TabRounding              = 5.0f;

      style.Colors[ImGuiCol_Text]                  = ImVec4(1.000f, 1.000f, 1.000f, 1.000f);
      style.Colors[ImGuiCol_TextDisabled]          = ImVec4(1.000f, 1.000f, 1.000f, 0.361f);
      style.Colors[ImGuiCol_WindowBg]              = ImVec4(0.098f, 0.098f, 0.098f, 1.000f);
      style.Colors[ImGuiCol_ChildBg]               = ImVec4(1.000f, 0.000f, 0.000f, 0.000f);
      style.Colors[ImGuiCol_PopupBg]               = ImVec4(0.098f, 0.098f, 0.098f, 1.000f);
      style.Colors[ImGuiCol_Border]                = ImVec4(0.424f, 0.380f, 0.573f, 0.549f);
      style.Colors[ImGuiCol_BorderShadow]          = ImVec4(0.000f, 0.000f, 0.000f, 0.000f);
      style.Colors[ImGuiCol_FrameBg]               = ImVec4(0.157f, 0.157f, 0.157f, 1.000f);
      style.Colors[ImGuiCol_FrameBgHovered]        = ImVec4(0.380f, 0.424f, 0.573f, 0.549f);
      style.Colors[ImGuiCol_FrameBgActive]         = ImVec4(0.620f, 0.576f, 0.769f, 0.549f);
      style.Colors[ImGuiCol_TitleBg]               = ImVec4(0.098f, 0.098f, 0.098f, 1.000f);
      style.Colors[ImGuiCol_TitleBgActive]         = ImVec4(0.098f, 0.098f, 0.098f, 1.000f);
      style.Colors[ImGuiCol_TitleBgCollapsed]      = ImVec4(0.259f, 0.259f, 0.259f, 0.000f);
      style.Colors[ImGuiCol_MenuBarBg]             = ImVec4(0.000f, 0.000f, 0.000f, 0.000f);
      style.Colors[ImGuiCol_ScrollbarBg]           = ImVec4(0.157f, 0.157f, 0.157f, 0.000f);
      style.Colors[ImGuiCol_ScrollbarGrab]         = ImVec4(0.157f, 0.157f, 0.157f, 1.000f);
      style.Colors[ImGuiCol_ScrollbarGrabHovered]  = ImVec4(0.235f, 0.235f, 0.235f, 1.000f);
      style.Colors[ImGuiCol_ScrollbarGrabActive]   = ImVec4(0.294f, 0.294f, 0.294f, 1.000f);
      style.Colors[ImGuiCol_CheckMark]             = ImVec4(0.294f, 0.294f, 0.294f, 1.000f);
      style.Colors[ImGuiCol_SliderGrab]            = ImVec4(0.620f, 0.576f, 0.769f, 0.549f);
      style.Colors[ImGuiCol_SliderGrabActive]      = ImVec4(0.816f, 0.773f, 0.965f, 0.549f);
      style.Colors[ImGuiCol_Button]                = ImVec4(0.620f, 0.576f, 0.769f, 0.549f);
      style.Colors[ImGuiCol_ButtonHovered]         = ImVec4(0.737f, 0.694f, 0.886f, 0.549f);
      style.Colors[ImGuiCol_ButtonActive]          = ImVec4(0.816f, 0.773f, 0.965f, 0.549f);
      style.Colors[ImGuiCol_Header]                = ImVec4(0.620f, 0.576f, 0.769f, 0.549f);
      style.Colors[ImGuiCol_HeaderHovered]         = ImVec4(0.737f, 0.694f, 0.886f, 0.549f);
      style.Colors[ImGuiCol_HeaderActive]          = ImVec4(0.816f, 0.773f, 0.965f, 0.549f);
      style.Colors[ImGuiCol_Separator]             = ImVec4(0.620f, 0.576f, 0.769f, 0.549f);
      style.Colors[ImGuiCol_SeparatorHovered]      = ImVec4(0.737f, 0.694f, 0.886f, 0.549f);
      style.Colors[ImGuiCol_SeparatorActive]       = ImVec4(0.816f, 0.773f, 0.965f, 0.549f);
      style.Colors[ImGuiCol_ResizeGrip]            = ImVec4(0.620f, 0.576f, 0.769f, 0.549f);
      style.Colors[ImGuiCol_ResizeGripHovered]     = ImVec4(0.737f, 0.694f, 0.886f, 0.549f);
      style.Colors[ImGuiCol_ResizeGripActive]      = ImVec4(0.816f, 0.773f, 0.965f, 0.549f);
      style.Colors[ImGuiCol_Tab]                   = ImVec4(0.620f, 0.576f, 0.769f, 0.549f);
      style.Colors[ImGuiCol_TabHovered]            = ImVec4(0.737f, 0.694f, 0.886f, 0.549f);
      style.Colors[ImGuiCol_TabSelected]           = ImVec4(0.816f, 0.773f, 0.965f, 0.549f);
      style.Colors[ImGuiCol_TabDimmed]             = ImVec4(0.000f, 0.451f, 1.000f, 0.000f);
      style.Colors[ImGuiCol_TabDimmedSelected]     = ImVec4(0.133f, 0.259f, 0.424f, 0.000f);
      style.Colors[ImGuiCol_PlotLines]             = ImVec4(0.294f, 0.294f, 0.294f, 1.000f);
      style.Colors[ImGuiCol_PlotLinesHovered]      = ImVec4(0.737f, 0.694f, 0.886f, 0.549f);
      style.Colors[ImGuiCol_PlotHistogram]         = ImVec4(0.620f, 0.576f, 0.769f, 0.549f);
      style.Colors[ImGuiCol_PlotHistogramHovered]  = ImVec4(0.737f, 0.694f, 0.886f, 0.549f);
      style.Colors[ImGuiCol_TableHeaderBg]         = ImVec4(0.188f, 0.188f, 0.200f, 1.000f);
      style.Colors[ImGuiCol_TableBorderStrong]     = ImVec4(0.424f, 0.380f, 0.573f, 0.549f);
      style.Colors[ImGuiCol_TableBorderLight]      = ImVec4(0.424f, 0.380f, 0.573f, 0.292f);
      style.Colors[ImGuiCol_TableRowBg]            = ImVec4(0.000f, 0.000f, 0.000f, 0.000f);
      style.Colors[ImGuiCol_TableRowBgAlt]         = ImVec4(1.000f, 1.000f, 1.000f, 0.034f);
      style.Colors[ImGuiCol_TextSelectedBg]        = ImVec4(0.737f, 0.694f, 0.886f, 0.549f);
      style.Colors[ImGuiCol_DragDropTarget]        = ImVec4(1.000f, 1.000f, 0.000f, 0.900f);
      style.Colors[ImGuiCol_NavCursor]             = ImVec4(0.000f, 0.000f, 0.000f, 1.000f);
      style.Colors[ImGuiCol_NavWindowingHighlight] = ImVec4(1.000f, 1.000f, 1.000f, 0.700f);
      style.Colors[ImGuiCol_NavWindowingDimBg]     = ImVec4(0.800f, 0.800f, 0.800f, 0.200f);
      style.Colors[ImGuiCol_ModalWindowDimBg]      = ImVec4(0.800f, 0.800f, 0.800f, 0.350f);
    }

    // Enemymouse（作者: enemymouse）
    void StyleEnemymouse(ImGuiStyle& style) {
      style.WindowRounding = 3.0f;
      style.ChildRounding  = 3.0f;
      style.FrameRounding  = 3.0f;
      style.GrabMinSize    = 20.0f;
      style.GrabRounding   = 1.0f;

      style.Colors[ImGuiCol_Text]                  = ImVec4(0.000f, 1.000f, 1.000f, 1.000f);
      style.Colors[ImGuiCol_TextDisabled]          = ImVec4(0.000f, 0.400f, 0.408f, 1.000f);
      style.Colors[ImGuiCol_WindowBg]              = ImVec4(0.000f, 0.000f, 0.000f, 0.830f);
      style.Colors[ImGuiCol_ChildBg]               = ImVec4(0.000f, 0.000f, 0.000f, 0.000f);
      style.Colors[ImGuiCol_PopupBg]               = ImVec4(0.157f, 0.239f, 0.220f, 0.600f);
      style.Colors[ImGuiCol_Border]                = ImVec4(0.000f, 1.000f, 1.000f, 0.650f);
      style.Colors[ImGuiCol_BorderShadow]          = ImVec4(0.000f, 0.000f, 0.000f, 0.000f);
      style.Colors[ImGuiCol_FrameBg]               = ImVec4(0.439f, 0.800f, 0.800f, 0.180f);
      style.Colors[ImGuiCol_FrameBgHovered]        = ImVec4(0.439f, 0.800f, 0.800f, 0.270f);
      style.Colors[ImGuiCol_FrameBgActive]         = ImVec4(0.439f, 0.808f, 0.859f, 0.660f);
      style.Colors[ImGuiCol_TitleBg]               = ImVec4(0.137f, 0.176f, 0.208f, 0.730f);
      style.Colors[ImGuiCol_TitleBgActive]         = ImVec4(0.000f, 1.000f, 1.000f, 0.270f);
      style.Colors[ImGuiCol_TitleBgCollapsed]      = ImVec4(0.000f, 0.000f, 0.000f, 0.540f);
      style.Colors[ImGuiCol_MenuBarBg]             = ImVec4(0.000f, 0.000f, 0.000f, 0.200f);
      style.Colors[ImGuiCol_ScrollbarBg]           = ImVec4(0.220f, 0.286f, 0.298f, 0.710f);
      style.Colors[ImGuiCol_ScrollbarGrab]         = ImVec4(0.000f, 1.000f, 1.000f, 0.440f);
      style.Colors[ImGuiCol_ScrollbarGrabHovered]  = ImVec4(0.000f, 1.000f, 1.000f, 0.740f);
      style.Colors[ImGuiCol_ScrollbarGrabActive]   = ImVec4(0.000f, 1.000f, 1.000f, 1.000f);
      style.Colors[ImGuiCol_CheckMark]             = ImVec4(0.000f, 1.000f, 1.000f, 0.680f);
      style.Colors[ImGuiCol_SliderGrab]            = ImVec4(0.000f, 1.000f, 1.000f, 0.360f);
      style.Colors[ImGuiCol_SliderGrabActive]      = ImVec4(0.000f, 1.000f, 1.000f, 0.760f);
      style.Colors[ImGuiCol_Button]                = ImVec4(0.000f, 0.647f, 0.647f, 0.460f);
      style.Colors[ImGuiCol_ButtonHovered]         = ImVec4(0.008f, 1.000f, 1.000f, 0.430f);
      style.Colors[ImGuiCol_ButtonActive]          = ImVec4(0.000f, 1.000f, 1.000f, 0.620f);
      style.Colors[ImGuiCol_Header]                = ImVec4(0.000f, 1.000f, 1.000f, 0.330f);
      style.Colors[ImGuiCol_HeaderHovered]         = ImVec4(0.000f, 1.000f, 1.000f, 0.420f);
      style.Colors[ImGuiCol_HeaderActive]          = ImVec4(0.000f, 1.000f, 1.000f, 0.540f);
      style.Colors[ImGuiCol_Separator]             = ImVec4(0.000f, 0.498f, 0.498f, 0.330f);
      style.Colors[ImGuiCol_SeparatorHovered]      = ImVec4(0.000f, 0.498f, 0.498f, 0.470f);
      style.Colors[ImGuiCol_SeparatorActive]       = ImVec4(0.000f, 0.698f, 0.698f, 1.000f);
      style.Colors[ImGuiCol_ResizeGrip]            = ImVec4(0.000f, 1.000f, 1.000f, 0.540f);
      style.Colors[ImGuiCol_ResizeGripHovered]     = ImVec4(0.000f, 1.000f, 1.000f, 0.740f);
      style.Colors[ImGuiCol_ResizeGripActive]      = ImVec4(0.000f, 1.000f, 1.000f, 1.000f);
      style.Colors[ImGuiCol_Tab]                   = ImVec4(0.176f, 0.349f, 0.576f, 0.862f);
      style.Colors[ImGuiCol_TabHovered]            = ImVec4(0.259f, 0.588f, 0.976f, 0.800f);
      style.Colors[ImGuiCol_TabSelected]           = ImVec4(0.196f, 0.408f, 0.678f, 1.000f);
      style.Colors[ImGuiCol_TabDimmed]             = ImVec4(0.067f, 0.102f, 0.145f, 0.972f);
      style.Colors[ImGuiCol_TabDimmedSelected]     = ImVec4(0.133f, 0.259f, 0.424f, 1.000f);
      style.Colors[ImGuiCol_PlotLines]             = ImVec4(0.000f, 1.000f, 1.000f, 1.000f);
      style.Colors[ImGuiCol_PlotLinesHovered]      = ImVec4(0.000f, 1.000f, 1.000f, 1.000f);
      style.Colors[ImGuiCol_PlotHistogram]         = ImVec4(0.000f, 1.000f, 1.000f, 1.000f);
      style.Colors[ImGuiCol_PlotHistogramHovered]  = ImVec4(0.000f, 1.000f, 1.000f, 1.000f);
      style.Colors[ImGuiCol_TableHeaderBg]         = ImVec4(0.188f, 0.188f, 0.200f, 1.000f);
      style.Colors[ImGuiCol_TableBorderStrong]     = ImVec4(0.310f, 0.310f, 0.349f, 1.000f);
      style.Colors[ImGuiCol_TableBorderLight]      = ImVec4(0.227f, 0.227f, 0.247f, 1.000f);
      style.Colors[ImGuiCol_TableRowBg]            = ImVec4(0.000f, 0.000f, 0.000f, 0.000f);
      style.Colors[ImGuiCol_TableRowBgAlt]         = ImVec4(1.000f, 1.000f, 1.000f, 0.060f);
      style.Colors[ImGuiCol_TextSelectedBg]        = ImVec4(0.000f, 1.000f, 1.000f, 0.220f);
      style.Colors[ImGuiCol_DragDropTarget]        = ImVec4(1.000f, 1.000f, 0.000f, 0.900f);
      style.Colors[ImGuiCol_NavCursor]             = ImVec4(0.259f, 0.588f, 0.976f, 1.000f);
      style.Colors[ImGuiCol_NavWindowingHighlight] = ImVec4(1.000f, 1.000f, 1.000f, 0.700f);
      style.Colors[ImGuiCol_NavWindowingDimBg]     = ImVec4(0.800f, 0.800f, 0.800f, 0.200f);
      style.Colors[ImGuiCol_ModalWindowDimBg]      = ImVec4(0.039f, 0.098f, 0.086f, 0.510f);
    }

    // Gold（作者: CookiePLMonster）
    void StyleGold(ImGuiStyle& style) {
      style.WindowRounding           = 4.0f;
      style.WindowTitleAlign         = ImVec2(1.0f, 0.5f);
      style.WindowMenuButtonPosition = ImGuiDir_Right;
      style.PopupRounding            = 4.0f;
      style.FramePadding             = ImVec2(4.0f, 2.0f);
      style.FrameRounding            = 4.0f;
      style.ItemSpacing              = ImVec2(10.0f, 2.0f);
      style.IndentSpacing            = 12.0f;
      style.ScrollbarSize            = 10.0f;
      style.ScrollbarRounding        = 6.0f;
      style.GrabMinSize              = 10.0f;
      style.GrabRounding             = 4.0f;

      style.Colors[ImGuiCol_Text]                  = ImVec4(0.918f, 0.918f, 0.918f, 1.000f);
      style.Colors[ImGuiCol_TextDisabled]          = ImVec4(0.439f, 0.439f, 0.439f, 1.000f);
      style.Colors[ImGuiCol_WindowBg]              = ImVec4(0.059f, 0.059f, 0.059f, 1.000f);
      style.Colors[ImGuiCol_ChildBg]               = ImVec4(0.000f, 0.000f, 0.000f, 0.000f);
      style.Colors[ImGuiCol_PopupBg]               = ImVec4(0.078f, 0.078f, 0.078f, 0.940f);
      style.Colors[ImGuiCol_Border]                = ImVec4(0.510f, 0.357f, 0.149f, 1.000f);
      style.Colors[ImGuiCol_BorderShadow]          = ImVec4(0.000f, 0.000f, 0.000f, 0.000f);
      style.Colors[ImGuiCol_FrameBg]               = ImVec4(0.110f, 0.110f, 0.110f, 1.000f);
      style.Colors[ImGuiCol_FrameBgHovered]        = ImVec4(0.510f, 0.357f, 0.149f, 1.000f);
      style.Colors[ImGuiCol_FrameBgActive]         = ImVec4(0.776f, 0.549f, 0.208f, 1.000f);
      style.Colors[ImGuiCol_TitleBg]               = ImVec4(0.510f, 0.357f, 0.149f, 1.000f);
      style.Colors[ImGuiCol_TitleBgActive]         = ImVec4(0.910f, 0.639f, 0.129f, 1.000f);
      style.Colors[ImGuiCol_TitleBgCollapsed]      = ImVec4(0.000f, 0.000f, 0.000f, 0.510f);
      style.Colors[ImGuiCol_MenuBarBg]             = ImVec4(0.110f, 0.110f, 0.110f, 1.000f);
      style.Colors[ImGuiCol_ScrollbarBg]           = ImVec4(0.059f, 0.059f, 0.059f, 0.530f);
      style.Colors[ImGuiCol_ScrollbarGrab]         = ImVec4(0.208f, 0.208f, 0.208f, 1.000f);
      style.Colors[ImGuiCol_ScrollbarGrabHovered]  = ImVec4(0.467f, 0.467f, 0.467f, 1.000f);
      style.Colors[ImGuiCol_ScrollbarGrabActive]   = ImVec4(0.808f, 0.827f, 0.808f, 1.000f);
      style.Colors[ImGuiCol_CheckMark]             = ImVec4(0.776f, 0.549f, 0.208f, 1.000f);
      style.Colors[ImGuiCol_SliderGrab]            = ImVec4(0.910f, 0.639f, 0.129f, 1.000f);
      style.Colors[ImGuiCol_SliderGrabActive]      = ImVec4(0.910f, 0.639f, 0.129f, 1.000f);
      style.Colors[ImGuiCol_Button]                = ImVec4(0.510f, 0.357f, 0.149f, 1.000f);
      style.Colors[ImGuiCol_ButtonHovered]         = ImVec4(0.910f, 0.639f, 0.129f, 1.000f);
      style.Colors[ImGuiCol_ButtonActive]          = ImVec4(0.776f, 0.549f, 0.208f, 1.000f);
      style.Colors[ImGuiCol_Header]                = ImVec4(0.510f, 0.357f, 0.149f, 1.000f);
      style.Colors[ImGuiCol_HeaderHovered]         = ImVec4(0.910f, 0.639f, 0.129f, 1.000f);
      style.Colors[ImGuiCol_HeaderActive]          = ImVec4(0.929f, 0.647f, 0.137f, 1.000f);
      style.Colors[ImGuiCol_Separator]             = ImVec4(0.208f, 0.208f, 0.208f, 1.000f);
      style.Colors[ImGuiCol_SeparatorHovered]      = ImVec4(0.910f, 0.639f, 0.129f, 1.000f);
      style.Colors[ImGuiCol_SeparatorActive]       = ImVec4(0.776f, 0.549f, 0.208f, 1.000f);
      style.Colors[ImGuiCol_ResizeGrip]            = ImVec4(0.208f, 0.208f, 0.208f, 1.000f);
      style.Colors[ImGuiCol_ResizeGripHovered]     = ImVec4(0.910f, 0.639f, 0.129f, 1.000f);
      style.Colors[ImGuiCol_ResizeGripActive]      = ImVec4(0.776f, 0.549f, 0.208f, 1.000f);
      style.Colors[ImGuiCol_Tab]                   = ImVec4(0.510f, 0.357f, 0.149f, 1.000f);
      style.Colors[ImGuiCol_TabHovered]            = ImVec4(0.910f, 0.639f, 0.129f, 1.000f);
      style.Colors[ImGuiCol_TabSelected]           = ImVec4(0.776f, 0.549f, 0.208f, 1.000f);
      style.Colors[ImGuiCol_TabDimmed]             = ImVec4(0.067f, 0.098f, 0.149f, 0.970f);
      style.Colors[ImGuiCol_TabDimmedSelected]     = ImVec4(0.137f, 0.259f, 0.420f, 1.000f);
      style.Colors[ImGuiCol_PlotLines]             = ImVec4(0.608f, 0.608f, 0.608f, 1.000f);
      style.Colors[ImGuiCol_PlotLinesHovered]      = ImVec4(1.000f, 0.427f, 0.349f, 1.000f);
      style.Colors[ImGuiCol_PlotHistogram]         = ImVec4(0.898f, 0.698f, 0.000f, 1.000f);
      style.Colors[ImGuiCol_PlotHistogramHovered]  = ImVec4(1.000f, 0.600f, 0.000f, 1.000f);
      style.Colors[ImGuiCol_TableHeaderBg]         = ImVec4(0.188f, 0.188f, 0.200f, 1.000f);
      style.Colors[ImGuiCol_TableBorderStrong]     = ImVec4(0.310f, 0.310f, 0.349f, 1.000f);
      style.Colors[ImGuiCol_TableBorderLight]      = ImVec4(0.227f, 0.227f, 0.247f, 1.000f);
      style.Colors[ImGuiCol_TableRowBg]            = ImVec4(0.000f, 0.000f, 0.000f, 0.000f);
      style.Colors[ImGuiCol_TableRowBgAlt]         = ImVec4(1.000f, 1.000f, 1.000f, 0.060f);
      style.Colors[ImGuiCol_TextSelectedBg]        = ImVec4(0.259f, 0.588f, 0.976f, 0.350f);
      style.Colors[ImGuiCol_DragDropTarget]        = ImVec4(1.000f, 1.000f, 0.000f, 0.900f);
      style.Colors[ImGuiCol_NavCursor]             = ImVec4(0.259f, 0.588f, 0.976f, 1.000f);
      style.Colors[ImGuiCol_NavWindowingHighlight] = ImVec4(1.000f, 1.000f, 1.000f, 0.700f);
      style.Colors[ImGuiCol_NavWindowingDimBg]     = ImVec4(0.800f, 0.800f, 0.800f, 0.200f);
      style.Colors[ImGuiCol_ModalWindowDimBg]      = ImVec4(0.800f, 0.800f, 0.800f, 0.350f);
    }

    // Bootstrap Dark（作者: Madam-Herta）
    void StyleBootstrapDark(ImGuiStyle& style) {
      style.DisabledAlpha    = 0.5f;
      style.WindowPadding    = ImVec2(11.7f, 6.0f);
      style.WindowRounding   = 3.3f;
      style.WindowBorderSize = 0.0f;
      style.WindowMinSize    = ImVec2(20.0f, 20.0f);
      style.FramePadding     = ImVec2(20.0f, 9.9f);
      style.GrabMinSize      = 10.0f;

      style.Colors[ImGuiCol_Text]                  = ImVec4(1.000f, 1.000f, 1.000f, 1.000f);
      style.Colors[ImGuiCol_TextDisabled]          = ImVec4(0.584f, 0.596f, 0.616f, 1.000f);
      style.Colors[ImGuiCol_WindowBg]              = ImVec4(0.063f, 0.067f, 0.086f, 1.000f);
      style.Colors[ImGuiCol_ChildBg]               = ImVec4(0.043f, 0.047f, 0.059f, 1.000f);
      style.Colors[ImGuiCol_PopupBg]               = ImVec4(0.043f, 0.047f, 0.059f, 1.000f);
      style.Colors[ImGuiCol_Border]                = ImVec4(0.110f, 0.114f, 0.133f, 1.000f);
      style.Colors[ImGuiCol_BorderShadow]          = ImVec4(0.110f, 0.114f, 0.133f, 1.000f);
      style.Colors[ImGuiCol_FrameBg]               = ImVec4(0.063f, 0.067f, 0.086f, 1.000f);
      style.Colors[ImGuiCol_FrameBgHovered]        = ImVec4(0.059f, 0.529f, 0.976f, 1.000f);
      style.Colors[ImGuiCol_FrameBgActive]         = ImVec4(0.059f, 0.529f, 0.976f, 0.000f);
      style.Colors[ImGuiCol_TitleBg]               = ImVec4(0.047f, 0.051f, 0.063f, 1.000f);
      style.Colors[ImGuiCol_TitleBgActive]         = ImVec4(0.043f, 0.047f, 0.059f, 1.000f);
      style.Colors[ImGuiCol_TitleBgCollapsed]      = ImVec4(0.043f, 0.047f, 0.059f, 1.000f);
      style.Colors[ImGuiCol_MenuBarBg]             = ImVec4(0.043f, 0.047f, 0.059f, 1.000f);
      style.Colors[ImGuiCol_ScrollbarBg]           = ImVec4(0.043f, 0.047f, 0.059f, 1.000f);
      style.Colors[ImGuiCol_ScrollbarGrab]         = ImVec4(0.110f, 0.114f, 0.133f, 1.000f);
      style.Colors[ImGuiCol_ScrollbarGrabHovered]  = ImVec4(0.145f, 0.149f, 0.184f, 1.000f);
      style.Colors[ImGuiCol_ScrollbarGrabActive]   = ImVec4(0.486f, 0.486f, 0.486f, 1.000f);
      style.Colors[ImGuiCol_CheckMark]             = ImVec4(1.000f, 1.000f, 1.000f, 1.000f);
      style.Colors[ImGuiCol_SliderGrab]            = ImVec4(1.000f, 1.000f, 1.000f, 0.227f);
      style.Colors[ImGuiCol_SliderGrabActive]      = ImVec4(0.820f, 0.820f, 0.820f, 0.330f);
      style.Colors[ImGuiCol_Button]                = ImVec4(0.227f, 0.443f, 0.757f, 1.000f);
      style.Colors[ImGuiCol_ButtonHovered]         = ImVec4(0.208f, 0.471f, 0.851f, 1.000f);
      style.Colors[ImGuiCol_ButtonActive]          = ImVec4(0.059f, 0.529f, 0.976f, 1.000f);
      style.Colors[ImGuiCol_Header]                = ImVec4(0.259f, 0.588f, 0.976f, 0.310f);
      style.Colors[ImGuiCol_HeaderHovered]         = ImVec4(0.259f, 0.588f, 0.976f, 0.800f);
      style.Colors[ImGuiCol_HeaderActive]          = ImVec4(0.259f, 0.588f, 0.976f, 1.000f);
      style.Colors[ImGuiCol_Separator]             = ImVec4(0.388f, 0.388f, 0.388f, 0.620f);
      style.Colors[ImGuiCol_SeparatorHovered]      = ImVec4(0.137f, 0.439f, 0.800f, 0.780f);
      style.Colors[ImGuiCol_SeparatorActive]       = ImVec4(0.137f, 0.439f, 0.800f, 1.000f);
      style.Colors[ImGuiCol_ResizeGrip]            = ImVec4(0.349f, 0.349f, 0.349f, 0.170f);
      style.Colors[ImGuiCol_ResizeGripHovered]     = ImVec4(0.259f, 0.588f, 0.976f, 1.000f);
      style.Colors[ImGuiCol_ResizeGripActive]      = ImVec4(0.259f, 0.588f, 0.976f, 0.950f);
      style.Colors[ImGuiCol_Tab]                   = ImVec4(0.000f, 0.475f, 1.000f, 0.931f);
      style.Colors[ImGuiCol_TabHovered]            = ImVec4(0.259f, 0.588f, 0.976f, 0.800f);
      style.Colors[ImGuiCol_TabSelected]           = ImVec4(0.208f, 0.208f, 0.208f, 1.000f);
      style.Colors[ImGuiCol_TabDimmed]             = ImVec4(0.918f, 0.925f, 0.933f, 0.986f);
      style.Colors[ImGuiCol_TabDimmedSelected]     = ImVec4(0.741f, 0.820f, 0.914f, 1.000f);
      style.Colors[ImGuiCol_PlotLines]             = ImVec4(0.388f, 0.388f, 0.388f, 1.000f);
      style.Colors[ImGuiCol_PlotLinesHovered]      = ImVec4(1.000f, 0.427f, 0.349f, 1.000f);
      style.Colors[ImGuiCol_PlotHistogram]         = ImVec4(0.898f, 0.698f, 0.000f, 1.000f);
      style.Colors[ImGuiCol_PlotHistogramHovered]  = ImVec4(1.000f, 0.447f, 0.000f, 1.000f);
      style.Colors[ImGuiCol_TableHeaderBg]         = ImVec4(0.776f, 0.867f, 0.976f, 1.000f);
      style.Colors[ImGuiCol_TableBorderStrong]     = ImVec4(0.569f, 0.569f, 0.639f, 1.000f);
      style.Colors[ImGuiCol_TableBorderLight]      = ImVec4(0.678f, 0.678f, 0.737f, 1.000f);
      style.Colors[ImGuiCol_TableRowBg]            = ImVec4(0.000f, 0.000f, 0.000f, 0.000f);
      style.Colors[ImGuiCol_TableRowBgAlt]         = ImVec4(0.298f, 0.298f, 0.298f, 0.090f);
      style.Colors[ImGuiCol_TextSelectedBg]        = ImVec4(0.259f, 0.588f, 0.976f, 0.350f);
      style.Colors[ImGuiCol_DragDropTarget]        = ImVec4(0.259f, 0.588f, 0.976f, 0.950f);
      style.Colors[ImGuiCol_NavCursor]             = ImVec4(0.259f, 0.588f, 0.976f, 0.800f);
      style.Colors[ImGuiCol_NavWindowingHighlight] = ImVec4(0.698f, 0.698f, 0.698f, 0.700f);
      style.Colors[ImGuiCol_NavWindowingDimBg]     = ImVec4(0.200f, 0.200f, 0.200f, 0.200f);
      style.Colors[ImGuiCol_ModalWindowDimBg]      = ImVec4(0.200f, 0.200f, 0.200f, 0.350f);
    }

    // <<< gen_imgui_themes.py の生成範囲ここまで

    void StyleMoonLight(ImGuiStyle& style) {
      style.Colors[ImGuiCol_Text] = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
      style.Colors[ImGuiCol_TextDisabled] = ImVec4(0.2745098173618317f, 0.3176470696926117f, 0.4509803950786591f, 1.0f);
      style.Colors[ImGuiCol_WindowBg] = ImVec4(0.0784313753247261f, 0.08627451211214066f, 0.1019607856869698f, 1.0f);
      style.Colors[ImGuiCol_ChildBg] = ImVec4(0.09411764889955521f, 0.1019607856869698f, 0.1176470592617989f, 1.0f);
      style.Colors[ImGuiCol_PopupBg] = ImVec4(0.0784313753247261f, 0.08627451211214066f, 0.1019607856869698f, 1.0f);
      style.Colors[ImGuiCol_Border] = ImVec4(0.1568627506494522f, 0.168627455830574f, 0.1921568661928177f, 1.0f);
      style.Colors[ImGuiCol_BorderShadow] = ImVec4(0.0784313753247261f, 0.08627451211214066f, 0.1019607856869698f, 1.0f);
      style.Colors[ImGuiCol_FrameBg] = ImVec4(0.1137254908680916f, 0.125490203499794f, 0.1529411822557449f, 1.0f);
      style.Colors[ImGuiCol_FrameBgHovered] = ImVec4(0.1568627506494522f, 0.168627455830574f, 0.1921568661928177f, 1.0f);
      style.Colors[ImGuiCol_FrameBgActive] = ImVec4(0.1568627506494522f, 0.168627455830574f, 0.1921568661928177f, 1.0f);
      style.Colors[ImGuiCol_TitleBg] = ImVec4(0.0470588244497776f, 0.05490196123719215f, 0.07058823853731155f, 1.0f);
      style.Colors[ImGuiCol_TitleBgActive] = ImVec4(0.0470588244497776f, 0.05490196123719215f, 0.07058823853731155f, 1.0f);
      style.Colors[ImGuiCol_TitleBgCollapsed] = ImVec4(0.0784313753247261f, 0.08627451211214066f, 0.1019607856869698f, 1.0f);
      style.Colors[ImGuiCol_MenuBarBg] = ImVec4(0.09803921729326248f, 0.105882354080677f, 0.1215686276555061f, 1.0f);
      style.Colors[ImGuiCol_ScrollbarBg] = ImVec4(0.0470588244497776f, 0.05490196123719215f, 0.07058823853731155f, 1.0f);
      style.Colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.1176470592617989f, 0.1333333402872086f, 0.1490196138620377f, 1.0f);
      style.Colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.1568627506494522f, 0.168627455830574f, 0.1921568661928177f, 1.0f);
      style.Colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.1176470592617989f, 0.1333333402872086f, 0.1490196138620377f, 1.0f);
      style.Colors[ImGuiCol_CheckMark] = ImVec4(0.9725490212440491f, 1.0f, 0.4980392158031464f, 1.0f);
      style.Colors[ImGuiCol_SliderGrab] = ImVec4(0.9725490212440491f, 1.0f, 0.4980392158031464f, 1.0f);
      style.Colors[ImGuiCol_SliderGrabActive] = ImVec4(1.0f, 0.7960784435272217f, 0.4980392158031464f, 1.0f);
      style.Colors[ImGuiCol_Button] = ImVec4(0.1176470592617989f, 0.1333333402872086f, 0.1490196138620377f, 1.0f);
      style.Colors[ImGuiCol_ButtonHovered] = ImVec4(0.1803921610116959f, 0.1882352977991104f, 0.196078434586525f, 1.0f);
      style.Colors[ImGuiCol_ButtonActive] = ImVec4(0.1529411822557449f, 0.1529411822557449f, 0.1529411822557449f, 1.0f);
      style.Colors[ImGuiCol_Header] = ImVec4(0.1411764770746231f, 0.1647058874368668f, 0.2078431397676468f, 1.0f);
      style.Colors[ImGuiCol_HeaderHovered] = ImVec4(0.105882354080677f, 0.105882354080677f, 0.105882354080677f, 1.0f);
      style.Colors[ImGuiCol_HeaderActive] = ImVec4(0.0784313753247261f, 0.08627451211214066f, 0.1019607856869698f, 1.0f);
      style.Colors[ImGuiCol_Separator] = ImVec4(0.1294117718935013f, 0.1490196138620377f, 0.1921568661928177f, 1.0f);
      style.Colors[ImGuiCol_SeparatorHovered] = ImVec4(0.1568627506494522f, 0.1843137294054031f, 0.250980406999588f, 1.0f);
      style.Colors[ImGuiCol_SeparatorActive] = ImVec4(0.1568627506494522f, 0.1843137294054031f, 0.250980406999588f, 1.0f);
      style.Colors[ImGuiCol_ResizeGrip] = ImVec4(0.1450980454683304f, 0.1450980454683304f, 0.1450980454683304f, 1.0f);
      style.Colors[ImGuiCol_ResizeGripHovered] = ImVec4(0.9725490212440491f, 1.0f, 0.4980392158031464f, 1.0f);
      style.Colors[ImGuiCol_ResizeGripActive] = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
      style.Colors[ImGuiCol_Tab] = ImVec4(0.0784313753247261f, 0.08627451211214066f, 0.1019607856869698f, 1.0f);
      style.Colors[ImGuiCol_TabHovered] = ImVec4(0.1176470592617989f, 0.1333333402872086f, 0.1490196138620377f, 1.0f);
      style.Colors[ImGuiCol_TabActive] = ImVec4(0.1176470592617989f, 0.1333333402872086f, 0.1490196138620377f, 1.0f);
      style.Colors[ImGuiCol_TabUnfocused] = ImVec4(0.0784313753247261f, 0.08627451211214066f, 0.1019607856869698f, 1.0f);
      style.Colors[ImGuiCol_TabUnfocusedActive] = ImVec4(0.125490203499794f, 0.2745098173618317f, 0.572549045085907f, 1.0f);
      style.Colors[ImGuiCol_PlotLines] = ImVec4(0.5215686559677124f, 0.6000000238418579f, 0.7019608020782471f, 1.0f);
      style.Colors[ImGuiCol_PlotLinesHovered] = ImVec4(0.03921568766236305f, 0.9803921580314636f, 0.9803921580314636f, 1.0f);
      style.Colors[ImGuiCol_PlotHistogram] = ImVec4(0.8823529481887817f, 0.7960784435272217f, 0.5607843399047852f, 1.0f);
      style.Colors[ImGuiCol_PlotHistogramHovered] = ImVec4(0.95686274766922f, 0.95686274766922f, 0.95686274766922f, 1.0f);
      style.Colors[ImGuiCol_TableHeaderBg] = ImVec4(0.0470588244497776f, 0.05490196123719215f, 0.07058823853731155f, 1.0f);
      style.Colors[ImGuiCol_TableBorderStrong] = ImVec4(0.0470588244497776f, 0.05490196123719215f, 0.07058823853731155f, 1.0f);
      style.Colors[ImGuiCol_TableBorderLight] = ImVec4(0.0f, 0.0f, 0.0f, 1.0f);
      style.Colors[ImGuiCol_TableRowBg] = ImVec4(0.1176470592617989f, 0.1333333402872086f, 0.1490196138620377f, 1.0f);
      style.Colors[ImGuiCol_TableRowBgAlt] = ImVec4(0.09803921729326248f, 0.105882354080677f, 0.1215686276555061f, 1.0f);
      style.Colors[ImGuiCol_TextSelectedBg] = ImVec4(0.9372549057006836f, 0.9372549057006836f, 0.9372549057006836f, 1.0f);
      style.Colors[ImGuiCol_DragDropTarget] = ImVec4(0.4980392158031464f, 0.5137255191802979f, 1.0f, 1.0f);
      style.Colors[ImGuiCol_NavHighlight] = ImVec4(0.2666666805744171f, 0.2901960909366608f, 1.0f, 1.0f);
      style.Colors[ImGuiCol_NavWindowingHighlight] = ImVec4(0.4980392158031464f, 0.5137255191802979f, 1.0f, 1.0f);
      style.Colors[ImGuiCol_NavWindowingDimBg] = ImVec4(0.196078434586525f, 0.1764705926179886f, 0.5450980663299561f, 0.501960813999176f);
      style.Colors[ImGuiCol_ModalWindowDimBg] = ImVec4(0.196078434586525f, 0.1764705926179886f, 0.5450980663299561f, 0.501960813999176f);
    }

    // 個人設定には name で保存するため、既存の name を変えると保存済みの選択が外れる
    constexpr ImGuiTheme kThemes[] = {
      { "MoonLight",        StyleMoonLight },
      { "Dark",             [](ImGuiStyle& style) { ImGui::StyleColorsDark(&style); } },
      { "Light",            [](ImGuiStyle& style) { ImGui::StyleColorsLight(&style); } },
      { "Monochrome",       [](ImGuiStyle& style) { ApplyPalette(style, kMonochrome); } },
      { "Unreal",           StyleUnreal },
      { "Visual Studio",    StyleVisualStudio },
      { "Photoshop",        StylePhotoshop },
      { "Darcula",          StyleDarcula },
      { "Material Flat",    StyleMaterialFlat },
      { "Cherry",           StyleCherry },
      { "Soft Cherry",      StyleSoftCherry },
      { "Comfy",            StyleComfy },
      { "Enemymouse",       StyleEnemymouse },
      { "Gold",             StyleGold },
      { "Bootstrap Dark",   StyleBootstrapDark },
      { "Catppuccin Mocha", [](ImGuiStyle& style) { ApplyPalette(style, kCatppuccinMocha); } },
      { "Catppuccin Latte", [](ImGuiStyle& style) { ApplyPalette(style, kCatppuccinLatte); } },
      { "Nord",             [](ImGuiStyle& style) { ApplyPalette(style, kNord); } },
      { "Dracula",          [](ImGuiStyle& style) { ApplyPalette(style, kDracula); } },
      { "Gruvbox",          [](ImGuiStyle& style) { ApplyPalette(style, kGruvbox); } },
      { "Tokyo Night",      [](ImGuiStyle& style) { ApplyPalette(style, kTokyoNight); } },
      { "OneDark",          [](ImGuiStyle& style) { ApplyPalette(style, kOneDark); } },
      { "Monokai",          [](ImGuiStyle& style) { ApplyPalette(style, kMonokai); } },
    };

  } // namespace

  std::span<const ImGuiTheme> GetImGuiThemes() {
    return kThemes;
  }

  const EditorColors& GetEditorColors() {
    return editorColors;
  }

  void ApplyImGuiTheme(int index) {
    assert(0 <= index && index < static_cast<int>(std::size(kThemes)));
    // テーマが形も書いていることがあるので、別の style に適用して配色だけを取り出す
    ImGuiStyle themed;
    kThemes[index].apply(themed);

    ImGuiStyle& style = ImGui::GetStyle();
    style = ImGuiStyle();
    ApplyEditorShape(style);
    std::ranges::copy(themed.Colors, style.Colors);
    editorColors = MakeEditorColors(style);
  }

} // namespace Tako

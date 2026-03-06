#pragma once
#include "../Vendors/imgui/imgui.h"

// =========================================================================
//  CYBERPUNK THEME  -  Black + Electric Yellow, Modern Flat Design
//  Call once after ImGui::CreateContext() / StyleColorsDark()
// =========================================================================
inline void ApplyCyberpunkTheme()
{
    ImGuiStyle& style = ImGui::GetStyle();

    // ── Geometry ─────────────────────────────────────────────────────────
    style.WindowRounding    = 4.0f;
    style.ChildRounding     = 4.0f;
    style.FrameRounding     = 3.0f;
    style.PopupRounding     = 3.0f;
    style.ScrollbarRounding = 6.0f;
    style.GrabRounding      = 3.0f;
    style.TabRounding       = 3.0f;

    style.WindowPadding     = ImVec2(10, 10);
    style.FramePadding      = ImVec2(8, 5);
    style.ItemSpacing       = ImVec2(8, 6);
    style.ItemInnerSpacing  = ImVec2(6, 4);
    style.IndentSpacing     = 20.0f;

    style.ScrollbarSize     = 13.0f;
    style.GrabMinSize       = 9.0f;

    // Neon glow borders on input frames
    style.WindowBorderSize  = 1.0f;
    style.ChildBorderSize   = 1.0f;
    style.PopupBorderSize   = 1.0f;
    style.FrameBorderSize   = 1.0f;   // neon outline on all inputs
    style.TabBorderSize     = 0.0f;
    style.TabBarBorderSize  = 1.0f;

    style.WindowTitleAlign   = ImVec2(0.5f, 0.5f);   // centred title
    style.SeparatorTextAlign = ImVec2(0.5f, 0.5f);

    // ── Palette ──────────────────────────────────────────────────────────
    // Pure blacks
    const ImVec4 black          = ImVec4(0.04f, 0.04f, 0.05f, 1.00f);   // #0A0A0D
    const ImVec4 darkGray       = ImVec4(0.07f, 0.07f, 0.09f, 1.00f);   // #121217
    const ImVec4 panelBg        = ImVec4(0.09f, 0.09f, 0.11f, 1.00f);   // #17171C
    const ImVec4 frameBg        = ImVec4(0.11f, 0.11f, 0.13f, 1.00f);   // #1C1C21
    const ImVec4 frameBgHover   = ImVec4(0.15f, 0.15f, 0.18f, 1.00f);   // #26262E
    const ImVec4 frameBgActive  = ImVec4(0.18f, 0.18f, 0.22f, 1.00f);   // #2E2E38

    // Electric yellow / gold accent  (#FFD600)
    const ImVec4 accent         = ImVec4(1.00f, 0.84f, 0.00f, 1.00f);
    const ImVec4 accentHover    = ImVec4(1.00f, 0.90f, 0.25f, 1.00f);
    const ImVec4 accentActive   = ImVec4(0.90f, 0.75f, 0.00f, 1.00f);
    const ImVec4 accentDim      = ImVec4(0.55f, 0.46f, 0.00f, 1.00f);
    const ImVec4 accentSubtle   = ImVec4(1.00f, 0.84f, 0.00f, 0.15f);
    const ImVec4 accentGlow     = ImVec4(1.00f, 0.84f, 0.00f, 0.08f);   // very faint glow

    // Neon border — dim yellow for that cyberpunk outline glow
    const ImVec4 neonBorder     = ImVec4(0.40f, 0.34f, 0.00f, 0.50f);   // warm dim glow
    const ImVec4 neonBorderBright = ImVec4(0.70f, 0.59f, 0.00f, 0.70f);

    // Text
    const ImVec4 textPrimary    = ImVec4(0.93f, 0.93f, 0.94f, 1.00f);
    const ImVec4 textDisabled   = ImVec4(0.36f, 0.36f, 0.38f, 1.00f);

    // Borders
    const ImVec4 border         = ImVec4(0.20f, 0.19f, 0.10f, 1.00f);
    const ImVec4 borderShadow   = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);

    ImVec4* c = style.Colors;

    // ── Text ─────────────────────────────────────────────────────────────
    c[ImGuiCol_Text]                  = textPrimary;
    c[ImGuiCol_TextDisabled]          = textDisabled;

    // ── Backgrounds ──────────────────────────────────────────────────────
    c[ImGuiCol_WindowBg]              = panelBg;
    c[ImGuiCol_ChildBg]               = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    c[ImGuiCol_PopupBg]               = ImVec4(0.06f, 0.06f, 0.08f, 0.97f);
    c[ImGuiCol_MenuBarBg]             = black;

    // ── Borders ──────────────────────────────────────────────────────────
    c[ImGuiCol_Border]                = border;
    c[ImGuiCol_BorderShadow]          = borderShadow;

    // ── Frames (inputs, checkboxes, sliders bg) — neon outline ──────────
    c[ImGuiCol_FrameBg]               = frameBg;
    c[ImGuiCol_FrameBgHovered]        = frameBgHover;
    c[ImGuiCol_FrameBgActive]         = frameBgActive;

    // ── Title bar ────────────────────────────────────────────────────────
    c[ImGuiCol_TitleBg]               = black;
    c[ImGuiCol_TitleBgActive]         = ImVec4(0.06f, 0.06f, 0.07f, 1.00f);
    c[ImGuiCol_TitleBgCollapsed]      = ImVec4(0.04f, 0.04f, 0.05f, 0.80f);

    // ── Scrollbar ────────────────────────────────────────────────────────
    c[ImGuiCol_ScrollbarBg]           = ImVec4(0.05f, 0.05f, 0.06f, 0.60f);
    c[ImGuiCol_ScrollbarGrab]         = ImVec4(0.30f, 0.26f, 0.06f, 1.00f);
    c[ImGuiCol_ScrollbarGrabHovered]  = accentDim;
    c[ImGuiCol_ScrollbarGrabActive]   = accent;

    // ── Buttons ──────────────────────────────────────────────────────────
    c[ImGuiCol_Button]                = ImVec4(0.14f, 0.14f, 0.16f, 1.00f);
    c[ImGuiCol_ButtonHovered]         = ImVec4(0.28f, 0.26f, 0.10f, 1.00f);
    c[ImGuiCol_ButtonActive]          = accent;

    // ── Headers (CollapsingHeader, Selectable) ───────────────────────────
    c[ImGuiCol_Header]                = ImVec4(0.14f, 0.14f, 0.16f, 1.00f);
    c[ImGuiCol_HeaderHovered]         = accentSubtle;
    c[ImGuiCol_HeaderActive]          = ImVec4(1.00f, 0.84f, 0.00f, 0.28f);

    // ── Separator ────────────────────────────────────────────────────────
    c[ImGuiCol_Separator]             = neonBorder;
    c[ImGuiCol_SeparatorHovered]      = accentHover;
    c[ImGuiCol_SeparatorActive]       = accent;

    // ── Resize grip ──────────────────────────────────────────────────────
    c[ImGuiCol_ResizeGrip]            = ImVec4(1.00f, 0.84f, 0.00f, 0.10f);
    c[ImGuiCol_ResizeGripHovered]     = ImVec4(1.00f, 0.84f, 0.00f, 0.40f);
    c[ImGuiCol_ResizeGripActive]      = accent;

    // ── Tabs — strong yellow on active ───────────────────────────────────
    c[ImGuiCol_Tab]                   = darkGray;
    c[ImGuiCol_TabHovered]            = ImVec4(0.30f, 0.27f, 0.08f, 1.00f);
    c[ImGuiCol_TabActive]             = ImVec4(0.35f, 0.30f, 0.04f, 1.00f);  // strong yellow tint
    c[ImGuiCol_TabUnfocused]          = black;
    c[ImGuiCol_TabUnfocusedActive]    = ImVec4(0.18f, 0.16f, 0.04f, 1.00f);

    // ── Docking ──────────────────────────────────────────────────────────
    c[ImGuiCol_DockingPreview]        = ImVec4(1.00f, 0.84f, 0.00f, 0.45f);
    c[ImGuiCol_DockingEmptyBg]        = black;

    // ── Checkmark / Slider grab ──────────────────────────────────────────
    c[ImGuiCol_CheckMark]             = accent;
    c[ImGuiCol_SliderGrab]            = accentDim;
    c[ImGuiCol_SliderGrabActive]      = accent;

    // ── Plot ─────────────────────────────────────────────────────────────
    c[ImGuiCol_PlotLines]             = accent;
    c[ImGuiCol_PlotLinesHovered]      = accentHover;
    c[ImGuiCol_PlotHistogram]         = accent;
    c[ImGuiCol_PlotHistogramHovered]  = accentHover;

    // ── Table ────────────────────────────────────────────────────────────
    c[ImGuiCol_TableHeaderBg]         = ImVec4(0.10f, 0.10f, 0.12f, 1.00f);
    c[ImGuiCol_TableBorderStrong]     = neonBorder;
    c[ImGuiCol_TableBorderLight]      = ImVec4(0.16f, 0.15f, 0.08f, 1.00f);
    c[ImGuiCol_TableRowBg]            = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    c[ImGuiCol_TableRowBgAlt]         = ImVec4(1.00f, 1.00f, 1.00f, 0.015f);

    // ── Misc ─────────────────────────────────────────────────────────────
    c[ImGuiCol_TextSelectedBg]        = ImVec4(1.00f, 0.84f, 0.00f, 0.25f);
    c[ImGuiCol_DragDropTarget]        = accent;
    c[ImGuiCol_NavHighlight]          = accent;
    c[ImGuiCol_NavWindowingHighlight] = ImVec4(1.00f, 0.84f, 0.00f, 0.55f);
    c[ImGuiCol_NavWindowingDimBg]     = ImVec4(0.00f, 0.00f, 0.00f, 0.65f);
    c[ImGuiCol_ModalWindowDimBg]      = ImVec4(0.00f, 0.00f, 0.00f, 0.75f);
}

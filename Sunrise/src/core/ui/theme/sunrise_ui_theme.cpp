#include "sunrise_ui_theme.h"

#include <algorithm>
#include <cmath>
#include <imgui.h>

#include "../scaling/dpi/ui_dpi_scaling.h"

namespace sunrise::core::ui::theme {
namespace {

/** 8-pixel rounding softens the main surface without wasting panel space. */
constexpr float kWindowRounding = 8.0F;
/** 6-pixel rounding keeps controls distinct from the main surface. */
constexpr float kControlRounding = 6.0F;
/** 1-pixel borders stay clear at common display scales. */
constexpr float kBorderWidth = 1.0F;
/** Controls use color contrast instead of a second inner border. */
constexpr float kNoFrameBorderWidth = 0.0F;
/** 12 by 10 padding leaves room around dense settings controls. */
constexpr ImVec2 kWindowPadding{12.0F, 10.0F};
/** 10 by 6 spacing keeps settings easy to scan. */
constexpr ImVec2 kItemSpacing{10.0F, 6.0F};
/** 7 by 4 padding gives navigation and settings controls one shared spacing. */
constexpr ImVec2 kFramePadding{7.0F, 4.0F};

/** Near-white text stays readable on every Sunrise panel. */
constexpr ImVec4 kText{0.92F, 0.94F, 0.97F, 1.0F};
/** Muted blue-gray text marks inactive or explanatory content. */
constexpr ImVec4 kMutedText{0.48F, 0.53F, 0.61F, 1.0F};
/** The darkest blue-gray forms the main window canvas. */
constexpr ImVec4 kWindow{0.035F, 0.043F, 0.058F, 0.98F};
/** A raised blue-gray separates child panels from the main canvas. */
constexpr ImVec4 kPanel{0.055F, 0.066F, 0.087F, 1.0F};
/** A lighter panel tone marks passive controls and scrollbars. */
constexpr ImVec4 kControl{0.095F, 0.11F, 0.14F, 1.0F};
/** The hover tone gives a restrained pointer response. */
constexpr ImVec4 kControlHovered{0.14F, 0.16F, 0.20F, 1.0F};
/** Sunrise orange identifies selection and active controls. */
constexpr ImVec4 kAccent{0.95F, 0.42F, 0.16F, 1.0F};
/** A brighter orange keeps hovered active controls distinct. */
constexpr ImVec4 kAccentHovered{1.0F, 0.52F, 0.22F, 1.0F};
/** A deeper orange keeps text contrast on pressed controls. */
constexpr ImVec4 kAccentActive{0.82F, 0.31F, 0.10F, 1.0F};
/** A cool low-contrast edge separates panels without bright outlines. */
constexpr ImVec4 kBorder{0.16F, 0.19F, 0.24F, 1.0F};
/** Selection uses a translucent accent so selected text stays readable. */
constexpr ImVec4 kSelection{0.95F, 0.42F, 0.16F, 0.28F};
/** An unselected tab sits between the panel and a passive control. */
constexpr ImVec4 kTab{0.075F, 0.088F, 0.113F, 1.0F};
/** A selected tab reads as part of the panel below it. */
constexpr ImVec4 kTabSelected{0.12F, 0.14F, 0.175F, 1.0F};
/** A table header sits one step above the panel it is drawn on. */
constexpr ImVec4 kTableHeader{0.10F, 0.12F, 0.155F, 1.0F};
/** Alternate table rows shift just enough to follow a wide row across. */
constexpr ImVec4 kTableRowAlt{1.0F, 1.0F, 1.0F, 0.025F};
/** A zero-alpha shadow turns off the unused second window edge. */
constexpr ImVec4 kTransparent{};

/** Current base theme. Sunrise Original remains the default. */
Style g_selected = Style::rgb;

/**
 * Optional animation setting.
 * Phase 1 stores the state; ImAnim will consume it in the next integration stage.
 */
bool g_animated = false;

/** @return A brighter version of the supplied RGB accent. */
[[nodiscard]] ImVec4 rgb_hovered(const ImVec4& accent) noexcept {
    return {
        (std::min)(1.0F, accent.x + 0.15F),
        (std::min)(1.0F, accent.y + 0.15F),
        (std::min)(1.0F, accent.z + 0.15F),
        1.0F,
    };
}

/** @return A darker version of the supplied RGB accent. */
[[nodiscard]] ImVec4 rgb_active(const ImVec4& accent) noexcept {
    return {
        accent.x * 0.78F,
        accent.y * 0.78F,
        accent.z * 0.78F,
        1.0F,
    };
}

/**
 * Applies the colors that depend on the selected accent.
 * @param style Style receiving the colors.
 * @param accent Primary accent.
 * @param accentHovered Brighter interactive accent.
 * @param accentActive Darker pressed accent.
 */
void apply_accent(ImGuiStyle& style,
                  const ImVec4& accent,
                  const ImVec4& accentHovered,
                  const ImVec4& accentActive) noexcept {
    ImVec4 selection = accent;
    selection.w = 0.28F;

    ImVec4* colors = style.Colors;

    colors[ImGuiCol_FrameBgActive] = accentActive;
    colors[ImGuiCol_ScrollbarGrabActive] = accentActive;
    colors[ImGuiCol_CheckMark] = accent;
    colors[ImGuiCol_SliderGrab] = accent;
    colors[ImGuiCol_SliderGrabActive] = accentHovered;
    colors[ImGuiCol_ButtonActive] = accentActive;
    colors[ImGuiCol_Header] = selection;
    colors[ImGuiCol_HeaderActive] = accentActive;
    colors[ImGuiCol_SeparatorHovered] = accent;
    colors[ImGuiCol_SeparatorActive] = accentHovered;
    colors[ImGuiCol_ResizeGrip] = selection;
    colors[ImGuiCol_ResizeGripHovered] = accent;
    colors[ImGuiCol_ResizeGripActive] = accentHovered;
    colors[ImGuiCol_TextSelectedBg] = selection;
    colors[ImGuiCol_NavCursor] = accent;
    colors[ImGuiCol_TabSelectedOverline] = accent;
    colors[ImGuiCol_TextLink] = accent;
    colors[ImGuiCol_DragDropTarget] = accent;
}

} // namespace

/** @return The current color in the slow animated RGB cycle. */
ImVec4 animated_border_color() noexcept {
    /** One complete RGB cycle every 20 seconds. */
    constexpr float kCyclesPerSecond = 0.05F;
    constexpr float kSaturation = 0.85F;
    constexpr float kBrightness = 1.0F;

    const float hue = std::fmod(static_cast<float>(ImGui::GetTime()) * kCyclesPerSecond, 1.0F);

    float red = 0.0F;
    float green = 0.0F;
    float blue = 0.0F;

    ImGui::ColorConvertHSVtoRGB(hue, kSaturation, kBrightness, red, green, blue);

    return {red, green, blue, 1.0F};
}

/** Applies the selected colors and a fresh DPI-scaled copy of every authored size. */
void apply() noexcept {
    if (ImGui::GetCurrentContext() == nullptr) {
        return;
    }

    const float fontSizeBase = ImGui::GetStyle().FontSizeBase;

    ImGuiStyle style{};
    ImGui::StyleColorsDark(&style);

    style.WindowPadding = kWindowPadding;
    style.FramePadding = kFramePadding;
    style.ItemSpacing = kItemSpacing;
    style.WindowRounding = kWindowRounding;
    style.ChildRounding = kControlRounding;
    style.PopupRounding = kControlRounding;
    style.FrameRounding = kControlRounding;
    style.GrabRounding = kControlRounding;
    style.ScrollbarRounding = kControlRounding;
    style.WindowBorderSize = kBorderWidth;
    style.ChildBorderSize = kBorderWidth;
    style.PopupBorderSize = kBorderWidth;
    style.FrameBorderSize = kNoFrameBorderWidth;

    ImVec4* colors = style.Colors;

    colors[ImGuiCol_Text] = kText;
    colors[ImGuiCol_TextDisabled] = kMutedText;
    colors[ImGuiCol_WindowBg] = kWindow;
    colors[ImGuiCol_ChildBg] = kPanel;
    colors[ImGuiCol_PopupBg] = kPanel;
    colors[ImGuiCol_Border] = kBorder;
    colors[ImGuiCol_BorderShadow] = kTransparent;
    colors[ImGuiCol_FrameBg] = kControl;
    colors[ImGuiCol_FrameBgHovered] = kControlHovered;
    colors[ImGuiCol_TitleBg] = kWindow;
    colors[ImGuiCol_TitleBgActive] = kWindow;
    colors[ImGuiCol_TitleBgCollapsed] = kWindow;
    colors[ImGuiCol_ScrollbarBg] = kPanel;
    colors[ImGuiCol_ScrollbarGrab] = kControl;
    colors[ImGuiCol_ScrollbarGrabHovered] = kControlHovered;
    colors[ImGuiCol_Button] = kControl;
    colors[ImGuiCol_ButtonHovered] = kControlHovered;
    colors[ImGuiCol_HeaderHovered] = kControlHovered;
    colors[ImGuiCol_Separator] = kBorder;

    colors[ImGuiCol_Tab] = kTab;
    colors[ImGuiCol_TabHovered] = kControlHovered;
    colors[ImGuiCol_TabSelected] = kTabSelected;
    colors[ImGuiCol_TabDimmed] = kPanel;
    colors[ImGuiCol_TabDimmedSelected] = kControl;
    colors[ImGuiCol_TabDimmedSelectedOverline] = kBorder;
    colors[ImGuiCol_TableHeaderBg] = kTableHeader;
    colors[ImGuiCol_TableBorderStrong] = kBorder;
    colors[ImGuiCol_TableBorderLight] = kBorder;
    colors[ImGuiCol_TableRowBg] = kTransparent;
    colors[ImGuiCol_TableRowBgAlt] = kTableRowAlt;
    colors[ImGuiCol_TreeLines] = kBorder;

    if (g_selected == Style::rgb) {
        const ImVec4 accent = animated_border_color();
        apply_accent(style, accent, rgb_hovered(accent), rgb_active(accent));
    } else {
        // These are Stan's authored Sunrise accent values.
        apply_accent(style, kAccent, kAccentHovered, kAccentActive);

        // Keep the exact authored Sunrise selection value.
        colors[ImGuiCol_Header] = kSelection;
        colors[ImGuiCol_ResizeGrip] = kSelection;
        colors[ImGuiCol_TextSelectedBg] = kSelection;
    }

    // Scaling a fresh default style stops repeated monitor changes from building up error.
    const float scale = scaling::dpi::current();
    style.ScaleAllSizes(scale);

    // ScaleAllSizes truncates this one to a whole number, so any factor below 1 zeroes it and the
    // cursor draws with no area. Held at 1 instead, which is the size it is authored at.
    style.MouseCursorScale = (std::max)(1.0F, style.MouseCursorScale);
    style.FontSizeBase = fontSizeBase;
    style.FontScaleMain = scale;

    ImGui::GetStyle() = style;
}

/** Updates colors that change continuously while a frame is running. */
void update() noexcept {
    if (ImGui::GetCurrentContext() == nullptr || g_selected != Style::rgb) {
        return;
    }

    ImGuiStyle& style = ImGui::GetStyle();

    const ImVec4 accent = animated_border_color();
    apply_accent(style, accent, rgb_hovered(accent), rgb_active(accent));
}

/** @return The currently selected base theme. */
Style selected() noexcept {
    return g_selected;
}

/** Selects the base theme and immediately applies it when possible. */
void set_selected(Style style) noexcept {
    if (style != Style::sunriseOriginal && style != Style::rgb) {
        return;
    }

    if (g_selected == style) {
        return;
    }

    g_selected = style;

    if (ImGui::GetCurrentContext() != nullptr) {
        apply();
    }
}

/** @return Display name for a base theme. */
const char* display_name(Style style) noexcept {
    switch (style) {
    case Style::sunriseOriginal:
        return "Sunrise Original";

    case Style::rgb:
        return "RGB";

    default:
        return "Sunrise Original";
    }
}

/** @return True when the optional animation layer is enabled. */
bool animated() noexcept {
    return g_animated;
}

/** Enables or disables the optional animation layer. */
void set_animated(bool enabled) noexcept {
    g_animated = enabled;
}

} // namespace sunrise::core::ui::theme

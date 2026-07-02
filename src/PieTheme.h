#pragma once
#include <cstdint>
#include <imgui.h>

// Optional integration with the Pie UI sibling addon. Pie UI broadcasts its
// active colour palette over the Nexus event bus; Crafty Legend subscribes and
// tints its own ImGui UI to match. Pie UI is a SOFT dependency: when absent,
// disabled via the user toggle, or before its first broadcast, Active() is
// false and callers fall back to Crafty Legend's built-in purple/gold theme.
// The event handler may run off the render thread, so the cached palette is
// mutex-guarded and colours are applied on the render thread by the caller.

#define PIEUI_THEME_VERSION    1u
#define PIEUI_THEME_MAX_COLORS 96

// Fixed ABI mirrored from Pie UI. Pie ships its ENTIRE ImGui colour array in one
// go: colors[] is indexed by the ImGuiCol_ enum and copies straight into
// ImGuiStyle.Colors[] with no per-control mapping. `accent` is separate (Pie's
// signature trim-aware highlight) and is NOT an ImGuiCol slot. Colours are
// IM_COL32 values packed 0xAABBGGRR.
struct PieUiTheme {
    uint32_t version;                        // == PIEUI_THEME_VERSION; ignore if unknown
    uint32_t accent;                         // signature highlight (trim-aware); NOT an ImGuiCol
    uint32_t count;                          // valid entries in colors[] (Pie's ImGuiCol_COUNT)
    uint32_t colors[PIEUI_THEME_MAX_COLORS]; // IM_COL32 (0xAABBGGRR), indexed by ImGuiCol_
};

namespace PieTheme {

void Init();      // subscribe to EV_PIEUI_THEME + raise EV_PIEUI_REQUEST_THEME. Call from AddonLoad.
void Shutdown();  // unsubscribe from EV_PIEUI_THEME. Call from AddonUnload.

bool       HasPalette();  // true once a version-matched palette has been received
bool       Active();      // g_UsePieTheme && HasPalette()
PieUiTheme Palette();     // copy of the last-received palette (guarded)
uint32_t   Accent();      // raw accent IM_COL32 (guarded); 0 if no palette

ImVec4 Unpack(uint32_t packed);            // 0xAABBGGRR -> normalised ImVec4
ImVec4 AccentRGB(const ImVec4& fallback);  // accent RGB + fallback alpha when Active(), else fallback
ImU32  AccentU32(ImU32 fallback);          // accent RGB + fallback alpha byte when Active(), else fallback

} // namespace PieTheme

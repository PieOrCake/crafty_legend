#include "PieTheme.h"
#include "globals.h"   // APIDefs, g_UsePieTheme

#include <mutex>

namespace PieTheme {
namespace {
    std::mutex s_mutex;
    PieUiTheme s_palette{};
    bool       s_hasPalette = false;
    bool       s_subscribed = false;

    const char* const EV_THEME         = "EV_PIEUI_THEME";
    const char* const EV_REQUEST_THEME = "EV_PIEUI_REQUEST_THEME";

    // May be called on a non-render thread. Copy the struct; never store the pointer.
    void OnTheme(void* payload) {
        if (!payload) return;
        const PieUiTheme* incoming = (const PieUiTheme*)payload;
        if (incoming->version != PIEUI_THEME_VERSION) return; // ignore unknown versions
        std::lock_guard<std::mutex> lock(s_mutex);
        s_palette = *incoming;
        s_hasPalette = true;
    }
} // namespace

void Init() {
    if (!APIDefs || !APIDefs->Events_Subscribe) return;
    APIDefs->Events_Subscribe(EV_THEME, OnTheme);
    s_subscribed = true;
    // Pie may have loaded first: ask it to re-broadcast now.
    if (APIDefs->Events_Raise)
        APIDefs->Events_Raise(EV_REQUEST_THEME, nullptr);
}

void Shutdown() {
    if (APIDefs && s_subscribed && APIDefs->Events_Unsubscribe)
        APIDefs->Events_Unsubscribe(EV_THEME, OnTheme);
    s_subscribed = false;
}

bool HasPalette() {
    std::lock_guard<std::mutex> lock(s_mutex);
    return s_hasPalette;
}

PieUiTheme Palette() {
    std::lock_guard<std::mutex> lock(s_mutex);
    return s_palette;
}

bool Active() {
    return g_UsePieTheme && HasPalette();
}

uint32_t Accent() {
    std::lock_guard<std::mutex> lock(s_mutex);
    return s_hasPalette ? s_palette.accent : 0u;
}

ImVec4 Unpack(uint32_t c) {
    return ImVec4(
        ( c        & 0xFF) / 255.0f,
        ((c >> 8)  & 0xFF) / 255.0f,
        ((c >> 16) & 0xFF) / 255.0f,
        ((c >> 24) & 0xFF) / 255.0f);
}

ImVec4 AccentRGB(const ImVec4& fallback) {
    if (!Active()) return fallback;
    ImVec4 a = Unpack(Accent());
    return ImVec4(a.x, a.y, a.z, fallback.w);
}

ImU32 AccentU32(ImU32 fallback) {
    if (!Active()) return fallback;
    ImVec4 a = Unpack(Accent());
    ImU32  alpha = (fallback >> IM_COL32_A_SHIFT) & 0xFF;
    return IM_COL32((int)(a.x * 255.0f + 0.5f), (int)(a.y * 255.0f + 0.5f), (int)(a.z * 255.0f + 0.5f), (int)alpha);
}

} // namespace PieTheme

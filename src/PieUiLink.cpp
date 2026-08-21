#include "PieUiLink.h"
#include "globals.h"   // APIDefs

#include <atomic>
#include <cctype>

namespace CraftyLegend {
namespace PieUiLink {
namespace {

    // Pie UI event identifiers, mirrored from pie_ui/src/PieUiAPI.h. Note the
    // inconsistent spelling is Pie's, not a typo: the discovery handshake uses
    // EV_PIE_UI_*, everything else EV_PIEUI_*.
    const char* const EV_PIE_UI_PING         = "EV_PIE_UI_PING";          // we raise
    const char* const EV_PIE_UI_READY        = "EV_PIE_UI_READY";         // we subscribe
    const char* const EV_PIEUI_OPEN_CHATLINK = "EV_PIEUI_OPEN_CHATLINK";  // we raise

    // Latched by OnReady, which may run off the render thread.
    std::atomic<bool> s_present{false};
    bool              s_subscribed = false;

    void OnReady(void*) {
        s_present.store(true);
    }

} // namespace

void Init() {
    if (!APIDefs || !APIDefs->Events_Subscribe) return;
    // Subscribe before pinging: catches a Pie that loaded first (it replies to
    // the ping) as well as one that loads later (it announces on its own load).
    APIDefs->Events_Subscribe(EV_PIE_UI_READY, OnReady);
    s_subscribed = true;
    if (APIDefs->Events_Raise)
        APIDefs->Events_Raise(EV_PIE_UI_PING, nullptr);
}

void Shutdown() {
    if (APIDefs && s_subscribed && APIDefs->Events_Unsubscribe)
        APIDefs->Events_Unsubscribe(EV_PIE_UI_READY, OnReady);
    s_subscribed = false;
}

bool Present() {
    return s_present.load();
}

std::string MakeItemChatcode(uint32_t item_id) {
    static const char* B64 =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    const unsigned char b[6] = {
        0x02,                                    // link type: item
        0x01,                                    // count
        (unsigned char)( item_id        & 0xFF),
        (unsigned char)((item_id >>  8) & 0xFF),
        (unsigned char)((item_id >> 16) & 0xFF),
        0x00                                     // flags: no skin, no upgrades
    };
    std::string out = "[&";
    for (int i = 0; i < 6; i += 3) {             // 6 bytes divides exactly: no padding
        unsigned n = (b[i] << 16) | (b[i + 1] << 8) | b[i + 2];
        out += B64[(n >> 18) & 0x3F];
        out += B64[(n >> 12) & 0x3F];
        out += B64[(n >>  6) & 0x3F];
        out += B64[ n        & 0x3F];
    }
    return out + "]";
}

void OpenItemPreview(uint32_t item_id) {
    if (!APIDefs || !APIDefs->Events_Raise || item_id == 0) return;
    if (!s_present.load()) return;
    // Nexus delivers synchronously and Pie copies the string immediately, so
    // the local buffer need not outlive the raise.
    const std::string code = MakeItemChatcode(item_id);
    APIDefs->Events_Raise(EV_PIEUI_OPEN_CHATLINK, (void*)code.c_str());
}

bool IsPreviewableType(const std::string& type) {
    std::string t;
    t.reserve(type.size());
    for (char c : type) {
        if (c == '_' || c == ' ' || c == '-') continue;
        t += (char)std::tolower((unsigned char)c);
    }
    return t == "weapon" || t == "armor" || t == "armour" || t == "back";
}

} // namespace PieUiLink
} // namespace CraftyLegend

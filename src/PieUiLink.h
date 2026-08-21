#pragma once
#include <cstdint>
#include <string>

// Optional integration with the Pie UI sibling addon's chat-link service.
//
// Pie UI exposes one generic door, EV_PIEUI_OPEN_CHATLINK: raise it with a
// NUL-terminated "[&...]" GW2 chat-link string and Pie decodes it and performs
// that link's native action on the game thread. For an ITEM link (type byte
// 0x02) that action is the native wardrobe preview - the same window the user
// gets by clicking the link in chat. Crafty Legend never touches game memory
// and makes no game calls; it only encodes an item id and raises the event.
//
// Pie UI is a SOFT dependency. Present() latches false until Pie announces
// itself, and OpenItemPreview() on an absent Pie is a harmless no-op raise.
//
// Contract source: pie_ui/src/PieUiAPI.h (PIEUI_API_VERSION 1). The event-name
// strings are mirrored in the .cpp rather than vendored, matching how
// PieTheme.cpp already handles the theme events.

namespace CraftyLegend {
namespace PieUiLink {

void Init();      // subscribe EV_PIE_UI_READY + raise EV_PIE_UI_PING. Call from AddonLoad.
void Shutdown();  // unsubscribe EV_PIE_UI_READY. Call from AddonUnload.

// True once Pie UI has announced readiness this session. Latches: Pie has no
// "unloading" event, so a stale true can at worst leave a menu entry whose
// raise goes nowhere. Safe to poll every frame (may be set off the render
// thread, so it is an atomic behind this accessor).
bool Present();

// Encode an item id as a minimal six-byte GW2 item chat link, base64'd inside
// "[&...]": {0x02 type, 0x01 count, id lo/mid/hi, 0x00 flags}. Count 1 and no
// flags is all a preview needs (the flag bits select the optional skin and
// upgrade fields). Six bytes divide by three exactly, so there is no padding.
std::string MakeItemChatcode(uint32_t item_id);

// Ask Pie UI to open the native wardrobe preview for this item id. No-op when
// Pie is absent or the id is zero. Fire and forget - Pie returns nothing, and
// silently ignores anything it cannot preview.
void OpenItemPreview(uint32_t item_id);

// Whether an item of this data type has a native preview at all. Weapons,
// armour and back items do; trinkets, upgrade components and crafting
// materials have no preview slot and would open nothing. Accepts the raw
// `type` string from legendaries.json / items.json, whose spelling is not
// consistent (e.g. "upgrade_component" vs "upgradecomponent").
bool IsPreviewableType(const std::string& type);

} // namespace PieUiLink
} // namespace CraftyLegend

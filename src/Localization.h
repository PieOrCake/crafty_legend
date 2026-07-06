#pragma once
#include <cstdint>
#include <string>

// Crafty Legend internationalization (EN/DE/FR/ES — the languages Decoder Ring
// localizes). Localization is DISPLAY-ONLY: internal keys (wallet-by-name, the
// vendor-cost name map, alias groups) stay English; only rendered strings change.
//
// Language source, in priority order:
//   1. Decoder Ring's GetActiveLanguage() (API v5+) — the resolved language DR
//      is serving item names in, so CL matches it exactly.
//   2. Fallback: a sentinel probe against the Nexus Localization API (works when
//      DR is absent or older than v5).
//
// Text sources:
//   - Item / legendary / material names -> Decoder Ring (ItemName()).
//   - Currency / achievement names      -> GW2API localized fetch (?lang=).
//   - UI chrome                         -> compiled-in table (Tr()).
// All degrade gracefully to English.

namespace Localization {

void Init();      // register sentinel, subscribe DR events, raise DR ping. Call from AddonLoad.
void Shutdown();  // unsubscribe DR events. Call from AddonUnload.

// Call once per frame from AddonRender: re-checks the active language and, on a
// change, clears localized caches and kicks the currency/achievement refetch.
void Poll();

// Active resolved language as an immortal literal: "en" | "de" | "fr" | "es".
const char* ActiveLang();

// True when a compatible Decoder Ring service is currently published.
bool DecoderPresent();

// Localized display name for an item id via Decoder Ring. Returns englishFallback
// immediately when DR is absent, still fetching, or failed (the name pops in a
// frame later once DR resolves it). Never blocks.
std::string ItemName(uint32_t id, const std::string& englishFallback);

// Localized flavour/description text for an item id via Decoder Ring (schema v3+).
// Returns englishFallback when DR is absent/still-fetching, or when DR reports the
// item has no description. Never blocks. Markup is DR-stripped already; callers may
// still StripMarkup the fallback safely.
std::string ItemDescription(uint32_t id, const std::string& englishFallback);

// Localized UI-chrome string for a key. Falls back to the English entry, then to
// the key itself. Returned pointer is stable for the addon's lifetime.
const char* Tr(const char* key);

// Localized Miller-column title. Handles the composed forms ("Craft (WPN 500)",
// "Vendor - Dugan", "<item> - Acquisition", "Mystic Forge (~N attempts)") by
// localizing the descriptor words and keeping discipline codes / vendor names /
// numbers. Falls back to English for free-text acquisition-description titles.
std::string ColumnTitle(const std::string& title);

// One-line human-readable status for the debug window / Nexus log: active
// language, Decoder Ring presence + version + getter availability, cache size.
std::string DiagStatus();

} // namespace Localization

#pragma once
// Per-account record of which characters hold which crafting disciplines, and
// the matching logic behind the "who can craft this?" tooltip.
//
// This module is deliberately platform-free: no Windows headers, no ImGui, no
// DataManager/GW2API/Hoard types. It is compiled natively by tests/run_tests.sh.
// H&S plumbing lives in hoard.cpp; rendering lives in ui_helpers.cpp.
#include <string>
#include <vector>

namespace CraftyLegend {
namespace CharacterCrafting {

    // One discipline held by one character, as reported by
    // /v2/characters/<name>/crafting. `active` is false when the discipline is
    // retained but swapped out (GW2 allows only two active at a time).
    struct DisciplineRating {
        std::string discipline; // API English name, e.g. "Artificer"
        int  rating = 0;
        bool active = false;
    };

    struct CharacterEntry {
        std::string name;
        std::vector<DisciplineRating> disciplines;
    };

    // What we know about an account's character data as a whole.
    enum class DataState {
        NoData,  // nothing fetched and nothing cached
        Loading, // at least one character still outstanding
        Ready,   // every known character has been fetched
        Denied   // the API key lacks the `characters` permission
    };

    // Parse one /v2/characters/<name>/crafting response body.
    // Returns false for malformed JSON or a body with no `crafting` array
    // (which is what a GW2 API error object looks like).
    bool ParseCraftingJson(const std::string& body,
                           std::vector<DisciplineRating>& out);

    // Percent-encode one URL path segment. H&S pastes the endpoint string
    // straight onto https://api.guildwars2.com with no escaping, and GW2
    // character names contain spaces and may contain accented characters.
    std::string UrlEncodePathSegment(const std::string& s);

} // namespace CharacterCrafting
} // namespace CraftyLegend

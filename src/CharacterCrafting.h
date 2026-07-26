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

    // One character's best satisfying discipline for a given recipe.
    struct Match {
        std::string character;
        std::string discipline;
        int rating = 0;
    };

    struct QualificationResult {
        DataState state = DataState::NoData;
        std::vector<Match> canCraftNow; // qualifies, discipline active
        std::vector<Match> needsSwap;   // qualifies, discipline inactive
        bool  hasClosest = false;
        Match closest;                  // best non-qualifying holder
    };

    // Answer "who can craft this?" against an explicit roster.
    // `disciplines` are alternatives: holding ANY of them at >= `rating`
    // qualifies. `rating` 0 means no rating requirement.
    // Each character appears at most once, in `canCraftNow` if any satisfying
    // discipline is active, otherwise in `needsSwap`. Both lists are sorted by
    // rating descending, then character name ascending.
    QualificationResult Evaluate(const std::vector<CharacterEntry>& chars,
                                 const std::vector<std::string>& disciplines,
                                 int rating,
                                 DataState state);

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

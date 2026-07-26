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

    // --- Per-account store ---------------------------------------------
    // All functions below are safe to call from any thread: H&S delivers
    // proxy responses on its own worker thread while the render thread reads.

    // `dataDir` is the addon's data directory (GW2API::GetDataDirectory()).
    // Passed in rather than looked up so this module stays platform-free.
    // Loads the disk cache if one is present. Safe to call repeatedly; each
    // call resets in-memory state and reloads from disk.
    void Init(const std::string& dataDir);

    // Replace the account's character roster (from H&S's account list).
    // Already-fetched disciplines for surviving characters are retained;
    // characters no longer on the roster are dropped.
    void SetAccountCharacters(const std::string& account,
                              const std::vector<std::string>& names);

    // Record one character's disciplines. An empty vector is a valid answer
    // (a character with no crafting at all) and still marks it fetched.
    void SetCharacterDisciplines(const std::string& account,
                                 const std::string& character,
                                 std::vector<DisciplineRating> disciplines);

    // The API key lacks the `characters` permission. Stops the sweep for this
    // account until ForceRefresh.
    void MarkDenied(const std::string& account);

    // Next character on the account still awaiting a fetch, in roster order.
    // Returns false when the sweep is complete or the account is Denied.
    bool NextPendingCharacter(const std::string& account, std::string& out);

    // True when the account has never completed a sweep, or its last completed
    // sweep is more than 24 hours old, or ForceRefresh was called.
    bool IsStale(const std::string& account);

    // Re-queue every character on the account and clear any Denied state.
    // Existing data is kept so the tooltip stays useful during the refetch.
    void ForceRefresh(const std::string& account);

    // The tooltip's question, answered against one account's roster.
    QualificationResult Query(const std::string& account,
                              const std::vector<std::string>& disciplines,
                              int rating);

    void Save();
    void Load();

    // Test seam: pin the clock used by IsStale. 0 restores the real clock.
    void SetNowForTesting(long long epochSeconds);

} // namespace CharacterCrafting
} // namespace CraftyLegend

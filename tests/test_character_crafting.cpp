// Host-side tests for src/CharacterCrafting.cpp. Compiled natively (not MinGW)
// by tests/run_tests.sh — this module is deliberately free of Windows/ImGui
// dependencies so its logic can be tested off-game.
#include "../src/CharacterCrafting.h"
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <string>

static int g_failures = 0;

static void check(bool cond, const char* what) {
    if (cond) {
        printf("  ok   %s\n", what);
    } else {
        printf("  FAIL %s\n", what);
        ++g_failures;
    }
}

using namespace CraftyLegend::CharacterCrafting;

static void test_parse_crafting_json() {
    printf("test_parse_crafting_json\n");
    const std::string body =
        R"({"crafting":[{"discipline":"Artificer","rating":400,"active":true},)"
        R"({"discipline":"Chef","rating":275,"active":false}]})";
    std::vector<DisciplineRating> out;
    check(ParseCraftingJson(body, out), "parses a well-formed body");
    check(out.size() == 2, "returns both disciplines");
    check(out[0].discipline == "Artificer", "keeps the API's English name");
    check(out[0].rating == 400, "reads the rating");
    check(out[0].active == true, "reads active=true");
    check(out[1].active == false, "reads active=false");
}

static void test_parse_rejects_garbage() {
    printf("test_parse_rejects_garbage\n");
    std::vector<DisciplineRating> out;
    check(!ParseCraftingJson("not json at all", out), "rejects non-JSON");
    check(!ParseCraftingJson(R"({"text":"invalid key"})", out),
          "rejects a GW2 API error body");
    check(out.empty(), "leaves the output empty on failure");
}

static void test_parse_empty_crafting_array() {
    printf("test_parse_empty_crafting_array\n");
    std::vector<DisciplineRating> out;
    check(ParseCraftingJson(R"({"crafting":[]})", out),
          "a character with no disciplines is valid, not an error");
    check(out.empty(), "yields no disciplines");
}

static void test_url_encode_path_segment() {
    printf("test_url_encode_path_segment\n");
    check(UrlEncodePathSegment("Ellara Sunspear") == "Ellara%20Sunspear",
          "encodes spaces");
    check(UrlEncodePathSegment("Grim") == "Grim", "leaves plain names alone");
    check(UrlEncodePathSegment("Anna-Lena") == "Anna-Lena",
          "leaves hyphens alone (unreserved)");
    // GW2 names may contain accented characters, which arrive as UTF-8 bytes.
    check(UrlEncodePathSegment("Zo\xc3\xab") == "Zo%C3%AB",
          "percent-encodes UTF-8 bytes as uppercase hex");
}

// --- Task 2: qualification logic ---

static CharacterEntry mkChar(const std::string& name,
                             std::vector<DisciplineRating> d) {
    CharacterEntry c;
    c.name = name;
    c.disciplines = std::move(d);
    return c;
}

static std::vector<CharacterEntry> sampleRoster() {
    return {
        mkChar("Ellara Sunspear", {{"Artificer", 400, true},
                                   {"Chef", 275, false}}),
        mkChar("Grim Ashclaw",    {{"Huntsman", 400, true}}),
        mkChar("Toppled Oak",     {{"Weaponsmith", 400, false}}),
        mkChar("Sela Quickstep",  {{"Artificer", 275, true}}),
    };
}

static void test_evaluate_splits_active_and_inactive() {
    printf("test_evaluate_splits_active_and_inactive\n");
    auto r = Evaluate(sampleRoster(),
                      {"Artificer", "Weaponsmith", "Huntsman"}, 400,
                      DataState::Ready);
    check(r.canCraftNow.size() == 2, "two characters can craft now");
    check(r.needsSwap.size() == 1, "one character needs a swap");
    check(r.needsSwap[0].character == "Toppled Oak", "the swapper is Toppled Oak");
    check(r.needsSwap[0].discipline == "Weaponsmith", "and via Weaponsmith");
}

static void test_evaluate_any_discipline_qualifies() {
    printf("test_evaluate_any_discipline_qualifies\n");
    // Grim holds only Huntsman, but Huntsman is one of the listed alternatives.
    auto r = Evaluate({mkChar("Grim Ashclaw", {{"Huntsman", 400, true}})},
                      {"Artificer", "Weaponsmith", "Huntsman"}, 400,
                      DataState::Ready);
    check(r.canCraftNow.size() == 1, "one listed discipline is enough");
}

static void test_evaluate_rating_floor() {
    printf("test_evaluate_rating_floor\n");
    auto r = Evaluate(sampleRoster(), {"Artificer"}, 400, DataState::Ready);
    check(r.canCraftNow.size() == 1, "only the 400 Artificer qualifies");
    check(r.canCraftNow[0].character == "Ellara Sunspear", "and it is Ellara");
    check(r.needsSwap.empty(), "the 275 Artificer is not a swap candidate");
}

static void test_evaluate_zero_rating_needs_only_the_discipline() {
    printf("test_evaluate_zero_rating_needs_only_the_discipline\n");
    auto r = Evaluate(sampleRoster(), {"Artificer"}, 0, DataState::Ready);
    check(r.canCraftNow.size() == 2, "rating 0 means any rating qualifies");
}

static void test_evaluate_sorts_rating_then_name() {
    printf("test_evaluate_sorts_rating_then_name\n");
    std::vector<CharacterEntry> roster = {
        mkChar("Zara Vane",  {{"Artificer", 400, true}}),
        mkChar("Bram Kolt",  {{"Artificer", 500, true}}),
        mkChar("Alba Frost", {{"Artificer", 400, true}}),
    };
    auto r = Evaluate(roster, {"Artificer"}, 400, DataState::Ready);
    check(r.canCraftNow.size() == 3, "all three qualify");
    check(r.canCraftNow[0].character == "Bram Kolt", "highest rating first");
    check(r.canCraftNow[1].character == "Alba Frost", "then A-Z");
    check(r.canCraftNow[2].character == "Zara Vane", "then A-Z");
}

static void test_evaluate_picks_best_discipline_per_character() {
    printf("test_evaluate_picks_best_discipline_per_character\n");
    std::vector<CharacterEntry> roster = {
        mkChar("Dual Wield", {{"Artificer", 400, true},
                              {"Weaponsmith", 500, true}}),
    };
    auto r = Evaluate(roster, {"Artificer", "Weaponsmith"}, 400,
                      DataState::Ready);
    check(r.canCraftNow.size() == 1, "a character is listed once, not twice");
    check(r.canCraftNow[0].discipline == "Weaponsmith",
          "reported via the highest-rated satisfying discipline");
    check(r.canCraftNow[0].rating == 500, "with that discipline's rating");
}

static void test_evaluate_prefers_active_over_higher_inactive() {
    printf("test_evaluate_prefers_active_over_higher_inactive\n");
    // A character who can craft now should never be demoted to "needs a swap"
    // just because an inactive discipline of theirs rates higher.
    std::vector<CharacterEntry> roster = {
        mkChar("Swapper", {{"Artificer", 400, true},
                           {"Weaponsmith", 500, false}}),
    };
    auto r = Evaluate(roster, {"Artificer", "Weaponsmith"}, 400,
                      DataState::Ready);
    check(r.canCraftNow.size() == 1, "listed as able to craft now");
    check(r.needsSwap.empty(), "and not also listed as needing a swap");
    check(r.canCraftNow[0].discipline == "Artificer",
          "reported via the active discipline");
}

static void test_evaluate_closest_when_nobody_qualifies() {
    printf("test_evaluate_closest_when_nobody_qualifies\n");
    auto r = Evaluate(sampleRoster(), {"Artificer"}, 500, DataState::Ready);
    check(r.canCraftNow.empty(), "nobody can craft now");
    check(r.needsSwap.empty(), "and nobody is a swap away");
    check(r.hasClosest, "a closest holder is reported");
    check(r.closest.character == "Ellara Sunspear", "the 400 Artificer");
    check(r.closest.rating == 400, "with her rating");
    check(r.closest.discipline == "Artificer", "and the discipline");
}

static void test_evaluate_no_closest_when_nobody_has_the_discipline() {
    printf("test_evaluate_no_closest_when_nobody_has_the_discipline\n");
    auto r = Evaluate(sampleRoster(), {"Scribe"}, 400, DataState::Ready);
    check(r.canCraftNow.empty(), "nobody qualifies");
    check(!r.hasClosest, "and there is no closest to report");
}

static void test_evaluate_passes_state_through() {
    printf("test_evaluate_passes_state_through\n");
    auto r = Evaluate({}, {"Artificer"}, 400, DataState::Denied);
    check(r.state == DataState::Denied, "state is echoed for the renderer");
    check(r.canCraftNow.empty(), "with no matches");
}

static void test_parse_rejects_wrong_typed_field_without_throwing() {
    printf("test_parse_rejects_wrong_typed_field_without_throwing\n");
    const std::string body =
        R"({"crafting":[{"discipline":"Artificer","rating":"400","active":true}]})";
    std::vector<DisciplineRating> out;
    bool threw = false;
    bool result = false;
    try {
        result = ParseCraftingJson(body, out);
    } catch (...) {
        threw = true;
    }
    check(!threw, "does not throw on a wrong-typed field");
    check(!result, "and rejects the malformed entry");
    check(out.empty(), "leaving the output empty");
}

// --- Task 3: store, cache and staleness ---

static const char* kTestDir = "build/tests/data";

static void resetStore() {
    // A fresh Init on an empty directory clears in-memory state.
    system("rm -rf build/tests/data && mkdir -p build/tests/data");
    Init(kTestDir);
}

static void test_pending_characters_drain_in_order() {
    printf("test_pending_characters_drain_in_order\n");
    resetStore();
    SetAccountCharacters("Acct.1234", {"Ellara Sunspear", "Grim Ashclaw"});
    std::string next;
    check(NextPendingCharacter("Acct.1234", next), "first character is pending");
    check(next == "Ellara Sunspear", "and it is the first in the list");
    SetCharacterDisciplines("Acct.1234", "Ellara Sunspear",
                            {{"Artificer", 400, true}});
    check(NextPendingCharacter("Acct.1234", next), "second is still pending");
    check(next == "Grim Ashclaw", "and it is the second in the list");
    SetCharacterDisciplines("Acct.1234", "Grim Ashclaw", {});
    check(!NextPendingCharacter("Acct.1234", next), "nothing left pending");
}

static void test_state_progresses_to_ready() {
    printf("test_state_progresses_to_ready\n");
    resetStore();
    check(Query("Acct.1234", {"Artificer"}, 400).state == DataState::NoData,
          "unknown account reads NoData");
    SetAccountCharacters("Acct.1234", {"Ellara Sunspear"});
    check(Query("Acct.1234", {"Artificer"}, 400).state == DataState::Loading,
          "characters known but unfetched reads Loading");
    SetCharacterDisciplines("Acct.1234", "Ellara Sunspear",
                            {{"Artificer", 400, true}});
    auto r = Query("Acct.1234", {"Artificer"}, 400);
    check(r.state == DataState::Ready, "all fetched reads Ready");
    check(r.canCraftNow.size() == 1, "and the query answers");
}

static void test_denied_is_sticky_and_not_stale() {
    printf("test_denied_is_sticky_and_not_stale\n");
    resetStore();
    SetAccountCharacters("Acct.1234", {"Ellara Sunspear"});
    MarkDenied("Acct.1234");
    check(Query("Acct.1234", {"Artificer"}, 400).state == DataState::Denied,
          "denied account reads Denied");
    std::string next;
    check(!NextPendingCharacter("Acct.1234", next),
          "a denied account stops requesting characters");
    check(!IsStale("Acct.1234"),
          "and is not re-swept until a forced refresh");
    SetNowForTesting(1000000);
    MarkDenied("Acct.1234");
    SetNowForTesting(1000000 + 25 * 3600);
    check(!IsStale("Acct.1234"),
          "and stays not-stale well past the 24-hour window, since only "
          "ForceRefresh can reopen a denied account");
    SetNowForTesting(0); // back to the real clock
}

static void test_accounts_are_isolated() {
    printf("test_accounts_are_isolated\n");
    resetStore();
    SetAccountCharacters("Acct.1111", {"Ellara Sunspear"});
    SetCharacterDisciplines("Acct.1111", "Ellara Sunspear",
                            {{"Artificer", 400, true}});
    SetAccountCharacters("Acct.2222", {"Grim Ashclaw"});
    SetCharacterDisciplines("Acct.2222", "Grim Ashclaw",
                            {{"Huntsman", 400, true}});
    auto a = Query("Acct.1111", {"Artificer"}, 400);
    auto b = Query("Acct.2222", {"Artificer"}, 400);
    check(a.canCraftNow.size() == 1, "account 1 sees its own character");
    check(b.canCraftNow.empty(), "account 2 does not see account 1's");
}

static void test_character_list_change_drops_departed_characters() {
    printf("test_character_list_change_drops_departed_characters\n");
    resetStore();
    SetAccountCharacters("Acct.1234", {"Ellara Sunspear", "Deleted Alt"});
    SetCharacterDisciplines("Acct.1234", "Ellara Sunspear",
                            {{"Artificer", 400, true}});
    SetCharacterDisciplines("Acct.1234", "Deleted Alt",
                            {{"Artificer", 500, true}});
    SetAccountCharacters("Acct.1234", {"Ellara Sunspear"});
    auto r = Query("Acct.1234", {"Artificer"}, 400);
    check(r.canCraftNow.size() == 1, "the deleted character is gone");
    check(r.canCraftNow[0].character == "Ellara Sunspear", "the survivor remains");
    check(r.state == DataState::Ready,
          "the survivor's already-fetched data is kept, not refetched");
}

static void test_cache_round_trips_through_disk() {
    printf("test_cache_round_trips_through_disk\n");
    resetStore();
    SetAccountCharacters("Acct.1234", {"Ellara Sunspear"});
    SetCharacterDisciplines("Acct.1234", "Ellara Sunspear",
                            {{"Artificer", 400, true}, {"Chef", 275, false}});
    Save();
    Init(kTestDir); // fresh start, same directory
    auto r = Query("Acct.1234", {"Artificer"}, 400);
    check(r.state == DataState::Ready, "reloaded account is complete");
    check(r.canCraftNow.size() == 1, "and answers the query from disk");
    check(r.canCraftNow[0].rating == 400, "with the cached rating");
}

static void test_load_tolerates_a_malformed_cache_entry() {
    printf("test_load_tolerates_a_malformed_cache_entry\n");
    resetStore();
    SetAccountCharacters("Acct.5678", {"Grim Ashclaw"});
    SetCharacterDisciplines("Acct.5678", "Grim Ashclaw",
                            {{"Huntsman", 400, true}});
    Save();

    // Hand-edit the cache: give one account a wrong-typed field alongside the
    // well-formed account Save() just wrote.
    std::string path = std::string(kTestDir) + "/character_crafting.json";
    std::ifstream in(path, std::ios::binary);
    std::string body((std::istreambuf_iterator<char>(in)),
                     std::istreambuf_iterator<char>());
    in.close();
    // Splice a malformed account into the existing "accounts" object.
    const std::string marker = "\"accounts\": {";
    auto pos = body.find(marker);
    check(pos != std::string::npos, "cache file has the expected shape to edit");
    body.insert(pos + marker.size(),
                R"("A.1":{"completed_at":"soon","roster":["X"]},)");
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    out << body;
    out.close();

    bool threw = false;
    try {
        Init(kTestDir);
    } catch (...) {
        threw = true;
    }
    check(!threw, "Init/Load does not throw on a malformed cache entry");
    auto r = Query("Acct.5678", {"Huntsman"}, 400);
    check(r.state == DataState::Ready,
          "the other, well-formed account survives the malformed one");
    check(r.canCraftNow.size() == 1, "and still answers the query");
    check(Query("A.1", {"Huntsman"}, 400).state == DataState::NoData,
          "the malformed account itself is simply dropped");
}

static void test_staleness_uses_a_24_hour_window() {
    printf("test_staleness_uses_a_24_hour_window\n");
    resetStore();
    SetNowForTesting(1000000);
    check(IsStale("Acct.1234"), "an unknown account is stale");
    SetAccountCharacters("Acct.1234", {"Ellara Sunspear"});
    SetCharacterDisciplines("Acct.1234", "Ellara Sunspear",
                            {{"Artificer", 400, true}});
    check(!IsStale("Acct.1234"), "a freshly completed account is not stale");
    SetNowForTesting(1000000 + 23 * 3600);
    check(!IsStale("Acct.1234"), "still fresh at 23 hours");
    SetNowForTesting(1000000 + 25 * 3600);
    check(IsStale("Acct.1234"), "stale at 25 hours");
    SetNowForTesting(0); // back to the real clock
}

static void test_force_refresh_reopens_the_sweep() {
    printf("test_force_refresh_reopens_the_sweep\n");
    resetStore();
    SetAccountCharacters("Acct.1234", {"Ellara Sunspear"});
    SetCharacterDisciplines("Acct.1234", "Ellara Sunspear",
                            {{"Artificer", 400, true}});
    std::string next;
    check(!NextPendingCharacter("Acct.1234", next), "nothing pending when done");
    ForceRefresh("Acct.1234");
    check(IsStale("Acct.1234"), "forced account reads stale");
    check(NextPendingCharacter("Acct.1234", next), "and re-queues its characters");
    check(next == "Ellara Sunspear", "starting from the first");
    check(Query("Acct.1234", {"Artificer"}, 400).canCraftNow.size() == 1,
          "old data still answers while the refetch is in flight");
}

static void test_force_refresh_clears_denied() {
    printf("test_force_refresh_clears_denied\n");
    resetStore();
    SetAccountCharacters("Acct.1234", {"Ellara Sunspear"});
    MarkDenied("Acct.1234");
    ForceRefresh("Acct.1234");
    std::string next;
    check(NextPendingCharacter("Acct.1234", next),
          "a forced refresh retries a previously denied account");
}

int main() {
    test_parse_crafting_json();
    test_parse_rejects_garbage();
    test_parse_empty_crafting_array();
    test_url_encode_path_segment();
    test_evaluate_splits_active_and_inactive();
    test_evaluate_any_discipline_qualifies();
    test_evaluate_rating_floor();
    test_evaluate_zero_rating_needs_only_the_discipline();
    test_evaluate_sorts_rating_then_name();
    test_evaluate_picks_best_discipline_per_character();
    test_evaluate_prefers_active_over_higher_inactive();
    test_evaluate_closest_when_nobody_qualifies();
    test_evaluate_no_closest_when_nobody_has_the_discipline();
    test_evaluate_passes_state_through();
    test_parse_rejects_wrong_typed_field_without_throwing();
    test_pending_characters_drain_in_order();
    test_state_progresses_to_ready();
    test_denied_is_sticky_and_not_stale();
    test_accounts_are_isolated();
    test_character_list_change_drops_departed_characters();
    test_cache_round_trips_through_disk();
    test_load_tolerates_a_malformed_cache_entry();
    test_staleness_uses_a_24_hour_window();
    test_force_refresh_reopens_the_sweep();
    test_force_refresh_clears_denied();
    if (g_failures) {
        printf("\n%d check(s) FAILED\n", g_failures);
        return 1;
    }
    printf("\nAll checks passed\n");
    return 0;
}

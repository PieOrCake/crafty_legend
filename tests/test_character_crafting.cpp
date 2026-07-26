// Host-side tests for src/CharacterCrafting.cpp. Compiled natively (not MinGW)
// by tests/run_tests.sh — this module is deliberately free of Windows/ImGui
// dependencies so its logic can be tested off-game.
#include "../src/CharacterCrafting.h"
#include <cstdio>
#include <cstdlib>
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
    if (g_failures) {
        printf("\n%d check(s) FAILED\n", g_failures);
        return 1;
    }
    printf("\nAll checks passed\n");
    return 0;
}

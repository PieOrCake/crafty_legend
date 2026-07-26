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

int main() {
    test_parse_crafting_json();
    test_parse_rejects_garbage();
    test_parse_empty_crafting_array();
    test_url_encode_path_segment();
    if (g_failures) {
        printf("\n%d check(s) FAILED\n", g_failures);
        return 1;
    }
    printf("\nAll checks passed\n");
    return 0;
}

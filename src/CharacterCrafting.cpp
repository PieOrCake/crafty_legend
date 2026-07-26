#include "CharacterCrafting.h"

#include <algorithm>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace CraftyLegend {
namespace CharacterCrafting {

bool ParseCraftingJson(const std::string& body,
                       std::vector<DisciplineRating>& out) {
    out.clear();
    json j = json::parse(body, nullptr, false);
    if (j.is_discarded() || !j.is_object()) return false;
    auto it = j.find("crafting");
    if (it == j.end() || !it->is_array()) return false;
    for (const auto& e : *it) {
        if (!e.is_object()) continue;
        DisciplineRating d;
        d.discipline = e.value("discipline", std::string());
        d.rating     = e.value("rating", 0);
        d.active     = e.value("active", false);
        if (d.discipline.empty()) continue;
        out.push_back(std::move(d));
    }
    return true;
}

std::string UrlEncodePathSegment(const std::string& s) {
    static const char* kHex = "0123456789ABCDEF";
    std::string out;
    out.reserve(s.size() + 8);
    for (unsigned char c : s) {
        const bool unreserved =
            (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
            (c >= '0' && c <= '9') ||
            c == '-' || c == '_' || c == '.' || c == '~';
        if (unreserved) {
            out.push_back(static_cast<char>(c));
        } else {
            out.push_back('%');
            out.push_back(kHex[c >> 4]);
            out.push_back(kHex[c & 0x0F]);
        }
    }
    return out;
}

namespace {

bool IsListed(const std::vector<std::string>& disciplines,
              const std::string& name) {
    for (const auto& d : disciplines) {
        if (d == name) return true;
    }
    return false;
}

void SortMatches(std::vector<Match>& v) {
    std::sort(v.begin(), v.end(), [](const Match& a, const Match& b) {
        if (a.rating != b.rating) return a.rating > b.rating;
        return a.character < b.character;
    });
}

} // namespace

QualificationResult Evaluate(const std::vector<CharacterEntry>& chars,
                             const std::vector<std::string>& disciplines,
                             int rating,
                             DataState state) {
    QualificationResult res;
    res.state = state;
    if (disciplines.empty()) return res;

    for (const auto& c : chars) {
        // Best satisfying discipline for this character, preferring an active
        // one: a character who can craft right now must never be demoted to
        // "needs a swap" by a higher-rated inactive discipline.
        const DisciplineRating* bestActive   = nullptr;
        const DisciplineRating* bestInactive = nullptr;
        const DisciplineRating* bestAny      = nullptr; // for the "closest" line

        for (const auto& d : c.disciplines) {
            if (!IsListed(disciplines, d.discipline)) continue;
            if (!bestAny || d.rating > bestAny->rating) bestAny = &d;
            if (d.rating < rating) continue;
            if (d.active) {
                if (!bestActive || d.rating > bestActive->rating) bestActive = &d;
            } else {
                if (!bestInactive || d.rating > bestInactive->rating) bestInactive = &d;
            }
        }

        if (bestActive) {
            res.canCraftNow.push_back({c.name, bestActive->discipline, bestActive->rating});
        } else if (bestInactive) {
            res.needsSwap.push_back({c.name, bestInactive->discipline, bestInactive->rating});
        } else if (bestAny) {
            // Nobody qualifying yet — remember the highest holder seen so far.
            if (!res.hasClosest || bestAny->rating > res.closest.rating ||
                (bestAny->rating == res.closest.rating && c.name < res.closest.character)) {
                res.hasClosest = true;
                res.closest = {c.name, bestAny->discipline, bestAny->rating};
            }
        }
    }

    SortMatches(res.canCraftNow);
    SortMatches(res.needsSwap);

    // "Closest" is only meaningful when nothing qualifies at all.
    if (!res.canCraftNow.empty() || !res.needsSwap.empty()) {
        res.hasClosest = false;
        res.closest = Match{};
    }
    return res;
}

} // namespace CharacterCrafting
} // namespace CraftyLegend

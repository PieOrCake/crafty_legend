#include "CharacterCrafting.h"

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

} // namespace CharacterCrafting
} // namespace CraftyLegend

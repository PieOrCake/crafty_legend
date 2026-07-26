#include "CharacterCrafting.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <unordered_map>

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
    // e.value() throws json::type_error when a key exists but holds the wrong
    // JSON type (e.g. a string where a number is expected). This parse runs
    // on H&S's worker thread, so an escaping exception would be fatal. A body
    // with a wrong-typed field is treated the same as any other malformed
    // body: return false and leave `out` empty, rather than trying to salvage
    // a partial result from data that didn't match the expected shape.
    try {
        for (const auto& e : *it) {
            if (!e.is_object()) continue;
            DisciplineRating d;
            d.discipline = e.value("discipline", std::string());
            d.rating     = e.value("rating", 0);
            d.active     = e.value("active", false);
            if (d.discipline.empty()) continue;
            out.push_back(std::move(d));
        }
    } catch (const json::exception&) {
        out.clear();
        return false;
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

namespace {

constexpr long long kStaleAfterSeconds = 24 * 60 * 60;

struct CharacterRecord {
    std::vector<DisciplineRating> disciplines;
    bool fetched = false;
};

struct AccountRecord {
    std::vector<std::string> roster;                        // order matters
    std::unordered_map<std::string, CharacterRecord> chars; // keyed by name
    bool denied = false;
    long long completedAt = 0; // epoch seconds of the last full sweep
};

std::mutex                                          g_mutex;
std::unordered_map<std::string, AccountRecord>      g_accounts;
std::string                                         g_dataDir;
long long                                           g_fakeNow = 0;

long long NowSeconds() {
    if (g_fakeNow != 0) return g_fakeNow;
    using namespace std::chrono;
    return duration_cast<seconds>(system_clock::now().time_since_epoch()).count();
}

std::string CachePath() {
    if (g_dataDir.empty()) return std::string();
    return g_dataDir + "/character_crafting.json";
}

// Caller must hold g_mutex.
bool SweepCompleteLocked(const AccountRecord& a) {
    if (a.roster.empty()) return false;
    for (const auto& name : a.roster) {
        auto it = a.chars.find(name);
        if (it == a.chars.end() || !it->second.fetched) return false;
    }
    return true;
}

// Caller must hold g_mutex.
void StampIfCompleteLocked(AccountRecord& a) {
    if (SweepCompleteLocked(a)) a.completedAt = NowSeconds();
}

// Caller must hold g_mutex.
void SaveLocked() {
    const std::string path = CachePath();
    if (path.empty()) return;
    json root = json::object();
    json accounts = json::object();
    for (const auto& [name, a] : g_accounts) {
        json ja = json::object();
        ja["completed_at"] = a.completedAt;
        ja["denied"] = a.denied;
        ja["roster"] = a.roster;
        json jchars = json::object();
        for (const auto& [cname, c] : a.chars) {
            if (!c.fetched) continue;
            json jd = json::array();
            for (const auto& d : c.disciplines) {
                jd.push_back({{"discipline", d.discipline},
                              {"rating", d.rating},
                              {"active", d.active}});
            }
            jchars[cname] = jd;
        }
        ja["characters"] = jchars;
        accounts[name] = ja;
    }
    root["accounts"] = accounts;
    // error_handler_t::replace swaps invalid UTF-8 (e.g. from a mangled
    // account/character name) for U+FFFD instead of throwing json::type_error
    // 316 — this runs on H&S's worker thread, where an escaping exception
    // would be fatal.
    const std::string text =
        root.dump(2, ' ', false, json::error_handler_t::replace);

    // Atomic install: write to a .tmp sibling then rename into place so a
    // crash mid-write can never leave a truncated file that Load() would
    // have to discard wholesale (same pattern as FontManager.cpp).
    const std::string tmpPath = path + ".tmp";
    {
        std::ofstream f(tmpPath, std::ios::binary | std::ios::trunc);
        if (!f) return;
        f << text;
        if (!f.good()) return;
    }
    std::error_code ec;
    std::filesystem::rename(tmpPath, path, ec);
    if (ec) std::filesystem::remove(tmpPath, ec);
}

} // namespace

void Init(const std::string& dataDir) {
    {
        std::lock_guard<std::mutex> lock(g_mutex);
        g_dataDir = dataDir;
        g_accounts.clear();
    }
    Load();
}

void Load() {
    std::string path;
    {
        std::lock_guard<std::mutex> lock(g_mutex);
        path = CachePath();
    }
    if (path.empty()) return;
    std::ifstream f(path, std::ios::binary);
    if (!f) return;
    std::string body((std::istreambuf_iterator<char>(f)),
                     std::istreambuf_iterator<char>());
    json root = json::parse(body, nullptr, false);
    if (root.is_discarded() || !root.is_object()) return;
    auto ait = root.find("accounts");
    if (ait == root.end() || !ait->is_object()) return;

    // Parse into a local map first — file I/O and JSON parsing happen with
    // g_mutex released, so a live update (e.g. SetCharacterDisciplines
    // landing from H&S's worker thread) can race this read. Only merge
    // accounts the live store doesn't already know about, under the lock,
    // so a slow disk read can never clobber fresher in-memory data.
    //
    // This file can be hand-edited by a user, so every field access below
    // that could throw json::type_error on a wrong-typed value is guarded.
    std::unordered_map<std::string, AccountRecord> parsed;
    for (auto it = ait->begin(); it != ait->end(); ++it) {
        const json& ja = it.value();
        if (!ja.is_object()) continue;
        try {
            AccountRecord a;
            a.completedAt = ja.value("completed_at", 0LL);
            a.denied      = ja.value("denied", false);
            if (ja.contains("roster") && ja["roster"].is_array()) {
                for (const auto& n : ja["roster"]) {
                    if (n.is_string()) a.roster.push_back(n.get<std::string>());
                }
            }
            if (ja.contains("characters") && ja["characters"].is_object()) {
                for (auto cit = ja["characters"].begin();
                     cit != ja["characters"].end(); ++cit) {
                    CharacterRecord c;
                    c.fetched = true;
                    if (cit.value().is_array()) {
                        for (const auto& jd : cit.value()) {
                            if (!jd.is_object()) continue;
                            try {
                                DisciplineRating d;
                                d.discipline = jd.value("discipline", std::string());
                                d.rating     = jd.value("rating", 0);
                                d.active     = jd.value("active", false);
                                if (!d.discipline.empty())
                                    c.disciplines.push_back(std::move(d));
                            } catch (const json::exception&) {
                                continue; // skip this one malformed discipline
                            }
                        }
                    }
                    a.chars[cit.key()] = std::move(c);
                }
            }
            parsed[it.key()] = std::move(a);
        } catch (const json::exception&) {
            continue; // skip this one malformed account, keep the rest
        }
    }

    std::lock_guard<std::mutex> lock(g_mutex);
    for (auto& [name, a] : parsed) {
        if (g_accounts.find(name) != g_accounts.end()) continue; // live data wins
        g_accounts[name] = std::move(a);
    }
}

void Save() {
    std::lock_guard<std::mutex> lock(g_mutex);
    SaveLocked();
}

void SetAccountCharacters(const std::string& account,
                          const std::vector<std::string>& names) {
    if (account.empty()) return;
    std::lock_guard<std::mutex> lock(g_mutex);
    AccountRecord& a = g_accounts[account];
    a.roster = names;
    // Drop anyone no longer on the roster; keep the rest so a roster refresh
    // doesn't throw away data we already paid for.
    for (auto it = a.chars.begin(); it != a.chars.end();) {
        if (std::find(names.begin(), names.end(), it->first) == names.end()) {
            it = a.chars.erase(it);
        } else {
            ++it;
        }
    }
    StampIfCompleteLocked(a);
}

void SetCharacterDisciplines(const std::string& account,
                             const std::string& character,
                             std::vector<DisciplineRating> disciplines) {
    if (account.empty() || character.empty()) return;
    std::lock_guard<std::mutex> lock(g_mutex);
    AccountRecord& a = g_accounts[account];
    CharacterRecord& c = a.chars[character];
    c.disciplines = std::move(disciplines);
    c.fetched = true;
    // `denied` is sticky until ForceRefresh: a late in-flight character
    // response that lands after MarkDenied must not silently un-deny the
    // account.
    if (SweepCompleteLocked(a)) {
        a.completedAt = NowSeconds();
        SaveLocked();
    }
}

void MarkDenied(const std::string& account) {
    if (account.empty()) return;
    std::lock_guard<std::mutex> lock(g_mutex);
    AccountRecord& a = g_accounts[account];
    a.denied = true;
    a.completedAt = NowSeconds();
    SaveLocked(); // a denial should survive a restart, not depend on a later Save()
}

bool NextPendingCharacter(const std::string& account, std::string& out) {
    if (account.empty()) return false;
    std::lock_guard<std::mutex> lock(g_mutex);
    auto it = g_accounts.find(account);
    if (it == g_accounts.end() || it->second.denied) return false;
    for (const auto& name : it->second.roster) {
        auto c = it->second.chars.find(name);
        if (c == it->second.chars.end() || !c->second.fetched) {
            out = name;
            return true;
        }
    }
    return false;
}

bool IsStale(const std::string& account) {
    if (account.empty()) return false;
    std::lock_guard<std::mutex> lock(g_mutex);
    auto it = g_accounts.find(account);
    if (it == g_accounts.end()) return true;
    // A denied account is only reopened by ForceRefresh (which clears the
    // flag), not by the age-based sweep — otherwise the Task 4 sweep driver,
    // gated on IsStale, would re-check a permanently-denied account every
    // frame with no way to make progress.
    if (it->second.denied) return false;
    if (it->second.completedAt == 0) return true;
    return (NowSeconds() - it->second.completedAt) > kStaleAfterSeconds;
}

void ForceRefresh(const std::string& account) {
    if (account.empty()) return;
    std::lock_guard<std::mutex> lock(g_mutex);
    AccountRecord& a = g_accounts[account];
    a.denied = false;
    a.completedAt = 0;
    // Keep the disciplines so the tooltip keeps answering during the refetch,
    // but clear `fetched` so every character is re-queued.
    for (auto& [name, c] : a.chars) c.fetched = false;
}

QualificationResult Query(const std::string& account,
                          const std::vector<std::string>& disciplines,
                          int rating) {
    std::vector<CharacterEntry> roster;
    DataState state = DataState::NoData;
    {
        std::lock_guard<std::mutex> lock(g_mutex);
        auto it = g_accounts.find(account);
        if (it != g_accounts.end()) {
            const AccountRecord& a = it->second;
            if (a.denied) {
                state = DataState::Denied;
            } else if (a.roster.empty()) {
                state = DataState::NoData;
            } else {
                state = SweepCompleteLocked(a) ? DataState::Ready
                                               : DataState::Loading;
            }
            for (const auto& name : a.roster) {
                auto c = a.chars.find(name);
                if (c == a.chars.end() || c->second.disciplines.empty()) continue;
                CharacterEntry e;
                e.name = name;
                e.disciplines = c->second.disciplines;
                roster.push_back(std::move(e));
            }
        }
    }
    return Evaluate(roster, disciplines, rating, state);
}

void SetNowForTesting(long long epochSeconds) {
    std::lock_guard<std::mutex> lock(g_mutex);
    g_fakeNow = epochSeconds;
}

} // namespace CharacterCrafting
} // namespace CraftyLegend

#include "globals.h"
#include "ui_helpers.h"
#include "GW2API.h"
#include "DataManager.h"
#include "Localization.h"
#include "CharacterCrafting.h"
#include "hoard.h"
#include "IconManager.h"
// For MeaningfulMethods/ResolveActiveMethodIndex: the shopping list and the
// rolled-up row costs have to pick the same acquisition route the tree is
// showing, so they share the tree's resolver rather than re-deriving it.
#include "ui_tree.h"
#include <unordered_set>
#include <algorithm>
#include <climits>
#include <cmath>
#include <sstream>
#include <iomanip>
#include <chrono>
#include <ctime>
#include <cstring>
#include <mutex>
#include <shellapi.h>

// Helper: check if an item can be drilled into (has recipe or acquisition methods)
bool CanDrillInto(uint32_t item_id) {
    if (item_id == 0) return false;
    const auto* recipe = CraftyLegend::DataManager::GetRecipe(item_id);
    if (recipe) return true;
    const auto& acq = CraftyLegend::DataManager::GetAcquisitionMethods(item_id);
    if (!acq.empty()) return true;
    return false;
}

// Coin icon constants
static const float COIN_RADIUS = 4.0f;
static const float COIN_GAP = 2.0f; // gap after coin circle
static const float COIN_ADVANCE = COIN_RADIUS * 2 + COIN_GAP; // total width per coin icon

// Coin colors (fill + darker outline)
static const ImU32 COIN_GOLD_FILL = IM_COL32(255, 215, 0, 255);
static const ImU32 COIN_GOLD_EDGE = IM_COL32(180, 140, 0, 255);
static const ImU32 COIN_SILVER_FILL = IM_COL32(192, 192, 200, 255);
static const ImU32 COIN_SILVER_EDGE = IM_COL32(120, 120, 135, 255);
static const ImU32 COIN_COPPER_FILL = IM_COL32(184, 115, 51, 255);
static const ImU32 COIN_COPPER_EDGE = IM_COL32(120, 70, 30, 255);

// Draw a small coin circle inline at current cursor, advance cursor
static void DrawCoinInline(ImU32 fill, ImU32 edge) {
    ImVec2 pos = ImGui::GetCursorScreenPos();
    float textH = ImGui::GetTextLineHeight();
    float cx = pos.x + COIN_RADIUS;
    float cy = pos.y + textH * 0.5f;
    ImDrawList* dl = ImGui::GetWindowDrawList();
    dl->AddCircleFilled(ImVec2(cx, cy), COIN_RADIUS, fill, 12);
    dl->AddCircle(ImVec2(cx, cy), COIN_RADIUS, edge, 12, 1.2f);
    ImGui::Dummy(ImVec2(COIN_RADIUS * 2, textH));
    ImGui::SameLine(0, COIN_GAP);
}

// GW2 rarity border colors
ImU32 GetRarityBorderColor(const std::string& rarity) {
    if (rarity == "Legendary") return IM_COL32(160, 100, 200, 220); // Purple
    if (rarity == "Ascended")  return IM_COL32(230, 100, 140, 220); // Pink
    if (rarity == "Exotic")    return IM_COL32(255, 165, 0, 200);   // Orange
    if (rarity == "Rare")      return IM_COL32(255, 220, 50, 180);  // Yellow
    if (rarity == "Masterwork") return IM_COL32(30, 180, 30, 160);  // Green
    if (rarity == "Fine")      return IM_COL32(90, 160, 230, 160);  // Blue
    return 0; // No border for Basic/Junk/unknown
}

// Icon size constant (used throughout rendering)

// Helper: compute TP price string width for column sizing
float CalcPriceWidth(int total_copper) {
    if (total_copper <= 0) return 0.0f;
    int gold = total_copper / 10000;
    int silver = (total_copper % 10000) / 100;
    int copper = total_copper % 100;
    float w = 0.0f;
    if (gold > 0) {
        w += ImGui::CalcTextSize(std::to_string(gold).c_str()).x + COIN_ADVANCE + 2.0f;
        if (silver == 0 && copper == 0) return w;
    }
    if (gold > 0 || silver > 0) {
        std::string sStr = (gold > 0 && silver < 10 ? "0" : "") + std::to_string(silver);
        w += ImGui::CalcTextSize(sStr.c_str()).x + COIN_ADVANCE + 2.0f;
    }
    std::string cStr = ((gold > 0 || silver > 0) && copper < 10 ? "0" : "") + std::to_string(copper);
    w += ImGui::CalcTextSize(cStr.c_str()).x + COIN_ADVANCE + 2.0f;
    return w;
}

// Helper: render gold/silver/copper price inline with coin icons, returns width used
float RenderPrice(int total_copper) {
    if (total_copper <= 0) return 0.0f;
    int gold = total_copper / 10000;
    int silver = (total_copper % 10000) / 100;
    int copper = total_copper % 100;
    float startX = ImGui::GetCursorPosX();

    ImVec4 goldColor(1.0f, 0.84f, 0.0f, 1.0f);
    ImVec4 silverColor(0.75f, 0.75f, 0.78f, 1.0f);
    ImVec4 copperColor(0.72f, 0.45f, 0.20f, 1.0f);

    if (gold > 0) {
        ImGui::TextColored(goldColor, "%d", gold);
        ImGui::SameLine(0, 1);
        DrawCoinInline(COIN_GOLD_FILL, COIN_GOLD_EDGE);
        if (silver == 0 && copper == 0) {
            return ImGui::GetCursorPosX() - startX;
        }
    }
    if (gold > 0 || silver > 0) {
        if (gold > 0 && silver < 10) {
            ImGui::TextColored(silverColor, "0%d", silver);
        } else {
            ImGui::TextColored(silverColor, "%d", silver);
        }
        ImGui::SameLine(0, 1);
        DrawCoinInline(COIN_SILVER_FILL, COIN_SILVER_EDGE);
    }
    if ((gold > 0 || silver > 0) && copper < 10) {
        ImGui::TextColored(copperColor, "0%d", copper);
    } else {
        ImGui::TextColored(copperColor, "%d", copper);
    }
    ImGui::SameLine(0, 1);
    DrawCoinInline(COIN_COPPER_FILL, COIN_COPPER_EDGE);

    return ImGui::GetCursorPosX() - startX;
}

// Helper: get per-unit vendor coin cost (in copper) for an item, or 0 if none
int GetVendorCoinCost(uint32_t item_id) {
    if (item_id == 0) return 0;
    const auto& acqs = CraftyLegend::DataManager::GetAcquisitionMethods(item_id);
    for (const auto& acq : acqs) {
        for (const auto& req : acq.purchase_requirements) {
            if (req.first == "Coin") {
                try {
                    int copper = std::stoi(req.second);
                    if (copper > 0) return copper;
                } catch (...) {}
            }
        }
    }
    return 0;
}

// Helper: strip GW2 markup tags (e.g. <c=@flavor>text</c>) from strings
std::string StripMarkup(const std::string& text) {
    std::string result;
    result.reserve(text.size());
    size_t i = 0;
    while (i < text.size()) {
        if (text[i] == '<') {
            size_t close = text.find('>', i);
            if (close != std::string::npos) {
                i = close + 1;
                continue;
            }
        }
        result += text[i];
        i++;
    }
    return result;
}

// Helper: open GW2 Wiki page for an item name
void OpenWikiPage(const std::string& itemName) {
    std::string url = "https://wiki.guildwars2.com/wiki/";
    for (char c : itemName) {
        if (c == ' ') url += '_';
        else url += c;
    }
    ShellExecuteA(NULL, "open", url.c_str(), NULL, NULL, SW_SHOWNORMAL);
}

// --- Binding-aware owned count ---
// Account-bound items: show only current account's count
// Unbound items: show total across all accounts
int GetEffectiveOwnedCount(uint32_t item_id) {
    // One implementation, in DataManager, so Miller, the tree, pricing and the
    // shopping list can never drift apart on what "owned" means.
    return CraftyLegend::DataManager::EffectiveOwnedCount(item_id);
}

// --- Legendary completion % ---

// One step down the tree for completion purposes. `per_unit` is how many of the
// child are needed per ONE of the parent (fractional when a recipe yields a stack),
// which keeps the weights exact instead of rounding a craft count at every level.
struct CompletionChild {
    uint32_t item_id;
    double   per_unit;
};

// Children of an item: recipe ingredients when it has a recipe, otherwise the first
// acquisition method's purchase requirements — mirroring FlattenCraftingTree, so a
// vendor-bought gift is no longer a dead end. Wallet currencies (item_id 0) are
// skipped; they can't be normalised against material counts. Recipes and vendor
// data are immutable after load, so the result is cached permanently.
static const std::vector<CompletionChild>& CompletionChildren(uint32_t item_id) {
    static std::unordered_map<uint32_t, std::vector<CompletionChild>> cache;
    auto it = cache.find(item_id);
    if (it != cache.end()) return it->second;

    std::vector<CompletionChild> kids;
    const auto* recipe = CraftyLegend::DataManager::GetRecipe(item_id);
    if (recipe && !recipe->ingredients.empty()) {
        double output = std::max(1u, recipe->output_count);
        for (const auto& ing : recipe->ingredients) {
            if (ing.item_id != 0 && ing.count > 0)
                kids.push_back({ing.item_id, static_cast<double>(ing.count) / output});
        }
    } else {
        for (const auto& acq : CraftyLegend::DataManager::GetAcquisitionMethods(item_id)) {
            if (acq.purchase_requirements.empty()) continue;
            for (const auto& req : acq.purchase_requirements) {
                if (req.first == "Coin") continue; // gold, not a material
                int amount = 0;
                try {
                    size_t pos = 0;
                    amount = std::stoi(req.second, &pos);
                    if (pos != req.second.size() && req.second[pos] != ' ') amount = 0;
                } catch (...) { amount = 0; }
                if (amount <= 0) continue;
                uint32_t sub_id = CraftyLegend::DataManager::ResolveRequirementItemId(req.first);
                if (sub_id != 0) kids.push_back({sub_id, static_cast<double>(amount)});
            }
            break; // first acquisition method with requirements wins
        }
    }
    return cache.emplace(item_id, std::move(kids)).first->second;
}

// Total base-material units behind ONE of `item_id` — the weight its subtree carries
// in the completion percentage. A true leaf weighs 1. Without this, a vendor-bought
// gift standing in for hundreds of materials counted the same as a single ore.
// `blocked` is set when a cycle truncated the walk; such a result is context-
// dependent and must not be cached.
static double LeafWeight(uint32_t item_id, std::unordered_set<uint32_t>& onPath,
                         bool& blocked) {
    static std::unordered_map<uint32_t, double> cache;
    auto it = cache.find(item_id);
    if (it != cache.end()) return it->second;

    if (onPath.count(item_id)) { blocked = true; return 1.0; }
    const auto& kids = CompletionChildren(item_id);
    if (kids.empty()) return 1.0; // base material

    onPath.insert(item_id);
    double total = 0.0;
    bool childBlocked = false;
    for (const auto& k : kids) total += k.per_unit * LeafWeight(k.item_id, onPath, childBlocked);
    onPath.erase(item_id);

    if (total <= 0.0) total = 1.0;
    if (childBlocked) { blocked = true; return total; }
    cache[item_id] = total;
    return total;
}

static double LeafWeight(uint32_t item_id) {
    std::unordered_set<uint32_t> onPath;
    bool blocked = false;
    return LeafWeight(item_id, onPath, blocked);
}

// Accumulate needed/have weights for `count` of an item. Anything you already own is
// credited at that item's full subtree weight — so a finished gift counts for
// everything it took to make, not zero — and only the shortfall is expanded further.
// `pool` holds each item's still-unspent owned count: an item needed in two branches
// (Obsidian Shard turns up all over a tree) must not have the same stack credited
// against both, so ownership is consumed greedily as the walk encounters it.
static void WalkCompletion(uint32_t item_id, double count,
                           std::unordered_set<uint32_t>& onPath,
                           std::unordered_map<uint32_t, double>& pool,
                           double& need, double& have) {
    if (item_id == 0 || count <= 0.0) return;

    double w = LeafWeight(item_id);

    auto pit = pool.find(item_id);
    if (pit == pool.end())
        pit = pool.emplace(item_id, static_cast<double>(GetEffectiveOwnedCount(item_id))).first;
    double owned = std::min(pit->second, count);
    pit->second -= owned;
    if (owned > 0.0) {
        need += w * owned;
        have += w * owned;
    }

    double remaining = count - owned;
    if (remaining <= 0.0) return;

    const auto& kids = CompletionChildren(item_id);
    if (kids.empty() || onPath.count(item_id)) {
        need += w * remaining; // base material (or a cycle) — nothing left to expand
        return;
    }

    onPath.insert(item_id);
    for (const auto& k : kids)
        WalkCompletion(k.item_id, k.per_unit * remaining, onPath, pool, need, have);
    onPath.erase(item_id);
}

// A legendary with no crafting tree at all has no materials to measure — Prismatic
// Champion's Regalia is awarded for the Seasons of the Dragons achievement, not crafted,
// so its bar would sit at 0% until the moment it appears in the armoury. Fall back to how
// far along its achievement gates are, which is the only progress that exists for it.
static float AchievementGateCompletion(uint32_t legendary_id) {
    std::vector<CraftyLegend::Prerequisite> prereqs =
        CraftyLegend::DataManager::GetPrerequisites(legendary_id);
    double sum = 0.0;
    int n = 0;
    for (const auto& p : prereqs) {
        if (p.category != CraftyLegend::PrereqCategory::Achievement || p.achievement_id <= 0)
            continue;
        n++;
        if (p.completed) { sum += 1.0; continue; }
        CraftyLegend::GW2API::AchievementProgress ap;
        if (CraftyLegend::GW2API::GetAchievementProgress(p.achievement_id, ap)
            && ap.max > 0 && ap.current > 0)
            sum += std::min(1.0, static_cast<double>(ap.current) / static_cast<double>(ap.max));
    }
    if (n == 0) return 0.0f;
    return static_cast<float>(std::min(1.0, sum / n));
}

// The bar measures materials for the NEXT copy, and reads 100% only when the Legendary
// Armory is full for this item (max_count copies: 1 for armour and amulets, 2 for
// two-handed weapons and rings, 4 for one-handed weapons, 7 runes, 8 sigils).
//
// Deliberately NOT (copies + progress) / max_count: that would show a one-handed weapon
// you can forge right now as 25% just because the armoury could hold four. The copies
// you already have are surfaced as a "3/7" count on the row instead, so the bar stays a
// straight answer to "how close am I to the next one".
//
// Only armoury-bound copies count as owned — an unbound legendary in inventory can still
// be sold. That is also why the walk starts at the legendary's children rather than the
// legendary itself: self-crediting from the inventory pool is what made an owned
// legendary read 100% no matter what the armoury held.
static float ComputeLegendaryCompletion(uint32_t legendary_id) {
    int maxCount = 1;
    if (const CraftyLegend::Legendary* leg =
            CraftyLegend::DataManager::GetLegendaryById(legendary_id)) {
        maxCount = leg->max_count > 0 ? leg->max_count : 1;
    }
    if (CraftyLegend::GW2API::GetArmoryCount(legendary_id) >= maxCount) return 1.0f;

    double need = 0.0, have = 0.0;
    std::unordered_set<uint32_t> onPath;
    std::unordered_map<uint32_t, double> pool;
    onPath.insert(legendary_id);
    for (const auto& k : CompletionChildren(legendary_id))
        WalkCompletion(k.item_id, k.per_unit, onPath, pool, need, have);

    if (need <= 0.0) return AchievementGateCompletion(legendary_id);
    return static_cast<float>(std::min(1.0, have / need));
}

// Process a batch of queued completion recomputations per frame
void TickCompletionQueue() {
    if (g_CompletionQueue.empty()) return;
    int count = std::min(COMPLETION_BATCH_SIZE, static_cast<int>(g_CompletionQueue.size()));
    for (int i = 0; i < count; i++) {
        uint32_t id = g_CompletionQueue.back();
        g_CompletionQueue.pop_back();
        g_CompletionCache[id] = ComputeLegendaryCompletion(id);
    }
}

float GetLegendaryCompletion(uint32_t legendary_id) {
    if (!CraftyLegend::GW2API::HasAccountData()) return -1.0f;
    auto it = g_CompletionCache.find(legendary_id);
    if (it != g_CompletionCache.end()) return it->second;
    return -1.0f; // not yet computed
}

// True when an item offers a genuine choice of acquisition route. Checks the raw
// method list first because that is a reference, while MeaningfulMethods copies -
// and this sits on the per-frame pricing path for every visible row. MeaningfulMethods
// only ever removes entries, so fewer than two raw methods can never yield a choice.
static bool HasRouteChoice(uint32_t item_id) {
    return CraftyLegend::DataManager::GetAcquisitionMethods(item_id).size() >= 2 &&
           CraftyLegend::UI::MeaningfulMethods(item_id).size() >= 2;
}

// A purchase requirement's numeric amount, or 0 when the cost is descriptive
// ("Requires Raptor mastery") rather than a count. Trailing text after the number
// is tolerated so entries like "25 (Fractal Reliquary)" still parse.
static int ParseRequirementAmount(const std::string& value) {
    try {
        size_t pos = 0;
        int amount = std::stoi(value, &pos);
        if (pos != value.size() && value[pos] != ' ') return 0;
        return amount > 0 ? amount : 0;
    } catch (...) {
        return 0;
    }
}

// Accumulate one vendor method's purchase requirements. `mKey` is the tree key of
// the method node, so nested requirements resolve their own routes exactly as the
// tree draws them (RenderMethodChildren builds the same "#vreq:<i>" keys).
static void FlattenVendorRequirements(const CraftyLegend::AcquisitionMethod& acq,
    int remaining, const std::string& mKey,
    std::unordered_map<uint32_t, std::pair<std::string, int>>& itemMats,
    std::unordered_map<std::string, int>& walletMats,
    std::unordered_set<uint32_t>& visited,
    std::unordered_map<uint32_t, int>& pool,
    std::unordered_map<std::string, int>* nodeSpend);

// How much of `item_id` the walk still has to find, after spending whatever is
// left of the stack you already own.
//
// The stack has to be *spent*, not re-checked at every node. Mystic Coin is the
// clearest case: a Mystic Tribute wants 250 outright and another 186 through the
// Mystic Clover vendor route, and 200 in the bank covers part of one or the other
// but not both. Subtracting the full 200 at each node reported 50 missing when
// the true shortfall is 436 - 200 = 236. `pool` is seeded on first sight of an
// item and drawn down as the walk meets it, so the answer is the same whichever
// branch is walked first. This mirrors WalkCompletion, which already pools.
static int TakeFromPool(uint32_t item_id, int count,
                        std::unordered_map<uint32_t, int>& pool) {
    if (!CraftyLegend::GW2API::HasAccountData()) return count;
    auto it = pool.find(item_id);
    if (it == pool.end()) {
        it = pool.emplace(item_id, GetEffectiveOwnedCount(item_id)).first;
    }
    const int spend = std::min(it->second, count);
    it->second -= spend;
    return count - spend;
}

// Helper: recursively flatten a crafting tree into base materials.
// Base materials are items with no recipe that can't be drilled into further.
// Accumulates into a map keyed by item_id (or by name for item_id==0 wallet costs).
//
// `nodeKey` mirrors the tree's node key (see RenderNode) so a multi-route item is
// flattened down the route the user actually picked. Without it the list always
// described the recipe route, which could ask for materials the chosen vendor
// route never needs. In Miller layout there is no expand-state to read, so
// ResolveActiveMethodIndex falls back to its default rule and the result matches
// the old behaviour.
static void FlattenCraftingTree(uint32_t item_id, int count,
    std::unordered_map<uint32_t, std::pair<std::string, int>>& itemMats,
    std::unordered_map<std::string, int>& walletMats,
    std::unordered_set<uint32_t>& visited,
    const std::string& nodeKey,
    std::unordered_map<uint32_t, int>& pool,
    std::unordered_map<std::string, int>* nodeSpend) {
    if (item_id == 0 || count <= 0) return;
    if (visited.count(item_id)) return; // cycle guard

    // Spend what is left of the stack you own, rather than crediting all of it
    // against every branch that wants this item (see TakeFromPool).
    int remaining = TakeFromPool(item_id, count, pool);
    // Record what this node was actually credited, so the Miller columns and the
    // tree rows can show the same shortfall the list does instead of subtracting
    // the whole stack again at every branch.
    if (nodeSpend) (*nodeSpend)[nodeKey] += count - remaining;
    if (remaining <= 0) return;

    // Multi-route item: follow the active route instead of assuming the recipe.
    if (HasRouteChoice(item_id)) {
        auto methods = CraftyLegend::UI::MeaningfulMethods(item_id);
        int active = CraftyLegend::UI::ResolveActiveMethodIndex(item_id, nodeKey);
        if (active >= 0 && active < static_cast<int>(methods.size())) {
            const auto& method = methods[active];
            if (method.method == "vendor") {
                visited.insert(item_id);
                FlattenVendorRequirements(method, remaining,
                    nodeKey + "#m:" + std::to_string(active),
                    itemMats, walletMats, visited, pool, nodeSpend);
                visited.erase(item_id);
                return;
            }
            if (method.method == "trading_post") {
                // Buying it outright is the whole cost; do not expand the recipe.
                std::string tpName = CraftyLegend::DataManager::GetItemName(item_id);
                if (tpName.empty()) tpName = "Unknown #" + std::to_string(item_id);
                itemMats[item_id].first = tpName;
                itemMats[item_id].second += remaining;
                return;
            }
            // crafting / mystic_forge: fall through to the recipe walk below.
        }
    }

    // If item has a recipe, recurse into ingredients
    const auto* recipe = CraftyLegend::DataManager::GetRecipe(item_id);
    if (recipe && !recipe->ingredients.empty()) {
        visited.insert(item_id);
        int numCrafts = CraftyLegend::DataManager::CraftsNeeded(
            item_id, remaining, recipe->output_count);
        for (const auto& ing : recipe->ingredients) {
            if (ing.item_id == 0) {
                // Wallet/currency cost
                walletMats[ing.name] += static_cast<int>(ing.count) * numCrafts;
            } else {
                FlattenCraftingTree(ing.item_id, static_cast<int>(ing.count) * numCrafts,
                    itemMats, walletMats, visited,
                    nodeKey + "/" + std::to_string(ing.item_id), pool, nodeSpend);
            }
        }
        visited.erase(item_id);
        return;
    }

    // Leaf item. If it is traded for other ITEMS at a vendor, those items are what
    // you actually have to go and get - listing the leaf as well told you to buy
    // both the Ardent Glorious helm and the 250 Shards of Glory you would trade for
    // it. A cost paid in coin or wallet currency leaves the leaf itself as the thing
    // to acquire, so it is still listed then.
    // Any genuine choice of vendor was resolved above, so whatever is left here is
    // the item's only route and taking the first one with costs is not a guess.
    // The tree passes nodeKey as the method key in this single-route case
    // (RenderNode -> RenderMethodChildren), so the keys line up.
    const auto& acqs = CraftyLegend::DataManager::GetAcquisitionMethods(item_id);
    for (const auto& acq : acqs) {
        if (acq.purchase_requirements.empty()) continue;
        bool tradedForItems = false;
        for (const auto& req : acq.purchase_requirements) {
            if (req.first == "Coin") continue;
            if (ParseRequirementAmount(req.second) <= 0) continue;
            if (CraftyLegend::DataManager::ResolveRequirementItemId(req.first) != 0) {
                tradedForItems = true;
                break;
            }
        }
        visited.insert(item_id);
        FlattenVendorRequirements(acq, remaining, nodeKey, itemMats, walletMats, visited, pool, nodeSpend);
        visited.erase(item_id);
        if (tradedForItems) return; // its price replaces it on the list
        break;
    }

    std::string name = CraftyLegend::DataManager::GetItemName(item_id);
    if (name.empty()) name = "Unknown #" + std::to_string(item_id);
    itemMats[item_id].first = name;
    itemMats[item_id].second += remaining;
}

static void FlattenVendorRequirements(const CraftyLegend::AcquisitionMethod& acq,
    int remaining, const std::string& mKey,
    std::unordered_map<uint32_t, std::pair<std::string, int>>& itemMats,
    std::unordered_map<std::string, int>& walletMats,
    std::unordered_set<uint32_t>& visited,
    std::unordered_map<uint32_t, int>& pool,
    std::unordered_map<std::string, int>* nodeSpend) {
    int idx = 0;
    for (const auto& req : acq.purchase_requirements) {
        std::string reqKey = mKey + "#vreq:" + std::to_string(idx++);
        if (req.first == "Coin") continue; // direct gold cost handled by GetVendorCoinCost
        int sub_count = ParseRequirementAmount(req.second);
        if (sub_count <= 0) continue;
        uint32_t sub_id = CraftyLegend::DataManager::ResolveRequirementItemId(req.first);
        if (sub_id != 0) {
            FlattenCraftingTree(sub_id, sub_count * remaining, itemMats, walletMats, visited,
                reqKey + "/" + std::to_string(sub_id), pool, nodeSpend);
        }
        // A requirement that resolves to no item is a wallet currency. The shopping
        // list is a list of things to buy, and wallet costs are excluded from it
        // (see the end of BuildShoppingList), so there is nothing to record.
    }
}

// One walk of a legendary's whole tree, shared by the shopping list and by the
// owned-material allocation the columns and tree rows read. Keeping it in one place
// is what makes those three agree: same routes, same clover maths, and above all
// the same single stack of each owned material spent across the whole tree.
static void WalkLegendary(uint32_t legendary_id,
    std::unordered_map<uint32_t, std::pair<std::string, int>>& itemMats,
    std::unordered_map<std::string, int>& walletMats,
    std::unordered_map<std::string, int>* nodeSpend) {
    std::unordered_set<uint32_t> visited;
    // One stack per item for the whole walk, drawn down as it is spent.
    std::unordered_map<uint32_t, int> pool;

    // Recurse directly into the legendary's recipe ingredients, bypassing the
    // owned-count check for the legendary itself. The shopping list always shows
    // what's needed for the *next* craft, regardless of how many are already owned
    // (relevant for Runes/Sigils that can be owned multiple times, and for any
    // legendary that registers a count in the Legendary Armory).
    // The node keys below must match the ones RenderTree/RenderNode build, or the
    // route lookup reads a different item's expand-state: the legendary's id is the
    // root and every child appends "/<item id>".
    const std::string root = std::to_string(legendary_id);
    const auto* top_recipe = CraftyLegend::DataManager::GetRecipe(legendary_id);
    if (top_recipe && !top_recipe->ingredients.empty()) {
        for (const auto& ing : top_recipe->ingredients) {
            if (ing.item_id == 0) {
                walletMats[ing.name] += static_cast<int>(ing.count);
            } else {
                FlattenCraftingTree(ing.item_id, static_cast<int>(ing.count), itemMats, walletMats,
                    visited, root + "/" + std::to_string(ing.item_id), pool, nodeSpend);
            }
        }
    } else {
        // No recipe: the legendary is bought outright from a vendor. Expand that
        // vendor's costs directly rather than going through FlattenCraftingTree,
        // which would net the legendary against copies already in the armoury (the
        // list is always for the NEXT one) and then list the legendary itself as
        // something to shop for.
        auto methods = CraftyLegend::UI::MeaningfulMethods(legendary_id);
        int active = methods.empty() ? -1 : 0;
        if (methods.size() >= 2) {
            active = CraftyLegend::UI::ResolveActiveMethodIndex(legendary_id, root);
        }
        if (active >= 0 && active < static_cast<int>(methods.size())) {
            const std::string mKey = methods.size() >= 2
                ? root + "#m:" + std::to_string(active)
                : root;
            visited.insert(legendary_id);
            FlattenVendorRequirements(methods[active], 1, mKey,
                                      itemMats, walletMats, visited, pool, nodeSpend);
            visited.erase(legendary_id);
        }
    }
}

// How much of each node's requirement the account's stock already covers, for the
// legendary currently on screen. Cached: rebuilding is a full tree walk, and the
// tree asks once per visible row per frame.
static const std::unordered_map<std::string, int>& OwnedAllocation(uint32_t legendary_id) {
    static std::unordered_map<std::string, int> s_alloc;
    static uint32_t s_legendary = 0;
    static uint64_t s_accountRev = static_cast<uint64_t>(-1);
    static uint64_t s_expandRev  = static_cast<uint64_t>(-1);

    const uint64_t accountRev = CraftyLegend::GW2API::GetAccountRevision();
    const uint64_t expandRev  = CraftyLegend::DataManager::GetExpandRevision();
    if (legendary_id == s_legendary && accountRev == s_accountRev && expandRev == s_expandRev) {
        return s_alloc;
    }

    s_alloc.clear();
    s_legendary  = legendary_id;
    s_accountRev = accountRev;
    s_expandRev  = expandRev;
    if (legendary_id != 0) {
        std::unordered_map<uint32_t, std::pair<std::string, int>> itemMats;
        std::unordered_map<std::string, int> walletMats;
        WalkLegendary(legendary_id, itemMats, walletMats, &s_alloc);
    }
    return s_alloc;
}

int RemainingNeededAtNode(uint32_t legendary_id, const std::string& nodeKey,
                          uint32_t item_id, int count) {
    if (count <= 0) return 0;
    if (legendary_id == 0 || nodeKey.empty()) {
        return CraftyLegend::DataManager::RemainingNeeded(item_id, count);
    }
    const auto& alloc = OwnedAllocation(legendary_id);
    auto it = alloc.find(nodeKey);
    if (it == alloc.end()) {
        // The walk never reached this node - a stale column, or a route the active
        // resolution does not take. Fall back to the per-node rule rather than
        // pretending nothing is owned.
        return CraftyLegend::DataManager::RemainingNeeded(item_id, count);
    }
    const int remaining = count - it->second;
    return remaining > 0 ? remaining : 0;
}

void BuildShoppingList(uint32_t legendary_id) {
    g_ShoppingList.clear();
    if (legendary_id == 0) return;

    std::unordered_map<uint32_t, std::pair<std::string, int>> itemMats;
    std::unordered_map<std::string, int> walletMats;
    WalkLegendary(legendary_id, itemMats, walletMats, nullptr);

    // Convert to shopping entries
    // Note: FlattenCraftingTree already subtracts owned counts, so info.second
    // is the net amount still needed to purchase.
    for (const auto& [id, info] : itemMats) {
        if (info.second <= 0) continue; // already owned enough
        const auto* item = CraftyLegend::DataManager::GetItem(id);
        int tp_price = CraftyLegend::GW2API::HasPriceData() ? CraftyLegend::GW2API::GetSellPrice(id) : 0;
        int vendor_coin = GetVendorCoinCost(id); // per-unit vendor gold cost
        bool is_bound = item && item->binding != "none" && !item->binding.empty();
        // Anything gold can buy goes in the TP or Vendor group. What is left is a
        // real shortfall you have to go and earn, and it is listed rather than
        // dropped: silently omitting it understated the list (a Mystic Tribute can
        // leave you a couple of hundred Obsidian Shards short with nothing on
        // screen to say so). Only classify as unbuyable when we actually know -
        // before TP prices load, an unbound item's price is unknown, not absent.
        bool unbuyable = false;
        if (tp_price <= 0 && vendor_coin <= 0) {
            if (is_bound || CraftyLegend::GW2API::HasPriceData()) unbuyable = true;
        }
        ShoppingEntry e;
        e.item_id = id;
        e.name = info.first; // English canonical; localized at render time so it updates live
        e.required = info.second; // net amount to purchase
        e.owned = 0;
        e.is_unbuyable = unbuyable;
        e.is_vendor = (!unbuyable && tp_price <= 0 && vendor_coin > 0);
        e.tp_price = tp_price > 0 ? tp_price : vendor_coin; // prefer TP, fallback to vendor
        g_ShoppingList.push_back(e);
    }
    // Wallet currencies excluded - not purchasable on TP

    // Sort within each group (trading post / vendor / gather-or-earn) by sort mode
    auto group = [](const ShoppingEntry& e) { return e.is_unbuyable ? 2 : (e.is_vendor ? 1 : 0); };
    auto sorter = [&group](const ShoppingEntry& a, const ShoppingEntry& b) {
        // Primary: trading post, then vendor, then what gold cannot buy
        if (group(a) != group(b)) return group(a) < group(b);
        // Secondary: current sort mode
        if (g_ShoppingSort == ShoppingSort::Price) {
            long long pa = (long long)a.tp_price * a.required;
            long long pb = (long long)b.tp_price * b.required;
            if (pa != pb) return pa > pb; // descending price
            return a.name < b.name;
        }
        return a.name < b.name;
    };
    std::sort(g_ShoppingList.begin(), g_ShoppingList.end(), sorter);

    g_ShoppingListLegendaryId = legendary_id;
    g_ShoppingListDirty = false;
}

// Helper: recursively check if all leaf materials for an item are owned.
// Returns true if the item can be crafted/forged right now (all sub-materials met).
static bool IsReadyToCraft(uint32_t item_id, int count, std::unordered_set<uint32_t>& visited) {
    if (item_id == 0 || count <= 0) return true;
    if (!CraftyLegend::GW2API::HasAccountData()) return false;
    if (visited.count(item_id)) return false; // cycle guard

    // Check if we already own enough of this item directly
    int owned = GetEffectiveOwnedCount(item_id);
    int remaining = std::max(0, count - owned);
    if (remaining <= 0) return true;

    // If item has a recipe, check all ingredients recursively
    const auto* recipe = CraftyLegend::DataManager::GetRecipe(item_id);
    if (recipe && !recipe->ingredients.empty()) {
        visited.insert(item_id);
        int numCrafts = CraftyLegend::DataManager::CraftsNeeded(
            item_id, remaining, recipe->output_count);
        for (const auto& ing : recipe->ingredients) {
            if (!IsReadyToCraft(ing.item_id, static_cast<int>(ing.count) * numCrafts, visited)) {
                visited.erase(item_id);
                return false;
            }
        }
        visited.erase(item_id);
        return true;
    }

    // Leaf item with no recipe: not owned enough
    return false;
}

// Forward declaration for mutual recursion
// `nodeKey` is optional route context. When supplied, the recipe recursion below
// hands each ingredient to GetRouteAwarePrice so a multi-route descendant is priced
// down its active route; the cheapest-of-all-options logic at this level is
// unchanged either way. nullptr keeps the original route-blind behaviour.
static long long GetRecursivePrice(uint32_t item_id, int count, std::unordered_set<uint32_t>& visited,
                                   const std::string* nodeKey = nullptr);
static long long GetRouteAwarePrice(uint32_t item_id, int count,
                                    std::unordered_set<uint32_t>& visited,
                                    const std::string& nodeKey);

// Helper: compute vendor acquisition cost for an item (gold portion only).
// Returns -1 if no vendor cost is computable.
static long long GetVendorPrice(uint32_t item_id, int remaining, std::unordered_set<uint32_t>& visited) {
    const auto& acqs = CraftyLegend::DataManager::GetAcquisitionMethods(item_id);
    long long bestVendor = -1;
    for (const auto& acq : acqs) {
        if (acq.method != "vendor" || acq.purchase_requirements.empty()) continue;
        long long vendorSum = 0;
        bool hasAnyCost = false;
        for (const auto& req : acq.purchase_requirements) {
            if (req.first == "Coin") {
                try {
                    int copper = std::stoi(req.second);
                    if (copper > 0) { vendorSum += static_cast<long long>(copper) * remaining; hasAnyCost = true; }
                } catch (...) {}
                continue;
            }
            if (CraftyLegend::GW2API::GetWalletAmountByName(req.first) >= 0) continue;
            uint32_t sub_id = CraftyLegend::DataManager::ResolveRequirementItemId(req.first);
            if (sub_id == 0) continue;
            int sub_count = 0;
            try { sub_count = std::stoi(req.second); } catch (...) { continue; }
            if (sub_count <= 0) continue;
            visited.insert(item_id);
            long long sub_price = GetRecursivePrice(sub_id, sub_count * remaining, visited);
            visited.erase(item_id);
            if (sub_price > 0) { vendorSum += sub_price; hasAnyCost = true; }
        }
        if (hasAnyCost && (bestVendor < 0 || vendorSum < bestVendor)) {
            bestVendor = vendorSum;
        }
    }
    return bestVendor;
}

// Helper: recursively compute TP cost for an item.
// Considers recipe crafting, TP direct buy, and vendor costs; returns cheapest.
static long long GetRecursivePrice(uint32_t item_id, int count, std::unordered_set<uint32_t>& visited,
                                   const std::string* nodeKey) {
    if (item_id == 0 || count <= 0) return 0;
    if (!CraftyLegend::GW2API::HasPriceData()) return 0;
    if (visited.count(item_id)) return 0; // cycle guard

    // Account for owned items
    int remaining = count;
    if (CraftyLegend::GW2API::HasAccountData()) {
        int owned = GetEffectiveOwnedCount(item_id);
        remaining = std::max(0, remaining - owned);
    }
    if (remaining <= 0) return 0;

    long long bestPrice = -1; // -1 = no valid price yet

    // Option 1: TP direct buy
    int unitPrice = CraftyLegend::GW2API::GetSellPrice(item_id);
    if (unitPrice > 0) {
        long long tpCost = static_cast<long long>(unitPrice) * remaining;
        if (bestPrice < 0 || tpCost < bestPrice) bestPrice = tpCost;
    }

    // Option 2: Craft via recipe (recurse into ingredients)
    const auto* recipe = CraftyLegend::DataManager::GetRecipe(item_id);
    if (recipe && !recipe->ingredients.empty()) {
        visited.insert(item_id);
        int numCrafts = CraftyLegend::DataManager::CraftsNeeded(
            item_id, remaining, recipe->output_count);
        long long craftSum = 0;
        for (const auto& ing : recipe->ingredients) {
            int ingCount = static_cast<int>(ing.count) * numCrafts;
            craftSum += nodeKey
                ? GetRouteAwarePrice(ing.item_id, ingCount, visited,
                                     *nodeKey + "/" + std::to_string(ing.item_id))
                : GetRecursivePrice(ing.item_id, ingCount, visited);
        }
        visited.erase(item_id);
        if (bestPrice < 0 || craftSum < bestPrice) bestPrice = craftSum;
    }

    // Option 3: Vendor purchase
    long long vendorCost = GetVendorPrice(item_id, remaining, visited);
    if (vendorCost >= 0 && (bestPrice < 0 || vendorCost < bestPrice)) bestPrice = vendorCost;

    return bestPrice > 0 ? bestPrice : 0;
}

// As GetRecursivePrice, but at a node that offers a real choice of acquisition
// route it prices the ACTIVE route rather than the cheapest one. Everywhere else
// it hands straight back to GetRecursivePrice, so single-route items keep their
// existing "cheapest of trading post / craft / vendor" figure. Owned counts are
// still subtracted at every level, matching the rest of the price display.
static long long GetRouteAwarePrice(uint32_t item_id, int count,
                                    std::unordered_set<uint32_t>& visited,
                                    const std::string& nodeKey) {
    if (item_id == 0 || count <= 0) return 0;
    if (!CraftyLegend::GW2API::HasPriceData()) return 0;
    if (visited.count(item_id)) return 0; // cycle guard

    // No genuine choice here: price it the usual way, but keep the route context so
    // any multi-route item deeper in the recipe still follows its own active route.
    if (!HasRouteChoice(item_id)) return GetRecursivePrice(item_id, count, visited, &nodeKey);

    auto methods = CraftyLegend::UI::MeaningfulMethods(item_id);
    int active = CraftyLegend::UI::ResolveActiveMethodIndex(item_id, nodeKey);
    if (active < 0 || active >= static_cast<int>(methods.size())) {
        return GetRecursivePrice(item_id, count, visited, &nodeKey);
    }
    const auto& method = methods[active];

    int remaining = count;
    if (CraftyLegend::GW2API::HasAccountData()) {
        remaining = std::max(0, remaining - GetEffectiveOwnedCount(item_id));
    }
    if (remaining <= 0) return 0;

    if (method.method == "trading_post") {
        int unit = CraftyLegend::GW2API::GetSellPrice(item_id);
        return unit > 0 ? static_cast<long long>(unit) * remaining : 0;
    }

    if (method.method == "vendor") {
        long long sum = 0;
        const std::string mKey = nodeKey + "#m:" + std::to_string(active);
        visited.insert(item_id);
        int idx = 0;
        for (const auto& req : method.purchase_requirements) {
            const std::string reqKey = mKey + "#vreq:" + std::to_string(idx++);
            int amount = ParseRequirementAmount(req.second);
            if (req.first == "Coin") {
                if (amount > 0) sum += static_cast<long long>(amount) * remaining;
                continue;
            }
            if (amount <= 0) continue;
            uint32_t sub_id = CraftyLegend::DataManager::ResolveRequirementItemId(req.first);
            if (sub_id == 0) continue; // wallet currency: no gold equivalent
            sum += GetRouteAwarePrice(sub_id, amount * remaining, visited,
                                      reqKey + "/" + std::to_string(sub_id));
        }
        visited.erase(item_id);
        return sum;
    }

    // crafting / mystic_forge
    const auto* recipe = CraftyLegend::DataManager::GetRecipe(item_id);
    if (!recipe || recipe->ingredients.empty()) {
        return GetRecursivePrice(item_id, count, visited, &nodeKey);
    }
    visited.insert(item_id);
    int numCrafts = CraftyLegend::DataManager::CraftsNeeded(
        item_id, remaining, recipe->output_count);
    long long sum = 0;
    for (const auto& ing : recipe->ingredients) {
        sum += GetRouteAwarePrice(ing.item_id, static_cast<int>(ing.count) * numCrafts, visited,
                                  nodeKey + "/" + std::to_string(ing.item_id));
    }
    visited.erase(item_id);
    return sum;
}

// Helper: get the total gold cost for a material (recursive through crafting tree)
int GetMaterialTotalPrice(const CraftyLegend::RecipeIngredient& mat) {
    // Coin materials: count is already the total copper amount
    if (mat.name == "Coin" && mat.count > 0) return static_cast<int>(mat.count);
    if (mat.item_id == 0 || mat.count == 0) return 0;
    if (!CraftyLegend::GW2API::HasPriceData()) return 0;
    std::unordered_set<uint32_t> visited;
    long long price = GetRecursivePrice(mat.item_id, static_cast<int>(mat.count), visited);
    // Cap to int range
    if (price > INT_MAX) return INT_MAX;
    return static_cast<int>(price);
}

// The tree's variant: same figure, but multi-route nodes are priced down the route
// the tree is showing, so a row's cost agrees with the method row above it and with
// the route-aware heading total. An empty nodeKey means "no route context" and
// falls back to the cheapest-route figure Miller uses.
int GetMaterialTotalPriceForRoute(const CraftyLegend::RecipeIngredient& mat,
                                  const std::string& nodeKey) {
    if (nodeKey.empty()) return GetMaterialTotalPrice(mat);
    if (mat.name == "Coin" && mat.count > 0) return static_cast<int>(mat.count);
    if (mat.item_id == 0 || mat.count == 0) return 0;
    if (!CraftyLegend::GW2API::HasPriceData()) return 0;
    std::unordered_set<uint32_t> visited;
    long long price = GetRouteAwarePrice(mat.item_id, static_cast<int>(mat.count), visited, nodeKey);
    if (price > INT_MAX) return INT_MAX;
    return static_cast<int>(price);
}

// A "Coin" cost is rendered as a dim "Gold Cost" label plus the amount, rather than
// the usual "owned/needed" material row — but the affordability signal must survive:
// green when the wallet covers the cost, matching the green a normal material row
// gets when you own enough of it.
bool CanAffordCoinCost(int total_copper) {
    if (total_copper <= 0 || !CraftyLegend::GW2API::HasAccountData()) return false;
    int wallet = CraftyLegend::GW2API::GetWalletAmountByName("Coin");
    return wallet >= total_copper;
}

ImVec4 GoldCostLabelColor(int total_copper) {
    return CanAffordCoinCost(total_copper)
        ? ImVec4(0.35f, 0.82f, 0.35f, 1.0f)  // completedColor, matches DrawItemRow
        : ImVec4(0.60f, 0.60f, 0.65f, 1.0f);
}

// A coin cost gets the same behind-the-row progress bar a normal material row gets
// from DrawItemRow — green once the wallet covers it, blue and part-width while it
// does not. Coin rows are drawn by their own branches in Miller and the tree, so
// they cannot inherit DrawItemRow's bar; this keeps the two visually identical.
void DrawCoinCostBar(const ImVec2& rowPos, float rowWidth, float rowHeight, int total_copper) {
    if (total_copper <= 0 || !CraftyLegend::GW2API::HasAccountData()) return;
    int wallet = CraftyLegend::GW2API::GetWalletAmountByName("Coin");
    if (wallet <= 0) return;
    float pct = std::min(1.0f, static_cast<float>(wallet) / static_cast<float>(total_copper));
    if (pct <= 0.0f) return;
    ImU32 barCol = (wallet >= total_copper)
        ? IM_COL32(50, 180, 50, 35)   // green = affordable
        : IM_COL32(60, 140, 200, 25); // blue = partial
    const float rowPadX = 6.0f; // matches DrawItemRow's textPadX
    float barLeft = rowPos.x - rowPadX;
    ImGui::GetWindowDrawList()->AddRectFilled(
        ImVec2(barLeft, rowPos.y),
        ImVec2(barLeft + rowWidth * pct, rowPos.y + rowHeight),
        barCol, 2.0f);
}

// "12/40" for an achievement that is underway, or "" when there is nothing useful to
// show — unknown to H&S, already done, no max reported, or not started (the GW2 account
// endpoint omits unstarted achievements, which H&S surfaces as current == -1).
std::string AchievementProgressText(int achievement_id) {
    if (achievement_id <= 0) return "";
    CraftyLegend::GW2API::AchievementProgress p;
    if (!CraftyLegend::GW2API::GetAchievementProgress(achievement_id, p)) return "";
    if (p.done || p.max <= 0 || p.current < 0) return "";
    int cur = std::min(p.current, p.max);
    return std::to_string(cur) + "/" + std::to_string(p.max);
}

// g_DebugLog is written from the render thread (via AddDebugLog's other call
// sites) and from Hoard & Seek's own worker thread (crafting sweep responses
// in hoard.cpp arrive there — see OnCharacterCraftingResponse). It is also
// read and cleared every frame by ui.cpp's debug window. None of that is safe
// unguarded, so every access to g_DebugLog anywhere in the addon must go
// through this mutex. AddDebugLog, GetDebugLogSnapshot and ClearDebugLog are
// the only functions allowed to touch g_DebugLog directly; callers elsewhere
// (ui.cpp, addon.cpp) go through these three so the lock can never be
// bypassed by accident.
static std::mutex g_DebugLogMutex;

// Helper: add a timestamped debug log entry. Safe to call from any thread.
void AddDebugLog(const std::string& message) {
    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;

    // localtime_s (the Windows CRT form MinGW provides) writes into a
    // caller-supplied struct, unlike glibc's localtime/localtime_r, which
    // return a pointer to a shared static buffer. That makes it safe to call
    // without the mutex on its own, but the formatting is cheap and the lock
    // below already has to cover the vector mutation, so it is taken first
    // and held for the whole function rather than reasoning about two
    // separate safety arguments.
    std::lock_guard<std::mutex> lock(g_DebugLogMutex);

    std::stringstream ss;
    char timeStr[32];
    struct tm tmBuf{};
    // localtime_s returns nonzero on failure and leaves tmBuf untouched (still
    // value-initialised, i.e. the epoch), which would silently timestamp the
    // line "00:00:00" — use an obviously-wrong marker instead so a bad
    // timestamp cannot be mistaken for a real one.
    if (localtime_s(&tmBuf, &time) == 0) {
        std::strftime(timeStr, sizeof(timeStr), "%H:%M:%S", &tmBuf);
    } else {
        std::strncpy(timeStr, "??:??:??", sizeof(timeStr));
    }
    ss << timeStr << "." << std::setfill('0') << std::setw(3) << ms.count() << " " << message;

    g_DebugLog.push_back(ss.str());

    if (g_DebugLog.size() > MAX_DEBUG_LINES) {
        g_DebugLog.erase(g_DebugLog.begin());
    }
}

// Copy of the current log lines, taken under the same mutex as AddDebugLog.
// The render thread should use this (never g_DebugLog directly) so iterating
// the log can never race a worker-thread AddDebugLog call.
std::vector<std::string> GetDebugLogSnapshot() {
    std::lock_guard<std::mutex> lock(g_DebugLogMutex);
    return g_DebugLog;
}

// Clears the log under the same mutex as AddDebugLog.
void ClearDebugLog() {
    std::lock_guard<std::mutex> lock(g_DebugLogMutex);
    g_DebugLog.clear();
}

// Helper: format a material label like "42/77 Mystic Clover >" or "77 Mystic Clover >"
std::string FormatMaterialLabel(const CraftyLegend::RecipeIngredient& mat, bool* out_complete, bool* out_ready, bool append_drill_arrow) {
    std::string label;
    bool hasData = CraftyLegend::GW2API::HasAccountData();

    // Localized display name (display-only; mat.name stays English for wallet/logic lookups)
    std::string dispName = mat.item_id != 0
        ? Localization::ItemName(mat.item_id, mat.name)
        : CraftyLegend::GW2API::LocalizeCurrencyName(mat.name);

    // Determine owned count: check wallet for vendor costs (item_id==0), items otherwise
    int owned = 0;
    if (hasData) {
        if (mat.item_id == 0) {
            // Vendor cost material - look up wallet by currency name
            int wallet_amount = CraftyLegend::GW2API::GetWalletAmountByName(mat.name);
            if (wallet_amount >= 0) {
                owned = wallet_amount;
            }
        } else {
            owned = GetEffectiveOwnedCount(mat.item_id);
        }
    }

    if (hasData && mat.count > 0) {
        label = std::to_string(owned) + "/" + std::to_string(mat.count) + " " + dispName;
        bool complete = (owned >= (int)mat.count);
        if (out_complete) *out_complete = complete;
        // Check ready-to-craft: not directly owned enough, but all sub-materials are met
        if (out_ready) {
            if (complete) {
                *out_ready = false; // already complete, no need for ready indicator
            } else {
                std::unordered_set<uint32_t> visited;
                *out_ready = IsReadyToCraft(mat.item_id, (int)mat.count, visited);
            }
        }
    } else if (hasData && mat.count == 0) {
        // Non-numeric vendor cost (displayed as "Currency: cost_string")
        label = dispName;
        if (out_complete) *out_complete = false;
        if (out_ready) *out_ready = false;
    } else {
        if (mat.count > 1) {
            label = std::to_string(mat.count) + " " + dispName;
        } else if (mat.count == 1) {
            label = dispName;
        } else {
            label = dispName;
        }
        if (out_complete) *out_complete = false;
        if (out_ready) *out_ready = false;
    }
    if (append_drill_arrow && CanDrillInto(mat.item_id)) {
        label += " >";
    }
    return label;
}

// Shared per-material row renderer. Extracted verbatim from the Miller
// dynamic-column loop in ui.cpp so both layouts draw identical rows. Only
// visuals + the label Selectable live here; the caller owns click handling,
// selection, the per-account/gate hover tooltip and the right-click menu.
RowResult DrawItemRow(const RowVisual& v) {
    // Matches ui.cpp: internal text padding from column edge (Miller uses 6.0f).
    const float textPadX = 6.0f;
    // Semantic status colours are theme-independent (see ui.cpp comment) so the
    // same literals are safe to use here for the Selectable text.
    const ImVec4 completedColor(0.35f, 0.82f, 0.35f, 1.0f);
    const ImVec4 readyColor(0.35f, 0.78f, 0.88f, 1.0f);

    const CraftyLegend::RecipeIngredient& mat = *v.mat;

    float rowBaseX = ImGui::GetCursorPosX();
    float rowBaseY = ImGui::GetCursorPosY();

    // Row dimensions
    ImVec2 rowPos = ImGui::GetCursorScreenPos();
    float rowH = v.showIcons ? (ICON_SIZE + 2.0f) : ImGui::GetTextLineHeightWithSpacing();

    // Alternating row tinting for readability
    if (v.altTint) {
        ImGui::GetWindowDrawList()->AddRectFilled(
            ImVec2(rowPos.x - textPadX, rowPos.y),
            ImVec2(rowPos.x + v.rowWidth - textPadX, rowPos.y + rowH),
            IM_COL32(255, 255, 255, 8));
    }

    // Progress bar behind row (if account data available)
    if (v.hasAccountData && mat.count > 0 && mat.name != "Coin") {
        int pOwned = 0;
        if (mat.item_id == 0) {
            int wa = CraftyLegend::GW2API::GetWalletAmountByName(mat.name);
            if (wa >= 0) pOwned = wa;
        } else {
            pOwned = GetEffectiveOwnedCount(mat.item_id);
        }
        float pct = std::min(1.0f, (float)pOwned / (float)mat.count);
        if (pct > 0.0f) {
            float barLeft = rowPos.x - textPadX;
            float barFullW = v.rowWidth; // full column width
            ImU32 barCol = (pOwned >= (int)mat.count)
                ? IM_COL32(50, 180, 50, 35)   // green = complete
                : IM_COL32(60, 140, 200, 25); // blue = partial
            ImGui::GetWindowDrawList()->AddRectFilled(
                ImVec2(barLeft, rowPos.y),
                ImVec2(barLeft + barFullW * pct, rowPos.y + rowH),
                barCol, 2.0f);
        }
    }

    // Render price (right-aligned within priceMaxW, vertically centered)
    int totalPrice = GetMaterialTotalPrice(mat);
    if (v.priceMaxW > 0.0f) {
        if (totalPrice > 0) {
            float thisPW = CalcPriceWidth(totalPrice);
            float padLeft = v.priceMaxW - thisPW;
            if (padLeft > 0) ImGui::SetCursorPosX(rowBaseX + padLeft);
            if (v.showIcons) {
                float priceVOff = (rowH - ImGui::GetTextLineHeight()) * 0.5f;
                ImGui::SetCursorPosY(rowBaseY + priceVOff);
            }
            RenderPrice(totalPrice);
            ImGui::SameLine(0, 0);
            ImGui::SetCursorPosY(rowBaseY); // reset Y so icon isn't pushed down
        }
    }

    // Request icon if enabled
    Texture_t* icon = nullptr;
    if (v.showIcons) {
        uint32_t iconId = mat.item_id;
        std::string iconUrl;
        std::string iconName;

        if (iconId != 0) {
            // Normal item icon
            const auto* item = CraftyLegend::DataManager::GetItem(iconId);
            if (item) {
                iconUrl = item->icon;
                iconName = item->name;
            }
        } else {
            // Wallet currency - use synthetic ID (0x80000000 + currency_id)
            const auto* currency = CraftyLegend::DataManager::GetCurrencyByName(mat.name);
            if (currency) {
                iconId = 0x80000000 | currency->id;
                iconUrl = currency->icon;
                iconName = currency->name;
                if (g_LoggedIconRequests.find(iconId) == g_LoggedIconRequests.end()) {
                    g_LoggedIconRequests.insert(iconId); // log once per currency, not every frame
                    std::stringstream dbg;
                    dbg << "[CurrIcon] \"" << mat.name << "\" -> currency " << currency->id
                        << " (" << currency->name << ") iconUrl=" << (iconUrl.empty() ? "EMPTY" : "yes")
                        << " syntheticId=" << iconId;
                    AddDebugLog(dbg.str());
                }
            } else {
                static std::unordered_set<std::string> loggedMisses;
                if (loggedMisses.find(mat.name) == loggedMisses.end()) {
                    loggedMisses.insert(mat.name);
                    std::stringstream dbg;
                    dbg << "[CurrIcon] MISS: \"" << mat.name << "\" not found in currencies";
                    AddDebugLog(dbg.str());
                }
            }
        }

        if (iconId != 0) {
            try {
                icon = CraftyLegend::IconManager::GetIcon(iconId);
            } catch (...) {
                icon = nullptr;
            }

            if (!icon && !CraftyLegend::IconManager::IsIconLoaded(iconId)) {
                if (g_LoggedIconRequests.find(iconId) == g_LoggedIconRequests.end()) {
                    g_LoggedIconRequests.insert(iconId);
                    std::stringstream logMsg;
                    logMsg << "Requesting icon for " << iconId << " (" << mat.name << ")";
                    AddDebugLog(logMsg.str());
                }

                if (!iconUrl.empty()) {
                    CraftyLegend::IconManager::RequestIcon(iconId, iconUrl);
                } else if (!iconName.empty()) {
                    CraftyLegend::IconManager::RequestIconById(iconId, iconName);
                }
            }
        }
    }

    // Render icon at fixed position with rarity border
    if (v.showIcons) {
        float iconX = rowBaseX + v.priceMaxW + v.priceGap;
        ImGui::SetCursorPosX(iconX);
        if (icon && icon->Resource) {
            try {
                ImVec2 iconScreenPos = ImGui::GetCursorScreenPos();
                ImGui::Image(icon->Resource, ImVec2(ICON_SIZE, ICON_SIZE));
                // Tooltip on icon hover
                if (ImGui::IsItemHovered()) {
                    ImGui::BeginTooltip();
                    // Show larger icon in tooltip
                    ImGui::Image(icon->Resource, ImVec2(48, 48));
                    ImGui::SameLine();
                    ImGui::BeginGroup();
                    if (mat.item_id != 0) {
                        const auto* tipItem = CraftyLegend::DataManager::GetItem(mat.item_id);
                        if (tipItem) {
                            ImU32 rarityCol = GetRarityBorderColor(tipItem->rarity);
                            ImVec4 nameCol = rarityCol != 0
                                ? ImGui::ColorConvertU32ToFloat4(rarityCol)
                                : ImVec4(1,1,1,1);
                            nameCol.w = 1.0f;
                            ImGui::TextColored(nameCol, "%s", Localization::ItemName(mat.item_id, tipItem->name).c_str());
                            if (!tipItem->rarity.empty()) {
                                ImGui::TextColored(ImVec4(0.7f,0.7f,0.7f,1), "%s", Localization::Tr(tipItem->rarity.c_str()));
                            }
                            std::string rawDesc = Localization::ItemDescription(mat.item_id, tipItem->description);
                            if (!rawDesc.empty()) {
                                ImGui::PushTextWrapPos(300.0f);
                                std::string cleanDesc = StripMarkup(rawDesc);
                                ImGui::TextColored(ImVec4(0.6f,0.6f,0.6f,1), "%s", cleanDesc.c_str());
                                ImGui::PopTextWrapPos();
                            }
                        } else {
                            ImGui::Text("%s", Localization::ItemName(mat.item_id, mat.name).c_str());
                        }
                    } else {
                        // Wallet currency
                        ImGui::Text("%s", CraftyLegend::GW2API::LocalizeCurrencyName(mat.name).c_str());
                        ImGui::TextColored(ImVec4(0.7f,0.7f,0.7f,1), "%s", Localization::Tr("Wallet Currency"));
                    }
                    ImGui::EndGroup();
                    ImGui::EndTooltip();
                }
                // Rarity border
                if (mat.item_id != 0) {
                    const auto* matItem = CraftyLegend::DataManager::GetItem(mat.item_id);
                    if (matItem) {
                        ImU32 rarityCol = GetRarityBorderColor(matItem->rarity);
                        if (rarityCol != 0) {
                            ImGui::GetWindowDrawList()->AddRect(iconScreenPos,
                                ImVec2(iconScreenPos.x + ICON_SIZE, iconScreenPos.y + ICON_SIZE),
                                rarityCol, 2.0f, 0, 1.5f);
                        }
                    }
                }
                ImGui::SameLine(0, 0);
            } catch (...) {}
        }
    }

    bool isComplete = false;
    bool isReady = false;
    std::string label = FormatMaterialLabel(mat, &isComplete, &isReady, v.drillArrow);

    if (isComplete) {
        ImGui::PushStyleColor(ImGuiCol_Text, completedColor);
    } else if (isReady) {
        ImGui::PushStyleColor(ImGuiCol_Text, readyColor);
    }

    float selectW = v.rowWidth - v.labelStartX - textPadX;
    if (selectW < 50.0f) selectW = 50.0f;

    // Label at fixed position, full-height selectable with centered text
    ImGui::SetCursorPosX(rowBaseX + v.labelStartX);
    ImGui::SetCursorPosY(rowBaseY); // align to row top
    if (v.showIcons) {
        ImGui::PushStyleVar(ImGuiStyleVar_SelectableTextAlign, ImVec2(0.0f, 0.5f));
    }
    bool clicked = ImGui::Selectable(label.c_str(), v.selected, 0, ImVec2(selectW, rowH));
    // Double-click is reported separately (the tree toggles expand on it). Read it
    // straight after the Selectable so IsItemHovered still refers to that widget.
    bool doubleClicked = ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left);
    if (v.showIcons) {
        ImGui::PopStyleVar();
    }
    // Lock marker on achievement-gated rows (drawn after Selectable so it overlays)
    if (v.gates && !v.gates->empty()) {
        bool haveData = v.hasAccountData;
        bool allDone = true;
        for (const auto& g : *v.gates) if (!g.completed) { allDone = false; break; }
        ImU32 lockCol = !haveData ? IM_COL32(160, 160, 160, 210)
                      : (allDone ? IM_COL32(80, 200, 80, 235)
                                 : IM_COL32(235, 175, 45, 235));
        float lh = ImGui::GetTextLineHeight() * 0.72f;
        float scrollbarW = ImGui::GetStyle().ScrollbarSize;
        ImVec2 lc(rowPos.x + v.rowWidth - textPadX * 2 - scrollbarW - lh * 0.5f,
                  rowPos.y + rowH * 0.5f);
        ImDrawList* dl = ImGui::GetWindowDrawList();
        float bw = lh * 0.66f, bh = lh * 0.52f;
        ImVec2 bMin(lc.x - bw * 0.5f, lc.y - bh * 0.5f + lh * 0.16f);
        ImVec2 bMax(lc.x + bw * 0.5f, lc.y + bh * 0.5f + lh * 0.16f);
        dl->AddRectFilled(bMin, bMax, lockCol, 1.5f);
        dl->PathArcTo(ImVec2(lc.x, bMin.y), bw * 0.34f, 3.14159265f, 6.2831853f, 10);
        dl->PathStroke(lockCol, 0, 1.6f);
    }

    if (isComplete || isReady) {
        ImGui::PopStyleColor();
    }

    return { clicked, doubleClicked };
}

namespace {

// "Artificer, Weaponsmith or Huntsman at 400" — or, at five or more
// alternatives, the heading's own short codes so the line stays readable.
std::string DisciplineHeaderLine(const std::vector<std::string>& disciplines,
                                 int rating) {
    std::string names;
    if (disciplines.size() >= 5) {
        names = CraftyLegend::DataManager::FormatDisciplines(disciplines);
    } else {
        for (size_t i = 0; i < disciplines.size(); ++i) {
            if (i > 0) {
                names += (i + 1 == disciplines.size())
                    ? std::string(" ") + Localization::Tr("or") + " "
                    : ", ";
            }
            names += disciplines[i];
        }
    }
    std::string ratingText = (rating > 0)
        ? std::to_string(rating)
        : std::string("(") + Localization::Tr("any rating") + ")";
    return names + " " + Localization::Tr("at") + " " + ratingText;
}

void DrawMatchGroup(const char* headingKey,
                    const std::vector<CraftyLegend::CharacterCrafting::Match>& matches,
                    bool dimmed) {
    if (matches.empty()) return;
    ImGui::Spacing();
    if (dimmed) {
        ImGui::TextDisabled("%s", Localization::Tr(headingKey));
    } else {
        ImGui::TextColored(ImVec4(0.85f, 0.72f, 0.42f, 1.0f), "%s",
                           Localization::Tr(headingKey));
    }
    // Cap the list so a large account can't grow the tooltip past the window.
    constexpr size_t kMaxRows = 8;
    const size_t shown = matches.size() < kMaxRows ? matches.size() : kMaxRows;
    for (size_t i = 0; i < shown; ++i) {
        const auto& m = matches[i];
        if (dimmed) {
            ImGui::TextDisabled("  %s  -  %s %d",
                                m.character.c_str(), m.discipline.c_str(), m.rating);
        } else {
            ImGui::Text("  %s  -  %s %d",
                        m.character.c_str(), m.discipline.c_str(), m.rating);
        }
    }
    if (matches.size() > shown) {
        ImGui::TextDisabled("  +%d more", (int)(matches.size() - shown));
    }
}

} // namespace

void DrawCraftingDisciplineTooltip(const CraftyLegend::Recipe* recipe) {
    if (!recipe) return;
    if (recipe->type != "crafting") return;      // forge/vendor/TP have no gate
    if (recipe->disciplines.empty()) return;
    if (!ImGui::IsItemHovered()) return;

    // No Hoard & Seek (or no account data from it) means there is no roster to
    // talk about at all: per the design, the heading then behaves exactly as it
    // did before this feature — no tooltip.
    const CraftingSweepState sweep = GetCraftingSweepState();
    if (sweep == CraftingSweepState::NoHoard) return;

    const std::string account = CraftyLegend::GW2API::GetCurrentAccountName();
    const auto result = CraftyLegend::CharacterCrafting::Query(
        account, recipe->disciplines, (int)recipe->rating);

    using DataState = CraftyLegend::CharacterCrafting::DataState;

    // NoData means we do not even know the account's character list yet. This
    // used to return silently, which hid a real bug for months: single-account
    // users never had account detection run, so the roster was permanently empty
    // and the tooltip simply never appeared - and because this check came first,
    // none of the honest explanations below could ever be reached either. Say
    // something instead, so the next failure of this kind is diagnosable.
    const bool noRoster = (result.state == DataState::NoData);

    ImGui::BeginTooltip();
    ImGui::PushTextWrapPos(ImGui::GetFontSize() * 28.0f);

    ImGui::TextColored(ImVec4(0.85f, 0.72f, 0.42f, 1.0f), "%s",
                       DisciplineHeaderLine(recipe->disciplines,
                                            (int)recipe->rating).c_str());
    ImGui::Separator();

    // Each of these says why the list is short, rather than claiming a fetch is
    // still running when the sweep has in fact stopped for good.
    const bool haveNames = !result.canCraftNow.empty() || !result.needsSwap.empty();
    if (result.state == DataState::Denied) {
        // Persisted only for the API-key scope error — see OnCharacterCraftingResponse.
        ImGui::TextDisabled("%s", Localization::Tr("Character data unavailable"));
        ImGui::TextDisabled("%s",
            Localization::Tr("Your API key needs the 'characters' permission"));
    } else if (sweep == CraftingSweepState::HoardPermissionDenied && !haveNames) {
        ImGui::TextDisabled("%s", Localization::Tr("Character data unavailable"));
        ImGui::TextDisabled("%s",
            Localization::Tr("Hoard & Seek denied Crafty Legend permission"));
        ImGui::TextDisabled("%s", Localization::Tr(
            "Approve it in Hoard & Seek's settings, then use 'Refresh crafting levels'"));
    } else if (sweep == CraftingSweepState::VersionTooOld && !haveNames) {
        ImGui::TextDisabled("%s", Localization::Tr("Character data unavailable"));
        ImGui::TextDisabled("%s",
            Localization::Tr("Your Hoard & Seek is too old for character data"));
        ImGui::TextDisabled("%s",
            Localization::Tr("Update Hoard & Seek and relaunch the game"));
    } else if (noRoster) {
        // The roster arrives with Hoard & Seek's account list, which
        // EnsureAccountDetection retries until it lands - so "still loading" is a
        // claim we can now back, and the second line says what is outstanding.
        ImGui::TextDisabled("%s", Localization::Tr("Loading character data..."));
        ImGui::TextDisabled("%s",
            Localization::Tr("Waiting for the account's character list"));
    } else if (result.state == DataState::Loading && !haveNames) {
        ImGui::TextDisabled("%s", Localization::Tr("Loading character data..."));
    } else if (!haveNames) {
        ImGui::TextColored(ImVec4(0.85f, 0.45f, 0.35f, 1.0f), "%s",
                           Localization::Tr("No character can craft this yet"));
        if (result.hasClosest) {
            ImGui::TextDisabled("  %s: %s  -  %s %d / %d",
                                Localization::Tr("Closest"),
                                result.closest.character.c_str(),
                                result.closest.discipline.c_str(),
                                result.closest.rating,
                                (int)recipe->rating);
        }
    } else {
        DrawMatchGroup("Can craft now", result.canCraftNow, false);
        DrawMatchGroup("Needs a discipline swap", result.needsSwap, true);
        // Only promise more names when the sweep can actually still deliver them.
        if (result.state == DataState::Loading && sweep == CraftingSweepState::Active) {
            ImGui::Spacing();
            ImGui::TextDisabled("%s", Localization::Tr("Loading character data..."));
        }
    }

    ImGui::PopTextWrapPos();
    ImGui::EndTooltip();
}


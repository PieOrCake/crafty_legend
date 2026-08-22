// Host-side regression harness for the shopping list and the owned-material
// allocation the Miller columns and tree rows share with it.
//
// The addon is a Windows DLL, so this builds for Windows with MinGW and runs under
// Wine - see tests/run_shopping_list.sh, which also drops real trading post prices
// where GW2API::LoadPriceData() will find them.
//
// Checks, in order:
//   1. The list never asks for more of a material than (needed - owned).
//   2. Walking the tree the way the Miller columns do reaches the same totals as
//      the list. These used to disagree by design: the columns credited the whole
//      owned stack to every branch, the list shares one stack across the tree.
//   3. A route chosen in a Miller column reaches the list.
//   4. Group breakdown, including the "Gather or Earn" rows that used to be
//      dropped from the list entirely.

#include "../src/globals.h"
#include "../src/DataManager.h"
#include "../src/GW2API.h"
#include "../src/ui_helpers.h"
#include "../src/ui_tree.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <set>
#include <string>
#include <vector>

using namespace CraftyLegend;

typedef std::map<std::string, int> Rows;

static int g_failures = 0;

static void Check(bool ok, const char* what) {
    printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    if (!ok) g_failures++;
}

static Rows Build(uint32_t leg) {
    g_ShoppingListDirty = true;
    BuildShoppingList(leg);
    Rows rows;
    for (const auto& e : g_ShoppingList) rows[e.name] = e.required;
    return rows;
}

// A stash big enough that materials recurring across several branches are partly
// but not fully covered - the case where the two owned-count rules diverge.
static void SeedAccount() {
    const std::pair<uint32_t, int> owned[] = {
        {19721, 500}, {19976, 300}, {19925, 250}, {19675, 20},
        {24295, 250}, {24358, 250}, {24351, 250}, {24357, 250},
        {24289, 250}, {24300, 250}, {24283, 250}, {24350, 250},
        {24277, 250}, {24284, 250}, {24356, 250},
    };
    for (const auto& o : owned) GW2API::SetItemCount(o.first, o.second);
    GW2API::SetHasAccountData(true);
}

// ---------------------------------------------------------------------------
// 1. The list never over-charges for something you already own.
// ---------------------------------------------------------------------------
static void CheckNettingInvariant(const std::vector<Legendary>& legs) {
    int checked = 0, over = 0;
    for (const auto& leg : legs) {
        GW2API::SetHasAccountData(false);
        Rows gross = Build(leg.id);
        GW2API::SetHasAccountData(true);
        Rows net = Build(leg.id);
        std::map<std::string, uint32_t> ids;
        for (const auto& e : g_ShoppingList) ids[e.name] = e.item_id;

        for (const auto& kv : gross) {
            auto id = ids.find(kv.first);
            if (id == ids.end()) continue;
            int owned = DataManager::EffectiveOwnedCount(id->second);
            if (owned <= 0) continue;
            int ceiling = kv.second - owned;
            if (ceiling < 0) ceiling = 0;
            int actual = net.count(kv.first) ? net.at(kv.first) : 0;
            checked++;
            // Owning an INTERMEDIATE item can push the figure lower (a whole
            // sub-branch disappears); it must never push it higher.
            if (actual > ceiling) {
                over++;
                printf("      over-charged %-30s in %-24s gross %d owned %d -> %d\n",
                       kv.first.c_str(), leg.name.c_str(), kv.second, owned, actual);
            }
        }
    }
    printf("      %d owned materials checked\n", checked);
    Check(over == 0, "shopping list never charges more than (needed - owned)");
}

// ---------------------------------------------------------------------------
// 2. Miller's drilled counts agree with the list.
//
// Mirrors what ui.cpp does on a material click: scale the child by
// RemainingNeededAtNode for the node just clicked. Accumulating the leaves that
// walk reaches has to land on the same totals the list prints.
// ---------------------------------------------------------------------------
static void MillerLeaves(uint32_t leg, uint32_t item_id, int count,
                         const std::string& nodeKey, Rows& out,
                         std::set<uint32_t>& onPath, int depth) {
    if (item_id == 0 || count <= 0 || depth > 12) return;
    if (onPath.count(item_id)) return;

    int net = RemainingNeededAtNode(leg, nodeKey, item_id, count);
    if (net <= 0) return;

    auto methods = UI::MeaningfulMethods(item_id);
    if (DataManager::GetAcquisitionMethods(item_id).size() >= 2 && methods.size() >= 2) {
        int active = UI::ResolveActiveMethodIndex(item_id, nodeKey);
        if (active >= 0 && active < static_cast<int>(methods.size())) {
            const auto& m = methods[active];
            if (m.method == "vendor") {
                const std::string mKey = nodeKey + "#m:" + std::to_string(active);
                onPath.insert(item_id);
                int idx = 0;
                for (const auto& req : m.purchase_requirements) {
                    const std::string reqKey = mKey + "#vreq:" + std::to_string(idx++);
                    if (req.first == "Coin") continue;
                    int amount = 0;
                    try { amount = std::stoi(req.second); } catch (...) { continue; }
                    if (amount <= 0) continue;
                    uint32_t sub = DataManager::ResolveRequirementItemId(req.first);
                    if (sub == 0) continue;
                    MillerLeaves(leg, sub, amount * net,
                                 reqKey + "/" + std::to_string(sub), out, onPath, depth + 1);
                }
                onPath.erase(item_id);
                return;
            }
            if (m.method == "trading_post") { out[DataManager::GetItemName(item_id)] += net; return; }
        }
    }

    const Recipe* recipe = DataManager::GetRecipe(item_id);
    if (recipe && !recipe->ingredients.empty()) {
        int crafts = DataManager::CraftsNeeded(item_id, net, recipe->output_count);
        onPath.insert(item_id);
        for (const auto& ing : recipe->ingredients) {
            if (ing.item_id == 0) continue;
            MillerLeaves(leg, ing.item_id, static_cast<int>(ing.count) * crafts,
                         nodeKey + "/" + std::to_string(ing.item_id), out, onPath, depth + 1);
        }
        onPath.erase(item_id);
        return;
    }

    out[DataManager::GetItemName(item_id)] += net;

    // Leaf bought from a vendor: the list also expands the first method with costs.
    for (const auto& acq : DataManager::GetAcquisitionMethods(item_id)) {
        if (acq.purchase_requirements.empty()) continue;
        onPath.insert(item_id);
        int idx = 0;
        for (const auto& req : acq.purchase_requirements) {
            const std::string reqKey = nodeKey + "#vreq:" + std::to_string(idx++);
            if (req.first == "Coin") continue;
            int amount = 0;
            try { amount = std::stoi(req.second); } catch (...) { continue; }
            if (amount <= 0) continue;
            uint32_t sub = DataManager::ResolveRequirementItemId(req.first);
            if (sub == 0) continue;
            MillerLeaves(leg, sub, amount * net,
                         reqKey + "/" + std::to_string(sub), out, onPath, depth + 1);
        }
        onPath.erase(item_id);
        break;
    }
}

static void CheckMillerAgreesWithList(const std::vector<Legendary>& legs) {
    int disagree = 0;
    std::vector<std::pair<int, std::string>> worst;
    for (const auto& leg : legs) {
        Rows list = Build(leg.id);
        Rows miller;
        std::set<uint32_t> onPath;
        const std::string root = std::to_string(leg.id);
        const Recipe* top = DataManager::GetRecipe(leg.id);
        if (top && !top->ingredients.empty()) {
            for (const auto& ing : top->ingredients) {
                if (ing.item_id == 0) continue;
                MillerLeaves(leg.id, ing.item_id, static_cast<int>(ing.count),
                             root + "/" + std::to_string(ing.item_id), miller, onPath, 1);
            }
        } else {
            MillerLeaves(leg.id, leg.id, 1, root, miller, onPath, 0);
        }
        // The list drops rows it cannot price at all; compare only what it kept.
        int gap = 0;
        for (const auto& kv : list) {
            int m = miller.count(kv.first) ? miller.at(kv.first) : kv.second;
            gap += std::abs(m - kv.second);
        }
        if (gap > 0) { disagree++; worst.push_back({gap, leg.name}); }
    }
    std::sort(worst.begin(), worst.end(),
              [](const std::pair<int, std::string>& a, const std::pair<int, std::string>& b) {
                  return a.first > b.first; });
    for (size_t i = 0; i < worst.size() && i < 6; ++i)
        printf("      %s disagrees by %d units\n", worst[i].second.c_str(), worst[i].first);
    Check(disagree == 0, "drilling the Miller columns reaches the same totals as the list");
}

// ---------------------------------------------------------------------------
// 3. A route picked in a Miller column reaches the list.
// ---------------------------------------------------------------------------
static void CheckMillerRouteReachesList(const std::vector<Legendary>& legs) {
    int tested = 0, moved = 0, ignored = 0;
    for (const auto& leg : legs) {
        DataManager::InitializeColumns();
        DataManager::UpdateColumn(0, leg.id);
        if (DataManager::GetColumns().size() < 2) continue;

        // Find a direct component with a real route choice, and drill into it.
        int matIdx = -1;
        uint32_t matId = 0;
        int matCount = 0;
        {
            const auto& cols = DataManager::GetColumns();
            for (size_t i = 0; i < cols[1].materials.size(); ++i) {
                uint32_t id = cols[1].materials[i].item_id;
                if (id && UI::MeaningfulMethods(id).size() >= 2 &&
                    DataManager::GetAcquisitionMethods(id).size() >= 2) {
                    matIdx = static_cast<int>(i);
                    matId = id;
                    matCount = static_cast<int>(cols[1].materials[i].count);
                    break;
                }
            }
        }
        if (matIdx < 0) continue;

        DataManager::SetSelectedMaterial(1, matIdx);
        DataManager::UpdateColumn(1, matId, matCount);
        if (DataManager::GetColumns().size() < 3) continue;
        if (DataManager::GetColumns()[2].acquisitions.size() < 2) continue;

        Rows before = Build(leg.id);
        const std::string key = DataManager::GetColumnNodeKey(2);
        const int active = UI::ResolveActiveMethodIndex(matId, key);
        const int pick = (active == 0) ? 1 : 0;
        DataManager::HandleAcquisitionMethodSelection(2, pick);
        tested++;
        if (UI::ResolveActiveMethodIndex(matId, key) != pick) {
            ignored++;
            printf("      %s: picked method %d in Miller, list still resolves %d\n",
                   leg.name.c_str(), pick, UI::ResolveActiveMethodIndex(matId, key));
            continue;
        }
        if (Build(leg.id) != before) moved++;
    }
    printf("      %d Miller route picks; %d honoured, %d changed the list\n",
           tested, tested - ignored, moved);
    Check(tested > 0 && ignored == 0, "a route chosen in a Miller column reaches the shopping list");
}

// ---------------------------------------------------------------------------
// 4. Group breakdown.
// ---------------------------------------------------------------------------
static void ReportGroups(const std::vector<Legendary>& legs) {
    int tp = 0, vendor = 0, unbuyable = 0, legsWithUnbuyable = 0;
    std::map<std::string, int> common;
    for (const auto& leg : legs) {
        Build(leg.id);
        int u = 0;
        for (const auto& e : g_ShoppingList) {
            if (e.is_unbuyable) { unbuyable++; u++; common[e.name]++; }
            else if (e.is_vendor) vendor++;
            else tp++;
        }
        if (u) legsWithUnbuyable++;
    }
    printf("      rows across all legendaries: trading post %d, vendor %d, gather-or-earn %d\n",
           tp, vendor, unbuyable);
    printf("      legendaries with a gather-or-earn row: %d/%zu\n", legsWithUnbuyable, legs.size());
    std::vector<std::pair<int, std::string>> v;
    for (const auto& kv : common) v.push_back({kv.second, kv.first});
    std::sort(v.begin(), v.end(),
              [](const std::pair<int, std::string>& a, const std::pair<int, std::string>& b) {
                  return a.first > b.first; });
    for (size_t i = 0; i < v.size() && i < 8; ++i)
        printf("        x%-4d %s\n", v[i].first, v[i].second.c_str());
    Check(tp > 0 && unbuyable > 0, "list reports purchasable and gather-or-earn groups");
}

int main(int argc, char** argv) {
    if (!DataManager::Initialize()) { printf("Initialize failed\n"); return 1; }
    const bool prices = GW2API::LoadPriceData();
    printf("trading post prices: %s\n", prices ? "loaded" : "MISSING - run tests/run_shopping_list.sh");
    SeedAccount();

    const auto& legs = DataManager::GetLegendaries();
    printf("legendaries: %zu\n\n", legs.size());

    CheckNettingInvariant(legs);
    CheckMillerAgreesWithList(legs);
    CheckMillerRouteReachesList(legs);
    ReportGroups(legs);

    // Optional: dump one legendary's list in full.
    if (argc > 1) {
        uint32_t target = (uint32_t)strtoul(argv[1], nullptr, 10);
        printf("\n=== %s ===\n", DataManager::GetLegendaryName(target).c_str());
        Build(target);
        for (const auto& e : g_ShoppingList) {
            printf("  %7d  %-42s %s\n", e.required, e.name.c_str(),
                   e.is_unbuyable ? "(gather or earn)" : (e.is_vendor ? "(vendor)" : "(trading post)"));
        }
    }

    printf("\n%s (%d failing)\n", g_failures ? "FAILURES" : "all checks passed", g_failures);
    return g_failures ? 1 : 0;
}

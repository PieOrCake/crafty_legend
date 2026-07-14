#include "ui_tree.h"
#include "ui_helpers.h"
#include "imgui.h"
#include <algorithm>
#include <unordered_set>

namespace CraftyLegend { namespace UI {

void RenderTree(uint32_t legendaryId, float availWidth, float availHeight) {
    ImGui::BeginChild("TreeView", ImVec2(availWidth, availHeight), false);
    if (legendaryId == 0) {
        ImGui::TextDisabled("Select a legendary to see its crafting tree.");
    } else {
        ImGui::TextDisabled("Tree layout — placeholder (legendary %u)", legendaryId);
    }
    ImGui::EndChild();
}

// Mirrors the filter in DataManager::UpdateColumn (src/DataManager.cpp, ~line 2111-2117):
// when the item has a recipe, "trading_post" is not a meaningful acquisition choice
// because the recipe route already covers it.
std::vector<CraftyLegend::AcquisitionMethod> MeaningfulMethods(uint32_t item_id) {
    std::vector<CraftyLegend::AcquisitionMethod> methods =
        CraftyLegend::DataManager::GetAcquisitionMethods(item_id);

    const CraftyLegend::Recipe* recipe = CraftyLegend::DataManager::GetRecipe(item_id);
    if (recipe) {
        methods.erase(
            std::remove_if(methods.begin(), methods.end(),
                [](const CraftyLegend::AcquisitionMethod& a) { return a.method == "trading_post"; }),
            methods.end());
    }
    return methods;
}

// Total gold cost (copper) for a single ingredient leaf, or -1 if it cannot be
// priced in gold (missing TP data, or a wallet/currency requirement).
static long long GoldCostForIngredient(const CraftyLegend::RecipeIngredient& ing) {
    if (ing.count == 0) return 0; // contributes nothing
    if (ing.item_id == 0 && ing.name != "Coin") {
        // Wallet/currency cost (e.g. "Tales of Dungeon Delving") - not gold-comparable.
        return -1;
    }
    int price = GetMaterialTotalPrice(ing);
    if (price <= 0) {
        // 0 here means "no TP price known" (GetMaterialTotalPrice already special-cases
        // Coin and zero-count above), so treat as not gold-comparable.
        return -1;
    }
    return price;
}

long long RouteGoldCost(uint32_t item_id, const CraftyLegend::AcquisitionMethod& method, int count) {
    if (count <= 0) return 0;

    std::vector<CraftyLegend::RecipeIngredient> ingredients;

    if (method.method == "vendor") {
        for (const auto& req : method.purchase_requirements) {
            CraftyLegend::RecipeIngredient ing;
            ing.name = req.first;
            if (req.first == "Coin") {
                try {
                    long long copper = std::stoll(req.second);
                    if (copper <= 0) return -1;
                    ing.item_id = 0;
                    ing.count = static_cast<uint32_t>(copper) * static_cast<uint32_t>(count);
                } catch (...) {
                    return -1;
                }
            } else {
                uint32_t resolved = CraftyLegend::DataManager::ResolveItemIdByName(req.first);
                if (resolved == 0) {
                    // Unknown item name => a wallet/currency requirement, not gold-comparable.
                    return -1;
                }
                int amount = 0;
                try {
                    amount = std::stoi(req.second);
                } catch (...) {
                    return -1;
                }
                if (amount <= 0) return -1;
                ing.item_id = resolved;
                ing.count = static_cast<uint32_t>(amount) * static_cast<uint32_t>(count);
            }
            ingredients.push_back(ing);
        }
    } else if (method.method == "trading_post") {
        CraftyLegend::RecipeIngredient ing;
        ing.item_id = item_id;
        ing.count = static_cast<uint32_t>(count);
        ing.name = CraftyLegend::DataManager::GetItemName(item_id);
        ingredients.push_back(ing);
    } else {
        // mystic_forge / crafting - use the recipe's own ingredients, scaled by
        // how many craft operations are needed to produce `count` outputs.
        const CraftyLegend::Recipe* recipe = CraftyLegend::DataManager::GetRecipe(item_id);
        if (!recipe || recipe->ingredients.empty()) return -1;
        uint32_t output = std::max<uint32_t>(1, recipe->output_count);
        int numCrafts = (count + static_cast<int>(output) - 1) / static_cast<int>(output);
        for (const auto& ing : recipe->ingredients) {
            CraftyLegend::RecipeIngredient scaled = ing;
            scaled.count = ing.count * static_cast<uint32_t>(numCrafts);
            ingredients.push_back(scaled);
        }
    }

    std::unordered_set<uint32_t> visited;
    visited.insert(item_id);

    long long total = 0;
    for (const auto& ing : ingredients) {
        // Guard against a (malformed) self-referential ingredient causing recursion
        // back into this same item - treat it as not gold-comparable rather than loop.
        if (ing.item_id != 0 && visited.count(ing.item_id)) return -1;

        long long cost = GoldCostForIngredient(ing);
        if (cost < 0) return -1;
        total += cost;
    }
    return total;
}

int ResolveActiveMethodIndex(uint32_t item_id, const std::string& nodeKey) {
    auto methods = MeaningfulMethods(item_id);
    if (methods.size() < 2) return methods.empty() ? -1 : 0;
    // Expanded method wins.
    for (size_t i = 0; i < methods.size(); ++i) {
        if (CraftyLegend::DataManager::IsNodeExpanded(nodeKey + "#m:" + std::to_string(i)))
            return (int)i;
    }
    // All-gold => cheapest. Otherwise prefer a craftable route, else index 0.
    long long best = -1; int bestIdx = -1; bool allGold = true; int craftIdx = -1;
    for (size_t i = 0; i < methods.size(); ++i) {
        long long c = RouteGoldCost(item_id, methods[i], 1);
        if (c < 0) allGold = false; else if (best < 0 || c < best) { best = c; bestIdx = (int)i; }
        if (methods[i].method == "mystic_forge" || methods[i].method == "crafting") if (craftIdx < 0) craftIdx = (int)i;
    }
    if (allGold && bestIdx >= 0) return bestIdx;
    if (craftIdx >= 0) return craftIdx;
    return 0;
}

}}

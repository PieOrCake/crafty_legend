#include "globals.h"
#include "ui_helpers.h"
#include "GW2API.h"
#include "DataManager.h"
#include <algorithm>
#include <climits>
#include <cmath>
#include <sstream>
#include <iomanip>
#include <chrono>
#include <ctime>
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
    if (!CraftyLegend::GW2API::HasAccountData()) return 0;
    const auto* item = CraftyLegend::DataManager::GetItem(item_id);
    if (item && (item->binding == "account" || item->binding == "soul")
        && CraftyLegend::GW2API::HasCurrentAccount()) {
        return CraftyLegend::GW2API::GetOwnedCountForAccount(item_id,
            CraftyLegend::GW2API::GetCurrentAccountName());
    }
    return CraftyLegend::GW2API::GetOwnedCount(item_id);
}

// --- Legendary completion % ---

// Recursively collect total leaf material needs (no ownership subtraction)
static void CollectLeafMaterials(uint32_t item_id, int count,
    std::unordered_map<uint32_t, int>& leafItems,
    std::unordered_set<uint32_t>& visited) {
    if (item_id == 0 || count <= 0) return;
    if (visited.count(item_id)) return;

    const auto* recipe = CraftyLegend::DataManager::GetRecipe(item_id);
    if (recipe && !recipe->ingredients.empty()) {
        visited.insert(item_id);
        int output = std::max(1u, recipe->output_count);
        int numCrafts = (count + output - 1) / output;
        for (const auto& ing : recipe->ingredients) {
            if (ing.item_id != 0) {
                CollectLeafMaterials(ing.item_id, static_cast<int>(ing.count) * numCrafts,
                    leafItems, visited);
            }
            // Skip wallet currencies for completion % (hard to normalize)
        }
        visited.erase(item_id);
        return;
    }
    // Leaf material
    leafItems[item_id] += count;
}

static float ComputeLegendaryCompletion(uint32_t legendary_id) {
    std::unordered_map<uint32_t, int> leafItems;
    std::unordered_set<uint32_t> visited;
    CollectLeafMaterials(legendary_id, 1, leafItems, visited);

    if (leafItems.empty()) return 0.0f;

    double totalNeeded = 0, totalHave = 0;
    for (const auto& [id, needed] : leafItems) {
        int owned = GetEffectiveOwnedCount(id);
        totalNeeded += needed;
        totalHave += std::min(owned, needed);
    }
    return (totalNeeded > 0) ? static_cast<float>(totalHave / totalNeeded) : 0.0f;
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

// Helper: recursively flatten a crafting tree into base materials.
// Base materials are items with no recipe that can't be drilled into further.
// Accumulates into a map keyed by item_id (or by name for item_id==0 wallet costs).
static void FlattenCraftingTree(uint32_t item_id, int count,
    std::unordered_map<uint32_t, std::pair<std::string, int>>& itemMats,
    std::unordered_map<std::string, int>& walletMats,
    std::unordered_set<uint32_t>& visited) {
    if (item_id == 0 || count <= 0) return;
    if (visited.count(item_id)) return; // cycle guard

    // Subtract owned from required at each level
    int remaining = count;
    if (CraftyLegend::GW2API::HasAccountData()) {
        int owned = GetEffectiveOwnedCount(item_id);
        remaining = std::max(0, remaining - owned);
    }
    if (remaining <= 0) return;

    // If item has a recipe, recurse into ingredients
    const auto* recipe = CraftyLegend::DataManager::GetRecipe(item_id);
    if (recipe && !recipe->ingredients.empty()) {
        visited.insert(item_id);
        int output = std::max(1u, recipe->output_count);
        int numCrafts = (remaining + output - 1) / output;
        for (const auto& ing : recipe->ingredients) {
            if (ing.item_id == 0) {
                // Wallet/currency cost
                walletMats[ing.name] += static_cast<int>(ing.count) * numCrafts;
            } else {
                FlattenCraftingTree(ing.item_id, static_cast<int>(ing.count) * numCrafts,
                    itemMats, walletMats, visited);
            }
        }
        visited.erase(item_id);
        return;
    }

    // Leaf item: accumulate
    std::string name = CraftyLegend::DataManager::GetItemName(item_id);
    if (name.empty()) name = "Unknown #" + std::to_string(item_id);
    itemMats[item_id].first = name;
    itemMats[item_id].second += remaining;

    // Also walk vendor purchase sub-items so their gold costs are captured
    const auto& acqs = CraftyLegend::DataManager::GetAcquisitionMethods(item_id);
    for (const auto& acq : acqs) {
        if (acq.purchase_requirements.empty()) continue;
        visited.insert(item_id);
        for (const auto& req : acq.purchase_requirements) {
            if (req.first == "Coin") continue; // direct gold cost handled by GetVendorCoinCost
            // Parse count
            int sub_count = 0;
            try {
                size_t pos = 0;
                sub_count = std::stoi(req.second, &pos);
                if (pos != req.second.size() && req.second[pos] != ' ') sub_count = 0;
            } catch (...) { sub_count = 0; }
            if (sub_count <= 0) continue;
            uint32_t sub_id = CraftyLegend::DataManager::ResolveItemIdByName(req.first);
            if (sub_id != 0) {
                FlattenCraftingTree(sub_id, sub_count * remaining, itemMats, walletMats, visited);
            }
        }
        visited.erase(item_id);
        break; // use first acquisition method
    }
}

void BuildShoppingList(uint32_t legendary_id) {
    g_ShoppingList.clear();
    if (legendary_id == 0) return;

    std::unordered_map<uint32_t, std::pair<std::string, int>> itemMats;
    std::unordered_map<std::string, int> walletMats;
    std::unordered_set<uint32_t> visited;

    FlattenCraftingTree(legendary_id, 1, itemMats, walletMats, visited);

    // Convert to shopping entries
    // Note: FlattenCraftingTree already subtracts owned counts, so info.second
    // is the net amount still needed to purchase.
    for (const auto& [id, info] : itemMats) {
        if (info.second <= 0) continue; // already owned enough
        const auto* item = CraftyLegend::DataManager::GetItem(id);
        int tp_price = CraftyLegend::GW2API::HasPriceData() ? CraftyLegend::GW2API::GetSellPrice(id) : 0;
        int vendor_coin = GetVendorCoinCost(id); // per-unit vendor gold cost
        bool is_bound = item && item->binding != "none" && !item->binding.empty();
        if (is_bound && vendor_coin <= 0) continue; // bound item with no gold cost
        if (!is_bound && CraftyLegend::GW2API::HasPriceData() && tp_price <= 0 && vendor_coin <= 0) continue; // not purchasable
        ShoppingEntry e;
        e.item_id = id;
        e.name = info.first;
        e.required = info.second; // net amount to purchase
        e.owned = 0;
        e.is_vendor = (tp_price <= 0 && vendor_coin > 0);
        e.tp_price = tp_price > 0 ? tp_price : vendor_coin; // prefer TP, fallback to vendor
        g_ShoppingList.push_back(e);
    }
    // Wallet currencies excluded - not purchasable on TP

    // Sort within each group (vendor vs TP) by current sort mode
    auto sorter = [](const ShoppingEntry& a, const ShoppingEntry& b) {
        // Primary: group by is_vendor (TP first, vendor second)
        if (a.is_vendor != b.is_vendor) return !a.is_vendor;
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
        int output = std::max(1u, recipe->output_count);
        int numCrafts = (remaining + output - 1) / output;
        // Mystic Clover: ~33% success rate, need ~3x attempts
        if (item_id == 19675 && numCrafts > 1) numCrafts *= 3;
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
static long long GetRecursivePrice(uint32_t item_id, int count, std::unordered_set<uint32_t>& visited);

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
            uint32_t sub_id = CraftyLegend::DataManager::ResolveItemIdByName(req.first);
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
static long long GetRecursivePrice(uint32_t item_id, int count, std::unordered_set<uint32_t>& visited) {
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
        int output = std::max(1u, recipe->output_count);
        int numCrafts = (remaining + output - 1) / output;
        // Mystic Clover: ~33% success rate, need ~3x attempts
        if (item_id == 19675 && numCrafts > 1) numCrafts *= 3;
        long long craftSum = 0;
        for (const auto& ing : recipe->ingredients) {
            craftSum += GetRecursivePrice(ing.item_id, static_cast<int>(ing.count) * numCrafts, visited);
        }
        visited.erase(item_id);
        if (bestPrice < 0 || craftSum < bestPrice) bestPrice = craftSum;
    }

    // Option 3: Vendor purchase
    long long vendorCost = GetVendorPrice(item_id, remaining, visited);
    if (vendorCost >= 0 && (bestPrice < 0 || vendorCost < bestPrice)) bestPrice = vendorCost;

    return bestPrice > 0 ? bestPrice : 0;
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

// Helper: add a timestamped debug log entry
void AddDebugLog(const std::string& message) {
    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;
    
    std::stringstream ss;
    char timeStr[32];
    std::strftime(timeStr, sizeof(timeStr), "%H:%M:%S", std::localtime(&time));
    ss << timeStr << "." << std::setfill('0') << std::setw(3) << ms.count() << " " << message;
    
    g_DebugLog.push_back(ss.str());
    
    if (g_DebugLog.size() > MAX_DEBUG_LINES) {
        g_DebugLog.erase(g_DebugLog.begin());
    }
}

// Helper: format a material label like "42/77 Mystic Clover >" or "77 Mystic Clover >"
std::string FormatMaterialLabel(const CraftyLegend::RecipeIngredient& mat, bool* out_complete, bool* out_ready) {
    std::string label;
    bool hasData = CraftyLegend::GW2API::HasAccountData();

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
        label = std::to_string(owned) + "/" + std::to_string(mat.count) + " " + mat.name;
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
        label = mat.name;
        if (out_complete) *out_complete = false;
        if (out_ready) *out_ready = false;
    } else {
        if (mat.count > 1) {
            label = std::to_string(mat.count) + " " + mat.name;
        } else if (mat.count == 1) {
            label = mat.name;
        } else {
            label = mat.name;
        }
        if (out_complete) *out_complete = false;
        if (out_ready) *out_ready = false;
    }
    if (CanDrillInto(mat.item_id)) {
        label += " >";
    }
    return label;
}


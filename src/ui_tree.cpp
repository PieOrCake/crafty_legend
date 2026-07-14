#include "ui_tree.h"
#include "ui_helpers.h"
#include "GW2API.h"
#include "globals.h"
#include "Localization.h"
#include "imgui.h"
#include <algorithm>
#include <string>
#include <unordered_set>

namespace CraftyLegend { namespace UI {

// ---------------------------------------------------------------------------
// Layout constants
// ---------------------------------------------------------------------------
static const float INDENT_STEP    = 14.0f; // horizontal step per tree depth
static const int   MAX_TREE_DEPTH = 12;    // backstop against pathological recursion

// Row height matches DrawItemRow's own computation so rails/arrow line up with
// the row body (icon rows are taller than text rows).
static float TreeRowHeight() {
    return g_ShowItemIcons ? (ICON_SIZE + 2.0f)
                           : ImGui::GetTextLineHeightWithSpacing();
}

// Draw one dotted vertical rail per ancestor depth at the current row's left,
// then return the window-relative x where this node's content should begin.
static float DrawRails(int depth) {
    ImVec2 origin   = ImGui::GetCursorScreenPos(); // screen-space row top-left
    float  originWX = ImGui::GetCursorPosX();      // window-relative x
    float  rowH     = TreeRowHeight();

    ImDrawList* dl  = ImGui::GetWindowDrawList();
    const ImU32 col = IM_COL32(52, 80, 106, 255);

    for (int i = 0; i < depth; ++i) {
        float x = origin.x + i * INDENT_STEP + 6.0f;
        // Dotted rail: short 2px segments with 2px gaps down the row height.
        for (float y = origin.y; y < origin.y + rowH; y += 4.0f) {
            float y2 = std::min(y + 2.0f, origin.y + rowH);
            dl->AddLine(ImVec2(x, y), ImVec2(x, y2), col, 1.0f);
        }
    }
    return originWX + depth * INDENT_STEP;
}

// A borderless, clickable expand/collapse arrow (▸ collapsed, ▾ expanded).
// Reserves a fixed-width slot the full row height so it vertically aligns with
// the row body. Returns true when clicked.
static bool DrawArrow(bool expanded) {
    const float slotW = 12.0f;
    float rowH = TreeRowHeight();
    ImVec2 p   = ImGui::GetCursorScreenPos();

    bool clicked = ImGui::InvisibleButton("##arrow", ImVec2(slotW, rowH));
    bool hovered = ImGui::IsItemHovered();
    ImU32 col    = hovered ? IM_COL32(235, 235, 235, 255)
                           : IM_COL32(150, 160, 175, 255);

    ImDrawList* dl = ImGui::GetWindowDrawList();
    float cx = p.x + slotW * 0.5f;
    float cy = p.y + rowH * 0.5f;
    float r  = 3.5f;
    if (expanded) {
        // ▾ pointing down
        dl->AddTriangleFilled(ImVec2(cx - r, cy - r * 0.6f),
                              ImVec2(cx + r, cy - r * 0.6f),
                              ImVec2(cx,     cy + r * 0.7f), col);
    } else {
        // ▸ pointing right
        dl->AddTriangleFilled(ImVec2(cx - r * 0.6f, cy - r),
                              ImVec2(cx - r * 0.6f, cy + r),
                              ImVec2(cx + r * 0.7f, cy),     col);
    }
    return clicked;
}

// Recursively render one crafting-tree node. `depth` 0 == a direct component of
// the legendary. `path` is the "/"-joined chain of item ids from the legendary
// down to (and including) this item; it doubles as the node's expand-state key.
// `onPath` holds the ancestor item ids so a true cycle cannot loop forever
// (a diamond, where an item legitimately appears in two branches, is allowed).
static void RenderNode(uint32_t item_id, int count, int depth,
                       const std::string& path,
                       std::unordered_set<uint32_t>& onPath) {
    if (depth > MAX_TREE_DEPTH) return;

    const std::string& nodeKey = path; // already includes this item's id
    bool expandable = CanDrillInto(item_id) && onPath.find(item_id) == onPath.end();
    bool expanded   = expandable && CraftyLegend::DataManager::IsNodeExpanded(nodeKey);

    // Rails + indentation, then the arrow slot.
    float contentX = DrawRails(depth);
    ImGui::SetCursorPosX(contentX);
    if (expandable) {
        if (DrawArrow(expanded)) {
            CraftyLegend::DataManager::SetNodeExpanded(nodeKey, !expanded);
            CraftyLegend::DataManager::SaveSession();
            expanded = !expanded;
        }
        ImGui::SameLine(0, 2);
    } else {
        ImGui::Dummy(ImVec2(12.0f, TreeRowHeight()));
        ImGui::SameLine(0, 2);
    }

    // Build a synthetic ingredient for the shared row renderer.
    CraftyLegend::RecipeIngredient mat;
    mat.item_id = item_id;
    mat.count   = static_cast<uint32_t>(count < 0 ? 0 : count);
    const auto* item = CraftyLegend::DataManager::GetItem(item_id);
    mat.name = item ? item->name : "";

    std::vector<CraftyLegend::Prerequisite> gates =
        CraftyLegend::DataManager::GetItemAchievementGates(item_id);

    // rowBaseX is the current cursor x (just past the arrow slot). DrawItemRow
    // places the icon at rowBaseX and the label at rowBaseX + labelStartX, so
    // labelStartX is the icon-column width (no price column in Task 5).
    RowVisual v{};
    v.mat            = &mat;
    v.hasAccountData = CraftyLegend::GW2API::HasAccountData();
    v.showIcons      = g_ShowItemIcons;
    v.rowWidth       = ImGui::GetContentRegionAvail().x;
    v.labelStartX    = g_ShowItemIcons ? (ICON_SIZE + ICON_GAP) : 4.0f;
    v.priceMaxW      = 0.0f;
    v.priceGap       = 0.0f;
    v.selected       = false;
    v.altTint        = false;
    v.gates          = &gates;

    ImGui::PushID(static_cast<int>(item_id));
    DrawItemRow(v);
    ImGui::PopID();

    if (expanded) {
        onPath.insert(item_id);
        const auto* recipe = CraftyLegend::DataManager::GetRecipe(item_id);
        if (recipe) {
            uint32_t out = recipe->output_count > 0 ? recipe->output_count : 1;
            int crafts = (count + static_cast<int>(out) - 1) / static_cast<int>(out);
            if (crafts < 1) crafts = 1;
            for (const auto& ing : recipe->ingredients) {
                RenderNode(ing.item_id, static_cast<int>(ing.count) * crafts, depth + 1,
                           nodeKey + "/" + std::to_string(ing.item_id), onPath);
            }
        }
        onPath.erase(item_id);
    }
}

void RenderTree(uint32_t legendaryId, float availWidth, float availHeight) {
    ImGui::BeginChild("TreeView", ImVec2(availWidth, availHeight), false);
    if (legendaryId == 0) {
        ImGui::TextDisabled("Select a legendary to see its crafting tree.");
        ImGui::EndChild();
        return;
    }

    // Locate the legendary for its heading name.
    const CraftyLegend::Legendary* leg = nullptr;
    for (const auto& l : CraftyLegend::DataManager::GetLegendaries()) {
        if (l.id == legendaryId) { leg = &l; break; }
    }

    // Heading: the legendary as a title row with no arrow, then a separator.
    ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(199, 155, 240, 255));
    ImGui::TextUnformatted(leg ? Localization::ItemName(legendaryId, leg->name).c_str() : "");
    ImGui::PopStyleColor();
    ImGui::Separator();

    // Direct crafting components (the legendary recipe's ingredients) are depth 0.
    const auto* recipe = CraftyLegend::DataManager::GetRecipe(legendaryId);
    std::unordered_set<uint32_t> onPath;
    onPath.insert(legendaryId);
    if (recipe) {
        std::string root = std::to_string(legendaryId);
        for (const auto& ing : recipe->ingredients) {
            RenderNode(ing.item_id, static_cast<int>(ing.count), 0,
                       root + "/" + std::to_string(ing.item_id), onPath);
        }
    } else {
        ImGui::TextDisabled("No crafting recipe available for this legendary.");
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
    if (recipe && !recipe->ingredients.empty()) {
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
        // GetMaterialTotalPrice (via GoldCostForIngredient below) returns the best/cheapest
        // price across routes, not a TP-only price. That's safe here only because
        // MeaningfulMethods never exposes trading_post for recipe-bearing items - callers
        // must not invoke RouteGoldCost with an explicit trading_post method on such an item.
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

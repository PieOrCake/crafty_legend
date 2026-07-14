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

// Matches ui.cpp's Miller-column convention (see ui.cpp ~line 463/1048): internal
// text padding from a column/content edge. Tree rows have no left-hand price
// column (Task 8 pins cost to the panel's right edge instead), so the tree's
// analogue of Miller's `labelStartX = textPadX + maxPriceW + priceGap + iconColW`
// simplifies to `textPadX + iconColW` (maxPriceW and priceGap both 0 here).
static const float textPadX = 6.0f;

// Right-pinned acquisition-cost column: draws `copper` right-aligned to the
// tree panel's content-region right edge, at `rowBaseY`, without disturbing the
// caller's layout cursor (it saves and restores the cursor position). Used for
// every row so costs line up in a column regardless of indentation depth.
// Draws nothing when there's no priceable cost (copper <= 0), per Task 8's
// "don't render a 0 or garbage" requirement.
static void DrawRightPinnedCost(float rowBaseY, float leftBound, int copper) {
    if (copper <= 0) return;
    float w = CalcPriceWidth(copper);
    if (w <= 0.0f) return;
    float rightEdge = ImGui::GetWindowContentRegionMax().x;
    ImVec2 saved = ImGui::GetCursorPos();
    ImGui::SetCursorPosY(rowBaseY);
    ImGui::SetCursorPosX(std::max(leftBound, rightEdge - w));
    RenderPrice(copper);
    ImGui::SetCursorPos(saved);
}

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

// Forward declarations for the multi-route chooser (defined after RenderNode,
// since RenderMethodChildren recurses back into RenderNode).
static bool RenderMethodRow(uint32_t item_id,
                            const CraftyLegend::AcquisitionMethod& method,
                            int count, int depth,
                            const std::string& nodeKey, const std::string& mKey,
                            bool expanded, bool active);
static void RenderMethodChildren(uint32_t item_id,
                                 const CraftyLegend::AcquisitionMethod& method,
                                 int count, int depth,
                                 const std::string& nodeKey, const std::string& mKey,
                                 std::unordered_set<uint32_t>& onPath);

// Route-aware rolled-up gold cost (copper) for `count` of item_id, following the
// ACTIVE acquisition route at every multi-route node (mirrors the expand-tree in
// RenderNode). Returns -1 if any followed leaf can't be priced in gold. Defined
// near the bottom of the file; forward-declared here for RenderTree.
static long long ActiveRouteGoldCost(uint32_t item_id, int count,
                                     const std::string& nodeKey,
                                     std::unordered_set<uint32_t>& visited,
                                     int depth = 0);

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

    // Unique per-node ID scope, keyed on the full node path so diamonds (the
    // same item_id reachable via two branches) don't collide on ImGui IDs.
    // Must wrap both the arrow's InvisibleButton and DrawItemRow's Selectable.
    ImGui::PushID(nodeKey.c_str());

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
    // Right-pinned acquisition cost (Task 8): computed once so we can both
    // reserve room for it (narrowing the Selectable) and draw it afterwards at
    // a fixed x, consistent across all depths.
    float rowBaseY = ImGui::GetCursorPosY();
    int   rowCost  = GetMaterialTotalPrice(mat);
    float costW    = rowCost > 0 ? CalcPriceWidth(rowCost) : 0.0f;
    const float costGap = 8.0f;

    RowVisual v{};
    v.mat            = &mat;
    v.hasAccountData = CraftyLegend::GW2API::HasAccountData();
    v.showIcons      = g_ShowItemIcons;
    v.rowWidth       = ImGui::GetContentRegionAvail().x - (costW > 0.0f ? costW + costGap : 0.0f);
    v.labelStartX    = textPadX + (g_ShowItemIcons ? (ICON_SIZE + ICON_GAP) : 0.0f);
    v.priceMaxW      = 0.0f;
    v.priceGap       = 0.0f;
    v.selected       = false;
    v.altTint        = false;
    v.gates          = &gates;

    DrawItemRow(v);
    DrawRightPinnedCost(rowBaseY, contentX, rowCost);
    ImGui::PopID();

    if (expanded) {
        auto methods = MeaningfulMethods(item_id);
        if (methods.size() >= 2) {
            // Multi-route chooser: a dim sub-heading, then one accordion node per
            // acquisition method. Expanding a method makes it active and collapses
            // its siblings (handled inside RenderMethodRow via SetActiveMethod).
            ImGui::SetCursorPosX(DrawRails(depth + 1));
            ImGui::TextColored(ImVec4(0.40f, 0.47f, 0.55f, 1.0f), "%s",
                Localization::Tr("Choose one of these acquisition methods"));

            int active = ResolveActiveMethodIndex(item_id, nodeKey);
            for (size_t i = 0; i < methods.size(); ++i) {
                std::string mKey = nodeKey + "#m:" + std::to_string(i);
                bool mExpanded = CraftyLegend::DataManager::IsNodeExpanded(mKey);
                mExpanded = RenderMethodRow(item_id, methods[i], count, depth + 1,
                                            nodeKey, mKey, mExpanded, (int)i == active);
                if (mExpanded) {
                    onPath.insert(item_id);
                    RenderMethodChildren(item_id, methods[i], count, depth + 2,
                                         nodeKey, mKey, onPath);
                    onPath.erase(item_id);
                }
            }
        } else {
            // Single-route (Task 5): recurse straight into the recipe ingredients.
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
            } else if (methods.size() == 1) {
                // No recipe but a single meaningful acquisition method (e.g. a
                // vendor-only component): render that method's children directly
                // so the node doesn't expand to nothing.
                RenderMethodChildren(item_id, methods[0], count, depth + 1,
                                     nodeKey, nodeKey, onPath);
            }
            onPath.erase(item_id);
        }
    }
}

// Friendly, translatable label for an acquisition method row.
static std::string MethodLabel(const CraftyLegend::AcquisitionMethod& m) {
    if (!m.display_name.empty()) return m.display_name;
    if (m.method == "vendor")       return Localization::Tr("Vendor");
    if (m.method == "mystic_forge") return Localization::Tr("Mystic Forge");
    if (m.method == "crafting")     return Localization::Tr("Craft");
    if (m.method == "trading_post") return Localization::Tr("Trading Post");
    return m.method.empty() ? "?" : m.method;
}

// Parse a purchase-requirement amount string ("500", "500 (per stack)") into a
// positive integer, mirroring BuildVendorCostMaterials' convention. Returns
// false when the string is not a plain leading integer (a descriptive cost).
static bool ParseReqAmount(const std::string& s, int& out) {
    try {
        size_t pos = 0;
        int v = std::stoi(s, &pos);
        if (pos == s.size() || (pos < s.size() && s[pos] == ' ')) { out = v; return true; }
    } catch (...) {}
    return false;
}

// A basic right-edge cost summary for a vendor route that isn't gold-comparable
// (uses wallet currencies). Task 7 owns the authoritative roll-up; this is a
// simple readable hint only.
static std::string VendorCostSummary(
    const std::vector<std::pair<std::string, std::string>>& reqs, int count) {
    if (count < 1) count = 1;
    std::string out;
    for (const auto& req : reqs) {
        std::string piece;
        int parsed = 0;
        if (ParseReqAmount(req.second, parsed) && parsed > 0) {
            long long total = static_cast<long long>(parsed) * count;
            piece = std::to_string(total) + " " + req.first;
        } else {
            piece = req.first;
        }
        if (!out.empty()) out += "  +  ";
        out += piece;
    }
    return out;
}

// Draw a non-expandable leaf row (rails + empty arrow slot + shared row body),
// used for a vendor method's currency/item requirements. Wallet currencies keep
// item_id 0 so DrawItemRow tallies them from the wallet.
static void DrawLeafRow(const CraftyLegend::RecipeIngredient& mat, int depth,
                        const std::string& idKey) {
    ImGui::PushID(idKey.c_str());
    float contentX = DrawRails(depth);
    ImGui::SetCursorPosX(contentX);
    ImGui::Dummy(ImVec2(12.0f, TreeRowHeight()));
    ImGui::SameLine(0, 2);

    std::vector<CraftyLegend::Prerequisite> gates =
        mat.item_id ? CraftyLegend::DataManager::GetItemAchievementGates(mat.item_id)
                    : std::vector<CraftyLegend::Prerequisite>{};

    float rowBaseY = ImGui::GetCursorPosY();
    int   rowCost  = GetMaterialTotalPrice(mat);
    float costW    = rowCost > 0 ? CalcPriceWidth(rowCost) : 0.0f;
    const float costGap = 8.0f;

    RowVisual v{};
    v.mat            = &mat;
    v.hasAccountData = CraftyLegend::GW2API::HasAccountData();
    v.showIcons      = g_ShowItemIcons;
    v.rowWidth       = ImGui::GetContentRegionAvail().x - (costW > 0.0f ? costW + costGap : 0.0f);
    v.labelStartX    = textPadX + (g_ShowItemIcons ? (ICON_SIZE + ICON_GAP) : 0.0f);
    v.priceMaxW      = 0.0f;
    v.priceGap       = 0.0f;
    v.selected       = false;
    v.altTint        = false;
    v.gates          = &gates;
    DrawItemRow(v);
    DrawRightPinnedCost(rowBaseY, contentX, rowCost);
    ImGui::PopID();
}

// One accordion method row: rails + arrow, method label, an "in total" tag when
// active, inactive rows dimmed, and the route cost on the right edge. Returns the
// (possibly toggled) expand state for this method. Clicking the arrow to expand
// makes this method active (collapsing siblings); collapsing just clears it.
static bool RenderMethodRow(uint32_t item_id,
                            const CraftyLegend::AcquisitionMethod& method,
                            int count, int depth,
                            const std::string& nodeKey, const std::string& mKey,
                            bool expanded, bool active) {
    ImGui::PushID(mKey.c_str());

    float contentX = DrawRails(depth);
    ImGui::SetCursorPosX(contentX);
    if (DrawArrow(expanded)) {
        if (!expanded) {
            CraftyLegend::DataManager::SetActiveMethod(nodeKey, mKey); // accordion
            expanded = true;
        } else {
            CraftyLegend::DataManager::SetNodeExpanded(mKey, false);
            expanded = false;
        }
        CraftyLegend::DataManager::SaveSession();
    }
    ImGui::SameLine(0, 2);

    float  alpha = active ? 1.0f : 0.42f;
    ImVec4 labelCol(0.85f, 0.72f, 0.42f, alpha); // dim gold
    std::string label = MethodLabel(method);
    ImGui::TextColored(labelCol, "%s", label.c_str());

    if (active) {
        ImGui::SameLine(0, 6);
        ImGui::TextColored(ImVec4(0.45f, 0.78f, 0.52f, 1.0f), "%s",
            Localization::Tr("in total"));
    }

    // Right-edge route cost (basic; Task 7 owns the authoritative roll-up).
    float rightEdge = ImGui::GetWindowContentRegionMax().x;
    long long gold = RouteGoldCost(item_id, method, count);
    if (gold >= 0) {
        int copper = gold > 0x7fffffffLL ? 0x7fffffff : static_cast<int>(gold);
        float w = CalcPriceWidth(copper);
        if (w > 0.0f) {
            ImGui::SameLine();
            ImGui::SetCursorPosX(std::max(contentX, rightEdge - w));
            if (!active) ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.42f);
            RenderPrice(copper);
            if (!active) ImGui::PopStyleVar();
        }
    } else if (method.method == "vendor" && !method.purchase_requirements.empty()) {
        std::string txt = VendorCostSummary(method.purchase_requirements, count);
        if (!txt.empty()) {
            float w = ImGui::CalcTextSize(txt.c_str()).x;
            ImGui::SameLine();
            ImGui::SetCursorPosX(std::max(contentX, rightEdge - w));
            ImGui::TextColored(ImVec4(0.72f, 0.74f, 0.80f, alpha), "%s", txt.c_str());
        }
    }

    ImGui::PopID();
    return expanded;
}

// Children of an expanded method: forge/crafting recurses into the recipe
// ingredients (as Task 5); a vendor lists its purchase requirements as leaves.
static void RenderMethodChildren(uint32_t item_id,
                                 const CraftyLegend::AcquisitionMethod& method,
                                 int count, int depth,
                                 const std::string& nodeKey, const std::string& mKey,
                                 std::unordered_set<uint32_t>& onPath) {
    if (method.method == "vendor") {
        int qty = count < 1 ? 1 : count;
        int idx = 0;
        for (const auto& req : method.purchase_requirements) {
            CraftyLegend::RecipeIngredient mat;
            mat.item_id = CraftyLegend::DataManager::ResolveItemIdByName(req.first);
            int parsed = 0;
            if (ParseReqAmount(req.second, parsed) && parsed > 0) {
                mat.count = static_cast<uint32_t>(static_cast<long long>(parsed) * qty);
                mat.name  = req.first;
            } else {
                // Descriptive (non-numeric) cost: keep as a zero-count label row.
                mat.count = 0;
                mat.name  = (qty > 1)
                    ? req.first + ": " + std::to_string(qty) + " x " + req.second
                    : req.first + ": " + req.second;
            }
            // Keyed on the method (mKey), not just the item's node path, so two
            // methods on the same item can never collide on ImGui IDs.
            DrawLeafRow(mat, depth, mKey + "#vreq:" + std::to_string(idx++));
        }
    } else {
        const auto* recipe = CraftyLegend::DataManager::GetRecipe(item_id);
        if (recipe) {
            uint32_t out = recipe->output_count > 0 ? recipe->output_count : 1;
            int crafts = (count + static_cast<int>(out) - 1) / static_cast<int>(out);
            if (crafts < 1) crafts = 1;
            for (const auto& ing : recipe->ingredients) {
                RenderNode(ing.item_id, static_cast<int>(ing.count) * crafts, depth,
                           nodeKey + "/" + std::to_string(ing.item_id), onPath);
            }
        }
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

    // Route-aware rolled-up total cost, right-aligned on the heading line. Follows
    // the active acquisition route at every multi-route node in the tree. Only
    // shown when the whole tree prices out in gold (>0). (Task 8 owns alignment
    // polish; this is functional and simple.)
    {
        static uint32_t s_cachedLegendaryId = 0;
        static uint64_t s_cachedExpandRevision = static_cast<uint64_t>(-1);
        static uint64_t s_cachedPriceRevision = static_cast<uint64_t>(-1);
        static long long s_cachedHeadingGold = -1;

        uint64_t curRevision = CraftyLegend::DataManager::GetExpandRevision();
        uint64_t curPriceRevision = CraftyLegend::GW2API::GetPriceRevision();
        if (legendaryId != s_cachedLegendaryId || curRevision != s_cachedExpandRevision ||
            curPriceRevision != s_cachedPriceRevision) {
            std::unordered_set<uint32_t> costVisited;
            s_cachedHeadingGold = ActiveRouteGoldCost(
                legendaryId, 1, std::to_string(legendaryId), costVisited);
            s_cachedLegendaryId = legendaryId;
            s_cachedExpandRevision = curRevision;
            s_cachedPriceRevision = curPriceRevision;
        }
        long long headingGold = s_cachedHeadingGold;
        if (headingGold > 0) {
            int copper = headingGold > 0x7fffffffLL ? 0x7fffffff
                                                    : static_cast<int>(headingGold);
            float w = CalcPriceWidth(copper);
            if (w > 0.0f) {
                float rightEdge = ImGui::GetWindowContentRegionMax().x;
                ImGui::SameLine();
                ImGui::SetCursorPosX(std::max(ImGui::GetCursorPosX(), rightEdge - w));
                RenderPrice(copper);
            }
        }
    }
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

// Route-aware rolled-up gold cost. At a multi-route node (>=2 meaningful methods)
// it follows the ACTIVE method (per ResolveActiveMethodIndex, honouring tree
// expand-state); a vendor/trading_post active route is terminal and priced via
// RouteGoldCost (the same routine the method row shows). Otherwise it recurses the
// item's recipe (single-route items and the chosen craft/forge route), pricing
// wallet/Coin ingredients as leaves and drilling item ingredients route-aware so a
// nested multi-route child follows its own active route. Ignores owned counts
// (gross craft cost), matching the per-route figures on the method rows. Returns
// -1 whenever any followed portion isn't gold-comparable (wallet currency, no TP
// price, or a cycle). nodeKey mirrors RenderNode's key derivation so the resolver
// sees the same expand-state the tree does.
static long long ActiveRouteGoldCost(uint32_t item_id, int count,
                                     const std::string& nodeKey,
                                     std::unordered_set<uint32_t>& visited,
                                     int depth) {
    if (depth > MAX_TREE_DEPTH) return -1; // backstop, mirrors RenderNode's guard
    if (count <= 0) return 0;
    if (item_id == 0) return 0;
    if (visited.count(item_id)) return -1; // cycle => not gold-comparable

    // Multi-route node: follow the active route.
    auto methods = MeaningfulMethods(item_id);
    if (methods.size() >= 2) {
        int active = ResolveActiveMethodIndex(item_id, nodeKey);
        if (active < 0 || active >= static_cast<int>(methods.size())) return -1;
        const auto& method = methods[active];
        if (method.method == "vendor" || method.method == "trading_post") {
            // Terminal route in the tree; price exactly as the method row does.
            return RouteGoldCost(item_id, method, count);
        }
        // crafting / mystic_forge: fall through and recurse the recipe below.
    }

    // Recurse the recipe (single-route item, or a chosen craft/forge route).
    const CraftyLegend::Recipe* recipe = CraftyLegend::DataManager::GetRecipe(item_id);
    if (recipe && !recipe->ingredients.empty()) {
        visited.insert(item_id);
        uint32_t output = std::max<uint32_t>(1, recipe->output_count);
        int numCrafts = (count + static_cast<int>(output) - 1) / static_cast<int>(output);
        long long total = 0;
        for (const auto& ing : recipe->ingredients) {
            long long effCount = static_cast<long long>(ing.count) * numCrafts;
            long long c;
            if (ing.item_id == 0) {
                // Wallet currency or a "Coin" copper amount - price as a leaf.
                CraftyLegend::RecipeIngredient scaled = ing;
                scaled.count = static_cast<uint32_t>(effCount);
                c = GoldCostForIngredient(scaled);
            } else {
                c = ActiveRouteGoldCost(ing.item_id, static_cast<int>(effCount),
                                        nodeKey + "/" + std::to_string(ing.item_id),
                                        visited, depth + 1);
            }
            if (c < 0) { visited.erase(item_id); return -1; }
            total += c;
        }
        visited.erase(item_id);
        return total;
    }

    // Leaf with no recipe: price on the trading post.
    CraftyLegend::RecipeIngredient leaf;
    leaf.item_id = item_id;
    leaf.count   = static_cast<uint32_t>(count);
    leaf.name    = CraftyLegend::DataManager::GetItemName(item_id);
    return GoldCostForIngredient(leaf);
}

}}

#pragma once
#include <string>
#include <unordered_set>
#include <cstdint>
#include "DataManager.h"
#include <imgui.h>

// Icon layout constants (used directly in ui.cpp render code)
inline constexpr float ICON_SIZE = 28.0f;
inline constexpr float ICON_GAP  = 4.0f;

// Item drill-down / rarity
bool  CanDrillInto(uint32_t item_id);
ImU32 GetRarityBorderColor(const std::string& rarity);

// Price rendering
float CalcPriceWidth(int total_copper);
float RenderPrice(int total_copper);

// Vendor cost helper
int GetVendorCoinCost(uint32_t item_id);

// Text helpers
std::string StripMarkup(const std::string& text);
void        OpenWikiPage(const std::string& itemName);

// Account-aware owned count
int GetEffectiveOwnedCount(uint32_t item_id);

// Completion % cache
void  TickCompletionQueue();
float GetLegendaryCompletion(uint32_t legendary_id);

// Shopping list builder
void BuildShoppingList(uint32_t legendary_id);

// Material price (recursive TP/vendor)
int GetMaterialTotalPrice(const CraftyLegend::RecipeIngredient& mat);

// Debug log
void AddDebugLog(const std::string& message);

// Material label formatter
std::string FormatMaterialLabel(const CraftyLegend::RecipeIngredient& mat,
                                bool* out_complete = nullptr,
                                bool* out_ready    = nullptr);

// Shared per-material row renderer (used by the Miller layout and, later, the
// tree layout). Draws visuals only: alternating tint, progress bar, price,
// icon + rarity border + hover tooltip, label Selectable and the achievement
// lock marker. It does NOT handle clicks, selection, drilling or session state
// — the caller owns those, driven by RowResult::clicked.
struct RowVisual {
    const CraftyLegend::RecipeIngredient* mat; // item_id, count, name
    bool  hasAccountData;
    bool  showIcons;
    float rowWidth;     // full row width, for the progress bar
    float labelStartX;  // window-x where the label/selectable begins
    float priceMaxW;    // width reserved for right-aligned price (0 = none)
    float priceGap;     // gap after price column
    bool  selected;
    bool  altTint;      // alternating-row background
    const std::vector<CraftyLegend::Prerequisite>* gates; // achievement gates (may be null/empty)
};
struct RowResult { bool clicked; };
RowResult DrawItemRow(const RowVisual& v);

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

#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include "DataManager.h"

namespace CraftyLegend { namespace UI {
    // Renders the expanding-tree breakdown for the selected legendary into the
    // current ImGui window region. Called from AddonRender when g_UseTreeLayout.
    void RenderTree(uint32_t legendaryId, float availWidth, float availHeight);

    // Returns the meaningful acquisition methods for an item (trading_post dropped
    // when a recipe exists), in stable order. Empty/size-1 => no chooser.
    std::vector<CraftyLegend::AcquisitionMethod> MeaningfulMethods(uint32_t item_id);

    // Total gold cost (copper) to obtain `count` of item_id via a given method's
    // recipe ingredients, recursively, ignoring owned counts. Returns -1 if the
    // route is not fully gold-priced (any leaf lacks a TP price / uses currency).
    long long RouteGoldCost(uint32_t item_id, const CraftyLegend::AcquisitionMethod& method, int count);

    // The route that feeds totals for item_id, given expand-state + default rule.
    // nodeKey is the tree key of the item node (see Task 5).
    int ResolveActiveMethodIndex(uint32_t item_id, const std::string& nodeKey);
}}

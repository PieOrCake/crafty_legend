#pragma once
#include <cstdint>

namespace CraftyLegend { namespace UI {
    // Renders the expanding-tree breakdown for the selected legendary into the
    // current ImGui window region. Called from AddonRender when g_UseTreeLayout.
    void RenderTree(uint32_t legendaryId, float availWidth, float availHeight);
}}

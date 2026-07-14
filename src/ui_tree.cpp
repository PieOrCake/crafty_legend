#include "ui_tree.h"
#include "imgui.h"

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

}}

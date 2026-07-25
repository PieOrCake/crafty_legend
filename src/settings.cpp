#include "globals.h"
#include "settings.h"
#include "GW2API.h"
#include <fstream>
#include <filesystem>

// Display settings persistence
void SaveDisplaySettings() {
    std::string dir = CraftyLegend::GW2API::GetDataDirectory();
    if (dir.empty()) return;
    try { std::filesystem::create_directories(dir); } catch (...) {}
    std::string path = dir + "/display_settings.json";
    std::ofstream file(path);
    if (!file.is_open()) return;
    file << "{\n";
    file << "  \"show_item_icons\": " << (g_ShowItemIcons ? "true" : "false") << ",\n";
    file << "  \"show_debug_window\": " << (g_ShowDebugWindow ? "true" : "false") << ",\n";
    file << "  \"owned_filter_mode\": " << g_OwnedFilterMode << ",\n";
    file << "  \"show_qa_icon\": " << (g_ShowQAIcon ? "true" : "false") << ",\n";
    file << "  \"use_pie_theme\": " << (g_UsePieTheme ? "true" : "false") << ",\n";
    file << "  \"use_tree_layout\": " << (g_UseTreeLayout ? "true" : "false") << ",\n";
    file << "  \"first_run_notice_done\": " << (g_FirstRunNoticeDone ? "true" : "false") << ",\n";
    file << "  \"miller_win_x\": " << g_MillerGeom.x << ",\n";
    file << "  \"miller_win_y\": " << g_MillerGeom.y << ",\n";
    file << "  \"miller_win_w\": " << g_MillerGeom.w << ",\n";
    file << "  \"miller_win_h\": " << g_MillerGeom.h << ",\n";
    file << "  \"tree_win_x\": " << g_TreeGeom.x << ",\n";
    file << "  \"tree_win_y\": " << g_TreeGeom.y << ",\n";
    file << "  \"tree_win_w\": " << g_TreeGeom.w << ",\n";
    file << "  \"tree_win_h\": " << g_TreeGeom.h << ",\n";
    file << "  \"use_custom_font\": " << (g_UseCustomFont ? "true" : "false") << ",\n";
    file << "  \"custom_font_size\": " << g_CustomFontSize << ",\n";
    file << "  \"prereq_panel_height\": " << g_PrereqPanelHeight << "\n";
    file << "}\n";
}

void SaveLastUpdated() {
    std::string dir = CraftyLegend::GW2API::GetDataDirectory();
    if (dir.empty()) return;
    try { std::filesystem::create_directories(dir); } catch (...) {}
    std::string path = dir + "/last_updated.txt";
    std::ofstream file(path);
    if (file.is_open()) file << g_HoardLastUpdated << " " << g_HoardRefreshAvailableAt;
}

void LoadLastUpdated() {
    std::string dir = CraftyLegend::GW2API::GetDataDirectory();
    if (dir.empty()) return;
    std::string path = dir + "/last_updated.txt";
    std::ifstream file(path);
    if (file.is_open()) {
        file >> g_HoardLastUpdated;
        if (!(file >> g_HoardRefreshAvailableAt)) g_HoardRefreshAvailableAt = 0;
    }
}

void LoadDisplaySettings() {
    std::string dir = CraftyLegend::GW2API::GetDataDirectory();
    if (dir.empty()) return;
    std::string path = dir + "/display_settings.json";
    std::ifstream file(path);
    if (!file.is_open()) return;
    std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    // Simple parse - look for true/false values
    if (content.find("\"show_item_icons\": true") != std::string::npos) g_ShowItemIcons = true;
    else if (content.find("\"show_item_icons\": false") != std::string::npos) g_ShowItemIcons = false;
    if (content.find("\"show_debug_window\": true") != std::string::npos) g_ShowDebugWindow = true;
    else if (content.find("\"show_debug_window\": false") != std::string::npos) g_ShowDebugWindow = false;
    // Legacy key: the old boolean only distinguished "show everything" from
    // "hide anything I own one of". Read it first so an existing settings file
    // migrates, then let the new key win if it is also present.
    if (content.find("\"show_owned_legendaries\": true") != std::string::npos) g_OwnedFilterMode = OWNED_FILTER_SHOW_ALL;
    else if (content.find("\"show_owned_legendaries\": false") != std::string::npos) g_OwnedFilterMode = OWNED_FILTER_HIDE_ANY;
    {
        std::string needle = "\"owned_filter_mode\":";
        size_t p = content.find(needle);
        if (p != std::string::npos) {
            try {
                int mode = std::stoi(content.substr(p + needle.size()));
                if (mode >= OWNED_FILTER_SHOW_ALL && mode <= OWNED_FILTER_HIDE_FULL) g_OwnedFilterMode = mode;
            } catch (...) {}
        }
    }
    if (content.find("\"show_qa_icon\": true") != std::string::npos) g_ShowQAIcon = true;
    else if (content.find("\"show_qa_icon\": false") != std::string::npos) g_ShowQAIcon = false;
    if (content.find("\"use_pie_theme\": true") != std::string::npos) g_UsePieTheme = true;
    else if (content.find("\"use_pie_theme\": false") != std::string::npos) g_UsePieTheme = false;
    if (content.find("\"use_tree_layout\": true") != std::string::npos) g_UseTreeLayout = true;
    else if (content.find("\"use_tree_layout\": false") != std::string::npos) g_UseTreeLayout = false;
    if (content.find("\"first_run_notice_done\": true") != std::string::npos) g_FirstRunNoticeDone = true;
    else if (content.find("\"first_run_notice_done\": false") != std::string::npos) g_FirstRunNoticeDone = false;

    // Per-layout window geometry (floats). Leaves the field untouched if absent,
    // so an older settings file simply keeps the "not yet recorded" default.
    auto readFloat = [&](const char* key, float& out) {
        std::string needle = std::string("\"") + key + "\":";
        size_t p = content.find(needle);
        if (p == std::string::npos) return;
        p += needle.size();
        try { out = std::stof(content.substr(p)); } catch (...) {}
    };
    readFloat("miller_win_x", g_MillerGeom.x);
    readFloat("miller_win_y", g_MillerGeom.y);
    readFloat("miller_win_w", g_MillerGeom.w);
    readFloat("miller_win_h", g_MillerGeom.h);
    readFloat("tree_win_x", g_TreeGeom.x);
    readFloat("tree_win_y", g_TreeGeom.y);
    readFloat("tree_win_w", g_TreeGeom.w);
    readFloat("tree_win_h", g_TreeGeom.h);

    if (content.find("\"use_custom_font\": true") != std::string::npos) g_UseCustomFont = true;
    else if (content.find("\"use_custom_font\": false") != std::string::npos) g_UseCustomFont = false;
    readFloat("custom_font_size", g_CustomFontSize);
    if (g_CustomFontSize < 8.0f || g_CustomFontSize > 48.0f) g_CustomFontSize = 16.0f; // sanity clamp
    readFloat("prereq_panel_height", g_PrereqPanelHeight);
    if (g_PrereqPanelHeight < 60.0f || g_PrereqPanelHeight > 2000.0f) g_PrereqPanelHeight = 120.0f;
}

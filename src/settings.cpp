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
    file << "  \"show_owned_legendaries\": " << (g_ShowOwnedLegendaries ? "true" : "false") << ",\n";
    file << "  \"show_qa_icon\": " << (g_ShowQAIcon ? "true" : "false") << ",\n";
    file << "  \"use_pie_theme\": " << (g_UsePieTheme ? "true" : "false") << "\n";
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
    if (content.find("\"show_owned_legendaries\": true") != std::string::npos) g_ShowOwnedLegendaries = true;
    else if (content.find("\"show_owned_legendaries\": false") != std::string::npos) g_ShowOwnedLegendaries = false;
    if (content.find("\"show_qa_icon\": true") != std::string::npos) g_ShowQAIcon = true;
    else if (content.find("\"show_qa_icon\": false") != std::string::npos) g_ShowQAIcon = false;
    if (content.find("\"use_pie_theme\": true") != std::string::npos) g_UsePieTheme = true;
    else if (content.find("\"use_pie_theme\": false") != std::string::npos) g_UsePieTheme = false;
}

#include "DataManager.h"
#include "GW2API.h"
#include <fstream>
#include <sstream>
#include <iostream>
#include <algorithm>
#include <unordered_set>
#include <functional>
#include <windows.h>
#include <filesystem>

namespace CraftyLegend {

    // Helper: build vendor cost materials from purchase requirements, handling non-numeric costs
    static void BuildVendorCostMaterials(std::vector<RecipeIngredient>& out,
        const std::vector<std::pair<std::string, std::string>>& requirements, int qty) {
        // Map known item names to their GW2 item IDs for owned count lookups
        static const std::unordered_map<std::string, uint32_t> name_to_item_id = {
            {"Mystic Coins", 19976},
            {"Mystic Coin", 19976},
            {"Obsidian Shards", 19925},
            {"Obsidian Shard", 19925},
            {"Glob of Ectoplasm", 19721},
            {"Mystic Clover", 19675},
            {"Philosopher's Stone", 20796},
            {"Thermocatalytic Reagent", 46747},
            {"Jug of Water", 12156},
            {"Vial of Condensed Mists Essence", 38014},
            {"Icy Runestone", 19676},
            {"Bloodstone Shard", 19925},
            {"Crystalline Ore", 46682},
            // Gen3 (EoD) materials
            {"Antique Summoning Stone", 96978},
            {"Antique Summoning Stones", 96978},
            {"Jade Runestone", 96722},
            {"Jade Runestones", 96722},
            {"Chunk of Pure Jade", 97102},
            {"Chunks of Pure Jade", 97102},
            {"Chunk of Ancient Ambergris", 96347},
            {"Chunks of Ancient Ambergris", 96347},
            {"Blessing of the Jade Empress", 97829},
            {"Blessings of the Jade Empress", 97829},
            {"Memory of Aurene", 96088},
            {"Memories of Aurene", 96088},
            {"Hydrocatalytic Reagent", 95813},
            {"Hydrocatalytic Reagents", 95813},
            {"Amalgamated Draconic Lodestone", 92687},
            {"Amalgamated Draconic Lodestones", 92687},
            {"Chunk of Petrified Echovald Resin", 96471},
            {"Chunks of Petrified Echovald Resin", 96471},
            // SotO (Obsidian Armor) materials
            {"Lesser Vision Crystal", 49523},
            {"Lesser Vision Crystals", 49523},
            {"Provisioner Token", 88926},
            {"Provisioner Tokens", 88926},
            {"Amalgamated Rift Essence", 100930},
            {"Amalgamated Rift Essences", 100930},
            {"Case of Captured Lightning", 100267},
            {"Clot of Congealed Screams", 100098},
            {"Pouch of Stardust", 99964},
            {"Ball of Dark Energy", 71994},
            {"Cube of Stabilized Dark Energy", 73137},
            // VoE sub-components
            {"Concentrated Chromatic Sap", 105848},
            {"Chromatic Sap", 106385},
            {"Gift of Shipwreck Strand Exploration", 106467},
            {"Survivor's Enchanted Compass", 106370},
            {"Patron of the Magical Arts Plaque", 105933},
            {"Raw Enchanting Stone", 105686},
            {"Gift of Starlit Weald Exploration", 106672},
            {"Seer Wreath of Service", 106627},
            {"Sun Bead", 19717},
            {"Sun Beads", 19717},
            // SotO map currencies are wallet currencies (IDs 66, 72, 73, 75) - not items
            {"Exotic Essence of Luck", 45178},
            // Raid/PvP/WvW armor materials
            {"Legendary Insight", 98327},
            {"Legendary Insights", 98327},
            {"Envoy Insignia", 80516},
            {"Chak Egg", 72205},
            {"Chak Eggs", 72205},
            {"Auric Ingot", 73537},
            {"Auric Ingots", 73537},
            {"Reclaimed Metal Plate", 74356},
            {"Reclaimed Metal Plates", 74356},
            {"Mist Core Fragment", 77531},
            {"Mist Core Fragments", 77531},
            {"Record of League Victories", 82700},
            {"Legendary War Insight", 83584},
            {"Legendary War Insights", 83584},
            {"Memory of Battle", 71581},
            {"Memories of Battle", 71581},
            // LWS3 map currencies (inventory items)
            {"Blood Ruby", 79280},
            {"Blood Rubies", 79280},
            {"Fire Orchid Blossom", 81127},
            {"Fire Orchid Blossoms", 81127},
            // Grandmaster Marks
            {"Grandmaster Armorsmith's Mark", 80685},
            {"Grandmaster Leatherworker's Mark", 80799},
            {"Grandmaster Tailor's Mark", 80857},
            {"Grandmaster Mark Shard", 87557},
            {"Grandmaster Mark Shards", 87557},
        };

        for (const auto& req : requirements) {
            RecipeIngredient material;
            // Set real item ID if this is a known item, otherwise 0 (wallet/currency)
            auto id_it = name_to_item_id.find(req.first);
            if (id_it != name_to_item_id.end()) {
                material.item_id = id_it->second;
            } else {
                // Fallback: search loaded items by name
                material.item_id = 0;
                for (const auto& [id, it] : DataManager::GetItems()) {
                    if (it.name == req.first) {
                        material.item_id = id;
                        break;
                    }
                }
            }
            // Try to parse as pure integer for multiplication
            bool is_numeric = !req.second.empty() && req.second != "Unknown";
            int parsed = 0;
            if (is_numeric) {
                try {
                    size_t pos = 0;
                    parsed = std::stoi(req.second, &pos);
                    // Only treat as numeric if the entire string was consumed (or just trailing parens/spaces)
                    is_numeric = (pos == req.second.size() || req.second[pos] == ' ');
                } catch (...) {
                    is_numeric = false;
                }
            }
            if (is_numeric && parsed > 0) {
                material.count = parsed * qty;
                material.name = req.first;
            } else {
                material.count = 0;
                if (qty > 1) {
                    material.name = req.first + ": " + std::to_string(qty) + " x " + req.second;
                } else {
                    material.name = req.first + ": " + req.second;
                }
            }
            out.push_back(material);
        }
    }

    // Helper function to get the directory where the DLL is located
    std::string GetDllDirectory() {
        char dllPath[MAX_PATH];
        HMODULE hModule = NULL;
        
        // Get the handle to the current module (the DLL)
        if (GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, 
                               (LPCSTR)GetDllDirectory, &hModule)) {
            // Get the full path of the DLL
            if (GetModuleFileNameA(hModule, dllPath, MAX_PATH)) {
                // Extract directory from full path
                std::string path(dllPath);
                size_t lastSlash = path.find_last_of("\\/");
                if (lastSlash != std::string::npos) {
                    return path.substr(0, lastSlash);
                }
            }
        }
        
        return ""; // Failed to get directory
    }
    
    // Static data members
    std::vector<Legendary> DataManager::s_legendaries;
    std::unordered_map<uint32_t, Item> DataManager::s_items;
    std::unordered_map<uint32_t, Recipe> DataManager::s_recipes;
    std::vector<Currency> DataManager::s_currencies;
    std::unordered_map<uint32_t, std::vector<AcquisitionMethod>> DataManager::s_acquisition_methods;
    // JSON data storage
    json DataManager::s_legendaries_json;
    json DataManager::s_items_json;
    json DataManager::s_recipes_json;
    
    // Debug information
    std::string DataManager::s_debug_legendaries_path = "FAILED";
    std::string DataManager::s_debug_items_path = "FAILED";
    std::string DataManager::s_debug_recipes_path = "FAILED";
    
    // Column data
    std::vector<ColumnData> DataManager::s_columns;
    
    // Session scroll state
    float DataManager::s_session_scroll_x = 0.0f;
    float DataManager::s_session_col0_scroll_y = 0.0f;
    std::vector<float> DataManager::s_session_col_scroll_y;
    
    bool DataManager::Initialize() {
        // Load all JSON data
        bool legendaries_loaded = LoadLegendaries();
        bool items_loaded = LoadItems();
        bool recipes_loaded = LoadRecipes();
        bool currencies_loaded = LoadCurrencies();
        
        // Always populate acquisition methods
        int populated_count = 0;
        for (const auto& item_pair : s_items) {
            const Item& item = item_pair.second;
            std::vector<AcquisitionMethod> methods;
            
            // Start with base acquisition methods and filter/prioritize them
            std::vector<std::string> base_methods;
            std::vector<std::string> filtered_methods;
            
            // Filter out all methods except the three we want to show: crafting, mystic_forge, vendor
            // (trading_post is kept in code but not shown in UI)
            for (const std::string& method : item.acquisition) {
                // Only allow the three specified methods for UI display
                if (method == "crafting" || method == "mystic_forge" || 
                    method == "vendor") {
                    filtered_methods.push_back(method);
                }
                // All other methods are excluded (including salvaging, wvw, pvp, raid, etc.)
            }
            
            base_methods = filtered_methods;
            
            // Add trading post for non-bound items (DISABLED - not showing in UI)
            // if (item.binding == "none") {
            //     bool has_trading_post = false;
            //     for (const std::string& method : base_methods) {
            //         if (method == "trading_post") {
            //             has_trading_post = true;
            //             break;
            //         }
            //     }
            //     if (!has_trading_post) {
            //         base_methods.push_back("trading_post");
            //     }
            // }
            
            for (const std::string& method : base_methods) {
                methods.push_back(CreateAcquisitionMethod(method, &item));
            }
            
            // Items sold by multiple vendors: replace single vendor with specific options
            if (item.id == 80058) { // Mist Band (Infused)
                methods.erase(std::remove_if(methods.begin(), methods.end(),
                    [](const AcquisitionMethod& m) { return m.method == "vendor"; }), methods.end());
                AcquisitionMethod fractal;
                fractal.method = "vendor";
                fractal.display_name = "Vendor - Fractal";
                fractal.description = "Available from vendors";
                fractal.vendor_name = "BUY-2046 PFR";
                fractal.vendor_location = "Mistlock Observatory";
                fractal.purchase_requirements = {
                    {"Pristine Fractal Relic", "100"},
                    {"Integrated Fractal Matrix", "2"}
                };
                methods.push_back(fractal);
                AcquisitionMethod pvp;
                pvp.method = "vendor";
                pvp.display_name = "Vendor - PvP League";
                pvp.description = "Available from vendors";
                pvp.vendor_name = "Ascended Armor League Vendor";
                pvp.vendor_location = "Heart of the Mists";
                pvp.purchase_requirements = {
                    {"Ascended Shard of Glory", "150"},
                    {"Shard of Glory", "170"}
                };
                methods.push_back(pvp);
                AcquisitionMethod wvw;
                wvw.method = "vendor";
                wvw.display_name = "Vendor - WvW";
                wvw.description = "Available from vendors";
                wvw.vendor_name = "Skirmish Supervisor";
                wvw.vendor_location = "WvW";
                wvw.purchase_requirements = {
                    {"WvW Skirmish Claim Ticket", "250"},
                    {"Memory of Battle", "350"}
                };
                methods.push_back(wvw);
            }
            
            s_acquisition_methods[item.id] = methods;
            populated_count++;
        }
        
        // Initialize columns
        ResetColumns();
        
        return true;
    }
    
    void DataManager::Shutdown() {
        s_legendaries.clear();
        s_items.clear();
        s_recipes.clear();
        s_acquisition_methods.clear();
        s_columns.clear();
    }
    
    bool DataManager::LoadLegendaries() {
        std::string dllDir = GetDllDirectory();
        if (!dllDir.empty()) {
            std::replace(dllDir.begin(), dllDir.end(), '\\', '/');
        }
        
        std::vector<std::string> paths_to_try;
        if (!dllDir.empty()) {
            paths_to_try = {
                dllDir + "/CraftyLegend/legendaries.json",
                dllDir + "/legendaries.json",
                dllDir + "/../CraftyLegend/legendaries.json",
                dllDir + "/../../CraftyLegend/legendaries.json"
            };
        }
        // Also try relative paths
        paths_to_try.push_back("CraftyLegend/legendaries.json");
        paths_to_try.push_back("./CraftyLegend/legendaries.json");
        paths_to_try.push_back("legendaries.json");
        
        std::ifstream file;
        for (const auto& path : paths_to_try) {
            file.open(path);
            if (file.is_open()) {
                s_debug_legendaries_path = path;
                break;
            }
        }
        
        if (!file.is_open()) {
            s_debug_legendaries_path = "ALL_PATHS_FAILED (DLL dir: " + dllDir + ")";
            return false;
        }
        
        try {
            // Parse JSON file
            file >> s_legendaries_json;
            
            // Convert JSON data to internal structures
            for (const auto& legendary_json : s_legendaries_json["legendaries"]) {
                Legendary legendary;
                legendary.id = legendary_json["id"];
                legendary.name = legendary_json["name"];
                legendary.icon = legendary_json["icon"];
                legendary.description = legendary_json["description"];
                legendary.type = legendary_json["type"];
                
                if (legendary_json.contains("weapon_type")) {
                    legendary.weapon_type = legendary_json["weapon_type"];
                }
                if (legendary_json.contains("armor_type")) {
                    legendary.armor_type = legendary_json["armor_type"];
                }
                if (legendary_json.contains("trinket_type")) {
                    legendary.trinket_type = legendary_json["trinket_type"];
                }
                if (legendary_json.contains("back_type")) {
                    legendary.back_type = legendary_json["back_type"];
                }
                
                legendary.binding = legendary_json.value("binding", "none");
                legendary.generation = legendary_json.value("generation", 1);
                
                if (legendary_json.contains("acquisition")) {
                    for (const auto& acq : legendary_json["acquisition"]) {
                        legendary.acquisition.push_back(acq);
                    }
                }
                
                s_legendaries.push_back(legendary);
                
                // Also add this legendary as an item so it gets acquisition methods
                Item item;
                item.id = legendary.id;
                item.name = legendary.name;
                item.icon = legendary.icon;
                item.description = legendary.description;
                item.type = legendary.type;
                item.rarity = "Legendary";
                item.binding = legendary.binding;
                item.acquisition = legendary.acquisition;
                
                s_items[item.id] = item;
            }
            
            return true;
        } catch (const std::exception& e) {
            // If JSON parsing fails, return false
            s_debug_legendaries_path += " (PARSE_ERROR)";
            return false;
        }
    }
    
    bool DataManager::LoadItems() {
        // Get the DLL directory
        std::string dllDir = GetDllDirectory();
        
        // Convert backslashes to forward slashes for consistency
        if (!dllDir.empty()) {
            std::replace(dllDir.begin(), dllDir.end(), '\\', '/');
        }
        
        // Construct the full path to the JSON file
        std::string json_path = dllDir.empty() ? "CraftyLegend/items.json" : dllDir + "/CraftyLegend/items.json";
        
        std::ifstream file(json_path);
        if (!file.is_open()) {
            // Try alternative paths based on DLL directory
            std::vector<std::string> paths_to_try;
            
            if (!dllDir.empty()) {
                paths_to_try = {
                    dllDir + "/CraftyLegend/items.json",
                    dllDir + "/items.json",
                    dllDir + "/../CraftyLegend/items.json",
                    dllDir + "/../../CraftyLegend/items.json"
                };
            } else {
                // Fallback to relative paths if we can't get DLL directory
                paths_to_try = {
                    "CraftyLegend/items.json",
                    "./CraftyLegend/items.json",
                    "../CraftyLegend/items.json",
                    "../../CraftyLegend/items.json",
                    "items.json",
                    "./items.json",
                    "../items.json",
                    "../../items.json"
                };
            }
            
            for (const auto& path : paths_to_try) {
                file.open(path);
                if (file.is_open()) {
                    s_debug_items_path = path;
                    break;
                }
            }
            
            if (!file.is_open()) {
                s_debug_items_path = "ALL_PATHS_FAILED (DLL dir: " + dllDir + ")";
                return false;
            }
        } else {
            s_debug_items_path = json_path;
        }
        
        try {
            // Parse JSON file
            file >> s_items_json;
            
            // Convert JSON data to internal structures
            for (const auto& item_json : s_items_json["items"]) {
                Item item;
                item.id = static_cast<uint32_t>(std::stoul(item_json["id"].get<std::string>()));
                item.name = item_json["name"];
                item.icon = item_json["icon"];
                item.description = item_json.value("description", "");
                item.type = item_json["type"];
                item.rarity = item_json.value("rarity", "");
                item.binding = item_json["binding"];
                
                // Parse acquisition array
                if (item_json.contains("acquisition")) {
                    for (const auto& acq : item_json["acquisition"]) {
                        item.acquisition.push_back(acq);
                    }
                }
                
                s_items[item.id] = item;
            }
            
            return true;
        } catch (const std::exception& e) {
            // If JSON parsing fails, return false
            s_debug_items_path += " (PARSE_ERROR)";
            return false;
        }
    }
    
    bool DataManager::LoadRecipes() {
        // Get the DLL directory
        std::string dllDir = GetDllDirectory();
        
        // Convert backslashes to forward slashes for consistency
        if (!dllDir.empty()) {
            std::replace(dllDir.begin(), dllDir.end(), '\\', '/');
        }
        
        // Construct the full path to the JSON file
        std::string json_path = dllDir.empty() ? "CraftyLegend/recipes.json" : dllDir + "/CraftyLegend/recipes.json";
        
        std::ifstream file(json_path);
        if (!file.is_open()) {
            // Try alternative paths based on DLL directory
            std::vector<std::string> paths_to_try;
            
            if (!dllDir.empty()) {
                paths_to_try = {
                    dllDir + "/CraftyLegend/recipes.json",
                    dllDir + "/recipes.json",
                    dllDir + "/../CraftyLegend/recipes.json",
                    dllDir + "/../../CraftyLegend/recipes.json"
                };
            } else {
                // Fallback to relative paths if we can't get DLL directory
                paths_to_try = {
                    "CraftyLegend/recipes.json",
                    "./CraftyLegend/recipes.json",
                    "../CraftyLegend/recipes.json",
                    "../../CraftyLegend/recipes.json",
                    "recipes.json",
                    "./recipes.json",
                    "../recipes.json",
                    "../../recipes.json"
                };
            }
            
            for (const auto& path : paths_to_try) {
                file.open(path);
                if (file.is_open()) {
                    s_debug_recipes_path = path;
                    break;
                }
            }
            
            if (!file.is_open()) {
                s_debug_recipes_path = "ALL_PATHS_FAILED (DLL dir: " + dllDir + ")";
                return false;
            }
        } else {
            s_debug_recipes_path = json_path;
        }
        
        try {
            // Parse JSON file
            file >> s_recipes_json;
            
            // Convert JSON data to internal structures
            int recipe_count = 0;
            for (const auto& recipe_json : s_recipes_json["recipes"]) {
                Recipe recipe;
                recipe.id = static_cast<uint32_t>(std::stoul(recipe_json["output_id"].get<std::string>()));
                recipe.output_item_id = recipe.id;
                recipe.output_count = recipe_json["output_count"];
                recipe.type = recipe_json["type"];
                recipe.rating = recipe_json["rating"];
                
                // Parse disciplines
                for (const auto& disc : recipe_json["disciplines"]) {
                    recipe.disciplines.push_back(disc);
                }
                
                // Parse ingredients
                for (const auto& ing : recipe_json["ingredients"]) {
                    RecipeIngredient ingredient;
                    ingredient.item_id = static_cast<uint32_t>(std::stoul(ing["item_id"].get<std::string>()));
                    ingredient.count = ing["count"];
                    ingredient.name = ing["name"];
                    recipe.ingredients.push_back(ingredient);
                }
                
                s_recipes[recipe.output_item_id] = recipe;
                recipe_count++;
            }
            
            // Debug: Show recipe count
            return true;
        } catch (const std::exception& e) {
            // If JSON parsing fails, return false
            s_debug_recipes_path += " (PARSE_ERROR)";
            return false;
        }
    }
    
    // Helper function for smart sorting (ignores "The" prefix)
    std::string GetSortKey(const std::string& name) {
        if (name.length() > 4 && name.substr(0, 4) == "The ") {
            return name.substr(4); // Skip "The " and return the rest
        }
        return name;
    }
    
    bool DataManager::LoadCurrencies() {
        // Get the DLL directory
        std::string dllDir = GetDllDirectory();
        if (!dllDir.empty()) {
            std::replace(dllDir.begin(), dllDir.end(), '\\', '/');
        }
        
        // Try multiple paths (same pattern as LoadLegendaries/LoadItems)
        std::vector<std::string> paths_to_try;
        if (!dllDir.empty()) {
            paths_to_try = {
                dllDir + "/CraftyLegend/currencies.json",
                dllDir + "/currencies.json",
                dllDir + "/../CraftyLegend/currencies.json",
                dllDir + "/../../CraftyLegend/currencies.json"
            };
        }
        paths_to_try.push_back("CraftyLegend/currencies.json");
        paths_to_try.push_back("./CraftyLegend/currencies.json");
        paths_to_try.push_back("currencies.json");
        
        // Clear existing data
        s_currencies.clear();
        
        std::string currenciesPath;
        std::ifstream currenciesFile;
        for (const auto& path : paths_to_try) {
            currenciesFile.open(path);
            if (currenciesFile.is_open()) {
                currenciesPath = path;
                break;
            }
        }
        
        try {
            if (!currenciesFile.is_open()) {
                return false;
            }
            
            json currenciesJson;
            currenciesFile >> currenciesJson;
            currenciesFile.close();
            
            // Parse currencies
            if (!currenciesJson.contains("currencies") || !currenciesJson["currencies"].is_array()) {
                return false;
            }
            
            for (const auto& currencyJson : currenciesJson["currencies"]) {
                Currency currency;
                currency.id = currencyJson.value("id", 0);
                currency.name = currencyJson.value("name", "");
                currency.icon = currencyJson.value("icon", "");
                currency.description = currencyJson.value("description", "");
                
                s_currencies.push_back(currency);
            }
            
            return !s_currencies.empty();
            
        } catch (const std::exception& e) {
            return false;
        }
    }

    AcquisitionMethod DataManager::CreateAcquisitionMethod(const std::string& method, const Item* item) {
        AcquisitionMethod acq;
        acq.method = method;
        
        if (method == "crafting") {
            acq.display_name = "Crafting";
            acq.description = "Created through crafting";
            acq.details = {"Requires crafting discipline", "Uses various materials", "Need crafting station"};
        } else if (method == "vendor") {
            acq.description = "Available from vendors";
            acq.details = {"Visit specific vendors", "Pay with currency", "Fixed prices available"};
            
            // Add vendor-specific information
            if (item && item->id == 19675) { // Mystic Clover
                acq.display_name = "Vendor - Lyhr";
                acq.vendor_name = "Lyhr";
                acq.vendor_location = "Wizard's Tower";
                acq.purchase_requirements = {
                    {"Mystic Coins", "3"},
                    {"Obsidian Shards", "3"},
                    {"Spirit Shards", "3"},
                    {"Glob of Ectoplasm", "5"}
                };
            } else if (item && item->id == 19676) { // Icy Runestone
                acq.display_name = "Vendor - Rojan the Penitent";
                acq.vendor_name = "Rojan the Penitent";
                acq.vendor_location = "Frostgorge Sound";
                acq.purchase_requirements = {
                    {"Coin", "10000"}
                };
            } else if (item && item->id == 5 || (item && item->id == 19925)) { // Obsidian Shard
                acq.display_name = "Vendor - Multiple";
                acq.vendor_name = "Multiple Vendors";
                acq.vendor_location = "Various Locations";
                acq.purchase_requirements = {
                    {"Fractal Relics", "25 (Fractal Reliquary)"},
                    {"Karma", "2100 (Orr Karma Vendors)"},
                    {"Bandit Crests", "50 (Silverwastes)"}
                };
            } else if (item && item->id == 6) { // Mystic Coin
                acq.display_name = "Trading Post";
                acq.vendor_name = "Trading Post";
                acq.vendor_location = "Major Cities";
                acq.purchase_requirements = {
                    {"Coin", "0"}
                };
            } else if (item && item->id == 4) { // Spirit Shard
                acq.display_name = "Vendor - Mystic Forge Attendant";
                acq.vendor_name = "Mystic Forge Attendant";
                acq.vendor_location = "Lion's Arch, Mystic Forge";
                acq.purchase_requirements = {
                    {"Mystic Coins", "3"}
                };
            } else if (item && item->id == 3) { // Laurel
                acq.display_name = "Laurel Vendor";
                acq.vendor_name = "Laurel Vendor";
                acq.vendor_location = "Major Cities";
                acq.purchase_requirements = {
                    {"Achievement Points", "Earned"},
                    {"Daily/Monthly Achievements", "Complete"}
                };
            } else if (item && item->id == 32) { // Bloodstone Shard (old ID)
                acq.display_name = "Vendor - Miyani";
                acq.vendor_name = "Miyani";
                acq.vendor_location = "Trader's Forum, Lion's Arch";
                acq.purchase_requirements = {
                    {"Spirit Shards", "200"}
                };
            } else if (item && item->id == 20797) { // Bloodstone Shard (Gen2)
                acq.display_name = "Vendor - Miyani";
                acq.vendor_name = "Miyani";
                acq.vendor_location = "Trader's Forum, Lion's Arch";
                acq.purchase_requirements = {
                    {"Spirit Shards", "200"}
                };
            } else if (item && item->id == 79418) { // Mystic Runestone
                acq.display_name = "Vendor - Miyani";
                acq.vendor_name = "Miyani";
                acq.vendor_location = "Trader's Forum, Lion's Arch";
                acq.purchase_requirements = {
                    {"Coin", "10000"}
                };
            } else if (item && item->id == 86093) { // Funerary Incense
                acq.display_name = "Vendor - Palawa Joko's Valet";
                acq.vendor_name = "Palawa Joko's Valet";
                acq.vendor_location = "Domain of Vabbi";
                acq.purchase_requirements = {
                    {"Trade Contracts", "25"},
                    {"Elegy Mosaic", "1"}
                };
            // --- HoT Mastery Gifts ---
            } else if (item && item->id == 73469) { // Gift of the Itzel
                acq.display_name = "Vendor - Itzel Mastery Vendor";
                acq.vendor_name = "Itzel Mastery Vendor";
                acq.vendor_location = "Verdant Brink";
                acq.purchase_requirements = {
                    {"Airship Parts", "500"},
                    {"Coin", "10000"}
                };
            } else if (item && item->id == 76767) { // Gift of the Nuhoch
                acq.display_name = "Vendor - Nuhoch Mastery Vendor";
                acq.vendor_name = "Nuhoch Mastery Vendor";
                acq.vendor_location = "Tangled Depths";
                acq.purchase_requirements = {
                    {"Ley Line Crystals", "500"},
                    {"Coin", "10000"}
                };
            } else if (item && item->id == 76636) { // Gift of the Exalted
                acq.display_name = "Vendor - Exalted Mastery Vendor";
                acq.vendor_name = "Exalted Mastery Vendor";
                acq.vendor_location = "Auric Basin";
                acq.purchase_requirements = {
                    {"Lumps of Aurillium", "500"},
                    {"Coin", "10000"}
                };
            } else if (item && item->id == 71311) { // Gift of Gliding
                acq.display_name = "Vendor - Miyani";
                acq.vendor_name = "Miyani";
                acq.vendor_location = "Trader's Forum, Lion's Arch";
                acq.purchase_requirements = {
                    {"Airship Parts", "300"},
                    {"Lumps of Aurillium", "300"},
                    {"Ley Line Crystals", "300"}
                };
            // --- Miyani Vendor Items ---
            } else if (item && item->id == 70528) { // Gift of Glory
                acq.display_name = "Vendor - Miyani";
                acq.vendor_name = "Miyani";
                acq.vendor_location = "Trader's Forum, Lion's Arch";
                acq.purchase_requirements = {
                    {"Shard of Glory", "250"}
                };
            } else if (item && item->id == 71008) { // Gift of War
                acq.display_name = "Vendor - Miyani";
                acq.vendor_name = "Miyani";
                acq.vendor_location = "Trader's Forum, Lion's Arch";
                acq.purchase_requirements = {
                    {"Memory of Battle", "250"}
                };
            // --- PoF Map Gifts ---
            } else if (item && item->id == 86010) { // Gift of the Desolation
                acq.display_name = "Vendor - The Desolation";
                acq.vendor_name = "Heart Vendor";
                acq.vendor_location = "The Desolation";
                acq.purchase_requirements = {
                    {"Trade Contracts", "250"}
                };
            } else if (item && item->id == 86018) { // Gift of the Highlands
                acq.display_name = "Vendor - Desert Highlands";
                acq.vendor_name = "Heart Vendor";
                acq.vendor_location = "Desert Highlands";
                acq.purchase_requirements = {
                    {"Trade Contracts", "250"}
                };
            } else if (item && item->id == 85961) { // Gift of the Oasis
                acq.display_name = "Vendor - Crystal Oasis";
                acq.vendor_name = "Heart Vendor";
                acq.vendor_location = "Crystal Oasis";
                acq.purchase_requirements = {
                    {"Trade Contracts", "250"}
                };
            } else if (item && item->id == 86241) { // Gift of the Riverlands
                acq.display_name = "Vendor - Elon Riverlands";
                acq.vendor_name = "Heart Vendor";
                acq.vendor_location = "Elon Riverlands";
                acq.purchase_requirements = {
                    {"Trade Contracts", "250"}
                };
            } else if (item && item->id == 86330) { // Gift of the Rider
                acq.display_name = "Vendor - PoF Mastery";
                acq.vendor_name = "Mastery Vendor";
                acq.vendor_location = "Path of Fire Maps";
                acq.purchase_requirements = {
                    {"Trade Contracts", "Requires Raptor/Springer/Skimmer/Jackal mastery"}
                };
            // --- Gift of Condensed Might/Magic (Lyhr + Mystic Forge) ---
            } else if (item && item->id == 70867) { // Gift of Condensed Might
                acq.display_name = "Vendor - Lyhr";
                acq.vendor_name = "Lyhr";
                acq.vendor_location = "Wizard's Tower";
                acq.purchase_requirements = {
                    {"Gift of Fangs", "1"},
                    {"Gift of Scales", "1"},
                    {"Gift of Claws", "1"},
                    {"Gift of Bones", "1"},
                    {"Glob of Ectoplasm", "10"}
                };
            } else if (item && item->id == 76530) { // Gift of Condensed Magic
                acq.display_name = "Vendor - Lyhr";
                acq.vendor_name = "Lyhr";
                acq.vendor_location = "Wizard's Tower";
                acq.purchase_requirements = {
                    {"Gift of Blood", "1"},
                    {"Gift of Venom", "1"},
                    {"Gift of Totems", "1"},
                    {"Gift of Dust", "1"},
                    {"Glob of Ectoplasm", "10"}
                };
            // --- Augur's Stone ---
            } else if (item && item->id == 46752) { // Augur's Stone
                acq.display_name = "Vendor - Miyani";
                acq.vendor_name = "Miyani";
                acq.vendor_location = "Trader's Forum, Lion's Arch";
                acq.purchase_requirements = {
                    {"Spirit Shards", "20"},
                    {"Obsidian Shards", "6"},
                    {"Coin", "100000"}
                };
            // --- EoD Poems (Arborstone vendor) ---
            } else if (item && (item->id == 97160 || item->id == 96187 || item->id == 96035 ||
                                item->id == 95809 || item->id == 96173 || item->id == 97335 ||
                                item->id == 96951 || item->id == 95740 || item->id == 97257 ||
                                item->id == 96341 || item->id == 96036 || item->id == 97082 ||
                                item->id == 97800 || item->id == 97201 || item->id == 96849 ||
                                item->id == 95962)) { // Poem on [Weapon]
                acq.display_name = "Vendor - Leivas";
                acq.vendor_name = "Leivas";
                acq.vendor_location = "Arborstone, Echovald Wilds";
                acq.purchase_requirements = {
                    {"Imperial Favor", "1000"},
                    {"Research Note", "500"}
                };
            // --- EoD (Gen3) Vendors ---
            } else if (item && item->id == 97433) { // Gift of the Dragon Empire
                acq.display_name = "Vendor - Leivas";
                acq.vendor_name = "Leivas";
                acq.vendor_location = "Arborstone, Echovald Wilds";
                acq.purchase_requirements = {
                    {"Jade Runestones", "100"},
                    {"Chunks of Pure Jade", "200"},
                    {"Chunks of Ancient Ambergris", "100"},
                    {"Blessings of the Jade Empress", "5"},
                    {"Imperial Favor", "2500"}
                };
            } else if (item && item->id == 96993) { // Gift of Seitung Province
                acq.display_name = "Map Completion - Seitung Province";
                acq.vendor_name = "Map Reward";
                acq.vendor_location = "Seitung Province";
                acq.purchase_requirements = {
                    {"Seitung Province", "Map Completion"}
                };
            } else if (item && item->id == 95621) { // Gift of New Kaineng City
                acq.display_name = "Map Completion - New Kaineng City";
                acq.vendor_name = "Map Reward";
                acq.vendor_location = "New Kaineng City";
                acq.purchase_requirements = {
                    {"New Kaineng City", "Map Completion"}
                };
            } else if (item && item->id == 97232) { // Gift of the Echovald Forest
                acq.display_name = "Map Completion - The Echovald Wilds";
                acq.vendor_name = "Map Reward";
                acq.vendor_location = "The Echovald Wilds";
                acq.purchase_requirements = {
                    {"The Echovald Wilds", "Map Completion"}
                };
            } else if (item && item->id == 96083) { // Gift of Dragon's End
                acq.display_name = "Map Completion - Dragon's End";
                acq.vendor_name = "Map Reward";
                acq.vendor_location = "Dragon's End";
                acq.purchase_requirements = {
                    {"Dragon's End", "Map Completion"}
                };
            } else if (item && item->id == 96978) { // Antique Summoning Stone
                acq.display_name = "Vendor - Leivas";
                acq.vendor_name = "Leivas";
                acq.vendor_location = "Arborstone, Echovald Wilds";
                acq.purchase_requirements = {
                    {"Magnetite Shards", "10 (max 5/week)"}
                };
            // --- SotO (Obsidian Armor) Vendors ---
            } else if (item && (item->id == 100706 || item->id == 100524 || item->id == 100509 ||
                                item->id == 100050 || item->id == 100198 || item->id == 100946)) { // Arcanums
                acq.display_name = "Vendor - Lyhr";
                acq.vendor_name = "Lyhr";
                acq.vendor_location = "Wizard's Tower";
                acq.purchase_requirements = {
                    {"Lesser Vision Crystal", "1"}
                };
            } else if (item && item->id == 77451) { // Gift of Craftsmanship
                acq.display_name = "Vendor - Faction Provisioner";
                acq.vendor_name = "Faction Provisioner";
                acq.vendor_location = "Various Cities";
                acq.purchase_requirements = {
                    {"Provisioner Token", "50"}
                };
            } else if (item && item->id == 100852) { // Gift of Expertise
                acq.display_name = "Vendor - Lyhr";
                acq.vendor_name = "Lyhr";
                acq.vendor_location = "Wizard's Tower";
                acq.purchase_requirements = {
                    {"Amalgamated Rift Essence", "12"},
                    {"Eldritch Scroll", "1"},
                    {"Obsidian Shard", "50"},
                    {"Cube of Stabilized Dark Energy", "1"},
                    {"Glob of Ectoplasm", "10"}
                };
            } else if (item && item->id == 99962) { // Gift of Persistence
                acq.display_name = "Vendor - Lyhr";
                acq.vendor_name = "Lyhr";
                acq.vendor_location = "Wizard's Tower";
                acq.purchase_requirements = {
                    {"Ancient Coin", "250"},
                    {"Static Charge", "250"},
                    {"Pinch of Stardust", "250"},
                    {"Calcified Gasp", "250"}
                };
            } else if (item && item->id == 100466) { // Gift of the Astral Ward
                acq.display_name = "Vendor - Lyhr";
                acq.vendor_name = "Lyhr";
                acq.vendor_location = "Wizard's Tower";
                acq.purchase_requirements = {
                    {"Gift of Skywatch Archipelago", "1"},
                    {"Gift of Amnytas", "1"},
                    {"Gift of Inner Nayos", "1"},
                    {"Gift of Persistence", "1"},
                    {"Glob of Ectoplasm", "10"}
                };
            } else if (item && item->id == 100288) { // Gift of Stormy Skies
                acq.display_name = "Vendor - Lyhr";
                acq.vendor_name = "Lyhr";
                acq.vendor_location = "Wizard's Tower";
                acq.purchase_requirements = {
                    {"Gift of the Astral Ward", "1"},
                    {"Case of Captured Lightning", "5"},
                    {"Pouch of Stardust", "5"},
                    {"Clot of Congealed Screams", "5"},
                    {"Glob of Ectoplasm", "10"}
                };
            } else if (item && item->id == 100933) { // Gift of Mighty Prosperity
                acq.display_name = "Vendor - Lyhr";
                acq.vendor_name = "Lyhr";
                acq.vendor_location = "Wizard's Tower";
                acq.purchase_requirements = {
                    {"Mystic Clover", "9"},
                    {"Gift of Condensed Might", "1"},
                    {"Gift of Research", "1"},
                    {"Gift of Craftsmanship", "1"},
                    {"Glob of Ectoplasm", "10"}
                };
            } else if (item && item->id == 100512) { // Gift of Magical Prosperity
                acq.display_name = "Vendor - Lyhr";
                acq.vendor_name = "Lyhr";
                acq.vendor_location = "Wizard's Tower";
                acq.purchase_requirements = {
                    {"Mystic Clover", "9"},
                    {"Gift of Condensed Magic", "1"},
                    {"Gift of Research", "1"},
                    {"Gift of Craftsmanship", "1"},
                    {"Glob of Ectoplasm", "10"}
                };
            } else if (item && item->id == 97655) { // Gift of Research
                acq.display_name = "Vendor - Lyhr";
                acq.vendor_name = "Lyhr";
                acq.vendor_location = "Wizard's Tower";
                acq.purchase_requirements = {
                    {"Hydrocatalytic Reagent", "500"},
                    {"Thermocatalytic Reagent", "250"},
                    {"Exotic Essence of Luck", "250"},
                    {"Glob of Ectoplasm", "10"}
                };
            } else if (item && item->id == 100930) { // Amalgamated Rift Essence
                acq.display_name = "Vendor - Lyhr";
                acq.vendor_name = "Lyhr";
                acq.vendor_location = "Wizard's Tower";
                acq.purchase_requirements = {
                    {"Fine Rift Essence", "250"},
                    {"Masterwork Rift Essence", "100"},
                    {"Rare Rift Essence", "50"},
                    {"Glob of Ectoplasm", "60"}
                };
            } else if (item && item->id == 100544) { // Gift of Skywatch Archipelago
                acq.display_name = "Map Completion - Skywatch Archipelago";
                acq.vendor_name = "Map Reward";
                acq.vendor_location = "Skywatch Archipelago";
                acq.purchase_requirements = {
                    {"Skywatch Archipelago", "Map Completion"}
                };
            } else if (item && item->id == 100798) { // Gift of Amnytas
                acq.display_name = "Map Completion - Amnytas";
                acq.vendor_name = "Map Reward";
                acq.vendor_location = "Amnytas";
                acq.purchase_requirements = {
                    {"Amnytas", "Map Completion"}
                };
            } else if (item && item->id == 101595) { // Gift of Inner Nayos
                acq.display_name = "Map Completion - Inner Nayos";
                acq.vendor_name = "Map Reward";
                acq.vendor_location = "Inner Nayos";
                acq.purchase_requirements = {
                    {"Inner Nayos", "Map Completion"}
                };
            } else if (item && item->id == 100267) { // Case of Captured Lightning
                acq.display_name = "Vendor - Skywatch Archipelago";
                acq.vendor_name = "Renown Heart Vendors";
                acq.vendor_location = "Skywatch Archipelago";
                acq.purchase_requirements = {
                    {"Karma", "14000"}
                };
            } else if (item && item->id == 100098) { // Clot of Congealed Screams
                acq.display_name = "Vendor - Inner Nayos";
                acq.vendor_name = "Renown Heart Vendors";
                acq.vendor_location = "Inner Nayos";
                acq.purchase_requirements = {
                    {"Karma", "14000"}
                };
            } else if (item && item->id == 99964) { // Pouch of Stardust
                acq.display_name = "Vendor - Amnytas";
                acq.vendor_name = "Renown Heart Vendors";
                acq.vendor_location = "Amnytas";
                acq.purchase_requirements = {
                    {"Karma", "14000"}
                };
            } else if (item && item->id == 88926) { // Provisioner Token
                acq.display_name = "Vendor - Faction Provisioner";
                acq.vendor_name = "Faction Provisioner";
                acq.vendor_location = "Various Cities (daily)";
                acq.purchase_requirements = {
                    {"Rare Equipment", "1 per token (daily)"}
                };
            // --- Raid Armor Vendors ---
            } else if (item && item->id == 80516) { // Envoy Insignia
                acq.display_name = "Vendor - Scholar Glenna";
                acq.vendor_name = "Scholar Glenna";
                acq.vendor_location = "Raid Lobby";
                acq.purchase_requirements = {
                    {"Legendary Insight", "25"}
                };
            } else if (item && item->id == 98327) { // Legendary Insight
                acq.display_name = "Raid Drop";
                acq.vendor_name = "Raid Boss Reward";
                acq.vendor_location = "Raids";
                acq.purchase_requirements = {
                    {"Raid Boss Kill", "1 per boss per week"}
                };
            } else if (item && item->id == 78793) { // Gift of the Pact
                acq.display_name = "Vendor - Scholar Glenna";
                acq.vendor_name = "Scholar Glenna";
                acq.vendor_location = "Raid Lobby";
                acq.purchase_requirements = {
                    {"Magnetite Shard", "500"}
                };
            // --- WvW Ascended Precursors (all weights) ---
            // Heavy: Warhelm(81330) Pauldrons(81333) Breastplate(81304) Gauntlets(81349) Legplates(81418) Wargreaves(81336)
            // Medium: Faceguard(81315) Shoulderguards(81446) Brigandine(81434) Wristplates(81489) Legguards(81386) Shinplates(81420)
            // Light: Masque(81476) Epaulets(81285) Raiment(81338) Armguards(81428) Leggings(81279) Footgear(81500)
            } else if (item && (item->id == 81330 || item->id == 81333 || item->id == 81304 ||
                                item->id == 81349 || item->id == 81418 || item->id == 81336 ||
                                item->id == 81315 || item->id == 81446 || item->id == 81434 ||
                                item->id == 81489 || item->id == 81386 || item->id == 81420 ||
                                item->id == 81476 || item->id == 81285 || item->id == 81338 ||
                                item->id == 81428 || item->id == 81279 || item->id == 81500)) {
                acq.display_name = "Vendor - Skirmish Supervisor";
                acq.vendor_name = "Skirmish Supervisor";
                acq.vendor_location = "WvW";
                // Ticket cost varies by slot: chest=350, legs=260, others=175
                bool isChest = (item->id == 81304 || item->id == 81434 || item->id == 81338);
                bool isLegs  = (item->id == 81418 || item->id == 81386 || item->id == 81279);
                std::string tickets = isChest ? "350" : (isLegs ? "260" : "175");
                // Mark count: chest/legs=4, others=3
                std::string marks = (isChest || isLegs) ? "4" : "3";
                // Mark type varies by weight class
                bool isHeavy  = (item->id == 81330 || item->id == 81333 || item->id == 81304 ||
                                  item->id == 81349 || item->id == 81418 || item->id == 81336);
                bool isMedium = (item->id == 81315 || item->id == 81446 || item->id == 81434 ||
                                  item->id == 81489 || item->id == 81386 || item->id == 81420);
                std::string markName = isHeavy ? "Grandmaster Armorsmith's Mark" :
                                       (isMedium ? "Grandmaster Leatherworker's Mark" :
                                                   "Grandmaster Tailor's Mark");
                acq.purchase_requirements = {
                    {"Coin", "20000"},
                    {markName, marks},
                    {"WvW Skirmish Claim Ticket", tickets},
                    {"Memory of Battle", "250"}
                };
            // --- PvP Ascended Precursors (all weights) ---
            } else if (item && (item->id == 67145 || item->id == 67147 || item->id == 67143 ||
                                item->id == 67144 || item->id == 67146 || item->id == 67142 ||
                                item->id == 67156 || item->id == 67158 || item->id == 67115 ||
                                item->id == 67117 || item->id == 67157 || item->id == 67128 ||
                                item->id == 67141 || item->id == 67152 || item->id == 67118 ||
                                item->id == 67131 || item->id == 67151 || item->id == 67148)) {
                acq.display_name = "Vendor - PvP League";
                acq.vendor_name = "Ascended Armor League Vendor";
                acq.vendor_location = "Heart of the Mists";
                acq.purchase_requirements = {
                    {"PvP League Ticket", "10"},
                    {"Ascended Shard of Glory", "60"}
                };
            // --- PvP Armor Vendors ---
            } else if (item && item->id == 77531) { // Mist Core Fragment
                acq.display_name = "Vendor - PvP";
                acq.vendor_name = "Ascended Armor League Vendor";
                acq.vendor_location = "Heart of the Mists";
                acq.purchase_requirements = {
                    {"PvP League Ticket", "10"},
                    {"Ascended Shard of Glory", "60"}
                };
            } else if (item && item->id == 82700) { // Record of League Victories
                acq.display_name = "Vendor - PvP";
                acq.vendor_name = "Ascended Armor League Vendor";
                acq.vendor_location = "Heart of the Mists";
                acq.purchase_requirements = {
                    {"PvP League Ticket", "30"}
                };
            // --- WvW Armor Vendors ---
            } else if (item && item->id == 83584) { // Legendary War Insight
                acq.display_name = "Vendor - Skirmish Supervisor";
                acq.vendor_name = "Skirmish Supervisor";
                acq.vendor_location = "WvW";
                acq.purchase_requirements = {
                    {"WvW Skirmish Claim Ticket", "175"},
                    {"Memory of Battle", "250"}
                };
            // --- Legendary Trinket Vendors ---
            // Aurora (LWS3)
            } else if (item && item->id == 81815) { // Gift of Bloodstone Magic
                acq.display_name = "Vendor - Scholar Rakka";
                acq.vendor_name = "Scholar Rakka";
                acq.vendor_location = "Bloodstone Fen";
                acq.purchase_requirements = {
                    {"Unbound Magic", "500"},
                    {"Blood Ruby", "250"}
                };
            } else if (item && item->id == 82036) { // Gift of Dragon Magic
                acq.display_name = "Vendor - Gleam of Sentience";
                acq.vendor_name = "Gleam of Sentience";
                acq.vendor_location = "Draconis Mons";
                acq.purchase_requirements = {
                    {"Unbound Magic", "500"},
                    {"Fire Orchid Blossom", "250"}
                };
            } else if (item && item->id == 19663) { // Bottle of Elonian Wine
                acq.display_name = "Vendor - Master Craftsman";
                acq.vendor_name = "Master Craftsman";
                acq.vendor_location = "Crafting Stations";
                acq.purchase_requirements = {
                    {"Coin", "2528"}
                };
            // Transcendence (PvP)
            } else if (item && item->id == 79980) { // Mist Pendant
                acq.display_name = "Vendor - PvP";
                acq.vendor_name = "Ascended Trinket League Vendor";
                acq.vendor_location = "Heart of the Mists";
                acq.purchase_requirements = {
                    {"PvP League Ticket", "15"},
                    {"Ascended Shard of Glory", "100"}
                };
            } else if (item && item->id == 93284) { // Tome of the Mists
                acq.display_name = "Vendor - PvP";
                acq.vendor_name = "Tournament Vendor";
                acq.vendor_location = "Heart of the Mists";
                acq.purchase_requirements = {
                    {"PvP Tournament Voucher", "50"}
                };
            } else if (item && item->id == 93034) { // Mist Diamond
                acq.display_name = "Vendor - PvP";
                acq.vendor_name = "Tournament Vendor";
                acq.vendor_location = "Heart of the Mists";
                acq.purchase_requirements = {
                    {"PvP Tournament Voucher", "200"}
                };
            } else if (item && item->id == 93151) { // Mist-Enhanced Orichalcum
                acq.display_name = "Vendor - PvP";
                acq.vendor_name = "Tournament Vendor";
                acq.vendor_location = "Heart of the Mists";
                acq.purchase_requirements = {
                    {"PvP Tournament Voucher", "100"}
                };
            } else if (item && item->id == 77486) { // Certificate of Support
                acq.display_name = "Vendor - PvP";
                acq.vendor_name = "PvP Vendor";
                acq.vendor_location = "Heart of the Mists";
                acq.purchase_requirements = {
                    {"Ascended Shard of Glory", "100"}
                };
            } else if (item && item->id == 83872) { // Star of Glory
                acq.display_name = "Vendor - PvP";
                acq.vendor_name = "PvP League Vendor";
                acq.vendor_location = "Heart of the Mists";
                acq.purchase_requirements = {
                    {"PvP League Ticket", "15"},
                    {"Ascended Shard of Glory", "200"}
                };
            } else if (item && item->id == 83082) { // Glob of Condensed Spirit Energy
                acq.display_name = "Vendor - PvP/WvW";
                acq.vendor_name = "Competitive Vendor";
                acq.vendor_location = "Heart of the Mists / WvW";
                acq.purchase_requirements = {
                    {"Ascended Shard of Glory", "200"}
                };
            } else if (item && item->id == 82926) { // Jar of Distilled Glory
                acq.display_name = "Vendor - PvP";
                acq.vendor_name = "PvP League Vendor";
                acq.vendor_location = "Heart of the Mists";
                acq.purchase_requirements = {
                    {"PvP League Ticket", "30"},
                    {"Ascended Shard of Glory", "400"}
                };
            } else if (item && item->id == 82471) { // Record of League Participation
                acq.display_name = "Vendor - PvP";
                acq.vendor_name = "PvP League Vendor";
                acq.vendor_location = "Heart of the Mists";
                acq.purchase_requirements = {
                    {"PvP League Ticket", "30"}
                };
            // --- The Ascension precursor: Wings of Glory (PvP vendor) ---
            } else if (item && item->id == 77507) { // Recruit's Wings of Glory
                acq.display_name = "Vendor - PvP";
                acq.vendor_name = "PvP League Vendor";
                acq.vendor_location = "Heart of the Mists";
                acq.purchase_requirements = {
                    {"PvP League Ticket", "10"}
                };
            } else if (item && item->id == 77522) { // Veteran's Wings of Glory
                acq.display_name = "Vendor - PvP";
                acq.vendor_name = "PvP League Vendor";
                acq.vendor_location = "Heart of the Mists";
                acq.purchase_requirements = {
                    {"PvP League Ticket", "20"}
                };
            } else if (item && item->id == 77477) { // Champion's Wings of Glory
                acq.display_name = "Vendor - PvP";
                acq.vendor_name = "PvP League Vendor";
                acq.vendor_location = "Heart of the Mists";
                acq.purchase_requirements = {
                    {"PvP League Ticket", "30"}
                };
            } else if (item && item->id == 77503) { // Elite's Wings of Glory
                acq.display_name = "Vendor - PvP";
                acq.vendor_name = "PvP League Vendor";
                acq.vendor_location = "Heart of the Mists";
                acq.purchase_requirements = {
                    {"PvP League Ticket", "40"}
                };
            // Conflux (WvW)
            } else if (item && item->id == 80058) { // Mist Band (Infused) - handled by multi-vendor logic in Initialize()
                acq.display_name = "Vendor";
                acq.vendor_name = "Multiple Vendors";
                acq.vendor_location = "Fractals / PvP / WvW";
            // Grandmaster Marks (via Box of Grandmaster Marks from Skirmish Supervisor)
            } else if (item && (item->id == 80685 || item->id == 80799 || item->id == 80857)) {
                acq.display_name = "Vendor - Skirmish Supervisor";
                acq.vendor_name = "Skirmish Supervisor";
                acq.vendor_location = "WvW";
                acq.purchase_requirements = {
                    {"Grandmaster Mark Shard", "10"}
                };
            } else if (item && item->id == 87557) { // Grandmaster Mark Shard
                acq.display_name = "Vendor - Skirmish Supervisor";
                acq.vendor_name = "Skirmish Supervisor";
                acq.vendor_location = "WvW";
                acq.purchase_requirements = {
                    {"WvW Skirmish Claim Ticket", "1"}
                };
            } else if (item && item->id == 93147) { // Mist Pearl
                acq.display_name = "Vendor - WvW";
                acq.vendor_name = "Skirmish Supervisor";
                acq.vendor_location = "WvW";
                acq.purchase_requirements = {
                    {"WvW Skirmish Claim Ticket", "200"}
                };
            } else if (item && item->id == 93248) { // Mist-Enhanced Mithril
                acq.display_name = "Vendor - WvW";
                acq.vendor_name = "Skirmish Supervisor";
                acq.vendor_location = "WvW";
                acq.purchase_requirements = {
                    {"WvW Skirmish Claim Ticket", "100"}
                };
            } else if (item && item->id == 83620) { // Certificate of Honor
                acq.display_name = "Vendor - WvW";
                acq.vendor_name = "Skirmish Supervisor";
                acq.vendor_location = "WvW";
                acq.purchase_requirements = {
                    {"WvW Skirmish Claim Ticket", "100"},
                    {"Memory of Battle", "250"}
                };
            } else if (item && item->id == 84099) { // Certificate of Heroics
                acq.display_name = "Vendor - WvW";
                acq.vendor_name = "Skirmish Supervisor";
                acq.vendor_location = "WvW";
                acq.purchase_requirements = {
                    {"WvW Skirmish Claim Ticket", "175"},
                    {"Memory of Battle", "250"}
                };
            } else if (item && item->id == 81296) { // Legendary Spike
                acq.display_name = "WvW Drop";
                acq.vendor_name = "WvW Drop";
                acq.vendor_location = "World vs World";
                acq.purchase_requirements = {
                    {"WvW", "Random drop from enemy players/NPCs"}
                };
            // --- Warcry precursor: Wings of War (WvW vendor) ---
            } else if (item && item->id == 81455) { // Recruit's Wings of War
                acq.display_name = "Vendor - WvW";
                acq.vendor_name = "Legendary Commander War Razor";
                acq.vendor_location = "WvW spawn areas";
                acq.purchase_requirements = {
                    {"WvW Skirmish Claim Ticket", "350"}
                };
            } else if (item && item->id == 81356) { // Soldier's Wings of War
                acq.display_name = "Vendor - WvW";
                acq.vendor_name = "Legendary Commander War Razor";
                acq.vendor_location = "WvW spawn areas";
                acq.purchase_requirements = {
                    {"WvW Skirmish Claim Ticket", "525"}
                };
            } else if (item && item->id == 81288) { // General's Wings of War
                acq.display_name = "Vendor - WvW";
                acq.vendor_name = "Legendary Commander War Razor";
                acq.vendor_location = "WvW spawn areas";
                acq.purchase_requirements = {
                    {"WvW Skirmish Claim Ticket", "700"}
                };
            } else if (item && item->id == 81294) { // Commander's Wings of War
                acq.display_name = "Vendor - WvW";
                acq.vendor_name = "Legendary Commander War Razor";
                acq.vendor_location = "WvW spawn areas";
                acq.purchase_requirements = {
                    {"WvW Skirmish Claim Ticket", "875"}
                };
            // --- Gift of Warfare: Essences (WvW vendor) ---
            } else if (item && item->id == 81326) { // Essence of Strategy
                acq.display_name = "Vendor - WvW";
                acq.vendor_name = "Legendary Commander War Razor";
                acq.vendor_location = "WvW spawn areas";
                acq.purchase_requirements = {
                    {"Badge of Honor", "1000"}
                };
            } else if (item && item->id == 81522) { // Essence of Animosity
                acq.display_name = "Vendor - WvW";
                acq.vendor_name = "Legendary Commander War Razor";
                acq.vendor_location = "WvW spawn areas";
                acq.purchase_requirements = {
                    {"Testimony of Jade Heroics", "40"}
                };
            } else if (item && item->id == 81469) { // Essence of Carnage
                acq.display_name = "Vendor - WvW";
                acq.vendor_name = "Legendary Commander War Razor";
                acq.vendor_location = "WvW spawn areas";
                acq.purchase_requirements = {
                    {"Memory of Battle", "500"}
                };
            } else if (item && item->id == 81320) { // Essence of Annihilation
                acq.display_name = "Vendor - WvW";
                acq.vendor_name = "Legendary Commander War Razor";
                acq.vendor_location = "WvW spawn areas";
                acq.purchase_requirements = {
                    {"WvW Skirmish Claim Ticket", "350"}
                };
            // Endless Summer (VoE)
            } else if (item && item->id == 106712) { // Gift of the Survivors
                acq.display_name = "Vendor - Castaway Agnes";
                acq.vendor_name = "Castaway Agnes";
                acq.vendor_location = "Hullgarden, Shipwreck Strand";
                acq.purchase_requirements = {
                    {"Concentrated Chromatic Sap", "1"},
                    {"Gift of Shipwreck Strand Exploration", "1"},
                    {"Survivor's Enchanted Compass", "1"},
                    {"Aether-Rich Sap", "500"}
                };
            } else if (item && item->id == 105804) { // Gift of the People
                acq.display_name = "Vendor - Canach";
                acq.vendor_name = "Canach";
                acq.vendor_location = "Breezy Cay, Shipwreck Strand";
                acq.purchase_requirements = {
                    {"Patron of the Magical Arts Plaque", "1"},
                    {"Gift of Starlit Weald Exploration", "1"},
                    {"Seer Wreath of Service", "1"},
                    {"Antiquated Ducat", "500"}
                };
            // Endless Summer (VoE)
            } else if (item && item->id == 106986) { // Gift of the Hylek
                acq.display_name = "Vendor - Shaman Palak";
                acq.vendor_name = "Shaman Palak";
                acq.vendor_location = "Hullgarden, Shipwreck Strand";
                acq.purchase_requirements = {
                    {"Sun Bead", "250"},
                    {"Karma", "300000"},
                    {"Coin", "2000000"}
                };
            // VoE sub-components
            } else if (item && item->id == 105848) { // Concentrated Chromatic Sap
                acq.display_name = "Vendor - Sharpwhisker";
                acq.vendor_name = "Tyrian Alliance Representative Sharpwhisker";
                acq.vendor_location = "Glimmering Arches, Shipwreck Strand";
                acq.purchase_requirements = {
                    {"Chromatic Sap", "500"},
                    {"Coin", "2500000"},
                    {"Karma", "300000"}
                };
            } else if (item && item->id == 105933) { // Patron of the Magical Arts Plaque
                acq.display_name = "Vendor - Huntmaster Arnorr";
                acq.vendor_name = "Huntmaster Arnorr";
                acq.vendor_location = "Untamed Crags, Starlit Weald";
                acq.purchase_requirements = {
                    {"Raw Enchanting Stone", "500"},
                    {"Coin", "2500000"},
                    {"Karma", "300000"}
                };
            // Aetheric Anchor (VoE)
            } else if (item && item->id == 105875) { // Gift of Insight
                acq.display_name = "Vendor - Magister Sirkk";
                acq.vendor_name = "Magister Sirkk";
                acq.vendor_location = "Crumbling Precipice, Starlit Weald";
                acq.purchase_requirements = {
                    {"Mystic Clover", "100"},
                    {"Amalgamated Draconic Lodestone", "55"},
                    {"Gift of Condensed Might", "4"},
                    {"Gift of Condensed Magic", "4"}
                };
            } else if (item && item->id == 106632) { // Gift of the Elders
                acq.display_name = "Vendor - Foothold Bivouac";
                acq.vendor_name = "Foothold Bivouac Heart Vendor";
                acq.vendor_location = "Starlit Weald";
                acq.purchase_requirements = {
                    {"Gift of the Tides", "1"},
                    {"Bloodstone Shard", "1"},
                    {"Gift of Research", "1"},
                    {"Gift of the Mists", "1"}
                };
            } else if (item && item->id == 100400) { // Relic of the Sunless
                acq.display_name = "Vendor - Lyhr";
                acq.vendor_name = "Lyhr";
                acq.vendor_location = "Wizard's Tower";
                acq.purchase_requirements = {
                    {"Purified Rift Essence", "10"}
                };
            } else if (item && item->id == 20852) { // Eldritch Scroll
                acq.display_name = "Vendor - Miyani";
                acq.vendor_name = "Miyani";
                acq.vendor_location = "Trader's Forum, Lion's Arch";
                acq.purchase_requirements = {
                    {"Spirit Shards", "50"}
                };
            } else if (item && item->id == 95813) { // Hydrocatalytic Reagent
                acq.display_name = "Vendor - Master Craftsman";
                acq.vendor_name = "Master Craftsman";
                acq.vendor_location = "Crafting Stations";
                acq.purchase_requirements = {
                    {"Coin", "150"}
                };
            } else if (item && item->id == 20799) { // Mystic Crystal
                acq.display_name = "Vendor - Miyani";
                acq.vendor_name = "Miyani";
                acq.vendor_location = "Trader's Forum, Lion's Arch";
                acq.purchase_requirements = {
                    {"Spirit Shard", "3"}
                };
            } else if (item && item->id == 20796) { // Philosopher's Stone
                acq.display_name = "Vendor - Miyani";
                acq.vendor_name = "Miyani";
                acq.vendor_location = "Trader's Forum, Lion's Arch";
                acq.purchase_requirements = {
                    {"Spirit Shards", "1 (buys 10)"}
                };
            } else if (item && item->id == 19790) { // Spool of Gossamer Thread
                acq.display_name = "Vendor - Master Craftsman";
                acq.vendor_name = "Master Craftsman";
                acq.vendor_location = "Crafting Stations";
                acq.purchase_requirements = {
                    {"Coin", "64"}
                };
            // --- The Ascension: PvP Essences (vendor) ---
            } else if (item && item->id == 77552) { // Essence of Determination
                acq.display_name = "Vendor - PvP";
                acq.vendor_name = "PvP League Vendor";
                acq.vendor_location = "Heart of the Mists";
                acq.purchase_requirements = {
                    {"Shard of Glory", "25"}
                };
            } else if (item && item->id == 77488) { // Essence of Challenge
                acq.display_name = "Vendor - PvP";
                acq.vendor_name = "PvP League Vendor";
                acq.vendor_location = "Heart of the Mists";
                acq.purchase_requirements = {
                    {"Shard of Glory", "50"}
                };
            } else if (item && item->id == 77541) { // Essence of Discipline
                acq.display_name = "Vendor - PvP";
                acq.vendor_name = "PvP League Vendor";
                acq.vendor_location = "Heart of the Mists";
                acq.purchase_requirements = {
                    {"Shard of Glory", "75"}
                };
            } else if (item && item->id == 77535) { // Essence of Success
                acq.display_name = "Vendor - PvP";
                acq.vendor_name = "PvP League Vendor";
                acq.vendor_location = "Heart of the Mists";
                acq.purchase_requirements = {
                    {"Shard of Glory", "100"}
                };
            // --- PvP/WvW reward items ---
            } else if (item && item->id == 70820) { // Shard of Glory
                acq.display_name = "PvP Reward";
                acq.vendor_name = "PvP Reward Tracks";
                acq.vendor_location = "Heart of the Mists";
                acq.purchase_requirements = {
                    {"PvP Reward Tracks", "15 per track"}
                };
            } else if (item && item->id == 93022) { // Emblem of Victory
                acq.display_name = "PvP Achievement";
                acq.vendor_name = "Champion of the Gods";
                acq.vendor_location = "PvP";
                acq.purchase_requirements = {
                    {"Ranked PvP Wins", "120"}
                };
            } else if (item && item->id == 93146) { // Emblem of the Conqueror
                acq.display_name = "WvW Achievement";
                acq.vendor_name = "WvW Objective Captures";
                acq.vendor_location = "WvW";
                acq.purchase_requirements = {
                    {"Objective Captures", "500"}
                };
            // --- Gen2 Tributes (sold by various map vendors) ---
            } else if (item && (
                item->id == 79453 ||  // Tribute to Endeavor
                item->id == 79845 ||  // Tribute to Friendship
                item->id == 80201 ||  // Tribute to the Man o' War
                item->id == 81163 ||  // Tribute to Liturgy
                item->id == 81871 ||  // Tribute to Arah
                item->id == 81974 ||  // Tribute to the Queen
                item->id == 86266 ||  // Tribute to the Dark Arts
                item->id == 87165 ||  // Tribute to Resolution
                item->id == 87627 ||  // Tribute to the Call of the Void
                item->id == 88756 ||  // Tribute to Tlehco
                item->id == 89478 ||  // Tribute to Spero
                item->id == 90776     // Tribute to the Exitare
            )) {
                acq.display_name = "Vendor - Map Vendors";
                acq.vendor_name = "Various Heart/Mastery Vendors";
                acq.vendor_location = "HoT, PoF, LW, EoD maps";
                acq.purchase_requirements = {
                    {"Map Currencies", "Varies by vendor"}
                };
            // --- JW Precursor: Nyr Hrammr ---
            } else if (item && item->id == 103973) { // Nyr Hrammr
                acq.display_name = "Vendor - Ward Crafter";
                acq.vendor_name = "Ward Crafter Lucirae";
                acq.vendor_location = "Moon Camp Covert, Lowland Shore";
                acq.purchase_requirements = {
                    {"Legendary Weapon Collection", "Klobjarne Geirr Gifts"}
                };
            // --- VoE Precursor: Orrax Contained ---
            } else if (item && item->id == 104690) { // Orrax Contained
                acq.display_name = "Vendor - Ward Crafter";
                acq.vendor_name = "Ward Vendor";
                acq.vendor_location = "Visions of Eternity maps";
                acq.purchase_requirements = {
                    {"Collection", "Unknown Nightmares: Orrax Contained"}
                };
            // --- VoE: Gift of Titan Understanding ---
            } else if (item && item->id == 104791) { // Gift of Titan Understanding
                acq.display_name = "Vendor - Heart Vendor";
                acq.vendor_name = "Renown Heart Vendor";
                acq.vendor_location = "Mistburned Barrens";
                acq.purchase_requirements = {
                    {"Collection", "Unknown Nightmares: Gift of Shadows"}
                };
            // --- Dungeon Gifts (all from Dungeon Armor and Weapons vendor) ---
            } else if (item && item->id == 19664) { // Gift of Ascalon
                acq.display_name = "Vendor - Dungeon Merchant";
                acq.vendor_name = "Dungeon Armor and Weapons";
                acq.vendor_location = "Fort Marriner, Lion's Arch";
                acq.purchase_requirements = {
                    {"Tales of Dungeon Delving", "500 (Ascalonian Catacombs)"}
                };
            } else if (item && item->id == 19665) { // Gift of the Nobleman
                acq.display_name = "Vendor - Dungeon Merchant";
                acq.vendor_name = "Dungeon Armor and Weapons";
                acq.vendor_location = "Fort Marriner, Lion's Arch";
                acq.purchase_requirements = {
                    {"Tales of Dungeon Delving", "500 (Caudecus's Manor)"}
                };
            } else if (item && item->id == 19667) { // Gift of Thorns
                acq.display_name = "Vendor - Dungeon Merchant";
                acq.vendor_name = "Dungeon Armor and Weapons";
                acq.vendor_location = "Fort Marriner, Lion's Arch";
                acq.purchase_requirements = {
                    {"Tales of Dungeon Delving", "500 (Twilight Arbor)"}
                };
            } else if (item && item->id == 19666) { // Gift of the Forgeman
                acq.display_name = "Vendor - Dungeon Merchant";
                acq.vendor_name = "Dungeon Armor and Weapons";
                acq.vendor_location = "Fort Marriner, Lion's Arch";
                acq.purchase_requirements = {
                    {"Tales of Dungeon Delving", "500 (Sorrow's Embrace)"}
                };
            } else if (item && item->id == 19668) { // Gift of Baelfire
                acq.display_name = "Vendor - Dungeon Merchant";
                acq.vendor_name = "Dungeon Armor and Weapons";
                acq.vendor_location = "Fort Marriner, Lion's Arch";
                acq.purchase_requirements = {
                    {"Tales of Dungeon Delving", "500 (Citadel of Flame)"}
                };
            } else if (item && item->id == 19670) { // Gift of the Sanctuary
                acq.display_name = "Vendor - Dungeon Merchant";
                acq.vendor_name = "Dungeon Armor and Weapons";
                acq.vendor_location = "Fort Marriner, Lion's Arch";
                acq.purchase_requirements = {
                    {"Tales of Dungeon Delving", "500 (Honor of the Waves)"}
                };
            } else if (item && item->id == 19671) { // Gift of Knowledge
                acq.display_name = "Vendor - Dungeon Merchant";
                acq.vendor_name = "Dungeon Armor and Weapons";
                acq.vendor_location = "Fort Marriner, Lion's Arch";
                acq.purchase_requirements = {
                    {"Tales of Dungeon Delving", "500 (Crucible of Eternity)"}
                };
            } else if (item && item->id == 19669) { // Gift of Zhaitan
                acq.display_name = "Vendor - Dungeon Merchant";
                acq.vendor_name = "Dungeon Armor and Weapons";
                acq.vendor_location = "Fort Marriner, Lion's Arch";
                acq.purchase_requirements = {
                    {"Tales of Dungeon Delving", "500 (Ruined City of Arah)"}
                };
            // --- Master Craftsman vendors ---
            } else if (item && item->id == 46747) { // Thermocatalytic Reagent
                acq.display_name = "Vendor - Master Craftsman";
                acq.vendor_name = "Master Craftsman";
                acq.vendor_location = "Crafting Stations";
                acq.purchase_requirements = {
                    {"Coin", "150"}
                };
            } else if (item && item->id == 12156) { // Jug of Water
                acq.display_name = "Vendor - Master Craftsman";
                acq.vendor_name = "Master Craftsman";
                acq.vendor_location = "Crafting Stations";
                acq.purchase_requirements = {
                    {"Coin", "8"}
                };
            // --- Fractal vendor ---
            } else if (item && item->id == 38014) { // Vial of Condensed Mists Essence
                acq.display_name = "Vendor - INFUZ-5959";
                acq.vendor_name = "INFUZ-5959";
                acq.vendor_location = "Mistlock Observatory";
                acq.purchase_requirements = {
                    {"Fractal Relics", "75"}
                };
            // --- Laurel Merchant ---
            } else if (item && item->id == 39125) { // Mystic Binding Agent
                acq.display_name = "Vendor - Laurel Merchant";
                acq.vendor_name = "Laurel Merchant";
                acq.vendor_location = "Major Cities";
                acq.purchase_requirements = {
                    {"Laurels", "1"}
                };
            // --- HoT Precursor Collection vendors ---
            } else if (item && item->id == 74909) { // Sculptor's Tools
                acq.display_name = "Vendor - Lord Joshua";
                acq.vendor_name = "Lord Joshua";
                acq.vendor_location = "Shire of Beetletun, Queensdale";
                acq.purchase_requirements = {
                    {"Karma", "4500"}
                };
            } else if (item && item->id == 74536) { // Delicate Harp String
                acq.display_name = "Vendor - Marcello DiGiacomo";
                acq.vendor_name = "Marcello DiGiacomo";
                acq.vendor_location = "Trader's Forum, Lion's Arch";
                acq.purchase_requirements = {
                    {"Karma", "1050"}
                };
            } else if (item && item->id == 75272) { // Black Powder
                acq.display_name = "Vendor - Thegren Topjaw";
                acq.vendor_name = "Thegren Topjaw";
                acq.vendor_location = "Tela Range, Plains of Ashford";
                acq.purchase_requirements = {
                    {"Karma", "1500"}
                };
            } else if (item && item->id == 76076) { // Jar of Paint Base
                acq.display_name = "Vendor - Tirzie the Painter";
                acq.vendor_name = "Tirzie the Painter";
                acq.vendor_location = "Orlaf Escarpments, Queensdale";
                acq.purchase_requirements = {
                    {"Karma", "1848"}
                };
            } else if (item && item->id == 11) { // Elaborate Totem
                acq.display_name = "Vendor - Crafting Vendor";
                acq.vendor_name = "Crafting Vendor";
                acq.vendor_location = "Major Cities";
                acq.purchase_requirements = {
                    {"Karma", "Unknown"}
                };
            } else if (item && item->id == 12) { // Pile of Crystalline Dust
                acq.display_name = "Vendor - Crafting Vendor";
                acq.vendor_name = "Crafting Vendor";
                acq.vendor_location = "Major Cities";
                acq.purchase_requirements = {
                    {"Karma", "Unknown"}
                };
            } else if (item && item->id == 13) { // Vicious Fang
                acq.display_name = "Vendor - Crafting Vendor";
                acq.vendor_name = "Crafting Vendor";
                acq.vendor_location = "Major Cities";
                acq.purchase_requirements = {
                    {"Karma", "Unknown"}
                };
            } else {
                // Generic vendor information
                acq.display_name = "Vendor";
                acq.vendor_name = "Unknown Vendor";
                acq.vendor_location = "Various Locations";
                acq.purchase_requirements = {
                    {"Cost", "Unknown"}
                };
            }
        } else if (method == "mystic_forge") {
            acq.display_name = "Mystic Forge";
            acq.description = "Created in Mystic Forge";
            acq.details = {"Located in Lion's Arch", "Combine 4 items", "Random results possible"};
        } else if (method == "trading_post") {
            acq.display_name = "Trading Post";
            acq.description = "Available on Trading Post";
            acq.details = {"Buy from other players", "Check market prices", "May be expensive"};
        } else if (method == "achievement") {
            acq.display_name = "Achievement Reward";
            acq.description = "Earned through achievements";
            if (item && item->id == 19677) { // Gift of Exploration
                acq.display_name = "World Completion";
                acq.description = "100% map completion of all Central Tyrian maps";
                acq.details = {"Explore all maps in Central Tyria", "Two gifts awarded per completion"};
            } else {
                acq.details = {"Complete specific achievements"};
            }
        } else if (method == "reward_track") {
            acq.display_name = "Reward Track";
            acq.description = "Earned from reward tracks";
            if (item && item->id == 19678) { // Gift of Battle
                acq.display_name = "WvW Reward Track";
                acq.description = "Gift of Battle reward track in World vs World";
                acq.details = {"Play WvW game mode", "Complete the Gift of Battle track", "Requires significant WvW participation"};
            } else {
                acq.details = {"Complete reward tracks in PvP or WvW"};
            }
        } else {
            acq.display_name = method; // Fallback to method name
            acq.description = "Various acquisition methods";
            acq.details = {method};
        }
        
        return acq;
    }
    
    const std::vector<Legendary>& DataManager::GetLegendaries() {
        return s_legendaries;
    }
    
    const std::unordered_map<uint32_t, Item>& DataManager::GetItems() {
        return s_items;
    }
    
    const std::unordered_map<uint32_t, Recipe>& DataManager::GetRecipes() {
        return s_recipes;
    }
    
    const std::vector<Currency>& DataManager::GetCurrencies() {
        return s_currencies;
    }
    
    const Currency* DataManager::GetCurrency(uint32_t id) {
        for (const auto& currency : s_currencies) {
            if (currency.id == id) {
                return &currency;
            }
        }
        return nullptr;
    }
    
    const Currency* DataManager::GetCurrencyByName(const std::string& name) {
        // Map common plural/variant names to canonical GW2 API currency names
        static const std::unordered_map<std::string, std::string> aliases = {
            // Core currencies
            {"Spirit Shards", "Spirit Shard"},
            {"Laurels", "Laurel"},
            {"Coins", "Coin"},
            {"Badges of Honor", "Badge of Honor"},
            {"Guild Commendations", "Guild Commendation"},
            {"Transmutation Charges", "Transmutation Charge"},
            // Fractal currencies
            {"Fractal Relics", "Fractal Relic"},
            {"Pristine Fractal Relics", "Pristine Fractal Relic"},
            // PvP currencies
            {"League Ticket", "PvP League Ticket"},
            {"League Tickets", "PvP League Ticket"},
            {"Ascended Shard of Glory", "Ascended Shards of Glory"},
            {"Shard of Glory", "Ascended Shards of Glory"},
            {"Shards of Glory", "Ascended Shards of Glory"},
            // WvW currencies
            {"Skirmish Claim Ticket", "WvW Skirmish Claim Ticket"},
            {"Skirmish Claim Tickets", "WvW Skirmish Claim Ticket"},
            {"WvW Skirmish Claim Tickets", "WvW Skirmish Claim Ticket"},
            {"Magnetite Shards", "Magnetite Shard"},
            // HoT map currencies
            {"Airship Parts", "Airship Part"},
            {"Lumps of Aurillium", "Lump of Aurillium"},
            {"Ley Line Crystals", "Ley Line Crystal"},
            {"Bandit Crests", "Bandit Crest"},
            // PoF currencies
            {"Trade Contracts", "Trade Contract"},
            {"Elegy Mosaics", "Elegy Mosaic"},
            // LW/IBS currencies
            {"Geodes", "Geode"},
            // SotO currencies
            {"Ancient Coins", "Ancient Coin"},
            {"Static Charges", "Static Charge"},
            {"Pinches of Stardust", "Pinch of Stardust"},
            {"Calcified Gasps", "Calcified Gasp"},
            {"Antiquated Ducats", "Antiquated Ducat"},
            // JW currencies
            {"Aether-Rich Saps", "Aether-Rich Sap"},
            // Dungeon currencies
            {"Ascalonian Tears", "Ascalonian Tear"},
            {"Shards of Zhaitan", "Shard of Zhaitan"},
            {"Tales of Dungeon Delving", "Tales of Dungeon Delving"},
            // Raid currencies
            {"Legendary Insights", "Legendary Insight"},
            // Research
            {"Research Notes", "Research Note"},
        };
        
        // Try direct match first
        for (const auto& c : s_currencies) {
            if (c.name == name) return &c;
        }
        // Try alias
        auto it = aliases.find(name);
        if (it != aliases.end()) {
            for (const auto& c : s_currencies) {
                if (c.name == it->second) return &c;
            }
        }
        return nullptr;
    }
    
    std::string DataManager::GetCurrencyName(uint32_t id) {
        const Currency* currency = GetCurrency(id);
        return currency ? currency->name : "Unknown Currency";
    }
    
    size_t DataManager::GetAcquisitionMethodCount() {
        return s_acquisition_methods.size();
    }
    
    std::string DataManager::GetDebugPath(const std::string& type) {
        if (type == "legendaries") {
            return s_debug_legendaries_path;
        } else if (type == "items") {
            return s_debug_items_path;
        } else if (type == "recipes") {
            return s_debug_recipes_path;
        }
        return "UNKNOWN_TYPE";
    }
    
    std::string DataManager::FormatDisciplines(const std::vector<std::string>& disciplines) {
        if (disciplines.empty()) {
            return "";
        }
        
        std::string result;
        for (size_t i = 0; i < disciplines.size(); ++i) {
            const std::string& discipline = disciplines[i];
            
            if (discipline == "Artificer") {
                result += "ART";
            } else if (discipline == "Armorsmith") {
                result += "ARM";
            } else if (discipline == "Chef") {
                result += "CHF";
            } else if (discipline == "Huntsman") {
                result += "HNT";
            } else if (discipline == "Jeweler") {
                result += "JWL";
            } else if (discipline == "Leatherworker") {
                result += "LTW";
            } else if (discipline == "Scribe") {
                result += "SCB";
            } else if (discipline == "Tailor") {
                result += "TLR";
            } else if (discipline == "Weaponsmith") {
                result += "WPN";
            } else {
                result += discipline; // Fallback to original name
            }
            
            if (i < disciplines.size() - 1) {
                result += "/";
            }
        }
        
        return result;
    }
    
    const std::vector<AcquisitionMethod>& DataManager::GetAcquisitionMethods(uint32_t item_id) {
        static std::vector<AcquisitionMethod> empty;
        auto it = s_acquisition_methods.find(item_id);
        return it != s_acquisition_methods.end() ? it->second : empty;
    }
    
    const std::vector<RecipeIngredient>& DataManager::GetRecipeIngredients(uint32_t item_id) {
        static std::vector<RecipeIngredient> empty;
        auto it = s_recipes.find(item_id);
        return it != s_recipes.end() ? it->second.ingredients : empty;
    }
    
    const Recipe* DataManager::GetRecipe(uint32_t item_id) {
        auto it = s_recipes.find(item_id);
        return it != s_recipes.end() ? &it->second : nullptr;
    }
    
    const Item* DataManager::GetItem(uint32_t id) {
        auto it = s_items.find(id);
        return it != s_items.end() ? &it->second : nullptr;
    }
    
    std::string DataManager::GetItemName(uint32_t id) {
        const Item* item = GetItem(id);
        return item ? item->name : "Unknown Item";
    }
    
    uint32_t DataManager::ResolveItemIdByName(const std::string& name) {
        // Check the known name-to-id map first (handles plural/singular variants)
        static const std::unordered_map<std::string, uint32_t>& map = []() -> const std::unordered_map<std::string, uint32_t>& {
            // Reuse the same map from BuildVendorCostMaterials
            static const std::unordered_map<std::string, uint32_t> name_to_item_id = {
                {"Mystic Coins", 19976},
                {"Mystic Coin", 19976},
                {"Obsidian Shards", 19925},
                {"Obsidian Shard", 19925},
                {"Glob of Ectoplasm", 19721},
                {"Mystic Clover", 19675},
                {"Philosopher's Stone", 20796},
                {"Thermocatalytic Reagent", 46747},
                {"Jug of Water", 12156},
                {"Vial of Condensed Mists Essence", 38014},
                {"Icy Runestone", 19676},
                {"Bloodstone Shard", 19925},
                {"Crystalline Ore", 46682},
                {"Antique Summoning Stone", 96978},
                {"Antique Summoning Stones", 96978},
                {"Jade Runestone", 96722},
                {"Jade Runestones", 96722},
                {"Chunk of Pure Jade", 97102},
                {"Chunks of Pure Jade", 97102},
                {"Chunk of Ancient Ambergris", 96347},
                {"Chunks of Ancient Ambergris", 96347},
                {"Blessing of the Jade Empress", 97829},
                {"Blessings of the Jade Empress", 97829},
                {"Memory of Aurene", 96088},
                {"Memories of Aurene", 96088},
                {"Hydrocatalytic Reagent", 95813},
                {"Hydrocatalytic Reagents", 95813},
                {"Amalgamated Draconic Lodestone", 92687},
                {"Amalgamated Draconic Lodestones", 92687},
                {"Chunk of Petrified Echovald Resin", 96471},
                {"Chunks of Petrified Echovald Resin", 96471},
                {"Lesser Vision Crystal", 49523},
                {"Lesser Vision Crystals", 49523},
                {"Provisioner Token", 88926},
                {"Provisioner Tokens", 88926},
                {"Amalgamated Rift Essence", 100930},
                {"Amalgamated Rift Essences", 100930},
                {"Case of Captured Lightning", 100267},
                {"Clot of Congealed Screams", 100098},
                {"Pouch of Stardust", 99964},
                {"Ball of Dark Energy", 71994},
                {"Cube of Stabilized Dark Energy", 73137},
                {"Concentrated Chromatic Sap", 105848},
                {"Chromatic Sap", 106385},
                {"Gift of Shipwreck Strand Exploration", 106467},
                {"Survivor's Enchanted Compass", 106370},
                {"Patron of the Magical Arts Plaque", 105933},
                {"Raw Enchanting Stone", 105686},
                {"Gift of Starlit Weald Exploration", 106672},
                {"Seer Wreath of Service", 106627},
                {"Sun Bead", 19717},
                {"Sun Beads", 19717},
                {"Exotic Essence of Luck", 45178},
                {"Legendary Insight", 98327},
                {"Legendary Insights", 98327},
                {"Envoy Insignia", 80516},
                {"Chak Egg", 72205},
                {"Chak Eggs", 72205},
                {"Auric Ingot", 73537},
                {"Auric Ingots", 73537},
                {"Reclaimed Metal Plate", 74356},
                {"Reclaimed Metal Plates", 74356},
                {"Mist Core Fragment", 77531},
                {"Mist Core Fragments", 77531},
                {"Record of League Victories", 82700},
                {"Legendary War Insight", 83584},
                {"Legendary War Insights", 83584},
                {"Memory of Battle", 71581},
                {"Memories of Battle", 71581},
                {"Shard of Glory", 70820},
                {"Shards of Glory", 70820},
                // LWS3 map currencies (inventory items)
                {"Blood Ruby", 79280},
                {"Blood Rubies", 79280},
                {"Fire Orchid Blossom", 81127},
                {"Fire Orchid Blossoms", 81127},
                // Grandmaster Marks
                {"Grandmaster Armorsmith's Mark", 80685},
                {"Grandmaster Leatherworker's Mark", 80799},
                {"Grandmaster Tailor's Mark", 80857},
                {"Grandmaster Mark Shard", 87557},
                {"Grandmaster Mark Shards", 87557},
            };
            return name_to_item_id;
        }();
        auto it = map.find(name);
        if (it != map.end()) return it->second;
        // Fallback: search loaded items by exact name
        for (const auto& [id, item] : s_items) {
            if (item.name == name) return id;
        }
        return 0;
    }

    std::string DataManager::GetLegendaryName(uint32_t id) {
        for (const auto& legendary : s_legendaries) {
            if (legendary.id == id) {
                return legendary.name;
            }
        }
        return "Unknown Legendary";
    }
    
    void DataManager::ResetColumns() {
        s_columns.clear();
        
        // Create only column 0 (legendaries list); additional columns are created dynamically
        ColumnData column;
        column.selected_index = -1;
        column.selected_acquisition_index = -1;
        column.selected_material_index = -1;
        column.title = "Legendaries";
        column.items = s_legendaries;
        s_columns.push_back(column);
    }
    
    void DataManager::UpdateColumn(int column_index, uint32_t item_id, int item_count) {
        if (column_index >= s_columns.size() || column_index < 0) {
            return;
        }
        
        if (item_id == 0) {
            return;
        }
        
        // Column 0 is the legendary list; any column can spawn the next one
        // NOTE: Do not hold references to s_columns elements across resize() calls.
        
        if (column_index == 0) {
            // First column - select legendary
            for (size_t i = 0; i < s_columns[column_index].items.size(); ++i) {
                if (s_columns[column_index].items[i].id == item_id) {
                    s_columns[column_index].selected_index = static_cast<int>(i);
                    break;
                }
            }
            
            // Update column 1 (index 1) with acquisition methods and materials
            // Ensure the next column exists
            if (s_columns.size() <= column_index + 1) {
                s_columns.resize(column_index + 2);  // Make room for the next column
                s_columns[column_index + 1].title = "Details";
                s_columns[column_index + 1].selected_index = -1;
                s_columns[column_index + 1].selected_acquisition_index = -1;
                s_columns[column_index + 1].selected_material_index = -1;
            }
            
            ColumnData& next_column = s_columns[column_index + 1];
            
            // Clear existing data
            next_column.acquisitions.clear();
            next_column.materials.clear();
            next_column.selected_acquisition_index = -1;
            next_column.selected_material_index = -1;
            
            // Get data for column 1
            next_column.source_item_id = item_id;
            next_column.source_item_count = item_count;
            try {
                next_column.acquisitions = GetAcquisitionMethods(item_id);
                next_column.materials = GetRecipeIngredients(item_id);
            } catch (...) {
                // If data lookup fails, create empty vectors
                next_column.acquisitions.clear();
                next_column.materials.clear();
            }
            
            // Set title based on content
            const Recipe* recipe = GetRecipe(item_id);
            if (!next_column.materials.empty() && recipe) {
                // Filter out trading_post - not a meaningful choice when recipe exists
                next_column.acquisitions.erase(
                    std::remove_if(next_column.acquisitions.begin(), next_column.acquisitions.end(),
                        [](const AcquisitionMethod& a) { return a.method == "trading_post"; }),
                    next_column.acquisitions.end());
                
                if (next_column.acquisitions.size() > 1) {
                    // Multiple meaningful methods (e.g. mystic_forge + vendor) - show choice
                    const Item* clicked_item = GetItem(item_id);
                    next_column.title = (clicked_item ? clicked_item->name + " - " : "") + "Acquisition";
                    next_column.materials.clear();
                } else {
                    // Single or no acquisition method - show recipe directly
                    if (recipe->type == "mystic_forge") {
                        next_column.title = "Mystic Forge";
                    } else if (recipe->type == "crafting" && !recipe->disciplines.empty()) {
                        next_column.title = "Craft (" + FormatDisciplines(recipe->disciplines) + " " + std::to_string(recipe->rating) + ")";
                    } else {
                        next_column.title = "Materials";
                    }
                    next_column.acquisitions.clear();
                }
            } else if (next_column.acquisitions.size() > 1) {
                next_column.title = "Acquisition Methods";
                next_column.materials.clear();
            } else if (next_column.acquisitions.size() == 1) {
                const AcquisitionMethod& only_method = next_column.acquisitions[0];
                if (only_method.method == "vendor") {
                    next_column.title = !only_method.vendor_name.empty()
                        ? "Vendor - " + only_method.vendor_name : "Vendor Costs";
                    next_column.materials.clear();
                    int qty = next_column.source_item_count;
                    BuildVendorCostMaterials(next_column.materials, only_method.purchase_requirements, qty);
                } else if (only_method.method == "trading_post") {
                    next_column.title = "Trading Post";
                } else if (only_method.method == "mystic_forge") {
                    next_column.title = "Mystic Forge";
                } else {
                    next_column.title = only_method.display_name.empty()
                        ? only_method.method : only_method.display_name;
                }
                next_column.acquisitions.clear();
            } else if (!next_column.materials.empty()) {
                next_column.title = "Materials";
            } else {
                next_column.title = "Details";
            }
            
            // Truncate columns beyond the next one
            if (s_columns.size() > static_cast<size_t>(column_index + 2)) {
                s_columns.resize(column_index + 2);
            }
            
        } else {
            // Handle other columns - populate next column with clicked item's data
            {
                // Ensure the next column exists
                if (s_columns.size() <= column_index + 1) {
                    s_columns.resize(column_index + 2);  // Make room for the next column
                    s_columns[column_index + 1].title = "Details";
                    s_columns[column_index + 1].selected_index = -1;
                    s_columns[column_index + 1].selected_acquisition_index = -1;
                    s_columns[column_index + 1].selected_material_index = -1;
                }
                
                ColumnData& next_column = s_columns[column_index + 1];
                
                // Clear existing data
                next_column.acquisitions.clear();
                next_column.materials.clear();
                next_column.selected_acquisition_index = -1;
                next_column.selected_material_index = -1;
                
                // Get data for the clicked item - recipe takes priority over acquisition methods
                next_column.source_item_id = item_id;
                next_column.source_item_count = item_count;
                try {
                    const Recipe* item_recipe = GetRecipe(item_id);
                    if (item_recipe && !item_recipe->ingredients.empty()) {
                        // Check if item has multiple meaningful acquisition methods
                        auto acq_methods = GetAcquisitionMethods(item_id);
                        // Filter out trading_post when recipe exists
                        acq_methods.erase(
                            std::remove_if(acq_methods.begin(), acq_methods.end(),
                                [](const AcquisitionMethod& a) { return a.method == "trading_post"; }),
                            acq_methods.end());
                        
                        if (acq_methods.size() > 1) {
                            // Multiple meaningful methods - show acquisition choice
                            next_column.acquisitions = acq_methods;
                            const Item* clicked_item = GetItem(item_id);
                            next_column.title = (clicked_item ? clicked_item->name + " - " : "") + "Acquisition";
                        } else {
                            // Single method - show recipe directly
                            next_column.materials = item_recipe->ingredients;
                            // Scale ingredients by number of crafts needed
                            uint32_t out_cnt = item_recipe->output_count > 0 ? item_recipe->output_count : 1;
                            int crafts = (item_count + out_cnt - 1) / out_cnt;
                            if (crafts > 1) {
                                for (auto& mat : next_column.materials) mat.count *= crafts;
                            }
                            if (item_recipe->type == "mystic_forge") {
                                next_column.title = "Mystic Forge";
                            } else if (item_recipe->type == "crafting" && !item_recipe->disciplines.empty()) {
                                next_column.title = "Craft (" + FormatDisciplines(item_recipe->disciplines) + " " + std::to_string(item_recipe->rating) + ")";
                            } else {
                                next_column.title = "Materials";
                            }
                            next_column.acquisitions.clear();
                        }
                    } else {
                        // No recipe found - fall back to acquisition methods
                        next_column.acquisitions = GetAcquisitionMethods(item_id);
                    }
                } catch (...) {
                    next_column.acquisitions.clear();
                    next_column.materials.clear();
                }
                
                // Set title based on content
                // Title is already set by recipe-first logic above for items with recipes.
                // This section handles items that fell through to acquisition methods.
                if (!next_column.materials.empty() && next_column.acquisitions.empty()) {
                    // Materials already populated from recipe - title already set
                } else if (next_column.acquisitions.size() > 1) {
                    next_column.title = "Acquisition Methods";
                    next_column.materials.clear();
                } else if (next_column.acquisitions.size() == 1) {
                    const AcquisitionMethod& only_method = next_column.acquisitions[0];
                    if (only_method.method == "mystic_forge") {
                        next_column.title = "Mystic Forge";
                        const Recipe* item_recipe = GetRecipe(item_id);
                        if (item_recipe && !item_recipe->ingredients.empty()) {
                            next_column.materials = item_recipe->ingredients;
                            uint32_t out_cnt = item_recipe->output_count > 0 ? item_recipe->output_count : 1;
                            int crafts = (item_count + out_cnt - 1) / out_cnt;
                            if (crafts > 1) {
                                for (auto& mat : next_column.materials) mat.count *= crafts;
                            }
                        }
                    } else if (only_method.method == "vendor") {
                        next_column.title = !only_method.vendor_name.empty() 
                            ? "Vendor - " + only_method.vendor_name : "Vendor Costs";
                        next_column.materials.clear();
                        int qty = next_column.source_item_count;
                        BuildVendorCostMaterials(next_column.materials, only_method.purchase_requirements, qty);
                    } else if (only_method.method == "crafting") {
                        const Recipe* recipe = GetRecipe(item_id);
                        if (recipe && !recipe->disciplines.empty()) {
                            next_column.title = "Craft (" + FormatDisciplines(recipe->disciplines) + " " + std::to_string(recipe->rating) + ")";
                            next_column.materials = recipe->ingredients;
                            uint32_t out_cnt = recipe->output_count > 0 ? recipe->output_count : 1;
                            int crafts = (item_count + out_cnt - 1) / out_cnt;
                            if (crafts > 1) {
                                for (auto& mat : next_column.materials) mat.count *= crafts;
                            }
                        } else {
                            next_column.title = "Crafting Materials";
                            next_column.materials = GetRecipeIngredients(item_id);
                        }
                    } else if (only_method.method == "trading_post") {
                        next_column.title = "Trading Post";
                    } else {
                        next_column.title = only_method.display_name.empty() 
                            ? only_method.method : only_method.display_name;
                    }
                    next_column.acquisitions.clear();
                } else if (!next_column.materials.empty()) {
                    next_column.title = "Materials";
                } else {
                    next_column.title = "";
                }
                
                // Truncate columns beyond the next one
                if (s_columns.size() > static_cast<size_t>(column_index + 2)) {
                    s_columns.resize(column_index + 2);
                }
            }
        }
    }
    
    void DataManager::SetSelectedAcquisition(int column_index, int acquisition_index) {
        if (column_index >= 0 && column_index < s_columns.size()) {
            s_columns[column_index].selected_acquisition_index = acquisition_index;
        }
    }
    
    void DataManager::HandleAcquisitionMethodSelection(int column_index, int acquisition_index) {
        if (column_index < 0 || column_index >= static_cast<int>(s_columns.size())) {
            return;
        }
        
        // Set the selection first
        SetSelectedAcquisition(column_index, acquisition_index);
        
        // Get the selected acquisition method
        if (acquisition_index < 0 || acquisition_index >= s_columns[column_index].acquisitions.size()) {
            return;
        }
        
        // Copy the acquisition method - must not hold a reference across s_columns.resize()
        const AcquisitionMethod acq = s_columns[column_index].acquisitions[acquisition_index];
        
        // Ensure the next column exists before accessing it
        if (s_columns.size() <= static_cast<size_t>(column_index + 1)) {
            s_columns.resize(column_index + 2);
            s_columns[column_index + 1].selected_index = -1;
            s_columns[column_index + 1].selected_acquisition_index = -1;
            s_columns[column_index + 1].selected_material_index = -1;
        }
        
        ColumnData& next_column = s_columns[column_index + 1];
        
        // Clear existing data
        next_column.acquisitions.clear();
        next_column.materials.clear();
        next_column.selected_acquisition_index = -1;
        next_column.selected_material_index = -1;
        
        // Populate next column based on acquisition method
        if (acq.method == "vendor") {
            next_column.title = "Vendor - " + acq.vendor_name;
            // Add purchase requirements as materials, multiplied by required quantity
            int qty = s_columns[column_index].source_item_count;
            BuildVendorCostMaterials(next_column.materials, acq.purchase_requirements, qty);
        } else if (acq.method == "mystic_forge") {
            // Use source_item_id from the acquisition column to find the recipe
            uint32_t src_id = s_columns[column_index].source_item_id;
            int qty = s_columns[column_index].source_item_count;
            if (src_id > 0) {
                const Recipe* src_recipe = GetRecipe(src_id);
                if (src_recipe && !src_recipe->ingredients.empty()) {
                    next_column.materials = src_recipe->ingredients;
                    // Mystic Clover is gambled at ~33% success rate
                    if (src_id == 19675 && qty > 1) {
                        int expected_attempts = qty * 3; // ~33% chance per attempt
                        next_column.title = "Mystic Forge (~" + std::to_string(expected_attempts) + " attempts)";
                        for (auto& mat : next_column.materials) {
                            mat.count *= expected_attempts;
                        }
                    } else {
                        next_column.title = "Mystic Forge";
                        uint32_t out_cnt = src_recipe->output_count > 0 ? src_recipe->output_count : 1;
                        int crafts = (qty + out_cnt - 1) / out_cnt;
                        if (crafts > 1) {
                            for (auto& mat : next_column.materials) mat.count *= crafts;
                        }
                    }
                }
            } else {
                next_column.title = "Mystic Forge";
            }
        } else if (acq.method == "crafting") {
            // Use source_item_id from the acquisition column to find the recipe
            uint32_t src_id = s_columns[column_index].source_item_id;
            int qty = s_columns[column_index].source_item_count;
            if (src_id > 0) {
                const Recipe* src_recipe = GetRecipe(src_id);
                if (src_recipe) {
                    if (!src_recipe->disciplines.empty()) {
                        next_column.title = "Craft (" + FormatDisciplines(src_recipe->disciplines) + " " + std::to_string(src_recipe->rating) + ")";
                    } else {
                        next_column.title = "Crafting Materials";
                    }
                    next_column.materials = src_recipe->ingredients;
                    uint32_t out_cnt = src_recipe->output_count > 0 ? src_recipe->output_count : 1;
                    int crafts = (qty + out_cnt - 1) / out_cnt;
                    if (crafts > 1) {
                        for (auto& mat : next_column.materials) mat.count *= crafts;
                    }
                } else {
                    next_column.title = "Crafting Materials";
                    next_column.materials = GetRecipeIngredients(src_id);
                }
            } else {
                next_column.title = "Crafting Materials";
            }
        } else {
            next_column.title = acq.description;
            // Generic method - show details
            for (const std::string& detail : acq.details) {
                RecipeIngredient material;
                material.item_id = 0;
                material.count = 1;
                material.name = detail;
                next_column.materials.push_back(material);
            }
        }
        
        // Truncate columns beyond the next one
        if (s_columns.size() > static_cast<size_t>(column_index + 2)) {
            s_columns.resize(column_index + 2);
        }
    }
    
    uint32_t DataManager::GetParentItemIdFromAcquisitionColumn(int acquisition_column_index) {
        // Trace back from acquisition column to find the original item
        // acquisition_column_index is the column showing acquisition methods (e.g., column 3)
        // We need to find the item in the previous column that led to these acquisition methods
        
        // Look at the previous column which should have the selected item
        int item_column_index = acquisition_column_index - 1;
        if (item_column_index >= 0 && item_column_index < s_columns.size()) {
            const ColumnData& item_column = s_columns[item_column_index];
            if (item_column.selected_index >= 0 && 
                item_column.selected_index < item_column.items.size()) {
                return item_column.items[item_column.selected_index].id;
            }
        }
        
        return 0;
    }
    
    uint32_t DataManager::GetParentItemId(int column_index) {
        // Helper function to get the item ID from the parent column
        if (column_index >= 0 && column_index < s_columns.size()) {
            const ColumnData& column = s_columns[column_index];
            if (column.selected_index >= 0 && column.selected_index < column.items.size()) {
                return column.items[column.selected_index].id;
            }
        }
        return 0;
    }
    
    void DataManager::SetSelectedMaterial(int column_index, int material_index) {
        if (column_index >= 0 && column_index < s_columns.size()) {
            s_columns[column_index].selected_material_index = material_index;
        }
    }
    
    const std::vector<ColumnData>& DataManager::GetColumns() {
        return s_columns;
    }
    
    void DataManager::InitializeColumns() {
        s_columns.clear();
        
        // Create first column for legendary items
        ColumnData legendaryColumn;
        legendaryColumn.title = "Legendary Items";
        legendaryColumn.selected_index = -1;
        legendaryColumn.selected_acquisition_index = -1;
        legendaryColumn.selected_material_index = -1;
        
        // Populate with all legendary items
        legendaryColumn.items = s_legendaries;
        
        s_columns.push_back(legendaryColumn);
    }
    
    void DataManager::SetSessionScrollState(float scroll_x, float col0_scroll_y, const std::vector<float>& col_scroll_y) {
        s_session_scroll_x = scroll_x;
        s_session_col0_scroll_y = col0_scroll_y;
        s_session_col_scroll_y = col_scroll_y;
    }

    void DataManager::GetSessionScrollState(float& scroll_x, float& col0_scroll_y, std::vector<float>& col_scroll_y) {
        scroll_x = s_session_scroll_x;
        col0_scroll_y = s_session_col0_scroll_y;
        col_scroll_y = s_session_col_scroll_y;
    }

    void DataManager::SaveSession() {
        try {
            std::string dllDir = GetDllDirectory();
            if (dllDir.empty()) return;
            std::replace(dllDir.begin(), dllDir.end(), '\\', '/');
            std::string path = dllDir + "/CraftyLegend/session.json";

            json session;
            session["legendary_id"] = 0;
            session["path"] = json::array();

            if (!s_columns.empty() && s_columns[0].selected_index >= 0 &&
                s_columns[0].selected_index < static_cast<int>(s_columns[0].items.size())) {
                session["legendary_id"] = s_columns[0].items[s_columns[0].selected_index].id;

                for (size_t i = 1; i < s_columns.size(); ++i) {
                    const auto& col = s_columns[i];
                    if (col.source_item_id == 0 || col.title.empty()) break;
                    json step;
                    step["item_id"] = col.source_item_id;
                    step["count"] = col.source_item_count;
                    step["selected_material"] = col.selected_material_index;
                    step["selected_acquisition"] = col.selected_acquisition_index;
                    session["path"].push_back(step);
                }
            }

            // Save scroll positions
            session["scroll_x"] = s_session_scroll_x;
            session["col0_scroll_y"] = s_session_col0_scroll_y;
            json scrollArr = json::array();
            for (float sy : s_session_col_scroll_y) scrollArr.push_back(sy);
            session["col_scroll_y"] = scrollArr;

            std::ofstream f(path);
            if (f.is_open()) {
                f << session.dump(2);
            }
        } catch (...) {}
    }

    void DataManager::RestoreSession() {
        try {
            std::string dllDir = GetDllDirectory();
            if (dllDir.empty()) return;
            std::replace(dllDir.begin(), dllDir.end(), '\\', '/');
            std::string path = dllDir + "/CraftyLegend/session.json";

            std::ifstream f(path);
            if (!f.is_open()) return;

            json session = json::parse(f);
            uint32_t legendary_id = session.value("legendary_id", (uint32_t)0);
            if (legendary_id == 0) return;

            UpdateColumn(0, legendary_id);

            auto& steps = session["path"];
            for (size_t i = 0; i < steps.size(); ++i) {
                int selected_mat = steps[i].value("selected_material", -1);
                int selected_acq = steps[i].value("selected_acquisition", -1);
                int count = steps[i].value("count", 1);

                int col_index = static_cast<int>(i + 1);
                if (col_index >= static_cast<int>(s_columns.size())) break;

                auto& col = s_columns[col_index];

                // Restore material selection
                if (selected_mat >= 0 && selected_mat < static_cast<int>(col.materials.size())) {
                    col.selected_material_index = selected_mat;
                    uint32_t mat_id = col.materials[selected_mat].item_id;
                    if (mat_id != 0) {
                        UpdateColumn(col_index, mat_id, count);
                    }
                }
                // Restore acquisition selection
                else if (selected_acq >= 0 && selected_acq < static_cast<int>(col.acquisitions.size())) {
                    HandleAcquisitionMethodSelection(col_index, selected_acq);
                }
            }

            // Restore scroll positions
            s_session_scroll_x = session.value("scroll_x", 0.0f);
            s_session_col0_scroll_y = session.value("col0_scroll_y", 0.0f);
            s_session_col_scroll_y.clear();
            if (session.contains("col_scroll_y") && session["col_scroll_y"].is_array()) {
                for (const auto& v : session["col_scroll_y"]) {
                    s_session_col_scroll_y.push_back(v.get<float>());
                }
            }
        } catch (...) {}
    }

    // Helper: build a Prerequisite with all fields
    static Prerequisite MakePrereq(PrereqCategory cat, const std::string& name,
                                    const std::string& desc, uint32_t src_item,
                                    int mastery = -1, int mastery_lvl = -1, int achiev = -1) {
        Prerequisite p;
        p.category = cat;
        p.name = name;
        p.description = desc;
        p.source_item_id = src_item;
        p.mastery_id = mastery;
        p.mastery_level = mastery_lvl;
        p.achievement_id = achiev;
        p.completed = false;
        return p;
    }

    // Helper: get prerequisites for a specific item ID (not recursive)
    static void GetItemPrereqs(uint32_t item_id, std::vector<Prerequisite>& out) {
        using PC = PrereqCategory;
        // GW2 API mastery track IDs: 8=Gliding, 2=Itzel, 1=Exalted, 3=Nuhoch
        // GW2 API PoF mount mastery IDs: 14=Raptor, 17=Springer, 15=Skimmer, 18=Jackal
        switch (item_id) {
            // --- Map Completion ---
            case 19677: // Gift of Exploration
                out.push_back(MakePrereq(PC::MapCompletion, "World Completion",
                    "Complete all maps in Central Tyria (requires 100% map completion on both continents)",
                    item_id, -1, -1, 137)); // achievement 137 = Been There Done That
                break;
            // --- WvW ---
            case 19678: // Gift of Battle
                out.push_back(MakePrereq(PC::WvW, "Gift of Battle Reward Track",
                    "Complete the Gift of Battle WvW reward track", item_id));
                break;
            // WvW Ascended Precursors - require exotic skin from Triumphant Armor Reward Track
            case 81330: case 81333: case 81304: case 81349: case 81418: case 81336: // Heavy
            case 81315: case 81446: case 81434: case 81489: case 81386: case 81420: // Medium
            case 81476: case 81285: case 81338: case 81428: case 81279: case 81500: // Light
                out.push_back(MakePrereq(PC::WvW, "Triumphant Armor Reward Track",
                    "Complete the Triumphant Armor Reward Track in WvW to unlock the exotic skin (Tier 8, 5th reward, 40th of 40)",
                    item_id));
                break;
            // --- HoT Masteries ---
            case 73469: // Gift of the Itzel
                out.push_back(MakePrereq(PC::Mastery, "Itzel Lore: Adrenal Mushrooms",
                    "Requires Adrenal Mushrooms mastery (Itzel Lore track, Heart of Thorns)",
                    item_id, 2, 5)); // track 2 (Itzel Lore), level 5 (Adrenal Mushrooms, 0-indexed)
                break;
            case 76767: // Gift of the Nuhoch
                out.push_back(MakePrereq(PC::Mastery, "Nuhoch Training: Nuhoch Alchemy",
                    "Requires Nuhoch Alchemy mastery (Nuhoch Training track, Heart of Thorns)",
                    item_id, 3, 5)); // track 3 (Nuhoch Lore), level 5 (Nuhoch Alchemy, 0-indexed)
                break;
            case 76636: // Gift of the Exalted
                out.push_back(MakePrereq(PC::Mastery, "Exalted Lore: Exalted Gathering",
                    "Requires Exalted Gathering mastery (Exalted Lore track, Heart of Thorns)",
                    item_id, 1, 4)); // track 1 (Exalted Lore), level 4 (Exalted Gathering)
                break;
            case 71311: // Gift of Gliding
                out.push_back(MakePrereq(PC::Mastery, "Gliding: Ley Line Gliding",
                    "Requires Ley Line Gliding mastery (Gliding track, Heart of Thorns)",
                    item_id, 8, 5)); // track 8 (Gliding), level 5 (Ley Line Gliding)
                break;
            // --- PoF Masteries ---
            case 86330: // Gift of the Rider
                out.push_back(MakePrereq(PC::Mastery, "Mount Masteries",
                    "Requires Raptor, Springer, Skimmer, and Jackal mount masteries (Path of Fire)",
                    item_id)); // checked specially below
                break;
            // --- HoT Map Completion Gifts ---
            case 70797: // Gift of the Fleet
                out.push_back(MakePrereq(PC::MapCompletion, "Verdant Brink Map Completion",
                    "Complete all hearts, waypoints, vistas, hero points, and POIs in Verdant Brink", item_id));
                break;
            case 71943: // Gift of Tarir
                out.push_back(MakePrereq(PC::MapCompletion, "Auric Basin Map Completion",
                    "Complete all hearts, waypoints, vistas, hero points, and POIs in Auric Basin", item_id));
                break;
            case 74528: // Gift of the Chak
                out.push_back(MakePrereq(PC::MapCompletion, "Tangled Depths Map Completion",
                    "Complete all hearts, waypoints, vistas, hero points, and POIs in Tangled Depths", item_id));
                break;
            case 70698: // Gift of the Jungle
                out.push_back(MakePrereq(PC::MapCompletion, "Dragon's Stand Map Completion",
                    "Complete all hearts, waypoints, vistas, hero points, and POIs in Dragon's Stand", item_id));
                break;
            // --- PoF Map Currencies ---
            case 86010: // Gift of the Desolation
                out.push_back(MakePrereq(PC::MapCurrency, "The Desolation: Trade Contracts",
                    "Earn 250 Trade Contracts from The Desolation heart vendors", item_id));
                break;
            case 86018: // Gift of the Highlands
                out.push_back(MakePrereq(PC::MapCurrency, "Desert Highlands: Trade Contracts",
                    "Earn 250 Trade Contracts from Desert Highlands heart vendors", item_id));
                break;
            case 85961: // Gift of the Oasis
                out.push_back(MakePrereq(PC::MapCurrency, "Crystal Oasis: Trade Contracts",
                    "Earn 250 Trade Contracts from Crystal Oasis heart vendors", item_id));
                break;
            case 86241: // Gift of the Riverlands
                out.push_back(MakePrereq(PC::MapCurrency, "Elon Riverlands: Trade Contracts",
                    "Earn 250 Trade Contracts from Elon Riverlands heart vendors", item_id));
                break;
            // --- EoD Map Completion ---
            case 96993: // Gift of Seitung Province
                out.push_back(MakePrereq(PC::MapCompletion, "Seitung Province Map Completion",
                    "Complete all map objectives in Seitung Province", item_id));
                break;
            case 95621: // Gift of New Kaineng City
                out.push_back(MakePrereq(PC::MapCompletion, "New Kaineng City Map Completion",
                    "Complete all map objectives in New Kaineng City", item_id));
                break;
            case 97232: // Gift of the Echovald Forest
                out.push_back(MakePrereq(PC::MapCompletion, "The Echovald Wilds Map Completion",
                    "Complete all map objectives in The Echovald Wilds", item_id));
                break;
            case 96083: // Gift of Dragon's End
                out.push_back(MakePrereq(PC::MapCompletion, "Dragon's End Map Completion",
                    "Complete all map objectives in Dragon's End", item_id));
                break;
            // --- SotO Map Completion ---
            case 100544: // Gift of Skywatch Archipelago
                out.push_back(MakePrereq(PC::MapCompletion, "Skywatch Archipelago Map Completion",
                    "Complete all map objectives in Skywatch Archipelago", item_id));
                break;
            case 100798: // Gift of Amnytas
                out.push_back(MakePrereq(PC::MapCompletion, "Amnytas Map Completion",
                    "Complete all map objectives in Amnytas", item_id));
                break;
            case 101595: // Gift of Inner Nayos
                out.push_back(MakePrereq(PC::MapCompletion, "Inner Nayos Map Completion",
                    "Complete all map objectives in Inner Nayos", item_id));
                break;
            // --- JW Map Completion ---
            case 102929: // Gift of Lowland Shore
                out.push_back(MakePrereq(PC::MapCompletion, "Lowland Shore Map Completion",
                    "Complete all map objectives in Lowland Shore", item_id));
                break;
            case 102958: // Gift of Janthir Syntri
                out.push_back(MakePrereq(PC::MapCompletion, "Janthir Syntri Map Completion",
                    "Complete all map objectives in Janthir Syntri", item_id));
                break;
            case 103236: // Gift of the Ursus
                out.push_back(MakePrereq(PC::MapCompletion, "Ursus Oblivium Map Completion",
                    "Complete all map objectives in Ursus Oblivium", item_id));
                break;
            // --- VoE Map Completion ---
            case 104313: // Gift of Mistburned Barrens
                out.push_back(MakePrereq(PC::MapCompletion, "Mistburned Barrens Map Completion",
                    "Complete all map objectives in Mistburned Barrens", item_id));
                break;
            case 104896: // Gift of Bava Nisos
                out.push_back(MakePrereq(PC::MapCompletion, "Bava Nisos Map Completion",
                    "Complete all map objectives in Bava Nisos", item_id));
                break;
            // --- Salvage ---
            case 71994: // Ball of Dark Energy
                out.push_back(MakePrereq(PC::Salvage, "Salvage Ascended Equipment",
                    "Obtained by salvaging ascended weapons or armor with a Black Lion Salvage Kit", item_id));
                break;
            // --- Gen2 Collection/Achievement items ---
            case 71173: // Gift of the Raven Spirit
                out.push_back(MakePrereq(PC::Achievement, "Nevermore I: Ravenswood Branch",
                    "Complete the Nevermore I collection achievement", item_id, -1, -1, 2528));
                break;
            case 72083: // Gift of the Cosmos
                out.push_back(MakePrereq(PC::Achievement, "Astralaria I: The Device",
                    "Complete the Astralaria I collection achievement", item_id, -1, -1, 2571));
                break;
            case 76442: // Gift of the Catalyst
                out.push_back(MakePrereq(PC::Achievement, "HOPE I: Research",
                    "Complete the HOPE I collection achievement", item_id, -1, -1, 2450));
                break;
            case 78344: // Gift of Family
                out.push_back(MakePrereq(PC::Achievement, "Chuka and Champawat I: The Hunt",
                    "Complete the Chuka and Champawat I collection achievement", item_id, -1, -1, 2990));
                break;
            case 106986: // Gift of the Hylek
                out.push_back(MakePrereq(PC::Achievement, "Radiance of the Sun God",
                    "Complete the Radiance of the Sun God collection achievement (Legendary Trinkets)", item_id, -1, -1, 9183));
                break;
            case 105848: // Concentrated Chromatic Sap
                out.push_back(MakePrereq(PC::Achievement, "Shipwreck Strand Mastery",
                    "Complete 36 map achievements throughout Shipwreck Strand (required to purchase)", item_id));
                break;
            case 105933: // Patron of the Magical Arts Plaque
                out.push_back(MakePrereq(PC::Achievement, "Starlit Weald Mastery",
                    "Complete 36 map achievements throughout Starlit Weald (required to purchase)", item_id));
                break;
            case 106370: // Survivor's Enchanted Compass
                out.push_back(MakePrereq(PC::Achievement, "Shipwreck Strand Mastery",
                    "Complete 36 map achievements throughout Shipwreck Strand", item_id));
                break;
            case 106627: // Seer Wreath of Service
                out.push_back(MakePrereq(PC::Achievement, "Starlit Weald Mastery",
                    "Complete 36 map achievements throughout Starlit Weald", item_id));
                break;
            case 106467: // Gift of Shipwreck Strand Exploration
                out.push_back(MakePrereq(PC::MapCompletion, "Shipwreck Strand",
                    "Complete map exploration of Shipwreck Strand", item_id));
                break;
            case 106672: // Gift of Starlit Weald Exploration
                out.push_back(MakePrereq(PC::MapCompletion, "Starlit Weald",
                    "Complete map exploration of Starlit Weald", item_id));
                break;
            // --- Ad Infinitum (Fractal backpiece) ---
            case 37070: // Gift of Ascension
                out.push_back(MakePrereq(PC::Achievement, "Pact Reformer",
                    "Complete the Pact Reformer achievement (rescue 12 Pact soldiers in Fractals)", item_id, -1, -1, 2319));
                break;
            case 74377: // Gift of Infinity
                out.push_back(MakePrereq(PC::Achievement, "Legendary Backpack: Ad Infinitum",
                    "Complete the Ad Infinitum legendary backpack collection", item_id, -1, -1, 2295));
                break;
            // --- The Ascension (PvP backpiece) ---
            case 77493: // The Thrill of Battle
                out.push_back(MakePrereq(PC::Achievement, "Path of the Ascension I: The Thrill of Battle",
                    "Complete the Path of the Ascension I collection (PvP)", item_id, -1, -1, 2738));
                break;
            case 77497: // Tapestry of Sacrifice
                out.push_back(MakePrereq(PC::Achievement, "Path of the Ascension II: Tapestry of Sacrifice",
                    "Complete the Path of the Ascension II collection (PvP)", item_id, -1, -1, 2752));
                break;
            case 77538: // Monument of Legends
                out.push_back(MakePrereq(PC::Achievement, "Path of the Ascension III: Monument of Legends",
                    "Complete the Path of the Ascension III collection (PvP)", item_id, -1, -1, 2725));
                break;
            case 77548: // Hymn of Glory
                out.push_back(MakePrereq(PC::Achievement, "Path of the Ascension IV: Hymn of Glory",
                    "Complete the Path of the Ascension IV collection (PvP)", item_id, -1, -1, 2715));
                break;
            // --- Aurora (trinket) ---
            case 81729: // Spark of Sentience
                out.push_back(MakePrereq(PC::Achievement, "Aurora II: Empowering",
                    "Complete the Aurora II: Empowering collection (LW Season 3)", item_id, -1, -1, 3489));
                break;
            case 82008: // Gift of Valor
                out.push_back(MakePrereq(PC::Achievement, "Aurora: Awakening",
                    "Complete the Aurora: Awakening collection (LW Season 3)", item_id, -1, -1, 3522));
                break;
            // --- Coalescence (ring) ---
            case 86104: // Hateful Sworl
                out.push_back(MakePrereq(PC::Achievement, "Coalescence I: Unbridled",
                    "Complete the Coalescence I: Unbridled collection (Raid)", item_id, -1, -1, 4035));
                break;
            case 88909: // Gift of Complex Emotions
                out.push_back(MakePrereq(PC::Achievement, "Coalescence II: The Gift",
                    "Complete the Coalescence II: The Gift collection (Raid)", item_id, -1, -1, 4412));
                break;
            case 91193: // Gift of Patience
                out.push_back(MakePrereq(PC::Achievement, "Coalescence III: Culmination",
                    "Complete the Coalescence III: Culmination collection (Raid)", item_id, -1, -1, 4805));
                break;
            // --- Vision (trinket) ---
            case 91010: // Shattered Gift of Prescience
                out.push_back(MakePrereq(PC::Achievement, "Vision I: Awakening",
                    "Complete the Vision I: Awakening collection (LW Season 4)", item_id, -1, -1, 4762));
                break;
            case 91035: // Glimpse
                out.push_back(MakePrereq(PC::Achievement, "Vision II: Farsight",
                    "Complete the Vision II: Farsight collection (LW Season 4)", item_id, -1, -1, 4771));
                break;
            // --- Klobjarne Geirr (spear) ---
            case 103766: // Memory of the Bearkin's Hunts
                out.push_back(MakePrereq(PC::Achievement, "Klobjarne Geirr: Janthir Spear Kills",
                    "Complete the Janthir Spear Kills achievement (kill enemies with a spear in Janthir Wilds)", item_id, -1, -1, 8449));
                break;
            case 103833: // Memory of the Bearkin's Victories
                out.push_back(MakePrereq(PC::Achievement, "Klobjarne Geirr: Raid or Convergence Victories",
                    "Complete Raid or Convergence victories for Klobjarne Geirr", item_id, -1, -1, 8442));
                break;
            // --- Achievement reward legendaries ---
            case 95380: // Prismatic Champion's Regalia
                out.push_back(MakePrereq(PC::Achievement, "Seasons of the Dragons",
                    "Complete the Seasons of the Dragons achievement. This item cannot be crafted — it is awarded for free upon completion.",
                    item_id, -1, -1, 5790));
                break;
            default:
                break;
        }
        // Handle collection items generically
        const Item* item = DataManager::GetItem(item_id);
        if (item) {
            for (const auto& acq : item->acquisition) {
                if (acq == "collection") {
                    bool already_added = false;
                    for (const auto& p : out) {
                        if (p.source_item_id == item_id && (p.category == PC::Achievement || p.category == PC::Collection)) {
                            already_added = true;
                            break;
                        }
                    }
                    if (!already_added) {
                        // Walk up the recipe tree: collection item -> ... -> precursor -> legendary
                        uint32_t parent_id = 0;
                        for (const auto& [rid, recipe] : DataManager::GetRecipes()) {
                            for (const auto& ing : recipe.ingredients) {
                                if (ing.item_id == item_id) {
                                    parent_id = recipe.output_item_id;
                                    break;
                                }
                            }
                            if (parent_id != 0) break;
                        }

                        // Walk up to find the precursor (last item before legendary) and legendary name
                        std::string precursor_name;
                        uint32_t precursor_id = 0;
                        std::string legendary_name;
                        uint32_t walk_id = parent_id;
                        for (int depth = 0; depth < 10 && walk_id != 0; depth++) {
                            bool is_legendary = false;
                            for (const auto& leg : DataManager::GetLegendaries()) {
                                if (leg.id == walk_id) {
                                    legendary_name = leg.name;
                                    is_legendary = true;
                                    break;
                                }
                            }
                            if (is_legendary) break;

                            precursor_name = DataManager::GetItemName(walk_id);
                            precursor_id = walk_id;

                            uint32_t next_id = 0;
                            for (const auto& [rid, recipe] : DataManager::GetRecipes()) {
                                for (const auto& ing : recipe.ingredients) {
                                    if (ing.item_id == walk_id) {
                                        next_id = recipe.output_item_id;
                                        break;
                                    }
                                }
                                if (next_id != 0) break;
                            }
                            walk_id = next_id;
                        }

                        // Check if precursor is tradeable (Gen1) or account-bound (Gen2)
                        bool precursor_tradeable = false;
                        if (precursor_id != 0) {
                            const Item* prec = DataManager::GetItem(precursor_id);
                            if (prec) {
                                precursor_tradeable = (prec->binding.empty() || prec->binding == "none");
                            }
                        }

                        // Map collection names to GW2 API achievement IDs
                        static const std::unordered_map<std::string, int> collection_achievement_ids = {
                            // Gen1 Tier III precursor collections
                            {"Twilight III: Dusk", 2183},
                            {"Sunrise III: Dawn", 2630},
                            {"Bolt III: Zap", 2480},
                            {"The Bifrost III: The Legend", 2187},
                            {"The Dreamer III: The Lover", 2428},
                            {"Frostfang III: Tooth of Frostfang", 2393},
                            {"Incinerator III: Spark", 2502},
                            {"Kudzu III: Leaf of Kudzu", 2383},
                            {"Meteorlogicus III: Storm", 2449},
                            {"Howler III: Howl", 2260},
                            {"Quip III: Chaos Gun", 2524},
                            {"Rodgort III: Rodgort's Flame", 2388},
                            {"The Flameseeker Prophecies III: The Chosen", 2536},
                            {"The Juggernaut III: The Colossus", 2468},
                            {"The Minstrel III: The Bard", 2242},
                            {"The Moot III: The Energizer", 2374},
                            {"The Predator III: The Hunter", 2280},
                            {"Frenzy III: Rage", 2409},
                            {"Kamohoali'i Kotaki III: Carcharias", 2535},
                            {"Kraitkin III: Venom", 2296},
                            // Gen2 precursor collections (final tier)
                            {"Nevermore Collections", 2550},
                            {"Astralaria Collections", 2268},
                            {"HOPE Collections", 2250},
                            {"Chuka and Champawat Collections", 2951},
                        };

                        std::string prereq_name;
                        std::string desc;
                        std::string target = precursor_name.empty() ? "the precursor" : precursor_name;
                        if (!legendary_name.empty() && !precursor_name.empty()) {
                            if (precursor_tradeable) {
                                // Gen1: collection name follows "[Legendary] III: [Precursor]"
                                prereq_name = legendary_name + " III: " + precursor_name;
                                desc = "Complete this collection to craft " + target +
                                    ". Only needed if crafting " + target +
                                    " via the collection path instead of purchasing from the Trading Post.";
                            } else {
                                // Gen2: account-bound precursor, collection is required
                                prereq_name = legendary_name + " Collections";
                                desc = "Complete the " + legendary_name +
                                    " precursor crafting collections. Required for crafting " + target + ".";
                            }
                        } else if (!precursor_name.empty()) {
                            prereq_name = target + " Collection";
                            desc = "Complete this collection to craft " + target + ".";
                        } else {
                            prereq_name = "Precursor Collection";
                            desc = "Part of a precursor crafting collection.";
                        }

                        // Look up achievement ID for completion tracking
                        int achiev_id = -1;
                        auto ait = collection_achievement_ids.find(prereq_name);
                        if (ait != collection_achievement_ids.end()) {
                            achiev_id = ait->second;
                        }
                        out.push_back(MakePrereq(PC::Collection, prereq_name, desc, item_id, -1, -1, achiev_id));
                    }
                }
            }
        }
    }

    // Check completion of a single prerequisite against account data
    static void CheckPrereqCompletion(Prerequisite& p) {
        if (!GW2API::HasAccountData()) return;

        switch (p.category) {
            case PrereqCategory::MapCompletion:
                if (p.source_item_id != 0) {
                    // Check if player owns the map gift item (e.g. EoD map gifts)
                    p.completed = GW2API::GetOwnedCount(p.source_item_id) > 0;
                } else {
                    // Generic Tyria map completion (no specific item to check)
                    p.completed = GW2API::HasMapCompletion();
                }
                break;
            case PrereqCategory::WvW:
                p.completed = GW2API::GetOwnedCount(p.source_item_id) > 0;
                break;
            case PrereqCategory::Mastery:
                if (p.mastery_id >= 0 && p.mastery_level >= 0) {
                    p.completed = GW2API::GetMasteryLevel(p.mastery_id) >= p.mastery_level;
                }
                // Gift of the Rider: need all 4 PoF mount tracks
                if (p.source_item_id == 86330) {
                    p.completed = GW2API::GetMasteryLevel(14) >= 0 &&
                                  GW2API::GetMasteryLevel(17) >= 0 &&
                                  GW2API::GetMasteryLevel(15) >= 0 &&
                                  GW2API::GetMasteryLevel(18) >= 0;
                }
                // Also check if the gift item itself is already owned
                if (!p.completed && GW2API::GetOwnedCount(p.source_item_id) > 0) {
                    p.completed = true;
                }
                break;
            case PrereqCategory::Achievement:
                if (p.achievement_id >= 0) {
                    p.completed = GW2API::IsAchievementDone(p.achievement_id);
                }
                if (!p.completed && GW2API::GetOwnedCount(p.source_item_id) > 0) {
                    p.completed = true;
                }
                break;
            case PrereqCategory::Collection:
                if (p.achievement_id >= 0) {
                    p.completed = GW2API::IsAchievementDone(p.achievement_id);
                }
                if (!p.completed) {
                    p.completed = GW2API::GetOwnedCount(p.source_item_id) > 0;
                }
                break;
            case PrereqCategory::MapCurrency:
                p.completed = GW2API::GetOwnedCount(p.source_item_id) > 0;
                break;
            case PrereqCategory::Salvage:
                p.completed = GW2API::GetOwnedCount(p.source_item_id) > 0;
                break;
            default:
                break;
        }
    }

    // Recursive tree walker
    static void WalkCraftingTree(uint32_t item_id, std::vector<Prerequisite>& prereqs,
                                  std::unordered_set<uint32_t>& visited) {
        if (item_id == 0 || visited.count(item_id)) return;
        visited.insert(item_id);

        // Collect prerequisites for this item
        GetItemPrereqs(item_id, prereqs);

        // Walk recipe ingredients
        const Recipe* recipe = DataManager::GetRecipe(item_id);
        if (recipe) {
            for (const auto& ing : recipe->ingredients) {
                WalkCraftingTree(ing.item_id, prereqs, visited);
            }
        } else {
            // No recipe: walk vendor purchase requirement items
            const auto& acqs = DataManager::GetAcquisitionMethods(item_id);
            for (const auto& acq : acqs) {
                for (const auto& req : acq.purchase_requirements) {
                    if (req.first == "Coin") continue;
                    // Resolve name to item ID
                    uint32_t sub_id = 0;
                    for (const auto& [id, it] : DataManager::GetItems()) {
                        if (it.name == req.first) { sub_id = id; break; }
                    }
                    if (sub_id != 0) {
                        WalkCraftingTree(sub_id, prereqs, visited);
                    }
                }
            }
        }
    }

    std::vector<Prerequisite> DataManager::GetPrerequisites(uint32_t legendary_id) {
        std::vector<Prerequisite> prereqs;
        std::unordered_set<uint32_t> visited;
        WalkCraftingTree(legendary_id, prereqs, visited);

        // Append "Needed for: <item name>" context to every prerequisite
        // (skip Collection prereqs — their descriptions already explain the purpose)
        for (auto& p : prereqs) {
            if (p.source_item_id != 0 && p.source_item_id != legendary_id &&
                p.category != PrereqCategory::Collection) {
                std::string src_name = GetItemName(p.source_item_id);
                if (!src_name.empty()) {
                    p.description += "\n\nNeeded for: " + src_name;
                }
            }
        }

        // Deduplicate by name
        std::vector<Prerequisite> unique;
        std::unordered_set<std::string> seen_names;
        for (auto& p : prereqs) {
            if (seen_names.insert(p.name).second) {
                unique.push_back(std::move(p));
            }
        }

        // Sort by category then name
        std::sort(unique.begin(), unique.end(), [](const Prerequisite& a, const Prerequisite& b) {
            if (a.category != b.category) return static_cast<int>(a.category) < static_cast<int>(b.category);
            return a.name < b.name;
        });

        // Check completion against account data
        for (auto& p : unique) {
            CheckPrereqCompletion(p);
        }

        return unique;
    }

    std::vector<uint32_t> DataManager::GetAllTradeableItemIds() {
        std::unordered_set<uint32_t> visited;
        std::unordered_set<uint32_t> tradeable;

        // Recursive helper to walk a crafting tree
        std::function<void(uint32_t)> walkTree = [&](uint32_t item_id) {
            if (item_id == 0 || visited.count(item_id)) return;
            visited.insert(item_id);

            // Check if this item is tradeable (not bound)
            const Item* item = GetItem(item_id);
            if (item && (item->binding.empty() || item->binding == "none")) {
                tradeable.insert(item_id);
            }

            // Walk recipe ingredients
            const Recipe* recipe = GetRecipe(item_id);
            if (recipe) {
                for (const auto& ing : recipe->ingredients) {
                    walkTree(ing.item_id);
                }
            }
        };

        // Walk all legendary crafting trees
        for (const auto& leg : s_legendaries) {
            walkTree(leg.id);
        }

        return std::vector<uint32_t>(tradeable.begin(), tradeable.end());
    }

}

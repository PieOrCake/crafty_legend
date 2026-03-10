#pragma once

#include <vector>
#include <unordered_map>
#include <string>
#include <nlohmann/json.hpp>
#include <cstdint>
#include <memory>

using json = nlohmann::json;

namespace CraftyLegend {
    
    // Item data structure
    struct Item {
        uint32_t id;
        std::string name;
        std::string icon;
        std::string description;
        std::string type;
        std::string binding; // "none", "account", "soul"
        std::vector<std::string> acquisition;
    };
    
    // Recipe ingredient structure
    struct RecipeIngredient {
        uint32_t item_id;
        uint32_t count;
        std::string name;
    };
    
    // Recipe structure
    struct Recipe {
        uint32_t id;
        uint32_t output_item_id;
        uint32_t output_count;
        std::vector<RecipeIngredient> ingredients;
        std::vector<std::string> disciplines;
        uint32_t rating;
        std::string type;
    };
    
    // Currency structure
    struct Currency {
        uint32_t id;
        std::string name;
        std::string description;
    };
    
    // Legendary structure
    struct Legendary {
        uint32_t id;
        std::string name;
        std::string icon;
        std::string description;
        std::string type;
        std::string weapon_type;
        std::string armor_type;
        std::string trinket_type;
        std::string back_type;
        std::string binding;
        std::vector<std::string> acquisition;
        int generation = 1;
    };
    
    // Acquisition method structure for UI display
    struct AcquisitionMethod {
        std::string method;
        std::string display_name; // Formatted name for UI display
        std::string description;
        std::vector<std::string> details;
        std::string cost; // For trading post prices
        
        // Vendor-specific information
        std::string vendor_name;
        std::string vendor_location;
        std::vector<std::pair<std::string, std::string>> purchase_requirements; // {currency, amount}
    };
    
    // Prerequisite categories for the prerequisites panel
    enum class PrereqCategory {
        MapCompletion,
        Mastery,
        WvW,
        Collection,
        Achievement,
        Dungeon,
        Salvage,
        MapCurrency,
        Other
    };
    
    struct Prerequisite {
        PrereqCategory category;
        std::string name;
        std::string description;
        uint32_t source_item_id = 0; // The item that requires this prerequisite
        int mastery_id = -1;         // GW2 API mastery track ID (for Mastery prereqs)
        int mastery_level = -1;      // Required mastery level
        int achievement_id = -1;     // GW2 API achievement ID (for Achievement/Collection prereqs)
        bool completed = false;
    };
    
    // Column data for Miller columns
    struct ColumnData {
        std::string title;
        std::vector<Legendary> items;
        std::vector<AcquisitionMethod> acquisitions;
        std::vector<RecipeIngredient> materials;
        uint32_t source_item_id = 0; // The item this column is about
        int source_item_count = 1; // How many of this item are needed from parent recipe
        int selected_index;
        int selected_acquisition_index;
        int selected_material_index;
    };
    
    // Data manager with JSON loading
    class DataManager {
    public:
        static bool Initialize();
        static void Shutdown();
        
        // Data access
        static const std::vector<Legendary>& GetLegendaries();
        static const std::unordered_map<uint32_t, Item>& GetItems();
        static const std::unordered_map<uint32_t, Recipe>& GetRecipes();
        static const std::vector<Currency>& GetCurrencies();
        static const Currency* GetCurrency(uint32_t id);
        static std::string GetCurrencyName(uint32_t id);
        static size_t GetAcquisitionMethodCount();
        static const std::vector<AcquisitionMethod>& GetAcquisitionMethods(uint32_t item_id);
        static const std::vector<RecipeIngredient>& GetRecipeIngredients(uint32_t item_id);
        static const Recipe* GetRecipe(uint32_t item_id);
        static const Item* GetItem(uint32_t id);
        static std::string GetDebugPath(const std::string& type);
        
        // Name lookups
        static std::string GetItemName(uint32_t id);
        static std::string GetLegendaryName(uint32_t id);
        
        // Column management
        static void InitializeColumns();
        static void ResetColumns();
        static void UpdateColumn(int column_index, uint32_t item_id, int item_count = 1);
        static void SetSelectedAcquisition(int column_index, int acquisition_index);
        static void HandleAcquisitionMethodSelection(int column_index, int acquisition_index);
        static void SetSelectedMaterial(int column_index, int material_index);
        static uint32_t GetParentItemId(int column_index);
        static uint32_t GetParentItemIdFromAcquisitionColumn(int acquisition_column_index);
        static const std::vector<ColumnData>& GetColumns();
        
        // Session persistence
        static void SaveSession();
        static void RestoreSession();
        static void SetSessionScrollState(float scroll_x, float col0_scroll_y, const std::vector<float>& col_scroll_y);
        static void GetSessionScrollState(float& scroll_x, float& col0_scroll_y, std::vector<float>& col_scroll_y);
        
        // Prerequisites
        static std::vector<Prerequisite> GetPrerequisites(uint32_t legendary_id);
        
        // TP Prices - collect all non-bound item IDs across all crafting trees
        static std::vector<uint32_t> GetAllTradeableItemIds();
        
    private:
        // JSON loading
        static bool LoadLegendaries();
        static bool LoadItems();
        static bool LoadRecipes();
        static bool LoadCurrencies();
        
        // Static data members
        static std::vector<Legendary> s_legendaries;
        static std::unordered_map<uint32_t, Item> s_items;
        static std::unordered_map<uint32_t, Recipe> s_recipes;
        static std::vector<Currency> s_currencies;
        static std::unordered_map<uint32_t, std::vector<AcquisitionMethod>> s_acquisition_methods;
        
        // JSON data storage
        static json s_legendaries_json;
        static json s_items_json;
        static json s_recipes_json;
        
        // Debug information
        static std::string s_debug_legendaries_path;
        static std::string s_debug_items_path;
        static std::string s_debug_recipes_path;
        
        // Column data
        static std::vector<ColumnData> s_columns;
        
        // Session scroll state
        static float s_session_scroll_x;
        static float s_session_col0_scroll_y;
        static std::vector<float> s_session_col_scroll_y;
        
        // Helper methods
        static AcquisitionMethod CreateAcquisitionMethod(const std::string& method, const Item* item);
        static std::vector<std::string> ParseAcquisitionArray(const std::vector<std::string>& acquisition);
        static std::string FormatDisciplines(const std::vector<std::string>& disciplines);
    };
    
}

#pragma once

#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <set>
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
        std::string rarity; // "Legendary", "Ascended", "Exotic", "Rare", "Masterwork", "Fine", "Basic", "Junk"
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
        std::string vendor; // for type=="vendor": merchant name (e.g. "Dugan")
    };
    
    // Currency structure
    struct Currency {
        uint32_t id;
        std::string name;
        std::string icon;
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
        // Legendary Armory capacity: how many copies are useful to own. 1 for armour,
        // amulets and back items; 2 for two-handed weapons, rings and accessories;
        // 4 for one-handed weapons; 7 for Legendary Rune; 8 for Legendary Sigil.
        // Sourced from /v2/legendaryarmory by scripts/fetch_max_counts.py.
        int max_count = 1;
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
        // Non-zero only when `title` is a "Craft (WPN 500)" heading; holds the
        // item whose crafting recipe composed it. Lets the UI attach the
        // discipline tooltip to genuine craft headings without pattern-matching
        // the (localized, display-only) title text.
        uint32_t craft_heading_item_id = 0;
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
        static const Legendary* GetLegendaryById(uint32_t id); // nullptr if not a legendary
        static const std::unordered_map<uint32_t, Item>& GetItems();
        static const std::unordered_map<uint32_t, Recipe>& GetRecipes();
        static const std::vector<Currency>& GetCurrencies();
        static const Currency* GetCurrency(uint32_t id);
        static const Currency* GetCurrencyByName(const std::string& name);
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
        static uint32_t ResolveItemIdByName(const std::string& name);
        // Discipline names -> short codes ("Huntsman" -> "HNT"), used in column
        // headers and the tree's acquisition-method heading.
        static std::string FormatDisciplines(const std::vector<std::string>& disciplines);
        // Resolve a vendor purchase-requirement name to an item id, returning 0 for
        // anything the wallet knows (vendors charge the wallet, even for costs that
        // also exist as an inventory item — e.g. Testimony of Castoran Heroics).
        static uint32_t ResolveRequirementItemId(const std::string& name);
        
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
        // Achievement gates for a single item (non-recursive), completion resolved.
        // Used by the UI to mark/tooltip rows whose acquisition is locked behind an achievement.
        static std::vector<Prerequisite> GetItemAchievementGates(uint32_t item_id);
        // Every tier of every precursor collection chain. A prereq only ever names the
        // tier currently in progress, so these must be queried in full or the panel can
        // never work out which tier that is.
        static std::vector<int> GetCollectionChainAchievementIds();
        
        // TP Prices - collect all non-bound item IDs across all crafting trees
        static std::vector<uint32_t> GetAllTradeableItemIds();
        
        // Favourites
        static bool IsFavourite(uint32_t legendary_id);
        static void ToggleFavourite(uint32_t legendary_id);
        static void LoadFavourites();
        static void SaveFavourites();

        // Tree expand-state (persisted alongside session data)
        static bool IsNodeExpanded(const std::string& key);
        static void SetNodeExpanded(const std::string& key, bool expanded);
        // Accordion: mark methodKey active under parentKey, collapsing sibling method keys
        // (any expanded key beginning with parentKey + "#m:") except methodKey.
        static void SetActiveMethod(const std::string& parentKey, const std::string& methodKey);
        static const std::set<std::string>& GetExpandedNodes();
        // Monotonic counter bumped on any expand-state mutation (SetNodeExpanded,
        // SetActiveMethod, session restore). Lets callers memoize work derived from
        // the expanded set without recomputing every frame.
        static uint64_t GetExpandRevision();

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

        // Favourites data
        static std::unordered_set<uint32_t> s_favourites;
        static std::string GetFavouritesPath();

        // Tree expand-state data (persisted in session.json under "tree_expanded")
        static std::set<std::string> s_expandedNodes;

    };
    
}

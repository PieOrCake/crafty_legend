#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <mutex>
#include <nlohmann/json.hpp>

namespace CraftyLegend {

    struct ApiKeyInfo {
        bool valid = false;
        std::string account_name;
        std::string key_name;
        std::vector<std::string> permissions;
        std::string error;
    };

    enum class FetchStatus {
        Idle,
        InProgress,
        Success,
        Error
    };

    class GW2API {
    public:
        // Data path helper
        static std::string GetDataDirectory();

        // API Key management
        static void SetApiKey(const std::string& key);
        static const std::string& GetApiKey();
        static bool LoadApiKey();
        static bool SaveApiKey();

        // Validation (async)
        static void ValidateApiKeyAsync();
        static FetchStatus GetValidationStatus();
        static const ApiKeyInfo& GetApiKeyInfo();

        // Account data fetching (async)
        static void FetchAccountDataAsync();
        static FetchStatus GetFetchStatus();
        static const std::string& GetFetchStatusMessage();

        // Owned item counts
        static int GetOwnedCount(uint32_t item_id);
        static bool HasAccountData();
        static bool LoadAccountData();

        // Wallet
        static int GetWalletAmount(int currency_id);
        static int GetWalletAmountByName(const std::string& currency_name);

        // Legendary Armory
        static bool IsLegendaryUnlocked(uint32_t item_id);

        // Masteries & Achievements
        static int GetMasteryLevel(int mastery_id);
        static bool IsAchievementDone(int achievement_id);
        static bool HasMapCompletion();

        // TP Prices
        static void FetchPricesAsync(const std::vector<uint32_t>& item_ids);
        static FetchStatus GetPriceFetchStatus();
        static const std::string& GetPriceFetchMessage();
        static int GetSellPrice(uint32_t item_id);  // unit price in copper, 0 if not on TP
        static bool HasPriceData();
        static bool LoadPriceData();

    private:
        static std::string s_api_key;
        static ApiKeyInfo s_key_info;
        static FetchStatus s_validation_status;
        static FetchStatus s_fetch_status;
        static std::string s_fetch_message;
        static std::unordered_map<uint32_t, int> s_owned_items;
        static std::unordered_map<int, int> s_wallet;
        static std::unordered_map<int, int> s_masteries;     // mastery_id -> level
        static std::unordered_map<int, bool> s_achievements;  // achievement_id -> done
        static std::unordered_set<uint32_t> s_legendary_armory; // unlocked legendary item IDs
        static bool s_has_account_data;
        static std::unordered_map<uint32_t, int> s_tp_prices;  // item_id -> sell unit_price in copper
        static bool s_has_price_data;
        static FetchStatus s_price_fetch_status;
        static std::string s_price_fetch_message;
        static std::mutex s_mutex;

        // HTTP helper
        static std::string HttpGet(const std::string& url);

        static bool EnsureDataDirectory();

        // Save aggregated data
        static bool SaveAccountData(const std::unordered_map<uint32_t, int>& items,
                                    const std::unordered_map<int, int>& wallet,
                                    const std::unordered_map<int, int>& masteries,
                                    const std::unordered_map<int, bool>& achievements,
                                    const std::unordered_set<uint32_t>& armory);
        static bool SavePriceData(const std::unordered_map<uint32_t, int>& prices);
    };

}

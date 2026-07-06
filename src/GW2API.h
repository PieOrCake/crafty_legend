#pragma once

#include <string>
#include <vector>
#include <map>
#include <unordered_map>
#include <unordered_set>
#include <mutex>
#include <nlohmann/json.hpp>

namespace CraftyLegend {

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

        // Owned item counts (data set by H&S events)
        static int GetOwnedCount(uint32_t item_id);
        static int GetOwnedCountForAccount(uint32_t item_id, const std::string& account_name);
        static std::map<std::string, int> GetPerAccountCounts(uint32_t item_id);
        static void SetItemCount(uint32_t item_id, int count);
        static void SetItemCountPerAccount(uint32_t item_id, const std::string& account_name, int count);
        static void ClearItemsAndWallet();
        static bool HasAccountData();
        static void SetHasAccountData(bool has_data);

        // Current account detection
        static void SetCurrentAccountName(const std::string& name);
        static std::string GetCurrentAccountName();
        static bool HasCurrentAccount();

        // Wallet (data set by H&S events)
        static int GetWalletAmount(int currency_id);
        static void SetWalletAmount(int currency_id, int amount);
        static void ClearWallet();
        static int GetWalletAmountByName(const std::string& currency_name);

        // Legendary Armory (data set by H&S events)
        static bool IsLegendaryUnlocked(uint32_t item_id);
        static void SetLegendaryUnlocked(uint32_t item_id);
        static void ClearLegendaryArmory();

        // Masteries & Achievements (data set by H&S events)
        static int GetMasteryLevel(int mastery_id);
        static void SetMasteryLevel(int mastery_id, int level);
        static bool IsAchievementDone(int achievement_id);
        static void SetAchievementDone(int achievement_id, bool done);
        static void ClearMasteriesAndAchievements();
        static bool HasMapCompletion();

        // TP Prices
        static void FetchPricesAsync(const std::vector<uint32_t>& item_ids);
        static FetchStatus GetPriceFetchStatus();
        static const std::string& GetPriceFetchMessage();
        static int GetSellPrice(uint32_t item_id);  // unit price in copper, 0 if not on TP
        static bool HasPriceData();
        static bool LoadPriceData();

        // Localized names (i18n; display-only). Fetched from the public /v2 API
        // with ?lang=. English keys/ids remain the internal matchers.
        static int GetCurrencyIdByName(const std::string& name); // -1 if unknown
        static void FetchCurrencyNamesAsync(const std::string& lang); // all currencies; also resets achiev cache
        static std::string LocalizeCurrencyName(const std::string& englishName);
        static void EnsureAchievementNames(const std::vector<int>& ids, const std::string& lang);
        static std::string LocalizeAchievementName(int achievement_id, const std::string& englishFallback);
        static std::string LocalizedDiag(); // "loc[fr] cur=79 ach=3" for the debug readout

    private:
        static std::unordered_map<uint32_t, int> s_owned_items;
        static std::unordered_map<uint32_t, std::map<std::string, int>> s_owned_items_per_account;
        static std::string s_current_account_name;
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

        // Localized-name caches (per active language; keyed by id)
        static std::unordered_map<int, std::string> s_currency_names;
        static std::unordered_map<int, std::string> s_achievement_names;
        static std::unordered_set<int> s_achievement_requested; // ids fetched/in-flight this lang
        static std::string s_localized_lang;                    // lang the caches hold
        static bool s_currency_fetch_inflight;

        // HTTP helper
        static std::string HttpGet(const std::string& url);

        static bool EnsureDataDirectory();

        static bool SavePriceData(const std::unordered_map<uint32_t, int>& prices);
    };

}

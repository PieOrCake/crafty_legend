#pragma once

#include <string>
#include <vector>
#include <map>
#include <unordered_map>
#include <unordered_set>
#include <mutex>
#include <atomic>
#include <cstdint>
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
        static bool HasWalletData();
        static int GetWalletAmountByName(const std::string& currency_name);

        // Legendary Armory (data set by H&S events).
        // Only armoury-bound copies count as owned — an unbound legendary sitting in
        // inventory can still be sold, so it is not ownership.
        static bool IsLegendaryUnlocked(uint32_t item_id);
        static void SetLegendaryUnlocked(uint32_t item_id, int count);
        // Copies held in the armoury (0 if none). Container legendaries sum across the
        // equipment forms they unlock, via GetArmoryAliasGroup.
        static int GetArmoryCount(uint32_t item_id);
        static void ClearLegendaryArmory();

        // Masteries & Achievements (data set by H&S events)
        // H&S reports per-achievement progress alongside the done flag. `current` is -1
        // when the account has not started the achievement (the GW2 account endpoint
        // omits unstarted achievements entirely) and `max` is -1 when unknown.
        struct AchievementProgress {
            int  current = -1;
            int  max     = -1;
            bool done    = false;
        };
        static int GetMasteryLevel(int mastery_id);
        static void SetMasteryLevel(int mastery_id, int level);
        static bool IsAchievementDone(int achievement_id);
        static void SetAchievementProgress(int achievement_id, int current, int max, bool done);
        // True when H&S has reported this achievement at all; fills `out` if so.
        static bool GetAchievementProgress(int achievement_id, AchievementProgress& out);
        static void ClearMasteriesAndAchievements();
        static bool HasMapCompletion();

        // TP Prices
        static void FetchPricesAsync(const std::vector<uint32_t>& item_ids);
        static FetchStatus GetPriceFetchStatus();
        static const std::string& GetPriceFetchMessage();
        static int GetSellPrice(uint32_t item_id);  // unit price in copper, 0 if not on TP
        static bool HasPriceData();
        static bool LoadPriceData();
        // Monotonic counter bumped every time the price cache is written (fetch
        // or load from disk). Lock-free read so render code can poll it cheaply
        // to invalidate cost caches when live TP prices change.
        static uint64_t GetPriceRevision();

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
        static std::unordered_map<int, AchievementProgress> s_achievements; // achievement_id -> progress
        static std::unordered_map<uint32_t, int> s_legendary_armory; // item id -> copies held
        static bool s_has_account_data;
        static std::unordered_map<uint32_t, int> s_tp_prices;  // item_id -> sell unit_price in copper
        static bool s_has_price_data;
        static FetchStatus s_price_fetch_status;
        static std::string s_price_fetch_message;
        static std::atomic<uint64_t> s_price_revision;
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

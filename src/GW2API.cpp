#include "GW2API.h"
#include <fstream>
#include <sstream>
#include <thread>
#include <filesystem>
#include <windows.h>
#include <wininet.h>

using json = nlohmann::json;

namespace CraftyLegend {

    // Static member initialization
    std::unordered_map<uint32_t, int> GW2API::s_owned_items;
    std::unordered_map<uint32_t, std::map<std::string, int>> GW2API::s_owned_items_per_account;
    std::string GW2API::s_current_account_name;
    std::unordered_map<int, int> GW2API::s_wallet;
    std::unordered_map<int, int> GW2API::s_masteries;
    std::unordered_map<int, bool> GW2API::s_achievements;
    std::unordered_set<uint32_t> GW2API::s_legendary_armory;
    bool GW2API::s_has_account_data = false;
    std::unordered_map<uint32_t, int> GW2API::s_tp_prices;
    bool GW2API::s_has_price_data = false;
    FetchStatus GW2API::s_price_fetch_status = FetchStatus::Idle;
    std::string GW2API::s_price_fetch_message;
    std::mutex GW2API::s_mutex;

    // Helper: get the DLL directory (same logic as DataManager)
    static std::string GetDllDir() {
        char dllPath[MAX_PATH];
        HMODULE hModule = NULL;
        if (GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                               (LPCSTR)GetDllDir, &hModule)) {
            if (GetModuleFileNameA(hModule, dllPath, MAX_PATH)) {
                std::string path(dllPath);
                size_t lastSlash = path.find_last_of("\\/");
                if (lastSlash != std::string::npos) {
                    return path.substr(0, lastSlash);
                }
            }
        }
        return "";
    }

    std::string GW2API::GetDataDirectory() {
        std::string dir = GetDllDir();
        if (!dir.empty()) {
            std::replace(dir.begin(), dir.end(), '\\', '/');
        }
        return dir + "/CraftyLegend";
    }

    bool GW2API::EnsureDataDirectory() {
        std::string dir = GetDataDirectory();
        try {
            std::filesystem::create_directories(dir);
            return true;
        } catch (...) {
            return false;
        }
    }

    // HTTP GET using WinINet
    std::string GW2API::HttpGet(const std::string& url) {
        HINTERNET hInternet = InternetOpenA("CraftyLegend/1.0",
            INTERNET_OPEN_TYPE_PRECONFIG, NULL, NULL, 0);
        if (!hInternet) return "";

        DWORD flags = INTERNET_FLAG_RELOAD | INTERNET_FLAG_NO_CACHE_WRITE |
                      INTERNET_FLAG_SECURE | INTERNET_FLAG_IGNORE_CERT_CN_INVALID;

        HINTERNET hUrl = InternetOpenUrlA(hInternet, url.c_str(), NULL, 0, flags, 0);
        if (!hUrl) {
            InternetCloseHandle(hInternet);
            return "";
        }

        std::string result;
        char buffer[8192];
        DWORD bytesRead;
        while (InternetReadFile(hUrl, buffer, sizeof(buffer), &bytesRead) && bytesRead > 0) {
            result.append(buffer, bytesRead);
        }

        InternetCloseHandle(hUrl);
        InternetCloseHandle(hInternet);
        return result;
    }

    // --- Owned counts ---

    int GW2API::GetOwnedCount(uint32_t item_id) {
        std::lock_guard<std::mutex> lock(s_mutex);
        auto it = s_owned_items.find(item_id);
        if (it != s_owned_items.end()) return it->second;
        return 0;
    }

    bool GW2API::HasAccountData() {
        std::lock_guard<std::mutex> lock(s_mutex);
        return s_has_account_data;
    }

    void GW2API::SetHasAccountData(bool has_data) {
        std::lock_guard<std::mutex> lock(s_mutex);
        s_has_account_data = has_data;
    }

    void GW2API::SetItemCount(uint32_t item_id, int count) {
        std::lock_guard<std::mutex> lock(s_mutex);
        if (count > 0) {
            s_owned_items[item_id] = count;
        } else {
            s_owned_items.erase(item_id);
        }
    }

    void GW2API::SetItemCountPerAccount(uint32_t item_id, const std::string& account_name, int count) {
        std::lock_guard<std::mutex> lock(s_mutex);
        if (count > 0) {
            s_owned_items_per_account[item_id][account_name] = count;
        } else {
            auto it = s_owned_items_per_account.find(item_id);
            if (it != s_owned_items_per_account.end()) {
                it->second.erase(account_name);
                if (it->second.empty()) s_owned_items_per_account.erase(it);
            }
        }
    }

    int GW2API::GetOwnedCountForAccount(uint32_t item_id, const std::string& account_name) {
        std::lock_guard<std::mutex> lock(s_mutex);
        auto it = s_owned_items_per_account.find(item_id);
        if (it != s_owned_items_per_account.end()) {
            auto ait = it->second.find(account_name);
            if (ait != it->second.end()) return ait->second;
        }
        return 0;
    }

    std::map<std::string, int> GW2API::GetPerAccountCounts(uint32_t item_id) {
        std::lock_guard<std::mutex> lock(s_mutex);
        auto it = s_owned_items_per_account.find(item_id);
        if (it != s_owned_items_per_account.end()) return it->second;
        return {};
    }

    void GW2API::SetCurrentAccountName(const std::string& name) {
        std::lock_guard<std::mutex> lock(s_mutex);
        s_current_account_name = name;
    }

    std::string GW2API::GetCurrentAccountName() {
        std::lock_guard<std::mutex> lock(s_mutex);
        return s_current_account_name;
    }

    bool GW2API::HasCurrentAccount() {
        std::lock_guard<std::mutex> lock(s_mutex);
        return !s_current_account_name.empty();
    }

    void GW2API::ClearItemsAndWallet() {
        std::lock_guard<std::mutex> lock(s_mutex);
        s_owned_items.clear();
        s_owned_items_per_account.clear();
        s_wallet.clear();
    }

    void GW2API::SetWalletAmount(int currency_id, int amount) {
        std::lock_guard<std::mutex> lock(s_mutex);
        s_wallet[currency_id] = amount;
    }

    void GW2API::ClearWallet() {
        std::lock_guard<std::mutex> lock(s_mutex);
        s_wallet.clear();
    }

    int GW2API::GetWalletAmount(int currency_id) {
        std::lock_guard<std::mutex> lock(s_mutex);
        auto it = s_wallet.find(currency_id);
        if (it != s_wallet.end()) return it->second;
        return 0;
    }

    bool GW2API::IsLegendaryUnlocked(uint32_t item_id) {
        std::lock_guard<std::mutex> lock(s_mutex);
        return s_legendary_armory.count(item_id) > 0;
    }

    void GW2API::SetLegendaryUnlocked(uint32_t item_id) {
        std::lock_guard<std::mutex> lock(s_mutex);
        s_legendary_armory.insert(item_id);
    }

    void GW2API::ClearLegendaryArmory() {
        std::lock_guard<std::mutex> lock(s_mutex);
        s_legendary_armory.clear();
    }

    int GW2API::GetWalletAmountByName(const std::string& name) {
        // Map currency display names to GW2 API wallet currency IDs
        static const std::unordered_map<std::string, int> name_to_id = {
            // Core currencies
            {"Coin", 1}, {"Gold", 1},
            {"Karma", 2},
            {"Laurel", 3}, {"Laurels", 3},
            {"Gem", 4}, {"Gems", 4},
            {"Spirit Shard", 23}, {"Spirit Shards", 23},
            {"Transmutation Charge", 18}, {"Transmutation Charges", 18},
            // Dungeon currencies
            {"Ascalonian Tear", 5}, {"Ascalonian Tears", 5},
            {"Shard of Zhaitan", 6}, {"Shards of Zhaitan", 6},
            {"Seal of Beetletun", 9}, {"Seals of Beetletun", 9},
            {"Manifesto of the Moletariate", 10}, {"Manifestos of the Moletariate", 10},
            {"Deadly Bloom", 11}, {"Deadly Blooms", 11},
            {"Symbol of Koda", 12}, {"Symbols of Koda", 12},
            {"Flame Legion Charr Carving", 13}, {"Flame Legion Charr Carvings", 13},
            {"Knowledge Crystal", 14}, {"Knowledge Crystals", 14},
            {"Tales of Dungeon Delving", 69},
            // Fractal currencies
            {"Fractal Relic", 7}, {"Fractal Relics", 7},
            {"Pristine Fractal Relic", 24}, {"Pristine Fractal Relics", 24},
            {"Unstable Fractal Essence", 59}, {"Unstable Fractal Essences", 59},
            // WvW currencies
            {"Badge of Honor", 15}, {"Badges of Honor", 15},
            {"WvW Skirmish Claim Ticket", 26}, {"WvW Skirmish Claim Tickets", 26},
            {"Proof of Heroics", 31},
            {"Testimony of Desert Heroics", 36},
            {"Testimony of Jade Heroics", 65},
            {"Testimony of Castoran Heroics", 82},
            // PvP currencies
            {"PvP League Ticket", 30}, {"PvP League Tickets", 30},
            {"PvP Tournament Voucher", 46}, {"PvP Tournament Vouchers", 46},
            {"Ascended Shard of Glory", 33}, {"Ascended Shards of Glory", 33},
            // Guild
            {"Guild Commendation", 16}, {"Guild Commendations", 16},
            // Living World / Core Tyria map currencies
            {"Geode", 25}, {"Geodes", 25},
            {"Bandit Crest", 27}, {"Bandit Crests", 27},
            {"Bandit Skeleton Key", 40}, {"Bandit Skeleton Keys", 40},
            {"Zephyrite Lockpick", 43}, {"Zephyrite Lockpicks", 43},
            // HoT map currencies
            {"Airship Part", 19}, {"Airship Parts", 19},
            {"Ley Line Crystal", 20}, {"Ley Line Crystals", 20},
            {"Lump of Aurillium", 22}, {"Lumps of Aurillium", 22},
            {"Exalted Key", 37}, {"Exalted Keys", 37},
            {"Pact Crowbar", 41}, {"Pact Crowbars", 41},
            {"Vial of Chak Acid", 42}, {"Vials of Chak Acid", 42},
            {"Machete", 38}, {"Machetes", 38},
            {"Unbound Magic", 32},
            // Raid currencies
            {"Magnetite Shard", 28}, {"Magnetite Shards", 28},
            {"Gaeting Crystal", 39}, {"Gaeting Crystals", 39},
            {"Legendary Insight", 70}, {"Legendary Insights", 70},
            {"Provisioner Token", 29}, {"Provisioner Tokens", 29},
            // PoF map currencies
            {"Trade Contract", 34}, {"Trade Contracts", 34},
            {"Elegy Mosaic", 35}, {"Elegy Mosaics", 35},
            {"Trader's Key", 44},
            {"Volatile Magic", 45},
            // LW Season currencies
            {"Racing Medallion", 47}, {"Racing Medallions", 47},
            {"Mistborn Key", 49}, {"Mistborn Keys", 49},
            {"Festival Token", 50}, {"Festival Tokens", 50},
            {"Cache Key", 51}, {"Cache Keys", 51},
            {"War Supplies", 58},
            {"Tyrian Defense Seal", 60}, {"Tyrian Defense Seals", 60},
            // Strike / Prophet currencies
            {"Red Prophet Shard", 52}, {"Red Prophet Shards", 52},
            {"Green Prophet Shard", 53}, {"Green Prophet Shards", 53},
            {"Blue Prophet Crystal", 54}, {"Blue Prophet Crystals", 54},
            {"Green Prophet Crystal", 55}, {"Green Prophet Crystals", 55},
            {"Red Prophet Crystal", 56}, {"Red Prophet Crystals", 56},
            {"Blue Prophet Shard", 57}, {"Blue Prophet Shards", 57},
            // EoD currencies
            {"Imperial Favor", 68},
            {"Research Note", 61}, {"Research Notes", 61},
            {"Jade Sliver", 64}, {"Jade Slivers", 64},
            {"Jade Miner's Keycard", 71},
            // SotO currencies
            {"Ancient Coin", 66}, {"Ancient Coins", 66},
            {"Unusual Coin", 62}, {"Unusual Coins", 62},
            {"Astral Acclaim", 63},
            {"Static Charge", 72}, {"Static Charges", 72},
            {"Pinch of Stardust", 73},
            {"Calcified Gasp", 75}, {"Calcified Gasps", 75},
            {"Canach Coins", 67},
            // JW currencies
            {"Ursus Oblige", 76},
            {"Fine Rift Essence", 78},
            {"Masterwork Rift Essence", 80},
            {"Rare Rift Essence", 79},
            {"Antiquated Ducat", 81}, {"Antiquated Ducats", 81},
            {"Aether-Rich Sap", 83},
        };
        auto it = name_to_id.find(name);
        if (it == name_to_id.end()) return -1; // Not a wallet currency
        return GetWalletAmount(it->second);
    }

    // --- Masteries & Achievements ---

    int GW2API::GetMasteryLevel(int mastery_id) {
        std::lock_guard<std::mutex> lock(s_mutex);
        auto it = s_masteries.find(mastery_id);
        if (it != s_masteries.end()) return it->second;
        return -1; // Not unlocked
    }

    bool GW2API::IsAchievementDone(int achievement_id) {
        std::lock_guard<std::mutex> lock(s_mutex);
        auto it = s_achievements.find(achievement_id);
        if (it != s_achievements.end()) return it->second;
        return false;
    }

    void GW2API::SetMasteryLevel(int mastery_id, int level) {
        std::lock_guard<std::mutex> lock(s_mutex);
        s_masteries[mastery_id] = level;
    }

    void GW2API::SetAchievementDone(int achievement_id, bool done) {
        std::lock_guard<std::mutex> lock(s_mutex);
        s_achievements[achievement_id] = done;
    }

    void GW2API::ClearMasteriesAndAchievements() {
        std::lock_guard<std::mutex> lock(s_mutex);
        s_masteries.clear();
        s_achievements.clear();
    }

    bool GW2API::HasMapCompletion() {
        // Gift of Exploration (19677) in inventory means world completion done
        // but it may have been consumed already - check achievements instead
        // Achievement 137 = "Been There, Done That" (world completion)
        if (IsAchievementDone(137)) return true;
        // Fallback: check if Gift of Exploration is owned
        return GetOwnedCount(19677) > 0;
    }

    // --- TP Prices ---

    void GW2API::FetchPricesAsync(const std::vector<uint32_t>& item_ids) {
        {
            std::lock_guard<std::mutex> lock(s_mutex);
            if (s_price_fetch_status == FetchStatus::InProgress) return;
            s_price_fetch_status = FetchStatus::InProgress;
            s_price_fetch_message = "Fetching TP prices...";
        }

        // Copy IDs for the thread
        std::vector<uint32_t> ids = item_ids;

        std::thread([ids]() {
            try {
                std::unordered_map<uint32_t, int> prices;
                int total = static_cast<int>(ids.size());
                int fetched = 0;

                // Batch in groups of 200 (API limit)
                for (size_t offset = 0; offset < ids.size(); offset += 200) {
                    size_t end = std::min(offset + 200, ids.size());
                    std::string idList;
                    for (size_t i = offset; i < end; i++) {
                        if (!idList.empty()) idList += ",";
                        idList += std::to_string(ids[i]);
                    }

                    {
                        std::lock_guard<std::mutex> lock(s_mutex);
                        s_price_fetch_message = "Fetching TP prices... (" +
                            std::to_string(fetched) + "/" + std::to_string(total) + ")";
                    }

                    std::string url = "https://api.guildwars2.com/v2/commerce/prices?ids=" + idList;
                    std::string response = HttpGet(url);
                    if (!response.empty()) {
                        try {
                            json j = json::parse(response);
                            if (j.is_array()) {
                                for (const auto& entry : j) {
                                    if (!entry.contains("id") || !entry.contains("sells")) continue;
                                    uint32_t id = entry["id"].get<uint32_t>();
                                    int sell_price = 0;
                                    if (entry["sells"].contains("unit_price")) {
                                        sell_price = entry["sells"]["unit_price"].get<int>();
                                    }
                                    if (sell_price > 0) {
                                        prices[id] = sell_price;
                                    }
                                }
                            }
                        } catch (...) {}
                    }
                    fetched = static_cast<int>(end);
                }

                SavePriceData(prices);

                {
                    std::lock_guard<std::mutex> lock(s_mutex);
                    s_tp_prices = prices;
                    s_has_price_data = true;
                    s_price_fetch_status = FetchStatus::Success;
                    s_price_fetch_message = "Prices loaded (" +
                        std::to_string(prices.size()) + " items)";
                }
            } catch (const std::exception& e) {
                std::lock_guard<std::mutex> lock(s_mutex);
                s_price_fetch_status = FetchStatus::Error;
                s_price_fetch_message = std::string("Price error: ") + e.what();
            }
        }).detach();
    }

    FetchStatus GW2API::GetPriceFetchStatus() {
        std::lock_guard<std::mutex> lock(s_mutex);
        return s_price_fetch_status;
    }

    const std::string& GW2API::GetPriceFetchMessage() {
        std::lock_guard<std::mutex> lock(s_mutex);
        return s_price_fetch_message;
    }

    int GW2API::GetSellPrice(uint32_t item_id) {
        std::lock_guard<std::mutex> lock(s_mutex);
        auto it = s_tp_prices.find(item_id);
        if (it != s_tp_prices.end()) return it->second;
        return 0;
    }

    bool GW2API::HasPriceData() {
        std::lock_guard<std::mutex> lock(s_mutex);
        return s_has_price_data;
    }

    bool GW2API::SavePriceData(const std::unordered_map<uint32_t, int>& prices) {
        EnsureDataDirectory();
        std::string path = GetDataDirectory() + "/tp_prices.json";
        json j;
        json arr = json::array();
        for (const auto& [id, price] : prices) {
            arr.push_back({{"id", id}, {"price", price}});
        }
        j["prices"] = arr;

        std::ofstream file(path);
        if (!file.is_open()) return false;
        file << j.dump(2);
        return true;
    }

    bool GW2API::LoadPriceData() {
        std::string path = GetDataDirectory() + "/tp_prices.json";
        std::ifstream file(path);
        if (!file.is_open()) return false;

        try {
            json j;
            file >> j;

            std::lock_guard<std::mutex> lock(s_mutex);
            s_tp_prices.clear();

            if (j.contains("prices") && j["prices"].is_array()) {
                for (const auto& entry : j["prices"]) {
                    uint32_t id = entry["id"].get<uint32_t>();
                    int price = entry["price"].get<int>();
                    s_tp_prices[id] = price;
                }
            }

            s_has_price_data = true;
            return true;
        } catch (...) {
            return false;
        }
    }

}

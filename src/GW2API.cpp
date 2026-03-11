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
    std::string GW2API::s_api_key;
    ApiKeyInfo GW2API::s_key_info;
    FetchStatus GW2API::s_validation_status = FetchStatus::Idle;
    FetchStatus GW2API::s_fetch_status = FetchStatus::Idle;
    std::string GW2API::s_fetch_message;
    std::unordered_map<uint32_t, int> GW2API::s_owned_items;
    std::unordered_map<int, int> GW2API::s_wallet;
    std::unordered_map<int, int> GW2API::s_masteries;
    std::unordered_map<int, bool> GW2API::s_achievements;
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

    // --- API Key Management ---

    void GW2API::SetApiKey(const std::string& key) {
        std::lock_guard<std::mutex> lock(s_mutex);
        s_api_key = key;
    }

    const std::string& GW2API::GetApiKey() {
        return s_api_key;
    }

    bool GW2API::LoadApiKey() {
        std::string path = GetDataDirectory() + "/api_key.json";
        std::ifstream file(path);
        if (!file.is_open()) return false;

        try {
            json j;
            file >> j;
            if (j.contains("api_key")) {
                std::lock_guard<std::mutex> lock(s_mutex);
                s_api_key = j["api_key"].get<std::string>();
                return true;
            }
        } catch (...) {}
        return false;
    }

    bool GW2API::SaveApiKey() {
        EnsureDataDirectory();
        std::string dir = GetDataDirectory();
        std::string path = dir + "/api_key.json";

        json j;
        {
            std::lock_guard<std::mutex> lock(s_mutex);
            j["api_key"] = s_api_key;
        }

        std::ofstream file(path);
        if (!file.is_open()) return false;
        file << j.dump(2);
        return true;
    }

    // --- Validation ---

    void GW2API::ValidateApiKeyAsync() {
        {
            std::lock_guard<std::mutex> lock(s_mutex);
            s_validation_status = FetchStatus::InProgress;
            s_key_info = ApiKeyInfo{};
        }

        std::string key;
        {
            std::lock_guard<std::mutex> lock(s_mutex);
            key = s_api_key;
        }

        std::thread([key]() {
            ApiKeyInfo info;
            try {
                // Fetch token info
                std::string url = "https://api.guildwars2.com/v2/tokeninfo?access_token=" + key;
                std::string response = HttpGet(url);
                if (response.empty()) {
                    info.error = "No response from API";
                    std::lock_guard<std::mutex> lock(s_mutex);
                    s_key_info = info;
                    s_validation_status = FetchStatus::Error;
                    return;
                }

                json j = json::parse(response);
                if (j.contains("text")) {
                    info.error = j["text"].get<std::string>();
                    std::lock_guard<std::mutex> lock(s_mutex);
                    s_key_info = info;
                    s_validation_status = FetchStatus::Error;
                    return;
                }

                info.key_name = j.value("name", "");
                if (j.contains("permissions")) {
                    for (const auto& p : j["permissions"]) {
                        info.permissions.push_back(p.get<std::string>());
                    }
                }

                // Fetch account name
                std::string acc_url = "https://api.guildwars2.com/v2/account?access_token=" + key;
                std::string acc_response = HttpGet(acc_url);
                if (!acc_response.empty()) {
                    json acc_j = json::parse(acc_response);
                    if (acc_j.contains("name")) {
                        info.account_name = acc_j["name"].get<std::string>();
                    }
                }

                info.valid = true;
                std::lock_guard<std::mutex> lock(s_mutex);
                s_key_info = info;
                s_validation_status = FetchStatus::Success;

            } catch (const std::exception& e) {
                info.error = std::string("Exception: ") + e.what();
                std::lock_guard<std::mutex> lock(s_mutex);
                s_key_info = info;
                s_validation_status = FetchStatus::Error;
            }
        }).detach();
    }

    FetchStatus GW2API::GetValidationStatus() {
        std::lock_guard<std::mutex> lock(s_mutex);
        return s_validation_status;
    }

    const ApiKeyInfo& GW2API::GetApiKeyInfo() {
        return s_key_info;
    }

    // --- Account Data Fetching ---

    void GW2API::FetchAccountDataAsync() {
        {
            std::lock_guard<std::mutex> lock(s_mutex);
            s_fetch_status = FetchStatus::InProgress;
            s_fetch_message = "Starting...";
        }

        std::string key;
        {
            std::lock_guard<std::mutex> lock(s_mutex);
            key = s_api_key;
        }

        std::thread([key]() {
            std::unordered_map<uint32_t, int> items;
            std::unordered_map<int, int> wallet;
            std::unordered_map<int, int> masteries;
            std::unordered_map<int, bool> achievements;

            try {
                // 1. Material Storage
                {
                    std::lock_guard<std::mutex> lock(s_mutex);
                    s_fetch_message = "Fetching material storage...";
                }
                std::string url = "https://api.guildwars2.com/v2/account/materials?access_token=" + key;
                std::string response = HttpGet(url);
                if (!response.empty()) {
                    json j = json::parse(response);
                    if (j.is_array()) {
                        for (const auto& slot : j) {
                            if (slot.is_null() || !slot.contains("id")) continue;
                            uint32_t id = slot["id"].get<uint32_t>();
                            int count = slot.value("count", 0);
                            if (count > 0) {
                                items[id] += count;
                            }
                        }
                    }
                }

                // 2. Bank
                {
                    std::lock_guard<std::mutex> lock(s_mutex);
                    s_fetch_message = "Fetching bank...";
                }
                url = "https://api.guildwars2.com/v2/account/bank?access_token=" + key;
                response = HttpGet(url);
                if (!response.empty()) {
                    json j = json::parse(response);
                    if (j.is_array()) {
                        for (const auto& slot : j) {
                            if (slot.is_null() || !slot.contains("id")) continue;
                            uint32_t id = slot["id"].get<uint32_t>();
                            int count = slot.value("count", 1);
                            items[id] += count;
                        }
                    }
                }

                // 3. Character Inventories
                {
                    std::lock_guard<std::mutex> lock(s_mutex);
                    s_fetch_message = "Fetching characters...";
                }
                url = "https://api.guildwars2.com/v2/characters?access_token=" + key;
                response = HttpGet(url);
                if (!response.empty()) {
                    json chars = json::parse(response);
                    if (chars.is_array()) {
                        for (const auto& char_name : chars) {
                            std::string name = char_name.get<std::string>();
                            {
                                std::lock_guard<std::mutex> lock(s_mutex);
                                s_fetch_message = "Fetching inventory: " + name + "...";
                            }

                            // URL-encode character name
                            std::string encoded_name;
                            for (char c : name) {
                                if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
                                    encoded_name += c;
                                } else {
                                    char hex[4];
                                    snprintf(hex, sizeof(hex), "%%%02X", (unsigned char)c);
                                    encoded_name += hex;
                                }
                            }

                            std::string inv_url = "https://api.guildwars2.com/v2/characters/" +
                                encoded_name + "/inventory?access_token=" + key;
                            std::string inv_response = HttpGet(inv_url);
                            if (!inv_response.empty()) {
                                try {
                                    json inv = json::parse(inv_response);
                                    if (inv.contains("bags") && inv["bags"].is_array()) {
                                        for (const auto& bag : inv["bags"]) {
                                            if (bag.is_null() || !bag.contains("inventory")) continue;
                                            for (const auto& item : bag["inventory"]) {
                                                if (item.is_null() || !item.contains("id")) continue;
                                                uint32_t id = item["id"].get<uint32_t>();
                                                int count = item.value("count", 1);
                                                items[id] += count;
                                            }
                                        }
                                    }
                                } catch (...) {}
                            }
                        }
                    }
                }

                // 4. Wallet
                {
                    std::lock_guard<std::mutex> lock(s_mutex);
                    s_fetch_message = "Fetching wallet...";
                }
                url = "https://api.guildwars2.com/v2/account/wallet?access_token=" + key;
                response = HttpGet(url);
                if (!response.empty()) {
                    json j = json::parse(response);
                    if (j.is_array()) {
                        for (const auto& entry : j) {
                            if (!entry.contains("id") || !entry.contains("value")) continue;
                            int id = entry["id"].get<int>();
                            int value = entry["value"].get<int>();
                            wallet[id] = value;
                        }
                    }
                }

                // 5. Masteries
                {
                    std::lock_guard<std::mutex> lock(s_mutex);
                    s_fetch_message = "Fetching masteries...";
                }
                url = "https://api.guildwars2.com/v2/account/masteries?access_token=" + key;
                response = HttpGet(url);
                if (!response.empty()) {
                    try {
                        json j = json::parse(response);
                        if (j.is_array()) {
                            for (const auto& entry : j) {
                                if (!entry.contains("id")) continue;
                                int id = entry["id"].get<int>();
                                int level = entry.value("level", 0);
                                masteries[id] = level;
                            }
                        }
                    } catch (...) {}
                }

                // 6. Achievements
                {
                    std::lock_guard<std::mutex> lock(s_mutex);
                    s_fetch_message = "Fetching achievements...";
                }
                url = "https://api.guildwars2.com/v2/account/achievements?access_token=" + key;
                response = HttpGet(url);
                if (!response.empty()) {
                    try {
                        json j = json::parse(response);
                        if (j.is_array()) {
                            for (const auto& entry : j) {
                                if (!entry.contains("id")) continue;
                                int id = entry["id"].get<int>();
                                bool done = entry.value("done", false);
                                achievements[id] = done;
                            }
                        }
                    } catch (...) {}
                }

                // Save and apply
                SaveAccountData(items, wallet, masteries, achievements);

                {
                    std::lock_guard<std::mutex> lock(s_mutex);
                    s_owned_items = items;
                    s_wallet = wallet;
                    s_masteries = masteries;
                    s_achievements = achievements;
                    s_has_account_data = true;
                    s_fetch_status = FetchStatus::Success;
                    s_fetch_message = "Done (" + std::to_string(items.size()) + " items, " +
                                     std::to_string(wallet.size()) + " currencies, " +
                                     std::to_string(masteries.size()) + " masteries)";
                }

            } catch (const std::exception& e) {
                std::lock_guard<std::mutex> lock(s_mutex);
                s_fetch_status = FetchStatus::Error;
                s_fetch_message = std::string("Error: ") + e.what();
            }
        }).detach();
    }

    FetchStatus GW2API::GetFetchStatus() {
        std::lock_guard<std::mutex> lock(s_mutex);
        return s_fetch_status;
    }

    const std::string& GW2API::GetFetchStatusMessage() {
        return s_fetch_message;
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

    int GW2API::GetWalletAmount(int currency_id) {
        std::lock_guard<std::mutex> lock(s_mutex);
        auto it = s_wallet.find(currency_id);
        if (it != s_wallet.end()) return it->second;
        return 0;
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

    bool GW2API::HasMapCompletion() {
        // Gift of Exploration (19677) in inventory means world completion done
        // but it may have been consumed already - check achievements instead
        // Achievement 137 = "Been There, Done That" (world completion)
        if (IsAchievementDone(137)) return true;
        // Fallback: check if Gift of Exploration is owned
        return GetOwnedCount(19677) > 0;
    }

    // --- Persistence ---

    bool GW2API::SaveAccountData(const std::unordered_map<uint32_t, int>& items,
                                  const std::unordered_map<int, int>& wallet,
                                  const std::unordered_map<int, int>& masteries,
                                  const std::unordered_map<int, bool>& achievements) {
        EnsureDataDirectory();
        std::string path = GetDataDirectory() + "/account_data.json";

        json j;
        json items_arr = json::array();
        for (const auto& [id, count] : items) {
            items_arr.push_back({{"id", id}, {"count", count}});
        }
        j["items"] = items_arr;

        json wallet_arr = json::array();
        for (const auto& [id, value] : wallet) {
            wallet_arr.push_back({{"id", id}, {"value", value}});
        }
        j["wallet"] = wallet_arr;

        json masteries_arr = json::array();
        for (const auto& [id, level] : masteries) {
            masteries_arr.push_back({{"id", id}, {"level", level}});
        }
        j["masteries"] = masteries_arr;

        json achievements_arr = json::array();
        for (const auto& [id, done] : achievements) {
            if (done) achievements_arr.push_back({{"id", id}, {"done", true}});
        }
        j["achievements"] = achievements_arr;

        std::ofstream file(path);
        if (!file.is_open()) return false;
        file << j.dump(2);
        return true;
    }

    bool GW2API::LoadAccountData() {
        std::string path = GetDataDirectory() + "/account_data.json";
        std::ifstream file(path);
        if (!file.is_open()) return false;

        try {
            json j;
            file >> j;

            std::lock_guard<std::mutex> lock(s_mutex);
            s_owned_items.clear();
            s_wallet.clear();
            s_masteries.clear();
            s_achievements.clear();

            if (j.contains("items") && j["items"].is_array()) {
                for (const auto& entry : j["items"]) {
                    uint32_t id = entry["id"].get<uint32_t>();
                    int count = entry["count"].get<int>();
                    s_owned_items[id] = count;
                }
            }

            if (j.contains("wallet") && j["wallet"].is_array()) {
                for (const auto& entry : j["wallet"]) {
                    int id = entry["id"].get<int>();
                    int value = entry["value"].get<int>();
                    s_wallet[id] = value;
                }
            }

            if (j.contains("masteries") && j["masteries"].is_array()) {
                for (const auto& entry : j["masteries"]) {
                    int id = entry["id"].get<int>();
                    int level = entry.value("level", 0);
                    s_masteries[id] = level;
                }
            }

            if (j.contains("achievements") && j["achievements"].is_array()) {
                for (const auto& entry : j["achievements"]) {
                    int id = entry["id"].get<int>();
                    bool done = entry.value("done", false);
                    s_achievements[id] = done;
                }
            }

            s_has_account_data = true;
            return true;
        } catch (...) {
            return false;
        }
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

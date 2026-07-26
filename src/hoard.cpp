#include "globals.h"
#include "hoard.h"
#include "settings.h"
#include "GW2API.h"
#include "DataManager.h"
#include "CharacterCrafting.h"
#include "../include/HoardAndSeekAPI.h"
#include <map>
#include <mutex>
#include <sstream>
#include <unordered_map>
#include <cstring>
#include <cctype>

// Forward declaration (defined later in this file)
static void StartAccountDetection();

// Schedule a backoff retry when H&S reports HOARD_STATUS_BUSY (rate-limited).
static void ScheduleBusyBackoff(uint32_t retry_after_ms) {
    uint32_t delay = retry_after_ms > 0 ? retry_after_ms : 500;
    auto target = std::chrono::steady_clock::now() + std::chrono::milliseconds(delay);
    if (!g_HoardBusyBackoff || target > g_HoardBusyRetryAt) {
        g_HoardBusyRetryAt = target;
    }
    g_HoardBusyBackoff = true;
}

// --- Hoard & Seek event callbacks ---

void OnHoardPong(void* aEventArgs) {
    g_HoardDetected = true;
    g_StartupPingDone = true;

    if (!aEventArgs) return; // Backward compat with older H&S
    HoardPongPayload* pong = static_cast<HoardPongPayload*>(aEventArgs);
    if (pong->api_version < HOARD_API_VERSION) return;

    g_HoardLastUpdated = pong->last_updated;
    g_HoardRefreshAvailableAt = pong->refresh_available_at;
    g_HoardAccountCount = pong->account_count;
    if (pong->has_data) {
        g_HoardDataAvailable = true;
        g_HoardRefreshNeeded = true;
        CraftyLegend::GW2API::SetHasAccountData(true);
        // Start account detection only for multi-account setups
        if (g_HoardAccountCount > 1 && !g_AccountDetectionDone) {
            StartAccountDetection();
        }
    }
    SaveLastUpdated();
}

void OnHoardDataUpdated(void* aEventArgs) {
    if (aEventArgs) {
        HoardDataReadyPayload* payload = static_cast<HoardDataReadyPayload*>(aEventArgs);
        if (payload->api_version >= HOARD_API_VERSION) {
            g_HoardLastUpdated = payload->last_updated;
            g_HoardRefreshAvailableAt = payload->refresh_available_at;
            g_HoardAccountCount = payload->account_count;
            SaveLastUpdated();
        }
    }
    g_HoardDetected = true;
    g_StartupPingDone = true;
    g_HoardDataAvailable = true;
    g_HoardRefreshNeeded = true;
    g_HoardRefreshPending = false;
    g_HoardFetching = false;
    g_HoardFetchMessage.clear();
    g_StatusMessageTime = std::chrono::steady_clock::now();
    // Clear old cached data so it gets refreshed
    CraftyLegend::GW2API::ClearItemsAndWallet();
    CraftyLegend::GW2API::ClearLegendaryArmory();
    CraftyLegend::GW2API::ClearMasteriesAndAchievements();
    CraftyLegend::GW2API::SetHasAccountData(true);
    g_HoardQueriedItems.clear();
    g_HoardQueriedWallets.clear();
    g_CompletionCacheDirty = true;
    g_ShoppingListDirty = true;
}

void OnHoardItemResponse(void* aEventArgs) {
    if (!aEventArgs) return;
    HoardQueryItemResponse* resp = static_cast<HoardQueryItemResponse*>(aEventArgs);
    if (resp->api_version < HOARD_API_VERSION) { delete resp; return; }
    if (resp->status == HOARD_STATUS_PENDING) {
        g_HoardPermissionPending = true;
        g_HoardPermissionRetryTime = std::chrono::steady_clock::now() + std::chrono::seconds(3);
        g_HoardQueriedItems.erase(resp->item_id);
        delete resp;
        return;
    }
    if (resp->status == HOARD_STATUS_DENIED) {
        g_HoardPermissionDenied = true;
        delete resp;
        return;
    }
    g_HoardPermissionPending = false;
    CraftyLegend::GW2API::SetItemCount(resp->item_id, resp->total_count);
    // Extract per-account counts from location entries
    std::map<std::string, int> perAccount;
    for (uint32_t i = 0; i < resp->location_count && i < 64; i++) {
        std::string acct = resp->locations[i].account_name;
        if (!acct.empty()) {
            perAccount[acct] += resp->locations[i].count;
            // Build character→account map from inventory-type locations
            // (locations that aren't known storage types are character names)
            const char* loc = resp->locations[i].location;
            if (loc[0] != '\0'
                && strcmp(loc, "Bank") != 0
                && strcmp(loc, "Material Storage") != 0
                && strcmp(loc, "Legendary Armory") != 0
                && strcmp(loc, "Shared Inventory") != 0
                && strcmp(loc, "Equipment") != 0
                && strcmp(loc, "Wardrobe") != 0
                && strcmp(loc, "Crafting Station") != 0
                && strcmp(loc, "Guild Bank") != 0) {
                g_CharToAccount[loc] = acct;
            }
        }
    }
    for (const auto& [acct, count] : perAccount) {
        CraftyLegend::GW2API::SetItemCountPerAccount(resp->item_id, acct, count);
    }
    delete resp;
    g_CompletionCacheDirty = true;
    g_ShoppingListDirty = true;
}

void OnHoardWalletResponse(void* aEventArgs) {
    if (!aEventArgs) return;
    HoardQueryWalletResponse* resp = static_cast<HoardQueryWalletResponse*>(aEventArgs);
    if (resp->api_version < HOARD_API_VERSION) { delete resp; return; }
    if (resp->status == HOARD_STATUS_PENDING) {
        g_HoardPermissionPending = true;
        g_HoardPermissionRetryTime = std::chrono::steady_clock::now() + std::chrono::seconds(3);
        g_HoardQueriedWallets.erase(resp->currency_id);
        delete resp;
        return;
    }
    if (resp->status == HOARD_STATUS_DENIED) {
        g_HoardPermissionDenied = true;
        delete resp;
        return;
    }
    g_HoardPermissionPending = false;
    if (resp->found) {
        CraftyLegend::GW2API::SetWalletAmount(static_cast<int>(resp->currency_id), resp->amount);
    }
    delete resp;
}

void OnAchievementResponse(void* aEventArgs) {
    if (!aEventArgs) return;
    HoardQueryAchievementResponse* resp = static_cast<HoardQueryAchievementResponse*>(aEventArgs);
    if (resp->api_version < HOARD_API_VERSION) { delete resp; return; }
    if (resp->status == HOARD_STATUS_PENDING) {
        g_HoardPermissionPending = true;
        g_HoardPermissionRetryTime = std::chrono::steady_clock::now() + std::chrono::seconds(3);
        g_HoardRefreshNeeded = true;
        delete resp;
        return;
    }
    if (resp->status == HOARD_STATUS_DENIED) {
        g_HoardPermissionDenied = true;
        delete resp;
        return;
    }
    if (resp->status == HOARD_STATUS_BUSY) {
        ScheduleBusyBackoff(resp->retry_after_ms);
        g_MasteryAchievementRefreshNeeded = true;
        delete resp;
        return;
    }
    g_HoardPermissionPending = false;
    for (uint32_t i = 0; i < resp->entry_count && i < 200; i++) {
        // Keep current/max as well as done — the prereq panel shows partial progress
        // (e.g. "12/40") for achievement gates that are underway.
        CraftyLegend::GW2API::SetAchievementProgress(
            static_cast<int>(resp->entries[i].id),
            resp->entries[i].current, resp->entries[i].max, resp->entries[i].done);
    }
    delete resp;
    g_PrereqDirty = true;
}

void OnMasteryResponse(void* aEventArgs) {
    if (!aEventArgs) return;
    HoardQueryMasteryResponse* resp = static_cast<HoardQueryMasteryResponse*>(aEventArgs);
    if (resp->api_version < HOARD_API_VERSION) { delete resp; return; }
    if (resp->status == HOARD_STATUS_PENDING) {
        g_HoardPermissionPending = true;
        g_HoardPermissionRetryTime = std::chrono::steady_clock::now() + std::chrono::seconds(3);
        g_HoardRefreshNeeded = true;
        delete resp;
        return;
    }
    if (resp->status == HOARD_STATUS_DENIED) {
        g_HoardPermissionDenied = true;
        delete resp;
        return;
    }
    if (resp->status == HOARD_STATUS_BUSY) {
        ScheduleBusyBackoff(resp->retry_after_ms);
        g_MasteryAchievementRefreshNeeded = true;
        delete resp;
        return;
    }
    g_HoardPermissionPending = false;
    for (uint32_t i = 0; i < resp->entry_count && i < 200; i++) {
        CraftyLegend::GW2API::SetMasteryLevel(
            static_cast<int>(resp->entries[i].id), resp->entries[i].level);
    }
    delete resp;
    g_PrereqDirty = true;
}

void OnHoardFetchProgress(void* aEventArgs) {
    if (!aEventArgs) return;
    HoardFetchProgressPayload* payload = static_cast<HoardFetchProgressPayload*>(aEventArgs);
    if (payload->api_version < HOARD_API_VERSION) return;
    g_HoardFetchMessage = payload->message;
    g_HoardFetching = true;
    g_HoardFetchError = false;
    // Do NOT delete — payload is stack-allocated by H&S
}

void OnHoardFetchError(void* aEventArgs) {
    if (!aEventArgs) return;
    HoardFetchErrorPayload* payload = static_cast<HoardFetchErrorPayload*>(aEventArgs);
    if (payload->api_version < HOARD_API_VERSION) return;
    g_HoardFetchError = true;
    g_HoardFetching = false;
    g_HoardRefreshPending = false;
    g_HoardErrorMessage = payload->message;
    g_StatusMessageTime = std::chrono::steady_clock::now();
    // Do NOT delete — payload is stack-allocated by H&S
}

// --- Account detection via MumbleLink + H&S ---

// Get display name for an account: label if available, otherwise account_name
std::string GetAccountDisplayName(const std::string& account_name) {
    auto it = g_AccountLabels.find(account_name);
    if (it != g_AccountLabels.end()) return it->second;
    return account_name;
}

// Called when H&S accounts are added/removed — refresh the account list
void OnAccountsChanged(void* aEventArgs) {
    StartAccountDetection();
}

// Try to resolve current character name to an account
void TryResolveCurrentAccount() {
    if (g_CurrentCharacterName.empty()) return;
    auto it = g_CharToAccount.find(g_CurrentCharacterName);
    if (it != g_CharToAccount.end()) {
        CraftyLegend::GW2API::SetCurrentAccountName(it->second);
    }
}

// Called when H&S returns the account list
void OnAccountsResponse(void* aEventArgs) {
    if (!aEventArgs) return;
    HoardQueryAccountsResponse* resp = static_cast<HoardQueryAccountsResponse*>(aEventArgs);
    if (resp->status != HOARD_STATUS_OK) { delete resp; return; }

    g_AccountNames.clear();
    g_AccountLabels.clear();
    g_CharToAccount.clear();
    for (uint32_t i = 0; i < resp->account_count && i < 16; i++) {
        std::string name = resp->accounts[i].account_name;
        if (!name.empty()) {
            g_AccountNames.push_back(name);
            std::string label = resp->accounts[i].label;
            if (!label.empty()) g_AccountLabels[name] = label;
            // Build character→account map from the account's character list
            std::vector<std::string> charNames;
            for (uint32_t c = 0; c < resp->accounts[i].character_count && c < 80; c++) {
                std::string ch = resp->accounts[i].characters[c];
                if (ch.empty()) continue;
                g_CharToAccount[ch] = name;
                charNames.push_back(ch);
            }
            CraftyLegend::CharacterCrafting::SetAccountCharacters(name, charNames);
        }
    }
    delete resp;

    g_AccountDetectionDone = true;
    TryResolveCurrentAccount();
}

// Called when MumbleLink identity updates (character change)
// eventArgs IS the Mumble::Identity pointer — use it directly, not the DataLink
void OnMumbleIdentityUpdated(void* aEventArgs) {
    if (!aEventArgs) return;
    const Mumble::Identity* id = static_cast<const Mumble::Identity*>(aEventArgs);
    // Safe read: Name is char[20], ensure null-terminated
    char buf[20] = {};
    memcpy(buf, id->Name, 19);
    std::string newName(buf);
    // Validate: GW2 names contain only ASCII letters, spaces, hyphens, and accented chars (UTF-8 >= 0x80)
    for (unsigned char c : newName) {
        if (c == ' ' || c == '-' || isalpha(c) || c >= 0x80) continue;
        return; // invalid character (path separators, digits, etc.)
    }
    if (newName.empty() || newName == g_CurrentCharacterName) return;
    std::string previousAccount = CraftyLegend::GW2API::GetCurrentAccountName();
    g_CurrentCharacterName = newName;
    TryResolveCurrentAccount();
    // If account changed, invalidate cached mastery/achievement data
    std::string newAccount = CraftyLegend::GW2API::GetCurrentAccountName();
    if (!newAccount.empty() && newAccount != previousAccount) {
        CraftyLegend::GW2API::ClearMasteriesAndAchievements();
        g_MasteryAchievementAccount.clear();
        g_MasteryAchievementRefreshNeeded = true;
        g_WalletRefreshNeeded = true;
        CraftyLegend::GW2API::ClearLegendaryArmory();
        g_ArmoryAccount.clear();
        g_ArmoryRefreshNeeded = true;
        g_PrereqDirty = true;
        g_CompletionCacheDirty = true;
    }
}

// Start account detection: query H&S for accounts list
static void StartAccountDetection() {
    HoardQueryAccountsRequest req{};
    req.api_version = HOARD_API_VERSION;
    strncpy(req.requester, "Crafty Legend", sizeof(req.requester));
    strncpy(req.response_event, CL_ACCOUNTS_RESPONSE, sizeof(req.response_event));
    APIDefs->Events_Raise(EV_HOARD_QUERY_ACCOUNTS, &req);
}

// Helper: fire H&S item query if not already queried
static void QueryHoardItem(uint32_t item_id) {
    if (item_id == 0 || !g_HoardDataAvailable) return;
    if (g_HoardPermissionPending || g_HoardPermissionDenied) return;
    if (g_HoardQueriedItems.count(item_id)) return;
    g_HoardQueriedItems.insert(item_id);
    HoardQueryItemRequest req{};
    req.api_version = HOARD_API_VERSION;
    strncpy(req.requester, "Crafty Legend", sizeof(req.requester));
    req.item_id = item_id;
    strncpy(req.response_event, CL_ITEM_RESPONSE, sizeof(req.response_event));
    APIDefs->Events_Raise(EV_HOARD_QUERY_ITEM, &req);
}

// Helper: fire H&S wallet query if not already queried
static void QueryHoardWallet(uint32_t currency_id) {
    if (currency_id == 0 || !g_HoardDataAvailable) return;
    if (g_HoardPermissionPending || g_HoardPermissionDenied) return;
    if (g_HoardQueriedWallets.count(currency_id)) return;
    g_HoardQueriedWallets.insert(currency_id);
    HoardQueryWalletRequest req{};
    req.api_version = HOARD_API_VERSION;
    strncpy(req.requester, "Crafty Legend", sizeof(req.requester));
    req.currency_id = currency_id;
    strncpy(req.response_event, CL_WALLET_RESPONSE, sizeof(req.response_event));
    // v3+: filter by current account for multi-account setups
    if (g_HoardAccountCount > 1) {
        std::string acct = CraftyLegend::GW2API::GetCurrentAccountName();
        if (!acct.empty()) {
            strncpy(req.account_filter, acct.c_str(), sizeof(req.account_filter));
        }
    }
    APIDefs->Events_Raise(EV_HOARD_QUERY_WALLET, &req);
}


static const uint32_t k_WalletIds[] = {
    1, 2, 3, 4, 5, 6, 7, 9, 10, 11, 12, 13, 14, 15, 16, 18, 19, 20, 22, 23, 24, 25, 26, 27, 28, 29,
    30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 49, 50, 51, 52, 53, 54,
    55, 56, 57, 58, 59, 60, 61, 62, 63, 64, 65, 66, 67, 68, 69, 70, 71, 72, 73, 75, 76, 78, 79, 80, 81, 82, 83
};

// Re-query all wallet currencies for the current account
void RefreshWallet() {
    if (!g_HoardDataAvailable) return;
    std::string currentAcct = CraftyLegend::GW2API::GetCurrentAccountName();
    // Skip only once we actually hold wallet data for this account. If the first
    // attempt populated nothing (H&S still PENDING/BUSY at that moment), the wallet
    // stays empty and must keep re-querying on each refresh until it fills — mirroring
    // how item queries self-heal. Without the HasWalletData() check this latched empty.
    if (!g_WalletRefreshNeeded && g_WalletAccount == currentAcct && !g_WalletAccount.empty()
        && CraftyLegend::GW2API::HasWalletData()) return;
    g_HoardQueriedWallets.clear();
    CraftyLegend::GW2API::ClearWallet();
    for (uint32_t cid : k_WalletIds) {
        QueryHoardWallet(cid);
    }
    g_WalletAccount = currentAcct;
    g_WalletRefreshNeeded = false;
}

// Helper: batch-query all items in crafting trees + all known currencies
void RefreshHoardData() {
    if (!g_HoardDataAvailable) return;
    // Query all items known to the data manager
    const auto& items = CraftyLegend::DataManager::GetItems();
    for (const auto& [id, item] : items) {
        QueryHoardItem(id);
    }
    // Query wallet (account-filtered when current account is known)
    RefreshWallet();

    g_HoardRefreshNeeded = false;

    // After all item queries, try to resolve current account from character→account map (multi-account only)
    if (g_HoardAccountCount > 1 && !CraftyLegend::GW2API::HasCurrentAccount()) {
        TryResolveCurrentAccount();
    }

    // Now query achievements/masteries and legendary armory for the current account
    RefreshMasteriesAndAchievements();
    RefreshLegendaryArmory();
}

// Helper: query achievements and masteries for the current account
void RefreshMasteriesAndAchievements() {
    if (!g_HoardDataAvailable) return;
    std::string currentAcct = CraftyLegend::GW2API::GetCurrentAccountName();

    // Skip if data is already cached for this account
    if (!g_MasteryAchievementRefreshNeeded && !g_MasteryAchievementAccount.empty()
        && g_MasteryAchievementAccount == currentAcct) {
        return;
    }

    // Collect all achievement/mastery IDs needed from prerequisites
    std::unordered_set<uint32_t> achIds;
    std::unordered_set<uint32_t> mastIds;
    achIds.insert(137); // Map completion: "Been There, Done That"
    mastIds.insert(14); mastIds.insert(15); mastIds.insert(17); mastIds.insert(18); // PoF mounts

    const auto& legendaries = CraftyLegend::DataManager::GetLegendaries();
    for (const auto& leg : legendaries) {
        auto prereqs = CraftyLegend::DataManager::GetPrerequisites(leg.id);
        for (const auto& p : prereqs) {
            if (p.achievement_id >= 0) achIds.insert(static_cast<uint32_t>(p.achievement_id));
            if (p.mastery_id >= 0) mastIds.insert(static_cast<uint32_t>(p.mastery_id));
        }
    }

    // Batch query achievements (up to 200 per request)
    {
        HoardQueryAchievementRequest req{};
        req.api_version = HOARD_API_VERSION;
        strncpy(req.requester, "Crafty Legend", sizeof(req.requester));
        strncpy(req.response_event, CL_ACHIEVEMENT_RESPONSE, sizeof(req.response_event));
        if (!currentAcct.empty()) strncpy(req.account_name, currentAcct.c_str(), sizeof(req.account_name));
        req.id_count = 0;
        for (uint32_t id : achIds) {
            req.ids[req.id_count++] = id;
            if (req.id_count >= 200) {
                APIDefs->Events_Raise(EV_HOARD_QUERY_ACHIEVEMENT, &req);
                req.id_count = 0;
            }
        }
        if (req.id_count > 0) {
            APIDefs->Events_Raise(EV_HOARD_QUERY_ACHIEVEMENT, &req);
        }
    }

    // Batch query masteries (up to 200 per request)
    {
        HoardQueryMasteryRequest req{};
        req.api_version = HOARD_API_VERSION;
        strncpy(req.requester, "Crafty Legend", sizeof(req.requester));
        strncpy(req.response_event, CL_MASTERY_RESPONSE, sizeof(req.response_event));
        if (!currentAcct.empty()) strncpy(req.account_name, currentAcct.c_str(), sizeof(req.account_name));
        req.id_count = 0;
        for (uint32_t id : mastIds) {
            req.ids[req.id_count++] = id;
            if (req.id_count >= 200) {
                APIDefs->Events_Raise(EV_HOARD_QUERY_MASTERY, &req);
                req.id_count = 0;
            }
        }
        if (req.id_count > 0) {
            APIDefs->Events_Raise(EV_HOARD_QUERY_MASTERY, &req);
        }
    }

    g_MasteryAchievementAccount = currentAcct;
    g_MasteryAchievementRefreshNeeded = false;
}

void OnHoardArmoryResponse(void* aEventArgs) {
    if (!aEventArgs) return;
    HoardQueryApiResponse* resp = static_cast<HoardQueryApiResponse*>(aEventArgs);
    if (resp->api_version < HOARD_API_VERSION) { delete resp; return; }
    if (resp->status == HOARD_STATUS_BUSY) {
        ScheduleBusyBackoff(resp->retry_after_ms);
        g_ArmoryRefreshNeeded = true;
        delete resp;
        return;
    }
    if (resp->status != HOARD_STATUS_OK) { delete resp; return; }
    try {
        auto j = json::parse(resp->json, nullptr, false);
        if (!j.is_discarded() && j.is_array()) {
            for (const auto& entry : j) {
                // Keep the copy count: runes (7), sigils (8), one-handed weapons (4),
                // rings/accessories (2) are useful in multiples, and completion % is
                // scaled against the armoury's max_count for the item.
                CraftyLegend::GW2API::SetLegendaryUnlocked(
                    entry["id"].get<uint32_t>(), entry.value("count", 1));
            }
        }
    } catch (...) {}
    delete resp;
    g_CompletionCacheDirty = true;
}

// Shared state for the crafting sweep. All of it is written by the render
// thread (on dispatch) and by H&S's worker thread (on response), so none of it
// is touched outside this mutex.
static std::mutex g_CraftingInFlightMutex;
static std::string g_CraftingInFlightChar;                       // "" when idle
static std::chrono::steady_clock::time_point g_CraftingClaimedAt; // watchdog
static std::chrono::steady_clock::time_point g_CraftingRetryAt;   // cooldown
static bool g_CraftingCooldown = false;
// Consecutive failures per "account\x1fcharacter", so one permanently broken
// character (renamed, deleted, or an endpoint that always 500s) cannot wedge
// the sweep — NextPendingCharacter would otherwise return it forever.
static std::unordered_map<std::string, int> g_CraftingFailures;

// A request that never gets a reply (H&S unloaded, or the request dropped)
// would hold the slot for the process lifetime. Give up on it after 30 s.
static constexpr int CRAFTING_WATCHDOG_SECONDS = 30;
// HoardAndSeekAPI.h requires 2-5 s between retries while permission is pending.
static constexpr int CRAFTING_COOLDOWN_SECONDS = 3;
// Attempts at one character before it is written off and the sweep moves on.
static constexpr int CRAFTING_MAX_FAILURES = 3;

static std::string CraftingFailureKey(const std::string& account,
                                      const std::string& character) {
    return account + '\x1f' + character;
}

// Claim the in-flight slot for `character`. Returns false if a request is
// already outstanding — only one character is queried at a time.
static bool ClaimCraftingSlot(const std::string& character) {
    std::lock_guard<std::mutex> lock(g_CraftingInFlightMutex);
    if (!g_CraftingInFlightChar.empty()) return false;
    g_CraftingInFlightChar = character;
    g_CraftingClaimedAt = std::chrono::steady_clock::now();
    return true;
}

// Release the slot and return whose request it was ("" if none).
static std::string ReleaseCraftingSlot() {
    std::lock_guard<std::mutex> lock(g_CraftingInFlightMutex);
    std::string was = g_CraftingInFlightChar;
    g_CraftingInFlightChar.clear();
    return was;
}

// Watchdog: drop a claim that has gone unanswered for too long.
static void ExpireStaleCraftingSlot() {
    std::lock_guard<std::mutex> lock(g_CraftingInFlightMutex);
    if (g_CraftingInFlightChar.empty()) return;
    if (std::chrono::steady_clock::now() - g_CraftingClaimedAt >
        std::chrono::seconds(CRAFTING_WATCHDOG_SECONDS)) {
        g_CraftingInFlightChar.clear();
    }
}

// Hold off the next request. Called from H&S's worker thread for any reply we
// could not use, so that no status can produce a per-frame retry loop.
static void ScheduleCraftingCooldown() {
    std::lock_guard<std::mutex> lock(g_CraftingInFlightMutex);
    g_CraftingRetryAt = std::chrono::steady_clock::now() +
                        std::chrono::seconds(CRAFTING_COOLDOWN_SECONDS);
    g_CraftingCooldown = true;
}

// True while the cooldown is still running. Clears itself once it expires.
static bool CraftingCooldownActive() {
    std::lock_guard<std::mutex> lock(g_CraftingInFlightMutex);
    if (!g_CraftingCooldown) return false;
    if (std::chrono::steady_clock::now() < g_CraftingRetryAt) return true;
    g_CraftingCooldown = false;
    return false;
}

// Count a failure against one character. Returns true once it has failed often
// enough to be written off.
static bool RecordCraftingFailure(const std::string& account,
                                  const std::string& character) {
    std::lock_guard<std::mutex> lock(g_CraftingInFlightMutex);
    return ++g_CraftingFailures[CraftingFailureKey(account, character)] >=
           CRAFTING_MAX_FAILURES;
}

static void ClearCraftingFailures(const std::string& account,
                                  const std::string& character) {
    std::lock_guard<std::mutex> lock(g_CraftingInFlightMutex);
    g_CraftingFailures.erase(CraftingFailureKey(account, character));
}

// The GW2 API reports a key missing the `characters` scope with an error object
// like {"text":"requires scope characters"}. That is the one parse failure that
// justifies denying the whole account; every other bad body is transient.
// The endpoint we would have asked for on `character`'s behalf. H&S echoes the
// endpoint back on every reply, which is the only field that identifies which
// character a reply is really for.
static std::string ExpectedCraftingEndpoint(const std::string& character) {
    return "/v2/characters/" +
           CraftyLegend::CharacterCrafting::UrlEncodePathSegment(character) +
           "/crafting";
}

static bool BodyIndicatesMissingScope(const char* json) {
    std::string body(json, strnlen(json, 512)); // error objects are tiny
    for (char& c : body) c = (char)tolower((unsigned char)c);
    return body.find("requires scope characters") != std::string::npos;
}

void OnCharacterCraftingResponse(void* aEventArgs) {
    if (!aEventArgs) return;
    HoardQueryApiResponse* resp = static_cast<HoardQueryApiResponse*>(aEventArgs);

    // Free the slot before any branch that returns — an early return that
    // skipped this (an older H&S replying with api_version 3, say) would wedge
    // the sweep for the rest of the session. ReleaseCraftingSlot does not touch
    // `resp`, so it is safe ahead of the version guard.
    //
    // Note H&S answers a cached query synchronously, from inside Events_Raise —
    // so this can run re-entrantly on the render thread. ClaimCraftingSlot has
    // already released its lock by then, so there is no deadlock.
    const std::string character = ReleaseCraftingSlot();

    // Invariant for every branch below: any path that does not successfully
    // store a character's disciplines must either schedule a back-off or leave
    // the sweep unable to re-fire, so that no reply can drive a per-frame retry.
    if (resp->api_version < HOARD_API_VERSION) {
        // An older H&S still answers EV_HOARD_QUERY_API, but with api_version 3.
        // g_HoardDataAvailable is set regardless, so without a back-off here the
        // sweep would re-fire on every single frame for the whole session.
        ScheduleCraftingCooldown();
        delete resp;
        return;
    }

    const std::string account = resp->account_name;

    if (resp->status == HOARD_STATUS_BUSY) {
        ScheduleBusyBackoff(resp->retry_after_ms);
        delete resp; // the character stays un-fetched, so it is retried
        return;
    }
    if (resp->status == HOARD_STATUS_DENIED) {
        // The user refused H&S's permission prompt, or the key lacks the
        // `characters` scope — stop the sweep for this account.
        CraftyLegend::CharacterCrafting::MarkDenied(account);
        g_CraftingRefreshNeeded = false;
        ScheduleCraftingCooldown();
        delete resp;
        return;
    }
    if (resp->status != HOARD_STATUS_OK) {
        // HOARD_STATUS_PENDING is the normal first-run state while the H&S
        // permission popup is on screen, and it can persist for minutes. Back
        // off rather than re-firing the same request on every frame.
        ScheduleCraftingCooldown();
        delete resp;
        return;
    }
    if (character.empty()) {
        // A reply for a claim the watchdog already gave up on. We cannot tell
        // whose it is, so drop it; the character stays un-fetched and is retried.
        ScheduleCraftingCooldown();
        delete resp;
        return;
    }

    // Confirm this reply is actually for the character the slot named. After a
    // watchdog expiry the slot can have been re-claimed by a later character,
    // so a late reply for the earlier one would otherwise be stored under the
    // wrong name — and would free the later character's slot, silently losing
    // its real reply too. The echoed endpoint is what disambiguates them.
    if (ExpectedCraftingEndpoint(character) != resp->endpoint) {
        ScheduleCraftingCooldown();
        delete resp;
        return;
    }

    std::vector<CraftyLegend::CharacterCrafting::DisciplineRating> disciplines;
    if (CraftyLegend::CharacterCrafting::ParseCraftingJson(resp->json, disciplines)) {
        CraftyLegend::CharacterCrafting::SetCharacterDisciplines(
            account, character, std::move(disciplines));
        ClearCraftingFailures(account, character);
    } else if (BodyIndicatesMissingScope(resp->json)) {
        // The key genuinely cannot read characters — no point sweeping on.
        CraftyLegend::CharacterCrafting::MarkDenied(account);
        g_CraftingRefreshNeeded = false;
    } else {
        // A transient error body: a 500, a rate-limit object, or a 404 for a
        // character renamed or deleted since H&S snapshotted the roster. Retry
        // it a few times, then write that one character off and move on —
        // MarkDenied persists to disk, so it must not be reached from here.
        ScheduleCraftingCooldown();
        if (RecordCraftingFailure(account, character)) {
            // Recording an empty discipline list marks it fetched, so
            // NextPendingCharacter stops handing back the same character.
            CraftyLegend::CharacterCrafting::SetCharacterDisciplines(account, character, {});
        }
    }
    delete resp;
}

void RefreshLegendaryArmory() {
    if (!g_HoardDataAvailable) return;
    std::string currentAcct = CraftyLegend::GW2API::GetCurrentAccountName();
    if (!g_ArmoryRefreshNeeded && g_ArmoryAccount == currentAcct && !g_ArmoryAccount.empty()) return;
    HoardQueryApiRequest req{};
    req.api_version = HOARD_API_VERSION;
    strncpy(req.requester, "Crafty Legend", sizeof(req.requester));
    strncpy(req.endpoint, "/v2/account/legendaryarmory", sizeof(req.endpoint));
    strncpy(req.response_event, CL_ARMORY_RESPONSE, sizeof(req.response_event));
    if (!currentAcct.empty()) strncpy(req.account_name, currentAcct.c_str(), sizeof(req.account_name));
    APIDefs->Events_Raise(EV_HOARD_QUERY_API, &req);
    g_ArmoryAccount = currentAcct;
    g_ArmoryRefreshNeeded = false;
}

// Fetch one character's crafting disciplines per call. The sweep is spread
// across frames — /v2/characters has no bulk form, so a 20-character account
// is 20 small requests and must not be fired all at once.
void RefreshCharacterCrafting() {
    if (!g_HoardDataAvailable) return;

    // Reclaim a slot whose reply never arrived, then honour any back-off asked
    // for by the last unusable reply. Both must precede ClaimCraftingSlot.
    ExpireStaleCraftingSlot();
    if (CraftingCooldownActive()) return;

    std::string currentAcct = CraftyLegend::GW2API::GetCurrentAccountName();
    if (currentAcct.empty()) return;

    // Only sweep when the account's cache is missing, expired, or forced.
    if (g_CraftingAccount != currentAcct) {
        g_CraftingAccount = currentAcct;
        g_CraftingRefreshNeeded = true;
    }
    if (!g_CraftingRefreshNeeded &&
        !CraftyLegend::CharacterCrafting::IsStale(currentAcct)) {
        return;
    }

    std::string character;
    if (!CraftyLegend::CharacterCrafting::NextPendingCharacter(currentAcct, character)) {
        g_CraftingRefreshNeeded = false; // sweep finished (or account denied)
        return;
    }

    if (!ClaimCraftingSlot(character)) return; // a request is already in flight

    std::string endpoint = "/v2/characters/" +
        CraftyLegend::CharacterCrafting::UrlEncodePathSegment(character) +
        "/crafting";

    HoardQueryApiRequest req{};
    req.api_version = HOARD_API_VERSION;
    strncpy(req.requester, "Crafty Legend", sizeof(req.requester));
    strncpy(req.endpoint, endpoint.c_str(), sizeof(req.endpoint) - 1);
    strncpy(req.response_event, CL_CRAFTING_RESPONSE, sizeof(req.response_event));
    strncpy(req.account_name, currentAcct.c_str(), sizeof(req.account_name) - 1);
    APIDefs->Events_Raise(EV_HOARD_QUERY_API, &req);
}

/*
 * HoardAndSeekAPI.h - Cross-addon event interface for Hoard & Seek
 * Version: 1
 */

#pragma once
#include <cstdint>

#define HOARD_API_VERSION 1
#define HOARD_REFRESH_COOLDOWN 300  // 5 minutes

// Broadcasts (raised by H&S)
#define EV_HOARD_DATA_UPDATED    "EV_HOARD_DATA_UPDATED"
#define EV_HOARD_FETCH_PROGRESS  "EV_HOARD_FETCH_PROGRESS"
#define EV_HOARD_FETCH_ERROR     "EV_HOARD_FETCH_ERROR"
#define EV_HOARD_PONG            "EV_HOARD_PONG"

// Requests (raised by your addon)
#define EV_HOARD_PING               "EV_HOARD_PING"
#define EV_HOARD_SEARCH             "EV_HOARD_SEARCH"
#define EV_HOARD_QUERY_ITEM         "EV_HOARD_QUERY_ITEM"
#define EV_HOARD_QUERY_WALLET       "EV_HOARD_QUERY_WALLET"
#define EV_HOARD_QUERY_ACHIEVEMENT  "EV_HOARD_QUERY_ACHIEVEMENT"
#define EV_HOARD_QUERY_MASTERY      "EV_HOARD_QUERY_MASTERY"

#pragma pack(push, 1)

struct HoardDataReadyPayload {
    uint32_t api_version;
    uint32_t item_count;
    uint32_t currency_count;
    int64_t  last_updated;          // Unix timestamp of last successful fetch (0 if never)
    int64_t  refresh_available_at;  // Unix timestamp when next refresh allowed (0 = now)
};

struct HoardQueryItemRequest {
    uint32_t api_version;
    uint32_t item_id;
    char response_event[64];
};

struct HoardItemLocationEntry {
    char location[64];
    char sublocation[64];
    int32_t count;
};

struct HoardQueryItemResponse {
    uint32_t api_version;
    uint32_t item_id;
    char name[128];
    char rarity[32];
    char type[32];
    int32_t total_count;
    uint32_t location_count;
    HoardItemLocationEntry locations[32];
};

struct HoardQueryWalletRequest {
    uint32_t api_version;
    uint32_t currency_id;
    char response_event[64];
};

struct HoardQueryWalletResponse {
    uint32_t api_version;
    uint32_t currency_id;
    char name[128];
    int32_t amount;
    bool found;
};

// --- Achievement proxy query ---

struct HoardQueryAchievementRequest {
    uint32_t api_version;
    uint32_t ids[200];
    uint32_t id_count;
    char response_event[64];
};

struct HoardAchievementEntry {
    uint32_t id;
    int32_t current;
    int32_t max;
    bool done;
    uint32_t bits[64];
    uint32_t bit_count;
};

struct HoardQueryAchievementResponse {
    uint32_t api_version;
    uint32_t entry_count;
    HoardAchievementEntry entries[200];
};

// --- Mastery proxy query ---

struct HoardQueryMasteryRequest {
    uint32_t api_version;
    uint32_t ids[200];
    uint32_t id_count;
    char response_event[64];
};

struct HoardMasteryEntry {
    uint32_t id;
    int32_t level;
};

struct HoardQueryMasteryResponse {
    uint32_t api_version;
    uint32_t entry_count;
    HoardMasteryEntry entries[200];
};

// --- Pong response payload ---

struct HoardPongPayload {
    uint32_t api_version;
    int64_t  last_updated;          // Unix timestamp of last successful fetch (0 if never)
    int64_t  refresh_available_at;  // Unix timestamp when next refresh allowed (0 = now)
    uint8_t  has_data;              // 1 if account data is loaded, 0 otherwise
};

// --- Fetch progress broadcast ---

struct HoardFetchProgressPayload {
    uint32_t api_version;
    char message[128];
    float progress;             // -1.0 = indeterminate, or 0.0-1.0
};

// --- Fetch error broadcast ---

struct HoardFetchErrorPayload {
    uint32_t api_version;
    char message[256];
};

#pragma pack(pop)

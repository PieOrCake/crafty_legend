#pragma once
#include <windows.h>
#include <imgui.h>
#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <chrono>
#include <atomic>
#include "DataManager.h"
#include "../include/nexus/Nexus.h"
#include "../include/mumble/Mumble.h"

// Quick Access / texture identifiers
#define QA_ID           "QA_CRAFTY_LEGEND"
#define TEX_ANVIL       "TEX_CRAFTY_ANVIL"
#define TEX_ANVIL_HOVER "TEX_CRAFTY_ANVIL_HOVER"

// H&S event response channel names
#define CL_ITEM_RESPONSE        "CL_ITEM_RESPONSE"
#define CL_WALLET_RESPONSE      "CL_WALLET_RESPONSE"
#define CL_ACHIEVEMENT_RESPONSE "CL_ACHIEVEMENT_RESPONSE"
#define CL_MASTERY_RESPONSE     "CL_MASTERY_RESPONSE"
#define CL_ACCOUNTS_RESPONSE    "CL_ACCOUNTS_RESPONSE"
#define CL_ARMORY_RESPONSE      "CL_ARMORY_RESPONSE"
#define CL_CRAFTING_RESPONSE    "CL_CRAFTING_RESPONSE"

// Shopping list entry
struct ShoppingEntry {
    uint32_t item_id;
    std::string name;
    int required;
    int owned;
    int tp_price;
    bool is_vendor;
};

enum class ShoppingSort { Name, Price };

// -------------------------------------------------------------------------
// Core Nexus / Addon globals
// -------------------------------------------------------------------------
extern HMODULE hSelf;
extern AddonDefinition_t AddonDef;
extern AddonAPI_t* APIDefs;
extern bool g_WindowVisible;

// -------------------------------------------------------------------------
// UI state
// -------------------------------------------------------------------------
extern bool g_ShowIngredients;
extern bool g_ShowAcquisition;
extern bool g_ShowRecipes;
extern char g_SearchFilter[256];
extern bool g_ShowItemIcons;
// How the legendary list treats items you already have. The armoury holds more
// than one copy of most things (4 one-handers, 7 runes...), so "owned" is
// ambiguous — the user picks which meaning they want.
enum OwnedFilterMode {
    OWNED_FILTER_SHOW_ALL = 0,  // never hide anything
    OWNED_FILTER_HIDE_ANY = 1,  // hide once the armoury holds a single copy
    OWNED_FILTER_HIDE_FULL = 2  // hide only once the armoury is full
};
extern int g_OwnedFilterMode;
extern bool g_ShowQAIcon;
extern bool g_UsePieTheme;
extern bool g_UseTreeLayout; // false = Miller columns (default), true = expanding tree
extern bool g_FirstRunNoticeDone;  // one-time missing-dependency notice shown
extern bool  g_UseCustomFont;  // use the bundled Inter TTF instead of the Nexus font
extern float g_CustomFontSize; // body pixel size for the bundled font
extern float g_PrereqPanelHeight; // user-resizable prerequisites pane height (px)

// Per-layout window geometry. The Miller and Tree layouts each remember their own
// window position and size; the main window is restored to the active layout's
// geometry whenever the layout is switched. w<=0 or h<=0 means "not yet recorded".
struct WindowGeom { float x = 0.0f, y = 0.0f, w = 0.0f, h = 0.0f;
                    bool valid() const { return w > 0.0f && h > 0.0f; } };
extern WindowGeom g_MillerGeom;
extern WindowGeom g_TreeGeom;

// Debug window
extern bool g_ShowDebugWindow;
extern std::vector<std::string> g_DebugLog;
extern std::unordered_set<uint32_t> g_LoggedIconRequests;
inline constexpr size_t MAX_DEBUG_LINES = 1000;

// Miller Columns
extern std::vector<CraftyLegend::ColumnData> g_Columns;
extern int g_SelectedColumn;

// Scroll animation
extern bool g_ScrollToEnd;
extern bool g_ScrollAnimating;
extern float g_ScrollStartX;
extern float g_ScrollTargetX;
extern double g_ScrollAnimStartTime;
extern float g_PreClickScrollX;
inline constexpr float SCROLL_ANIM_DURATION = 0.5f;

// Session scroll restore
extern bool g_PendingScrollRestore;
extern float g_PendingScrollX;
extern float g_PendingCol0ScrollY;
extern std::vector<float> g_PendingColScrollY;
extern float g_TrackedScrollX;
extern float g_TrackedCol0ScrollY;
extern std::vector<float> g_TrackedColScrollY;

// Prerequisites panel
extern std::vector<CraftyLegend::Prerequisite> g_Prerequisites;
extern uint32_t g_PrereqLegendaryId;
extern bool g_PrereqDirty;

// Shopping list
extern bool g_ShowShoppingList;
extern std::vector<ShoppingEntry> g_ShoppingList;
extern uint32_t g_ShoppingListLegendaryId;
extern bool g_ShoppingListDirty;
extern ShoppingSort g_ShoppingSort;

// -------------------------------------------------------------------------
// Hoard & Seek state
// -------------------------------------------------------------------------
extern bool g_HoardDetected;
extern bool g_HoardDataAvailable;
extern bool g_HoardRefreshNeeded;
extern bool g_HoardRefreshPending;
extern bool g_HoardFetching;
extern std::string g_HoardFetchMessage;
extern bool g_HoardFetchError;
extern std::string g_HoardErrorMessage;
extern bool g_HoardPingPending;
extern bool g_HoardPingFailed;
extern std::chrono::steady_clock::time_point g_HoardPingTime;
extern std::chrono::steady_clock::time_point g_StartupPingRetryTime;
extern bool g_StartupPingDone;
extern std::chrono::steady_clock::time_point g_StatusMessageTime;
extern int64_t g_HoardLastUpdated;
extern int64_t g_HoardRefreshAvailableAt;
extern std::unordered_set<uint32_t> g_HoardQueriedItems;
extern std::unordered_set<uint32_t> g_HoardQueriedWallets;
extern bool g_HoardPermissionPending;
extern bool g_HoardPermissionDenied;
extern std::chrono::steady_clock::time_point g_HoardPermissionRetryTime;
extern bool g_HoardBusyBackoff;
extern std::chrono::steady_clock::time_point g_HoardBusyRetryAt;

// -------------------------------------------------------------------------
// Account detection (MumbleLink + H&S)
// -------------------------------------------------------------------------
extern uint32_t g_HoardAccountCount;
extern Mumble::Data* g_MumbleData;
extern std::string g_CurrentCharacterName;
extern std::unordered_map<std::string, std::string> g_CharToAccount;
extern std::vector<std::string> g_AccountNames;
extern std::unordered_map<std::string, std::string> g_AccountLabels;
extern bool g_AccountDetectionDone;
extern std::string g_MasteryAchievementAccount;
extern bool g_MasteryAchievementRefreshNeeded;
extern std::string g_WalletAccount;
extern bool g_WalletRefreshNeeded;
extern std::string g_ArmoryAccount;
extern bool g_ArmoryRefreshNeeded;

// Character crafting-discipline sweep (one character per request)
extern std::string       g_CraftingAccount;      // account the sweep is running for
extern std::atomic<bool> g_CraftingRefreshNeeded;

// -------------------------------------------------------------------------
// Completion cache
// -------------------------------------------------------------------------
extern std::unordered_map<uint32_t, float> g_CompletionCache;
extern bool g_CompletionCacheDirty;
extern std::vector<uint32_t> g_CompletionQueue;
inline constexpr int COMPLETION_BATCH_SIZE = 5;

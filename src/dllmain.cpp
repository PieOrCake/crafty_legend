#include "globals.h"

// Version constants
#define V_MAJOR    0
#define V_MINOR    9
#define V_BUILD    6
#define V_REVISION 0

// -------------------------------------------------------------------------
// Global variable definitions (declared extern in globals.h)
// -------------------------------------------------------------------------

HMODULE hSelf;
AddonDefinition_t AddonDef{};
AddonAPI_t* APIDefs = nullptr;
bool g_WindowVisible = false;

// UI State
bool g_ShowIngredients    = true;
bool g_ShowAcquisition    = true;
bool g_ShowRecipes        = true;
char g_SearchFilter[256]  = "";
bool g_ShowItemIcons      = false;
bool g_ShowOwnedLegendaries = true;
bool g_ShowQAIcon         = true;
bool g_UsePieTheme        = true;

// Debug window
bool g_ShowDebugWindow = false;
std::vector<std::string> g_DebugLog;
std::unordered_set<uint32_t> g_LoggedIconRequests;

// Miller Columns
std::vector<CraftyLegend::ColumnData> g_Columns;
int g_SelectedColumn = 0;

// Scroll animation
bool  g_ScrollToEnd        = false;
bool  g_ScrollAnimating    = false;
float g_ScrollStartX       = 0.0f;
float g_ScrollTargetX      = 0.0f;
double g_ScrollAnimStartTime = 0.0;
float g_PreClickScrollX    = 0.0f;

// Session scroll restore
bool  g_PendingScrollRestore  = false;
float g_PendingScrollX        = 0.0f;
float g_PendingCol0ScrollY    = 0.0f;
std::vector<float> g_PendingColScrollY;
float g_TrackedScrollX        = 0.0f;
float g_TrackedCol0ScrollY    = 0.0f;
std::vector<float> g_TrackedColScrollY;

// Prerequisites panel
std::vector<CraftyLegend::Prerequisite> g_Prerequisites;
uint32_t g_PrereqLegendaryId = 0;
bool     g_PrereqDirty       = false;

// Shopping list
bool g_ShowShoppingList        = false;
std::vector<ShoppingEntry> g_ShoppingList;
uint32_t g_ShoppingListLegendaryId = 0;
bool     g_ShoppingListDirty   = true;
ShoppingSort g_ShoppingSort    = ShoppingSort::Name;

// Hoard & Seek state
bool  g_HoardDetected          = false;
bool  g_HoardDataAvailable     = false;
bool  g_HoardRefreshNeeded     = false;
bool  g_HoardRefreshPending    = false;
bool  g_HoardFetching          = false;
std::string g_HoardFetchMessage;
bool  g_HoardFetchError        = false;
std::string g_HoardErrorMessage;
bool  g_HoardPingPending       = false;
bool  g_HoardPingFailed        = false;
std::chrono::steady_clock::time_point g_HoardPingTime;
std::chrono::steady_clock::time_point g_StartupPingRetryTime;
bool  g_StartupPingDone        = false;
std::chrono::steady_clock::time_point g_StatusMessageTime;
int64_t g_HoardLastUpdated     = 0;
int64_t g_HoardRefreshAvailableAt = 0;
std::unordered_set<uint32_t> g_HoardQueriedItems;
std::unordered_set<uint32_t> g_HoardQueriedWallets;
bool  g_HoardPermissionPending = false;
bool  g_HoardPermissionDenied  = false;
std::chrono::steady_clock::time_point g_HoardPermissionRetryTime;
bool  g_HoardBusyBackoff       = false;
std::chrono::steady_clock::time_point g_HoardBusyRetryAt;

// Account detection
uint32_t g_HoardAccountCount = 0;
Mumble::Data* g_MumbleData   = nullptr;
std::string g_CurrentCharacterName;
std::unordered_map<std::string, std::string> g_CharToAccount;
std::vector<std::string> g_AccountNames;
std::unordered_map<std::string, std::string> g_AccountLabels;
bool  g_AccountDetectionDone          = false;
std::string g_MasteryAchievementAccount;
bool  g_MasteryAchievementRefreshNeeded = false;
std::string g_WalletAccount;
bool  g_WalletRefreshNeeded           = false;
std::string g_ArmoryAccount;
bool  g_ArmoryRefreshNeeded           = false;

// Completion cache
std::unordered_map<uint32_t, float> g_CompletionCache;
bool g_CompletionCacheDirty = true;
std::vector<uint32_t> g_CompletionQueue;

// -------------------------------------------------------------------------
// DLL entry point
// -------------------------------------------------------------------------
BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved) {
    switch (ul_reason_for_call) {
    case DLL_PROCESS_ATTACH: hSelf = hModule; break;
    case DLL_PROCESS_DETACH: break;
    case DLL_THREAD_ATTACH:  break;
    case DLL_THREAD_DETACH:  break;
    }
    return TRUE;
}

// -------------------------------------------------------------------------
// Addon export — version constants and AddonDef must stay in this file
// -------------------------------------------------------------------------
void AddonLoad(AddonAPI_t* aApi);
void AddonUnload();
void ProcessKeybind(const char* aIdentifier, bool aIsRelease);
void AddonRender();
void AddonOptions();

extern "C" __declspec(dllexport) AddonDefinition_t* GetAddonDef() {
    AddonDef.Signature   = 0x9ae2a2d9;
    AddonDef.APIVersion  = NEXUS_API_VERSION;
    AddonDef.Name        = "Crafty Legend";
    AddonDef.Version.Major    = V_MAJOR;
    AddonDef.Version.Minor    = V_MINOR;
    AddonDef.Version.Build    = V_BUILD;
    AddonDef.Version.Revision = V_REVISION;
    AddonDef.Author      = "PieOrCake.7635";
    AddonDef.Description = "Legendary crafting assistant. Requires Hoard & Seek addon for account data retrieval.";
    AddonDef.Load        = AddonLoad;
    AddonDef.Unload      = AddonUnload;
    AddonDef.Flags       = AF_None;
    AddonDef.Provider    = UP_GitHub;
    AddonDef.UpdateLink  = "https://github.com/PieOrCake/crafty_legend";
    return &AddonDef;
}

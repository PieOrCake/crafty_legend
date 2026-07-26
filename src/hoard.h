#pragma once
#include <string>

// H&S event callbacks (subscribed/unsubscribed in AddonLoad/AddonUnload)
void OnHoardPong(void* aEventArgs);
void OnHoardDataUpdated(void* aEventArgs);
void OnHoardItemResponse(void* aEventArgs);
void OnHoardWalletResponse(void* aEventArgs);
void OnAchievementResponse(void* aEventArgs);
void OnMasteryResponse(void* aEventArgs);
void OnHoardFetchProgress(void* aEventArgs);
void OnHoardFetchError(void* aEventArgs);
void OnAccountsChanged(void* aEventArgs);
void OnAccountsResponse(void* aEventArgs);
void OnMumbleIdentityUpdated(void* aEventArgs);
void OnHoardArmoryResponse(void* aEventArgs);
void OnCharacterCraftingResponse(void* aEventArgs);

// Refresh helpers called from the render loop (ui.cpp)
void TryResolveCurrentAccount();
void RefreshHoardData();
void RefreshWallet();
void RefreshMasteriesAndAchievements();
void RefreshLegendaryArmory();
void RefreshCharacterCrafting();
void ResetCraftingFailures(const std::string& account);

// Account display helper called from ui.cpp
std::string GetAccountDisplayName(const std::string& account_name);

// Why the character-crafting sweep is (or is not) able to produce data. The
// tooltip in ui_helpers.cpp reads this so each failure state can be described
// honestly instead of showing "Loading..." forever. The two latched states are
// set for the session only; nothing about them is persisted to disk.
enum class CraftingSweepState {
    Active,           // running normally (or already finished)
    NoHoard,          // Hoard & Seek absent or holding no account data
    VersionTooOld,    // latched off: H&S's api_version can never answer us
    HoardPermissionDenied // latched off: H&S refused Crafty Legend permission
};
CraftingSweepState GetCraftingSweepState();

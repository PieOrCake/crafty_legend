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

// Refresh helpers called from the render loop (ui.cpp)
void TryResolveCurrentAccount();
void RefreshHoardData();
void RefreshWallet();
void RefreshMasteriesAndAchievements();
void RefreshLegendaryArmory();

// Account display helper called from ui.cpp
std::string GetAccountDisplayName(const std::string& account_name);

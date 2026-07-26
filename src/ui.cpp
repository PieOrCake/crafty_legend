#include "globals.h"
#include "ui_helpers.h"
#include "hoard.h"
#include "settings.h"
#include "GW2API.h"
#include "DataManager.h"
#include "IconManager.h"
#include "../include/HoardAndSeekAPI.h"
#include <algorithm>
#include <map>
#include <sstream>
#include <vector>
#include <shellapi.h>
#include "PieTheme.h"
#include "Localization.h"
#include "ui_tree.h"
#include "FontManager.h"
#include <cstring>

// Crafty Legend's built-in purple/gold style, built once from the ambient ImGui
// style plus CL's colour and rounding customisations. Keeping a full ImGuiStyle
// lets us overlay Pie UI's entire colour palette in a single assignment when the
// Pie theme is active, rather than stacking a second set of PushStyleColor calls
// on top of our own. When Pie is absent/off this reproduces the original look.
static ImGuiStyle              g_BaseStyle;
static bool                    g_BaseStyleBuilt = false;
static std::vector<ImGuiStyle> g_StyleStack;

static void BuildBaseStyle() {
    g_BaseStyle = ImGui::GetStyle();
    ImGuiStyle& s = g_BaseStyle;
    s.Colors[ImGuiCol_WindowBg]             = ImVec4(0.08f, 0.06f, 0.12f, 0.95f);
    s.Colors[ImGuiCol_TitleBg]              = ImVec4(0.14f, 0.08f, 0.20f, 1.0f);
    s.Colors[ImGuiCol_TitleBgActive]        = ImVec4(0.25f, 0.15f, 0.35f, 1.0f);
    s.Colors[ImGuiCol_Border]               = ImVec4(0.40f, 0.28f, 0.55f, 0.5f);
    s.Colors[ImGuiCol_Button]               = ImVec4(0.25f, 0.15f, 0.35f, 0.8f);
    s.Colors[ImGuiCol_ButtonHovered]        = ImVec4(0.35f, 0.22f, 0.48f, 0.9f);
    s.Colors[ImGuiCol_ButtonActive]         = ImVec4(0.45f, 0.30f, 0.58f, 1.0f);
    s.Colors[ImGuiCol_Header]               = ImVec4(0.55f, 0.45f, 0.12f, 0.6f);
    s.Colors[ImGuiCol_HeaderHovered]        = ImVec4(0.65f, 0.52f, 0.15f, 0.7f);
    s.Colors[ImGuiCol_HeaderActive]         = ImVec4(0.75f, 0.60f, 0.18f, 0.8f);
    s.Colors[ImGuiCol_Separator]            = ImVec4(0.35f, 0.25f, 0.45f, 1.0f);
    s.Colors[ImGuiCol_FrameBg]              = ImVec4(0.14f, 0.10f, 0.20f, 0.8f);
    s.Colors[ImGuiCol_FrameBgHovered]       = ImVec4(0.22f, 0.15f, 0.30f, 0.8f);
    s.Colors[ImGuiCol_ScrollbarBg]          = ImVec4(0.08f, 0.06f, 0.12f, 0.5f);
    s.Colors[ImGuiCol_ScrollbarGrab]        = ImVec4(0.30f, 0.20f, 0.42f, 0.7f);
    s.Colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.40f, 0.28f, 0.55f, 0.8f);
    s.Colors[ImGuiCol_ScrollbarGrabActive]  = ImVec4(0.50f, 0.35f, 0.65f, 1.0f);
    s.WindowRounding    = 6.0f;
    s.FrameRounding     = 3.0f;
    s.ScrollbarRounding = 4.0f;
    g_BaseStyleBuilt = true;
}

static void PushGW2Theme() {
    if (!g_BaseStyleBuilt) BuildBaseStyle();
    g_StyleStack.push_back(ImGui::GetStyle());
    ImGuiStyle themed = g_BaseStyle;
    if (PieTheme::Active()) {
        // Pie ships its entire ImGui colour array; copy it straight into the
        // style (indexed by ImGuiCol_). Clamp to the smaller of both counts so a
        // version skew leaves trailing controls at our defaults, never OOB. Style
        // geometry (rounding/padding) from g_BaseStyle is kept as-is.
        const PieUiTheme p = PieTheme::Palette();
        int n = (int)p.count;
        if (n > ImGuiCol_COUNT)         n = ImGuiCol_COUNT;
        if (n > PIEUI_THEME_MAX_COLORS) n = PIEUI_THEME_MAX_COLORS;
        for (int i = 0; i < n; ++i)
            themed.Colors[i] = ImGui::ColorConvertU32ToFloat4(p.colors[i]);
    }
    ImGui::GetStyle() = themed;
}

static void PopGW2Theme() {
    if (!g_StyleStack.empty()) {
        ImGui::GetStyle() = g_StyleStack.back();
        g_StyleStack.pop_back();
    }
}

struct ThemeGuard {
    ThemeGuard()  { PushGW2Theme(); }
    ~ThemeGuard() { PopGW2Theme(); }
};

// First-run notice: if the user is missing an optional dependency — Hoard & Seek
// (item counts) or Decoder Ring (name translations) — show a one-time modal over
// the Crafty Legend window pointing them at the Nexus library. If BOTH are present
// it is never shown. Dismissal is persisted so it appears at most once.
// Call from inside the main window's Begin/End scope.
static void HandleFirstRunNotice() {
    if (g_FirstRunNoticeDone) return;

    static bool s_timerStarted = false;
    static std::chrono::steady_clock::time_point s_firstVisible;
    if (!s_timerStarted) { s_firstVisible = std::chrono::steady_clock::now(); s_timerStarted = true; }

    static bool s_missHS = false, s_missDR = false, s_opened = false;

    if (!s_opened) {
        bool hs = g_HoardDetected;
        bool dr = Localization::DecoderPresent();
        if (hs && dr) {
            // Both dependencies present: never bother the user.
            g_FirstRunNoticeDone = true;
            SaveDisplaySettings();
            return;
        }
        // H&S is detected via an async ping with no "absent" signal — give it a few
        // seconds to pong before judging it missing (Decoder Ring is synchronous, so
        // once H&S is known present we can decide immediately).
        bool graceElapsed = (std::chrono::steady_clock::now() - s_firstVisible) >= std::chrono::seconds(5);
        bool hsResolved = hs || graceElapsed;
        if (!hsResolved) return;
        s_missHS = !hs;
        s_missDR = !dr;
        s_opened = true;
        ImGui::OpenPopup("Optional Add-ons");
    }

    // Center the modal over the Crafty Legend window.
    ImVec2 wpos = ImGui::GetWindowPos();
    ImVec2 wsz  = ImGui::GetWindowSize();
    ImGui::SetNextWindowPos(ImVec2(wpos.x + wsz.x * 0.5f, wpos.y + wsz.y * 0.5f),
                            ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(430, 0), ImGuiCond_Appearing);

    if (ImGui::BeginPopupModal("Optional Add-ons", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::TextWrapped("Crafty Legend works best with these optional add-ons:");
        ImGui::Spacing();
        // Always list both dependencies so users learn about both, even if only one is missing.
        ImGui::BulletText("Hoard & Seek%s", s_missHS ? "" : " (installed)");
        ImGui::Indent();
        ImGui::TextWrapped("Reads your account so Crafty Legend can show how many of each material you already own.");
        ImGui::Unindent();
        ImGui::Spacing();
        ImGui::BulletText("Decoder Ring%s", s_missDR ? "" : " (installed)");
        ImGui::Indent();
        ImGui::TextWrapped("Translates item and legendary names into your game language (German, French, Spanish).");
        ImGui::Unindent();
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::TextWrapped("Both can be installed from the Nexus add-on library.");
        ImGui::Spacing();
        if (ImGui::Button("Got it", ImVec2(120, 0))) {
            g_FirstRunNoticeDone = true;
            SaveDisplaySettings();
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}

void AddonRender() {
    // Process icon queue even when window is hidden so icons stay loaded
    CraftyLegend::IconManager::Tick();

    // Re-check active language (Decoder Ring / Nexus); refreshes localized caches on change
    Localization::Poll();

    if (!g_WindowVisible) return;

    // Check if ping got a pong — allow up to 2 seconds for H&S to respond
    if (g_HoardPingPending) {
        if (g_HoardDetected) {
            g_HoardPingPending = false;
            g_HoardPingFailed = false;
            // Ping succeeded, now trigger the refresh
            APIDefs->Events_Raise("EV_HOARD_REFRESH", nullptr);
            g_HoardRefreshPending = true;
            g_CompletionCacheDirty = true;
        } else {
            auto elapsed = std::chrono::steady_clock::now() - g_HoardPingTime;
            if (elapsed >= std::chrono::seconds(2)) {
                g_HoardPingPending = false;
                g_HoardPingFailed = true;
                g_StatusMessageTime = std::chrono::steady_clock::now();
            }
        }
    }

    // Permission retry: after 3s delay, clear pending and allow queries to re-fire
    if (g_HoardPermissionPending && std::chrono::steady_clock::now() >= g_HoardPermissionRetryTime) {
        g_HoardPermissionPending = false;
        g_HoardQueriedItems.clear();
        g_HoardQueriedWallets.clear();
        g_HoardRefreshNeeded = true;
    }

    // Retry startup ping if H&S hasn't responded yet
    if (!g_StartupPingDone && !g_HoardPingPending
        && std::chrono::steady_clock::now() >= g_StartupPingRetryTime) {
        APIDefs->Events_Raise(EV_HOARD_PING, nullptr);
        g_StartupPingRetryTime = std::chrono::steady_clock::now() + std::chrono::seconds(2);
    }

    // Poll MumbleLink identity JSON if multi-account and we still don't have a valid character name
    if (g_HoardAccountCount > 1 && g_CurrentCharacterName.empty() && g_MumbleData && g_MumbleData->UITick > 0) {
        // Parse the wchar_t Identity[256] JSON for the "name" field
        std::wstring wIdent(g_MumbleData->Identity);
        std::string ident(wIdent.begin(), wIdent.end());
        if (!ident.empty()) {
            try {
                auto j = nlohmann::json::parse(ident);
                if (j.contains("name") && j["name"].is_string()) {
                    std::string charName = j["name"].get<std::string>();
                    if (!charName.empty() && charName != g_CurrentCharacterName) {
                        g_CurrentCharacterName = charName;
                        TryResolveCurrentAccount();
                    }
                }
            } catch (...) {}
        }
    }

    // While H&S has signalled rate-limit backoff, defer all retries until the
    // suggested retry time has passed.
    bool busyBlocked = g_HoardBusyBackoff
        && std::chrono::steady_clock::now() < g_HoardBusyRetryAt;
    if (g_HoardBusyBackoff && !busyBlocked) {
        g_HoardBusyBackoff = false;
    }

    // Trigger H&S batch query if needed
    if (g_HoardRefreshNeeded && g_HoardDataAvailable && !busyBlocked) {
        RefreshHoardData();
    }

    // Re-query masteries/achievements if account changed
    if (g_MasteryAchievementRefreshNeeded && g_HoardDataAvailable && !busyBlocked) {
        RefreshMasteriesAndAchievements();
    }

    // Re-query wallet if account changed
    if (g_WalletRefreshNeeded && g_HoardDataAvailable && !busyBlocked) {
        RefreshWallet();
    }

    // Sweep character crafting disciplines, one character per frame
    if (g_HoardDataAvailable && !busyBlocked) {
        RefreshCharacterCrafting();
    }

    // Recompute prerequisites when achievement/mastery data arrives
    if (g_PrereqDirty && g_PrereqLegendaryId != 0) {
        g_Prerequisites = CraftyLegend::DataManager::GetPrerequisites(g_PrereqLegendaryId);
        g_ShoppingListDirty = true;
        g_CompletionCacheDirty = true;
    }
    g_PrereqDirty = false;

    // When completion cache is dirty, queue all legendaries for amortized recomputation
    if (g_CompletionCacheDirty && CraftyLegend::GW2API::HasAccountData()) {
        g_CompletionCacheDirty = false;
        g_CompletionQueue.clear();
        const auto& legs = CraftyLegend::DataManager::GetLegendaries();
        for (const auto& leg : legs) {
            g_CompletionQueue.push_back(leg.id);
        }
    }
    // Process a small batch of completion recomputations this frame
    TickCompletionQueue();

    // Initialize columns if empty
    if (g_Columns.empty()) {
        try {
            const auto& legendaries = CraftyLegend::DataManager::GetLegendaries();
            if (legendaries.empty()) {
                if (!CraftyLegend::DataManager::Initialize()) return;
            }
            CraftyLegend::DataManager::InitializeColumns();
            CraftyLegend::DataManager::RestoreSession();
            g_Columns = CraftyLegend::DataManager::GetColumns();
            // Restore scroll positions from session
            CraftyLegend::DataManager::GetSessionScrollState(g_PendingScrollX, g_PendingCol0ScrollY, g_PendingColScrollY);
            g_PendingScrollRestore = true;
            // Restore prerequisites for the selected legendary
            if (!g_Columns.empty() && g_Columns[0].selected_index >= 0 &&
                g_Columns[0].selected_index < static_cast<int>(g_Columns[0].items.size())) {
                uint32_t legId = g_Columns[0].items[g_Columns[0].selected_index].id;
                g_PrereqLegendaryId = legId;
                g_Prerequisites = CraftyLegend::DataManager::GetPrerequisites(legId);
            }
        } catch (...) {
            return;
        }
    }

    // Purple and Gold color palette
    ImVec4 titleColor(0.90f, 0.78f, 0.30f, 1.0f);        // Bright gold
    ImVec4 separatorColor(0.35f, 0.25f, 0.45f, 1.0f);     // Muted purple
    ImVec4 sectionHeaderColor(0.80f, 0.68f, 0.28f, 1.0f);  // Rich gold
    ImVec4 subtypeColor(0.55f, 0.48f, 0.65f, 1.0f);        // Soft lavender
    ImVec4 colBgColor(0.12f, 0.10f, 0.18f, 0.6f);          // Dark purple bg
    ImVec4 colHeaderBg(0.25f, 0.16f, 0.35f, 0.9f);         // Purple header band
    ImVec4 completedColor(0.35f, 0.82f, 0.35f, 1.0f);      // Completed items
    ImVec4 readyColor(0.35f, 0.78f, 0.88f, 1.0f);          // Ready to craft
    ImVec4 dimTextColor(0.52f, 0.48f, 0.58f, 1.0f);        // Dimmed lavender

    // Custom-drawn Miller-column panels/headers/titles use the raw literals
    // above rather than the ImGui style, so they don't pick up the Pie palette
    // automatically. When the Pie UI theme is active, remap these decorative
    // colours from the already-applied Pie style (ThemeGuard ran before us) plus
    // Pie's accent. Semantic status colours (completed/ready/errors) are left
    // alone so green/cyan/red keep their meaning under any theme.
    ImU32 headerAccentLine = IM_COL32(200, 170, 60, 120);  // gold line under column headers
    if (PieTheme::Active()) {
        // IMPORTANT: this block runs BEFORE ThemeGuard applies the Pie palette to
        // ImGui's style (a dozen lines below), so ImGui::GetStyle() here is still
        // the default washed-out style — reading colours from it produces a pale
        // blue-grey. Read straight from Pie's palette instead.
        //
        // Drive the Miller-column header band and its border from Pie's Button
        // colour: Pie's resting button is a dark, saturated dimmed-accent (not the
        // muted tab or the bright trim), which reads well as a solid band. Heading
        // text stays white for guaranteed contrast on that band. The faint panel
        // behind the rows keeps a dark accent-derived tint so it sits below the
        // band. Semantic status colours are left alone.
        const PieUiTheme p = PieTheme::Palette();
        auto slot = [&](int i, const ImVec4& fb) {
            return (i >= 0 && (uint32_t)i < p.count) ? PieTheme::Unpack(p.colors[i]) : fb;
        };
        ImVec4 acc         = PieTheme::Unpack(PieTheme::Accent());
        ImVec4 btn         = slot(ImGuiCol_Button, ImVec4(acc.x * 0.38f, acc.y * 0.38f, acc.z * 0.38f, 1.0f)); // dark dimmed-accent band
        ImVec4 dis         = slot(ImGuiCol_TextDisabled, dimTextColor);
        titleColor         = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);                          // white heading text
        sectionHeaderColor = ImVec4(acc.x, acc.y, acc.z, 1.0f);                       // category labels = trim
        colHeaderBg        = ImVec4(btn.x, btn.y, btn.z, 0.95f);                      // button-colour header band
        colBgColor         = ImVec4(acc.x * 0.14f, acc.y * 0.14f, acc.z * 0.14f, 0.55f); // faint panel
        separatorColor     = ImVec4(btn.x, btn.y, btn.z, 1.0f);                       // button-colour border
        subtypeColor       = dis;
        dimTextColor       = dis;
        headerAccentLine   = ImGui::ColorConvertFloat4ToU32(ImVec4(btn.x, btn.y, btn.z, 1.0f)); // button-colour under-line
    }

    ThemeGuard themeGuard;

    // Default first-use geometry. Must come BEFORE the per-layout override below:
    // ImGui keeps only the most recent SetNextWindowSize before Begin, so the
    // Always override has to be the last size call to win.
    ImGui::SetNextWindowSize(ImVec2(1100, 500), ImGuiCond_FirstUseEver);

    // Per-layout window geometry: when the layout changes, restore the incoming
    // layout's remembered position/size (the outgoing one was captured live below).
    static bool s_lastLayoutTree = g_UseTreeLayout;
    if (g_UseTreeLayout != s_lastLayoutTree) {
        s_lastLayoutTree = g_UseTreeLayout;
        const WindowGeom& incoming = g_UseTreeLayout ? g_TreeGeom : g_MillerGeom;
        if (incoming.valid()) {
            ImGui::SetNextWindowPos(ImVec2(incoming.x, incoming.y), ImGuiCond_Always);
            ImGui::SetNextWindowSize(ImVec2(incoming.w, incoming.h), ImGuiCond_Always);
        }
        SaveDisplaySettings(); // persist the layout switch + latest geometries
    }

    if (ImGui::Begin("Crafty Legend", &g_WindowVisible, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse)) {

        // Capture this frame's geometry into the active layout's slot so it can be
        // restored when the user switches away and back.
        {
            ImVec2 wp = ImGui::GetWindowPos();
            ImVec2 ws = ImGui::GetWindowSize();
            WindowGeom& cur = g_UseTreeLayout ? g_TreeGeom : g_MillerGeom;
            cur.x = wp.x; cur.y = wp.y; cur.w = ws.x; cur.h = ws.y;
        }

        // Bundled custom font (main window only). Pushed here and popped just
        // before End; nullptr (still downloading / disabled) leaves the ambient
        // Nexus font in place. Balanced within the window's Begin/End.
        bool pushedFont = false;
        if (g_UseCustomFont) {
            if (ImFont* bodyFont = CraftyLegend::FontManager::GetBundled(g_CustomFontSize)) {
                ImGui::PushFont(bodyFont);
                pushedFont = true;
            }
        }

        // First-run optional-dependency notice (shown once if H&S or Decoder Ring is missing)
        HandleFirstRunNotice();

        // Account data & TP prices button row
        {
            auto priceFetchStatus = CraftyLegend::GW2API::GetPriceFetchStatus();
            bool priceFetching = (priceFetchStatus == CraftyLegend::FetchStatus::InProgress);

            // Shopping List toggle (before refresh buttons)
            {
                bool canShow = (g_PrereqLegendaryId != 0);
                if (!canShow) ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 0.4f);
                if (ImGui::SmallButton(g_ShowShoppingList ? Localization::Tr("Hide Shopping List") : Localization::Tr("Show Shopping List")) && canShow) {
                    g_ShowShoppingList = !g_ShowShoppingList;
                    if (g_ShowShoppingList) g_ShoppingListDirty = true;
                }
                if (!canShow) ImGui::PopStyleVar();
                ImGui::SameLine();
            }

            // Refresh buttons
            bool onCooldown = (g_HoardRefreshAvailableAt > 0 && std::time(nullptr) < (time_t)g_HoardRefreshAvailableAt);
            bool busy = priceFetching || g_HoardRefreshPending || g_HoardPingPending || onCooldown;
            if (busy) ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 0.5f);
            if (ImGui::SmallButton(Localization::Tr("Update Account Data")) && !busy) {
                // Always ping to verify H&S is still loaded
                g_HoardDetected = false;
                APIDefs->Events_Raise(EV_HOARD_PING, nullptr);
                g_HoardPingPending = true;
                g_HoardPingFailed = false;
                g_HoardPingTime = std::chrono::steady_clock::now();
            }
            ImGui::SameLine();
            if (ImGui::SmallButton(Localization::Tr("Refresh TP Prices")) && !priceFetching && !g_HoardRefreshPending) {
                auto ids = CraftyLegend::DataManager::GetAllTradeableItemIds();
                CraftyLegend::GW2API::FetchPricesAsync(ids);
            }
            if (busy) ImGui::PopStyleVar();

            // Auto-expire transient status messages after 5 seconds
            auto statusAge = std::chrono::steady_clock::now() - g_StatusMessageTime;
            bool statusExpired = statusAge >= std::chrono::seconds(5);

            // Status message
            if (g_HoardPingPending) {
                ImGui::SameLine();
                ImGui::TextColored(titleColor, "%s", Localization::Tr("Looking for Hoard & Seek..."));
            } else if (g_HoardFetching && !g_HoardFetchMessage.empty()) {
                ImGui::SameLine();
                ImGui::TextColored(titleColor, "%s", g_HoardFetchMessage.c_str());
            } else if (g_HoardRefreshPending) {
                ImGui::SameLine();
                ImGui::TextColored(titleColor, "%s", Localization::Tr("Waiting for Hoard & Seek..."));
            } else if (priceFetching) {
                ImGui::SameLine();
                ImGui::TextColored(titleColor, "%s",
                    CraftyLegend::GW2API::GetPriceFetchMessage().c_str());
            } else if (g_HoardPermissionPending) {
                ImGui::SameLine();
                ImGui::TextColored(titleColor, "%s", Localization::Tr("Waiting for Hoard & Seek permission..."));
            } else if (g_HoardPermissionDenied) {
                ImGui::SameLine();
                ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "%s", Localization::Tr("Hoard & Seek permission denied"));
            } else if (!statusExpired && g_HoardPingFailed) {
                ImGui::SameLine();
                ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "%s", Localization::Tr("Hoard & Seek addon not found"));
            } else if (!statusExpired && g_HoardFetchError) {
                ImGui::SameLine();
                ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "%s", g_HoardErrorMessage.c_str());
            } else if (!statusExpired && priceFetchStatus == CraftyLegend::FetchStatus::Success) {
                ImGui::SameLine();
                ImGui::TextColored(completedColor, "%s",
                    CraftyLegend::GW2API::GetPriceFetchMessage().c_str());
            } else if (!statusExpired && priceFetchStatus == CraftyLegend::FetchStatus::Error) {
                ImGui::SameLine();
                ImGui::TextColored(ImVec4(0.9f, 0.35f, 0.3f, 1.0f), "%s",
                    CraftyLegend::GW2API::GetPriceFetchMessage().c_str());
            } else if (g_HoardLastUpdated > 0) {
                time_t now = std::time(nullptr);
                int elapsed = (int)(now - (time_t)g_HoardLastUpdated);
                const char* ago;
                char buf[64];
                if (elapsed < 60) ago = "just now";
                else if (elapsed < 3600) { snprintf(buf, sizeof(buf), "%dm ago", elapsed / 60); ago = buf; }
                else if (elapsed < 86400) { snprintf(buf, sizeof(buf), "%dh ago", elapsed / 3600); ago = buf; }
                else { snprintf(buf, sizeof(buf), "%dd ago", elapsed / 86400); ago = buf; }
                ImGui::SameLine();
                if (onCooldown) {
                    int remaining = (int)((time_t)g_HoardRefreshAvailableAt - now);
                    ImGui::TextColored(dimTextColor, "(Account data: %s | Next refresh available in %dm %ds)", ago, remaining / 60, remaining % 60);
                } else {
                    ImGui::TextColored(dimTextColor, "(Account data: %s)", ago);
                }
            } else if (g_HoardDataAvailable) {
                ImGui::SameLine();
                ImGui::TextColored(dimTextColor, "%s", Localization::Tr("(Hoard & Seek connected)"));
            } else if (CraftyLegend::GW2API::HasPriceData()) {
                ImGui::SameLine();
                ImGui::TextColored(dimTextColor, "%s", Localization::Tr("(cached prices loaded)"));
            }

            // Detect price fetch completion and start status timer
            static auto lastPriceFetchStatus = CraftyLegend::FetchStatus::Idle;
            if (priceFetchStatus != lastPriceFetchStatus) {
                if (priceFetchStatus == CraftyLegend::FetchStatus::Success ||
                    priceFetchStatus == CraftyLegend::FetchStatus::Error) {
                    g_StatusMessageTime = std::chrono::steady_clock::now();
                }
                lastPriceFetchStatus = priceFetchStatus;
            }
            if (priceFetchStatus == CraftyLegend::FetchStatus::Success) {
                g_ShoppingListDirty = true;
            }
        }

        const auto& legendaries = CraftyLegend::DataManager::GetLegendaries();
        if (legendaries.empty()) {
            ImGui::Text("%s", Localization::Tr("No legendary items loaded."));
        } else {
            float totalAvailHeight = ImGui::GetContentRegionAvail().y;
            // Clamp the user-resizable prereq pane so the content area always
            // keeps a usable minimum (both layouts share this height).
            if (!g_Prerequisites.empty()) {
                float maxPanel = totalAvailHeight - 140.0f;
                if (maxPanel < 60.0f) maxPanel = 60.0f;
                if (g_PrereqPanelHeight < 60.0f)      g_PrereqPanelHeight = 60.0f;
                if (g_PrereqPanelHeight > maxPanel)   g_PrereqPanelHeight = maxPanel;
            }
            float prereqPanelHeight = g_Prerequisites.empty() ? 0.0f : g_PrereqPanelHeight;
            float scrollbarHeight = 14.0f; // Reserve space for horizontal scrollbar
            float availHeight = totalAvailHeight - prereqPanelHeight - (prereqPanelHeight > 0 ? 6.0f : 0.0f) - scrollbarHeight;
            if (availHeight < 100.0f) availHeight = 100.0f;
            float columnWidth = 200.0f;
            float separatorWidth = 1.0f;

            float columnPadding = 8.0f;  // Padding between columns
            float textPadX = 6.0f;       // Internal text padding from column edge
            float cornerRounding = 5.0f; // Rounded corner radius
            ImU32 borderColor = ImGui::ColorConvertFloat4ToU32(separatorColor);

            // Column 0: Legendary list (Gen1 + Gen2)
            // Compute column width to fit longest label
            float col0W = columnWidth;
            float legIconExtra = g_ShowItemIcons ? (ICON_SIZE + ICON_GAP) : 0.0f;
            for (const auto& leg : legendaries) {
                std::string subtype = !leg.weapon_type.empty() ? leg.weapon_type : (!leg.armor_type.empty() ? leg.armor_type : leg.trinket_type);
                std::string fullLabel = leg.name + (subtype.empty() ? "" : " (" + std::string(Localization::Tr(subtype.c_str())) + ")") + " >";
                float pctExtra = CraftyLegend::GW2API::HasAccountData() ? ImGui::CalcTextSize("100%").x + 8.0f : 0.0f;
                float w = ImGui::CalcTextSize(fullLabel.c_str()).x + legIconExtra + pctExtra + textPadX * 2 + 8;
                if (w > col0W) col0W = w;
            }

            // Pre-calculate total width of all columns for horizontal scroll
            float totalColumnsWidth = col0W;
            for (size_t col = 1; col < g_Columns.size(); col++) {
                const auto& colData = g_Columns[col];
                if (colData.title.empty()) break;
                float colW = columnWidth;
                float titleW = ImGui::CalcTextSize(Localization::ColumnTitle(colData.title).c_str()).x;
                if (titleW + textPadX * 2 + 8 > colW) colW = titleW + textPadX * 2 + 8;
                float colMaxPriceW = 0.0f;
                for (const auto& mat : colData.materials) {
                    int tp = GetMaterialTotalPrice(mat);
                    if (tp > 0) { float pw = CalcPriceWidth(tp); if (pw > colMaxPriceW) colMaxPriceW = pw; }
                }
                float iconExtra = g_ShowItemIcons ? (ICON_SIZE + ICON_GAP) : 0.0f;
                for (const auto& mat : colData.materials) {
                    if (mat.name == "Coin") {
                        float w = ImGui::CalcTextSize(Localization::Tr("Gold Cost")).x + 4 + colMaxPriceW;
                        if (w + textPadX * 2 + 8 > colW) colW = w + textPadX * 2 + 8;
                        continue;
                    }
                    bool dummy = false;
                    std::string lbl = FormatMaterialLabel(mat, &dummy);
                    float w = ImGui::CalcTextSize(lbl.c_str()).x + colMaxPriceW + iconExtra;
                    if (w + textPadX * 2 + 8 > colW) colW = w + textPadX * 2 + 8;
                }
                for (const auto& acq : colData.acquisitions) {
                    std::string lbl = acq.display_name + " >";
                    float w = ImGui::CalcTextSize(lbl.c_str()).x;
                    if (w + textPadX * 2 + 8 > colW) colW = w + textPadX * 2 + 8;
                }
                totalColumnsWidth += columnPadding + colW;
            }

            // Shopping List left panel
            float shoppingPanelW = 0.0f;
            if (g_ShowShoppingList && g_PrereqLegendaryId != 0) {
                // Rebuild if dirty or legendary changed
                if (g_ShoppingListDirty || g_ShoppingListLegendaryId != g_PrereqLegendaryId) {
                    BuildShoppingList(g_PrereqLegendaryId);
                }

                // Pre-calculate column widths from content
                float qtyColW = 0.0f;
                float nameColW = 0.0f;
                float priceColW = 0.0f;
                for (const auto& e : g_ShoppingList) {
                    std::string qtyStr = std::to_string(e.required);
                    float qw = ImGui::CalcTextSize(qtyStr.c_str()).x;
                    if (qw > qtyColW) qtyColW = qw;
                    float nw = ImGui::CalcTextSize(Localization::ItemName(e.item_id, e.name).c_str()).x;
                    if (nw > nameColW) nameColW = nw;
                    if (e.tp_price > 0 && e.required > 0) {
                        float pw = CalcPriceWidth(e.tp_price * e.required);
                        if (pw > priceColW) priceColW = pw;
                    }
                }
                float gap = 8.0f;
                shoppingPanelW = qtyColW + gap + nameColW + (priceColW > 0 ? gap + priceColW : 0) + textPadX * 2 + 8;
                if (shoppingPanelW < 200.0f) shoppingPanelW = 200.0f;
                if (shoppingPanelW > 450.0f) shoppingPanelW = 450.0f;
                float qtyEnd = textPadX + qtyColW + gap;
                float priceStart = shoppingPanelW - textPadX - priceColW;

                // Compute per-group totals
                long long tpCost = 0, vendorCost = 0;
                int tpCount = 0, vendorCount = 0;
                for (const auto& e : g_ShoppingList) {
                    if (e.is_vendor) {
                        vendorCount++;
                        if (e.tp_price > 0 && e.required > 0)
                            vendorCost += (long long)e.tp_price * e.required;
                    } else {
                        tpCount++;
                        if (e.tp_price > 0 && e.required > 0)
                            tpCost += (long long)e.tp_price * e.required;
                    }
                }

                ImGui::BeginChild("ShoppingPanel", ImVec2(shoppingPanelW, availHeight + scrollbarHeight), false);
                ImGui::Indent(textPadX);
                ImGui::TextColored(titleColor, "%s", Localization::Tr("Shopping List"));
                ImGui::SameLine();
                ImGui::TextColored(dimTextColor, "(%d)", tpCount + vendorCount);

                // Sort controls
                ImGui::SameLine();
                ImGui::TextColored(dimTextColor, " | ");
                ImGui::SameLine(0, 0);
                if (g_ShoppingSort == ShoppingSort::Name) {
                    ImGui::TextColored(sectionHeaderColor, "%s", Localization::Tr("Name"));
                } else {
                    if (ImGui::SmallButton(Localization::Tr("Name"))) {
                        g_ShoppingSort = ShoppingSort::Name;
                        g_ShoppingListDirty = true;
                    }
                }
                ImGui::SameLine(0, 4);
                if (g_ShoppingSort == ShoppingSort::Price) {
                    ImGui::TextColored(sectionHeaderColor, "%s", Localization::Tr("Price"));
                } else {
                    if (ImGui::SmallButton(Localization::Tr("Price"))) {
                        g_ShoppingSort = ShoppingSort::Price;
                        g_ShoppingListDirty = true;
                    }
                }

                if (tpCost > 0) {
                    ImGui::TextColored(dimTextColor, "%s", Localization::Tr("Total TP: "));
                    ImGui::SameLine(0, 0);
                    RenderPrice((int)std::min(tpCost, (long long)INT_MAX));
                    ImGui::NewLine();
                }
                if (vendorCost > 0) {
                    ImGui::TextColored(dimTextColor, "%s", Localization::Tr("Total Vendor: "));
                    ImGui::SameLine(0, 0);
                    RenderPrice((int)std::min(vendorCost, (long long)INT_MAX));
                    ImGui::NewLine();
                }
                ImGui::Separator();

                // Render helper lambda for a group of entries
                auto renderEntries = [&](bool vendor) {
                    for (const auto& e : g_ShoppingList) {
                        if (e.is_vendor != vendor) continue;
                        // Qty column (right-aligned)
                        std::string qtyStr = std::to_string(e.required);
                        float qw = ImGui::CalcTextSize(qtyStr.c_str()).x;
                        ImGui::SetCursorPosX(textPadX + qtyColW - qw);
                        ImGui::Text("%s", qtyStr.c_str());

                        // Name column
                        ImGui::SameLine(qtyEnd);
                        ImGui::Text("%s", Localization::ItemName(e.item_id, e.name).c_str());

                        // Price (right-aligned)
                        if (e.tp_price > 0 && e.required > 0) {
                            ImGui::SameLine(priceStart);
                            RenderPrice(e.tp_price * e.required);
                            ImGui::NewLine();
                        }
                    }
                };

                // TP Purchases section
                if (tpCount > 0) {
                    ImGui::TextColored(readyColor, "%s", Localization::Tr("Trading Post"));
                    ImGui::SameLine();
                    ImGui::TextColored(dimTextColor, "(%d)", tpCount);
                    renderEntries(false);
                }

                // Vendor Purchases section
                if (vendorCount > 0) {
                    if (tpCount > 0) ImGui::Spacing();
                    ImGui::TextColored(sectionHeaderColor, "%s", Localization::Tr("Vendor"));
                    ImGui::SameLine();
                    ImGui::TextColored(dimTextColor, "(%d)", vendorCount);
                    renderEntries(true);
                }

                ImGui::Unindent(textPadX);
                ImGui::EndChild();
                ImGui::SameLine(0, columnPadding);
            }

            // Begin horizontal scroll wrapper for all columns
            // During backward animation, inflate content width so ImGui doesn't clamp scroll
            float columnsScrollW = ImGui::GetContentRegionAvail().x;
            float effectiveWidth = totalColumnsWidth;
            if ((g_ScrollToEnd || g_ScrollAnimating) && g_ScrollStartX > 0.0f) {
                float viewportW = columnsScrollW;
                float needed = g_ScrollStartX + viewportW;
                if (needed > effectiveWidth) effectiveWidth = needed;
            }
            ImGui::SetNextWindowContentSize(ImVec2(effectiveWidth, 0.0f));
            ImGui::BeginChild("ColumnsScroll", ImVec2(columnsScrollW, availHeight + scrollbarHeight), false, ImGuiWindowFlags_HorizontalScrollbar | ImGuiWindowFlags_NoScrollbar);

            // Apply pending scroll restore (horizontal)
            if (g_PendingScrollRestore) {
                ImGui::SetScrollX(g_PendingScrollX);
            }

            // Capture scroll position each frame for use if a click happens
            if (!g_ScrollAnimating) {
                g_PreClickScrollX = ImGui::GetScrollX();
            }
            g_TrackedScrollX = ImGui::GetScrollX();

            // Smooth scroll animation
            if (g_ScrollToEnd && !g_ScrollAnimating) {
                // g_ScrollStartX was set at click time from g_PreClickScrollX
                float maxScroll = ImGui::GetScrollMaxX();
                // For backward animation, maxScroll reflects inflated content;
                // compute real target from actual column width
                float realMaxScroll = totalColumnsWidth - ImGui::GetWindowWidth();
                if (realMaxScroll < 0.0f) realMaxScroll = 0.0f;
                g_ScrollTargetX = realMaxScroll;
                if (std::abs(g_ScrollTargetX - g_ScrollStartX) > 1.0f) {
                    g_ScrollAnimStartTime = ImGui::GetTime();
                    g_ScrollAnimating = true;
                }
                g_ScrollToEnd = false;
            }
            if (g_ScrollAnimating) {
                double elapsed = ImGui::GetTime() - g_ScrollAnimStartTime;
                float t = (float)(elapsed / SCROLL_ANIM_DURATION);
                if (t >= 1.0f) {
                    t = 1.0f;
                    g_ScrollAnimating = false;
                }
                // Ease-out cubic
                float ease = 1.0f - (1.0f - t) * (1.0f - t) * (1.0f - t);
                ImGui::SetScrollX(g_ScrollStartX + (g_ScrollTargetX - g_ScrollStartX) * ease);
            }

            ImDrawList* drawList = ImGui::GetWindowDrawList();

            ImU32 colBgU32 = ImGui::ColorConvertFloat4ToU32(colBgColor);
            ImU32 colHeaderBgU32 = ImGui::ColorConvertFloat4ToU32(colHeaderBg);

            ImVec2 col0Pos = ImGui::GetCursorScreenPos();
            // Column 0 background fill
            drawList->AddRectFilled(col0Pos, ImVec2(col0Pos.x + col0W, col0Pos.y + availHeight),
                colBgU32, cornerRounding);
            ImGui::BeginChild("Col0", ImVec2(col0W, availHeight), false, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
            // Header band background
            ImVec2 headerStart = ImGui::GetCursorScreenPos();
            float headerH = ImGui::GetTextLineHeightWithSpacing() + 4.0f;
            drawList->AddRectFilled(headerStart, ImVec2(headerStart.x + col0W, headerStart.y + headerH),
                colHeaderBgU32, cornerRounding);
            // Gold accent line under header
            drawList->AddLine(ImVec2(headerStart.x + 2, headerStart.y + headerH),
                ImVec2(headerStart.x + col0W - 2, headerStart.y + headerH),
                headerAccentLine, 1.0f);
            ImGui::Indent(textPadX);
            ImGui::TextColored(titleColor, "%s", Localization::Tr("Legendary Items"));
            ImGui::Separator();
            ImGui::Spacing();

            // Search bar (fixed at top) with clear button
            float clearBtnW = ImGui::CalcTextSize("Clear").x + ImGui::GetStyle().FramePadding.x * 2;
            ImGui::PushItemWidth(col0W - textPadX * 2 - 4 - clearBtnW - 4);
            ImGui::InputTextWithHint("##LegSearch", "Search...", g_SearchFilter, sizeof(g_SearchFilter));
            ImGui::PopItemWidth();
            ImGui::SameLine(0, 4);
            if (ImGui::SmallButton("Clear##ClearSearch")) {
                g_SearchFilter[0] = '\0';
            }
            ImGui::Spacing();

            // Build lowercase filter for case-insensitive search
            std::string filterLower;
            if (g_SearchFilter[0] != '\0') {
                filterLower = g_SearchFilter;
                for (auto& c : filterLower) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            }

            // Scrollable item list
            float scrollHeight = ImGui::GetContentRegionAvail().y;
            ImGui::BeginChild("Col0Scroll", ImVec2(col0W - textPadX * 2, scrollHeight), false);

            // Apply pending Col0 scroll restore
            if (g_PendingScrollRestore) {
                ImGui::SetScrollY(g_PendingCol0ScrollY);
            }
            g_TrackedCol0ScrollY = ImGui::GetScrollY();

            // Render a category section: header + filtered items matching a predicate
            auto renderSection = [&](const char* label, auto predicate, bool first = false) {
                bool any = false;
                for (const auto& leg : legendaries) { if (predicate(leg)) { any = true; break; } }
                if (!any) return;
                if (!first) ImGui::Spacing();
                ImGui::TextColored(sectionHeaderColor, "%s", label);
                for (size_t i = 0; i < legendaries.size(); i++) {
                    const auto& leg = legendaries[i];
                    if (!predicate(leg)) continue;
                    bool isFav = CraftyLegend::DataManager::IsFavourite(leg.id);
                    // Armoury capacity for this item: runes 7, sigils 8, one-handed
                    // weapons 4, rings and two-handers 2, armour and amulets 1.
                    int legCap = leg.max_count > 0 ? leg.max_count : 1;
                    // Hide owned legendaries per the user's chosen meaning of "owned":
                    // either a single armoury copy, or a full armoury.
                    if (g_OwnedFilterMode != OWNED_FILTER_SHOW_ALL && CraftyLegend::GW2API::HasAccountData()) {
                        int held = CraftyLegend::GW2API::GetArmoryCount(leg.id);
                        int threshold = (g_OwnedFilterMode == OWNED_FILTER_HIDE_FULL) ? legCap : 1;
                        if (held >= threshold) continue;
                    }
                    if (!filterLower.empty()) {
                        std::string nameLower = leg.name;
                        for (auto& c : nameLower) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
                        std::string subtypeLower = !leg.weapon_type.empty() ? leg.weapon_type : (!leg.armor_type.empty() ? leg.armor_type : (!leg.trinket_type.empty() ? leg.trinket_type : leg.back_type));
                        for (auto& c : subtypeLower) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
                        if (nameLower.find(filterLower) == std::string::npos && subtypeLower.find(filterLower) == std::string::npos) continue;
                    }
                    bool isSel = (g_Columns.size() > 0 && g_Columns[0].selected_index == static_cast<int>(i));

                    // Completion % progress bar behind row
                    float completion = GetLegendaryCompletion(leg.id);
                    if (completion >= 0.0f) {
                        ImVec2 rowScreenPos = ImGui::GetCursorScreenPos();
                        float legRowH = g_ShowItemIcons ? ICON_SIZE : ImGui::GetTextLineHeightWithSpacing();
                        float barW = (col0W - textPadX * 2) * completion;
                        ImU32 barCol = (completion >= 1.0f)
                            ? IM_COL32(50, 180, 50, 35)
                            : IM_COL32(60, 140, 200, 25);
                        ImGui::GetWindowDrawList()->AddRectFilled(
                            ImVec2(rowScreenPos.x - textPadX, rowScreenPos.y),
                            ImVec2(rowScreenPos.x - textPadX + barW, rowScreenPos.y + legRowH),
                            barCol, 2.0f);
                    }

                    // Request and render legendary icon
                    if (g_ShowItemIcons && leg.id != 0) {
                        Texture_t* legIcon = nullptr;
                        try {
                            legIcon = CraftyLegend::IconManager::GetIcon(leg.id);
                        } catch (...) { legIcon = nullptr; }
                        if (!legIcon && !CraftyLegend::IconManager::IsIconLoaded(leg.id)) {
                            if (!leg.icon.empty()) {
                                CraftyLegend::IconManager::RequestIcon(leg.id, leg.icon);
                            } else {
                                CraftyLegend::IconManager::RequestIconById(leg.id, leg.name);
                            }
                        }
                        if (legIcon && legIcon->Resource) {
                            ImVec2 iconScreenPos = ImGui::GetCursorScreenPos();
                            ImGui::Image(legIcon->Resource, ImVec2(ICON_SIZE, ICON_SIZE));
                            // Rarity border for legendaries
                            ImU32 rarityCol = IM_COL32(160, 100, 200, 220);
                            ImGui::GetWindowDrawList()->AddRect(iconScreenPos,
                                ImVec2(iconScreenPos.x + ICON_SIZE, iconScreenPos.y + ICON_SIZE),
                                rarityCol, 2.0f, 0, 1.5f);
                            // Owned badge: green circle + white tick in bottom-right corner.
                            // Only a FULL armoury earns the tick — a partial holding (say
                            // 3 of 7 runes) shows its count next to the name instead.
                            if (CraftyLegend::GW2API::GetArmoryCount(leg.id) >= legCap) {
                                auto* dl = ImGui::GetWindowDrawList();
                                const float r = 5.0f;
                                ImVec2 bc(iconScreenPos.x + ICON_SIZE - r - 1.0f, iconScreenPos.y + ICON_SIZE - r - 1.0f);
                                dl->AddCircleFilled(bc, r, IM_COL32(40, 180, 40, 240));
                                // Tick: down-stroke then up-stroke
                                ImVec2 t0(bc.x - 2.8f, bc.y + 0.3f);
                                ImVec2 t1(bc.x - 0.5f, bc.y + 2.8f);
                                ImVec2 t2(bc.x + 3.2f, bc.y - 2.5f);
                                dl->AddLine(t0, t1, IM_COL32(255, 255, 255, 240), 1.5f);
                                dl->AddLine(t1, t2, IM_COL32(255, 255, 255, 240), 1.5f);
                            }
                            ImGui::SameLine(0, ICON_GAP);
                        } else {
                            ImGui::Dummy(ImVec2(ICON_SIZE, ICON_SIZE));
                            ImGui::SameLine(0, ICON_GAP);
                        }
                    }

                    std::string subtype = !leg.weapon_type.empty() ? leg.weapon_type : (!leg.armor_type.empty() ? leg.armor_type : (!leg.trinket_type.empty() ? leg.trinket_type : leg.back_type));
                    std::string subtypeSuffix = subtype.empty() ? "" : " (" + std::string(Localization::Tr(subtype.c_str())) + ")";
                    // Compute star size for favourites (drawn as overlay after Selectable)
                    float starSize = 0.0f;
                    ImVec2 starAnchor = ImGui::GetCursorScreenPos();
                    if (isFav) {
                        float sz = ImGui::GetTextLineHeight() * 0.5f;
                        starSize = sz * 2.0f + 4.0f;
                    }
                    std::string dispName = Localization::ItemName(leg.id, leg.name);
                    // Partial armoury holding, e.g. "3/7" on Legendary Rune. Shown only
                    // for items worth owning in multiples, and only once one is held —
                    // the completion bar tracks the next copy, so the count is how you
                    // see the ones already banked.
                    std::string copiesSuffix;
                    if (legCap > 1 && CraftyLegend::GW2API::HasAccountData()) {
                        int held = CraftyLegend::GW2API::GetArmoryCount(leg.id);
                        if (held > 0) {
                            copiesSuffix = "  " + std::to_string(std::min(held, legCap))
                                         + "/" + std::to_string(legCap);
                        }
                    }
                    std::string lbl = (isFav ? "     " : "") + dispName + subtypeSuffix + copiesSuffix + " >";
                    ImVec2 itemPos = ImGui::GetCursorScreenPos();
                    float selH = g_ShowItemIcons ? ICON_SIZE : 0;
                    if (g_ShowItemIcons) {
                        ImGui::PushStyleVar(ImGuiStyleVar_SelectableTextAlign, ImVec2(0.0f, 0.5f));
                    }
                    if (ImGui::Selectable(lbl.c_str(), isSel, 0, ImVec2(0, selH))) {
                        if (g_Columns.empty()) { g_Columns.resize(1); g_Columns[0].title = Localization::Tr("Legendary Items"); }
                        g_Columns[0].selected_index = static_cast<int>(i);
                        try {
                            CraftyLegend::DataManager::UpdateColumn(0, leg.id);
                            g_Columns = CraftyLegend::DataManager::GetColumns();
                            CraftyLegend::DataManager::SetSessionScrollState(g_TrackedScrollX, g_TrackedCol0ScrollY, g_TrackedColScrollY);
                            CraftyLegend::DataManager::SaveSession();
                            g_PrereqLegendaryId = leg.id;
                            g_Prerequisites = CraftyLegend::DataManager::GetPrerequisites(leg.id);
                        } catch (...) {}
                    }
                    if (g_ShowItemIcons) {
                        ImGui::PopStyleVar();
                    }
                    // Right-click context menu for legendaries
                    {
                        std::string legPopupId = std::string("LegCtx##") + label + "##" + std::to_string(leg.id);
                        if (ImGui::BeginPopupContextItem(legPopupId.c_str())) {
                            if (ImGui::MenuItem(Localization::Tr("Open on Wiki"))) {
                                OpenWikiPage(leg.name);
                            }
                            if (ImGui::MenuItem(isFav ? Localization::Tr("Remove Favourite") : Localization::Tr("Add Favourite"))) {
                                CraftyLegend::DataManager::ToggleFavourite(leg.id);
                            }
                            if (CraftyLegend::GW2API::HasAccountData() && CraftyLegend::GW2API::GetOwnedCount(leg.id) > 0) {
                                if (ImGui::MenuItem(Localization::Tr("Search in Hoard & Seek"))) {
                                    APIDefs->Events_Raise(EV_HOARD_SEARCH, (void*)leg.name.c_str());
                                }
                            }
                            ImGui::EndPopup();
                        }
                    }
                    // Draw favourite star as overlay (after Selectable so highlight covers the star area)
                    if (isFav) {
                        float sz = ImGui::GetTextLineHeight() * 0.5f;
                        float yOff = g_ShowItemIcons ? (ICON_SIZE * 0.5f) : (ImGui::GetTextLineHeightWithSpacing() * 0.5f);
                        ImVec2 sc(starAnchor.x + sz + 1.0f, starAnchor.y + yOff);
                        float outerR = sz;
                        float innerR = sz * 0.38f;
                        ImVec2 pts[10];
                        for (int s = 0; s < 10; s++) {
                            float angle = (float)s * 3.14159265f / 5.0f - 3.14159265f / 2.0f;
                            float r = (s % 2 == 0) ? outerR : innerR;
                            pts[s] = ImVec2(sc.x + r * cosf(angle), sc.y + r * sinf(angle));
                        }
                        ImU32 starCol = IM_COL32(255, 200, 50, 255);
                        ImDrawList* dl = ImGui::GetWindowDrawList();
                        for (int s = 0; s < 10; s++) {
                            dl->AddTriangleFilled(sc, pts[s], pts[(s + 1) % 10], starCol);
                        }
                    }
                    if (!subtypeSuffix.empty()) {
                        float padW = isFav ? ImGui::CalcTextSize("     ").x : 0.0f;
                        float nameW = ImGui::CalcTextSize(dispName.c_str()).x + padW;
                        float textVOff = g_ShowItemIcons ? (ICON_SIZE - ImGui::GetTextLineHeight()) * 0.5f : 0.0f;
                        ImVec2 subtypePos(itemPos.x + nameW, itemPos.y + textVOff);
                        ImGui::GetWindowDrawList()->AddText(subtypePos, ImGui::ColorConvertFloat4ToU32(subtypeColor), subtypeSuffix.c_str());
                    }
                    // Completion % text at right edge
                    if (completion >= 0.0f) {
                        char pctBuf[8];
                        snprintf(pctBuf, sizeof(pctBuf), "%d%%", (int)(completion * 100));
                        float pctW = ImGui::CalcTextSize(pctBuf).x;
                        float textVOff = g_ShowItemIcons ? (ICON_SIZE - ImGui::GetTextLineHeight()) * 0.5f : 0.0f;
                        float scrollbarW = ImGui::GetStyle().ScrollbarSize;
                        ImVec2 pctPos(itemPos.x + col0W - legIconExtra - textPadX * 2 - pctW - scrollbarW - 4.0f, itemPos.y + textVOff);
                        ImU32 pctCol = (completion >= 1.0f)
                            ? IM_COL32(80, 210, 80, 220)
                            : IM_COL32(180, 180, 180, 160);
                        ImGui::GetWindowDrawList()->AddText(pctPos, pctCol, pctBuf);
                    }
                }
            };

            // Favourites: all favourited legendaries pinned at the top
            renderSection(Localization::Tr("Favourites"), [](const CraftyLegend::Legendary& l) {
                return CraftyLegend::DataManager::IsFavourite(l.id);
            }, true);
            // Weapons: Gen 1-3 + Spear (15) + Sigil (18)
            renderSection(Localization::Tr("Weapons"), [](const CraftyLegend::Legendary& l) {
                return l.generation >= 1 && l.generation <= 3 || l.generation == 15 || l.generation == 18;
            });
            // Armour: Gen 4-7 + Rune (17)
            renderSection(Localization::Tr("Armour"), [](const CraftyLegend::Legendary& l) {
                return l.generation >= 4 && l.generation <= 7 || l.generation == 17;
            });
            // Trinkets: Trinkets (8-13) + Backpieces (14) + Relic (16)
            renderSection(Localization::Tr("Trinkets"), [](const CraftyLegend::Legendary& l) {
                return l.generation >= 8 && l.generation <= 14 || l.generation == 16;
            });

            ImGui::EndChild(); // Col0Scroll
            ImGui::Unindent(textPadX);
            ImGui::EndChild(); // Col0
            drawList->AddRect(col0Pos, ImVec2(col0Pos.x + col0W, col0Pos.y + availHeight),
                borderColor, cornerRounding);

            uint32_t selectedLegId = 0;
            if (!g_Columns.empty() && g_Columns[0].selected_index >= 0 &&
                g_Columns[0].selected_index < (int)g_Columns[0].items.size()) {
                selectedLegId = g_Columns[0].items[g_Columns[0].selected_index].id;
            }

            if (g_UseTreeLayout) {
                ImGui::SameLine(0.0f, columnPadding);
                float treeAvailWidth = columnsScrollW - col0W - columnPadding;
                if (treeAvailWidth < 0.0f) treeAvailWidth = 0.0f;
                CraftyLegend::UI::RenderTree(selectedLegId, treeAvailWidth, availHeight);
            } else {

            // Dynamic columns 1+
            bool columnsDirty = false;
            for (size_t col = 1; col < g_Columns.size() && !columnsDirty; col++) {
                const auto& colData = g_Columns[col];
                if (colData.title.empty()) break;

                // Compute column width: max of default and widest content
                float colW = columnWidth;
                float titleW = ImGui::CalcTextSize(Localization::ColumnTitle(colData.title).c_str()).x;
                if (titleW + textPadX * 2 + 8 > colW) colW = titleW + textPadX * 2 + 8;
                // Compute max price width first for consistent offset
                float colMaxPriceW = 0.0f;
                for (const auto& mat : colData.materials) {
                    int tp = GetMaterialTotalPrice(mat);
                    if (tp > 0) {
                        float pw = CalcPriceWidth(tp);
                        if (pw > colMaxPriceW) colMaxPriceW = pw;
                    }
                }
                float iconExtraR = g_ShowItemIcons ? (ICON_SIZE + ICON_GAP) : 0.0f;
                for (const auto& mat : colData.materials) {
                    if (mat.name == "Coin") {
                        float w = ImGui::CalcTextSize(Localization::Tr("Gold Cost")).x + 4 + colMaxPriceW;
                        if (w + textPadX * 2 + 8 > colW) colW = w + textPadX * 2 + 8;
                        continue;
                    }
                    bool dummy = false;
                    std::string lbl = FormatMaterialLabel(mat, &dummy);
                    float w = ImGui::CalcTextSize(lbl.c_str()).x + colMaxPriceW + iconExtraR;
                    if (w + textPadX * 2 + 8 > colW) colW = w + textPadX * 2 + 8;
                }
                for (const auto& acq : colData.acquisitions) {
                    std::string lbl = acq.display_name + " >";
                    float w = ImGui::CalcTextSize(lbl.c_str()).x;
                    if (w + textPadX * 2 + 8 > colW) colW = w + textPadX * 2 + 8;
                }

                ImGui::SameLine(0.0f, 0.0f);
                ImGui::SetCursorPosX(ImGui::GetCursorPosX() + columnPadding);

                ImVec2 colPos = ImGui::GetCursorScreenPos();
                // Column background fill
                drawList->AddRectFilled(colPos, ImVec2(colPos.x + colW, colPos.y + availHeight),
                    colBgU32, cornerRounding);
                std::string childId = "Col" + std::to_string(col);
                ImGui::BeginChild(childId.c_str(), ImVec2(colW, availHeight), false);

                // Apply pending column scroll restore
                size_t colScrollIdx = col - 1; // col 1 → index 0, col 2 → index 1, etc.
                if (g_PendingScrollRestore && colScrollIdx < g_PendingColScrollY.size()) {
                    ImGui::SetScrollY(g_PendingColScrollY[colScrollIdx]);
                }
                // Track column scroll Y
                if (g_TrackedColScrollY.size() <= colScrollIdx) {
                    g_TrackedColScrollY.resize(colScrollIdx + 1, 0.0f);
                }
                g_TrackedColScrollY[colScrollIdx] = ImGui::GetScrollY();

                // Header band background
                ImVec2 colHdrStart = ImGui::GetCursorScreenPos();
                drawList->AddRectFilled(colHdrStart, ImVec2(colHdrStart.x + colW, colHdrStart.y + headerH),
                    colHeaderBgU32, cornerRounding);
                // Gold accent line under header
                drawList->AddLine(ImVec2(colHdrStart.x + 2, colHdrStart.y + headerH),
                    ImVec2(colHdrStart.x + colW - 2, colHdrStart.y + headerH),
                    headerAccentLine, 1.0f);
                ImGui::Indent(textPadX);
                ImGui::TextColored(titleColor, "%s", Localization::ColumnTitle(colData.title).c_str());
                ImGui::Separator();
                ImGui::Spacing();

                // Render materials if present
                if (!colData.materials.empty()) {
                    // Compute max price width for alignment
                    float maxPriceW = 0.0f;
                    for (const auto& mat : colData.materials) {
                        int tp = GetMaterialTotalPrice(mat);
                        if (tp > 0) {
                            float pw = CalcPriceWidth(tp);
                            if (pw > maxPriceW) maxPriceW = pw;
                        }
                    }

                    // Fixed layout offsets for consistent alignment
                    float iconColW = g_ShowItemIcons ? (ICON_SIZE + ICON_GAP) : 0.0f;
                    float priceGap = (maxPriceW > 0.0f) ? 4.0f : 0.0f;
                    float iconGap = g_ShowItemIcons ? 4.0f : 0.0f;
                    // labelStart = textPadX + maxPriceW + priceGap + iconColW
                    float labelStartX = textPadX + maxPriceW + priceGap + iconColW;

                    for (size_t i = 0; i < colData.materials.size(); i++) {
                        const auto& mat = colData.materials[i];

                        // Achievement gate(s) on this material (drives lock marker + tooltip)
                        std::vector<CraftyLegend::Prerequisite> matGates;
                        if (mat.item_id != 0)
                            matGates = CraftyLegend::DataManager::GetItemAchievementGates(mat.item_id);

                        bool isSel = (colData.selected_material_index == static_cast<int>(i));

                        // Coin materials render a dimmed, non-selectable label. Kept here in
                        // the caller (not in DrawItemRow) because the label uses the
                        // theme-adjusted dimTextColor, which the shared helper cannot see.
                        if (mat.name == "Coin") {
                            float rowBaseX = ImGui::GetCursorPosX();
                            float rowBaseY = ImGui::GetCursorPosY();
                            ImVec2 rowPos = ImGui::GetCursorScreenPos();
                            float rowH = g_ShowItemIcons ? (ICON_SIZE + 2.0f) : ImGui::GetTextLineHeightWithSpacing();
                            // Alternating row tinting for readability
                            if (i % 2 == 1) {
                                ImGui::GetWindowDrawList()->AddRectFilled(
                                    ImVec2(rowPos.x - textPadX, rowPos.y),
                                    ImVec2(rowPos.x + colW - textPadX, rowPos.y + rowH),
                                    IM_COL32(255, 255, 255, 8));
                            }
                            // Render price (right-aligned within maxPriceW, vertically centered)
                            int totalPrice = GetMaterialTotalPrice(mat);
                            // Affordability bar, same as any other material row gets
                            DrawCoinCostBar(rowPos, colW, rowH, totalPrice);
                            if (maxPriceW > 0.0f && totalPrice > 0) {
                                float thisPW = CalcPriceWidth(totalPrice);
                                float padLeft = maxPriceW - thisPW;
                                if (padLeft > 0) ImGui::SetCursorPosX(rowBaseX + padLeft);
                                if (g_ShowItemIcons) {
                                    float priceVOff = (rowH - ImGui::GetTextLineHeight()) * 0.5f;
                                    ImGui::SetCursorPosY(rowBaseY + priceVOff);
                                }
                                RenderPrice(totalPrice);
                                ImGui::SameLine(0, 0);
                                ImGui::SetCursorPosY(rowBaseY);
                            }
                            ImGui::SetCursorPosX(rowBaseX + labelStartX);
                            // Green when the wallet covers it, else the theme-adjusted dim label.
                            ImGui::TextColored(CanAffordCoinCost(totalPrice)
                                                   ? GoldCostLabelColor(totalPrice)
                                                   : dimTextColor,
                                               "%s", Localization::Tr("Gold Cost"));
                            continue;
                        }

                        // Draw the row visuals through the shared helper. PushID keeps each
                        // row's Selectable ID unique so identity is preserved (and stays
                        // correct once the tree layout reuses the same helper).
                        RowVisual rv;
                        rv.mat            = &mat;
                        rv.hasAccountData = CraftyLegend::GW2API::HasAccountData();
                        rv.showIcons      = g_ShowItemIcons;
                        rv.rowWidth       = colW;
                        rv.labelStartX    = labelStartX;
                        rv.priceMaxW      = maxPriceW;
                        rv.priceGap       = priceGap;
                        rv.selected       = isSel;
                        rv.altTint        = (i % 2 == 1);
                        rv.gates          = &matGates;

                        ImGui::PushID(static_cast<int>(i));
                        RowResult rowResult = DrawItemRow(rv);
                        ImGui::PopID();

                        if (rowResult.clicked) {
                            CraftyLegend::DataManager::SetSelectedMaterial(col, static_cast<int>(i));
                            if (mat.item_id != 0) {
                                try {
                                    int drill_count = (int)mat.count;
                                    if (CraftyLegend::GW2API::HasAccountData()) {
                                        int owned = GetEffectiveOwnedCount(mat.item_id);
                                        drill_count = std::max(0, drill_count - owned);
                                    }
                                    CraftyLegend::DataManager::UpdateColumn(col, mat.item_id, drill_count);
                                } catch (...) {}
                            }
                            g_Columns = CraftyLegend::DataManager::GetColumns();
                            CraftyLegend::DataManager::SetSessionScrollState(g_TrackedScrollX, g_TrackedCol0ScrollY, g_TrackedColScrollY);
                            CraftyLegend::DataManager::SaveSession();
                            columnsDirty = true;
                            g_ScrollStartX = g_PreClickScrollX;
                            g_ScrollToEnd = true;
                            g_ScrollAnimating = false;
                        }

                        // Re-apply the completed/ready text colour around the retained hover
                        // tooltip + right-click menu so they inherit it exactly as before.
                        // In the original linear code a single PushStyleColor spanned from the
                        // Selectable through the tooltip and menu; DrawItemRow now balances
                        // that push internally, so re-derive the state and wrap the pieces
                        // that stayed in the caller. FormatMaterialLabel is a pure read.
                        bool isComplete = false;
                        bool isReady = false;
                        FormatMaterialLabel(mat, &isComplete, &isReady);
                        if (isComplete) {
                            ImGui::PushStyleColor(ImGuiCol_Text, completedColor);
                        } else if (isReady) {
                            ImGui::PushStyleColor(ImGuiCol_Text, readyColor);
                        }
                        // Hover tooltip: per-account breakdown (unbound) + achievement gate(s)
                        if (mat.item_id != 0 && ImGui::IsItemHovered()) {
                            std::map<std::string, int> breakdown;
                            if (CraftyLegend::GW2API::HasAccountData()) {
                                const auto* bItem = CraftyLegend::DataManager::GetItem(mat.item_id);
                                bool isBound = bItem && (bItem->binding == "account" || bItem->binding == "soul");
                                if (!isBound) breakdown = CraftyLegend::GW2API::GetPerAccountCounts(mat.item_id);
                            }
                            bool showAcct = false;
                            for (const auto& pc : breakdown) if (pc.second > 0) { showAcct = true; break; }
                            if (showAcct || !matGates.empty()) {
                                ImGui::BeginTooltip();
                                if (showAcct) {
                                    std::string currentAcct = CraftyLegend::GW2API::GetCurrentAccountName();
                                    for (const auto& [acct, cnt] : breakdown) {
                                        if (cnt <= 0) continue;
                                        std::string displayName = GetAccountDisplayName(acct);
                                        if (acct == currentAcct) {
                                            ImGui::TextColored(ImVec4(0.5f, 1.0f, 0.5f, 1.0f), "%s: %d", displayName.c_str(), cnt);
                                        } else {
                                            ImGui::Text("%s: %d", displayName.c_str(), cnt);
                                        }
                                    }
                                }
                                if (!matGates.empty()) {
                                    if (showAcct) ImGui::Separator();
                                    bool haveData = CraftyLegend::GW2API::HasAccountData();
                                    const char* gateLang = Localization::ActiveLang();
                                    for (const auto& g : matGates) {
                                        ImVec4 gc = !haveData ? ImVec4(0.82f, 0.82f, 0.82f, 1.0f)
                                                  : (g.completed ? ImVec4(0.5f, 1.0f, 0.5f, 1.0f)
                                                                 : ImVec4(1.0f, 0.75f, 0.25f, 1.0f));
                                        const char* tag = !haveData ? Localization::Tr("Requires achievement")
                                                        : (g.completed ? Localization::Tr("Achievement complete")
                                                                       : Localization::Tr("Locked - requires achievement"));
                                        std::string gName = g.name;
                                        if (g.achievement_id > 0 && std::strcmp(gateLang, "en") != 0) {
                                            CraftyLegend::GW2API::EnsureAchievementNames({g.achievement_id}, gateLang);
                                            gName = CraftyLegend::GW2API::LocalizeAchievementName(g.achievement_id, g.name);
                                        }
                                        std::string gProgress = AchievementProgressText(g.achievement_id);
                                        if (!gProgress.empty()) gName += " (" + gProgress + ")";
                                        ImGui::TextColored(gc, "%s: %s", tag, gName.c_str());
                                        if (!g.description.empty()) {
                                            ImGui::PushTextWrapPos(320.0f);
                                            ImGui::TextColored(ImVec4(0.65f, 0.65f, 0.65f, 1.0f), "%s", StripMarkup(g.description).c_str());
                                            ImGui::PopTextWrapPos();
                                        }
                                    }
                                }
                                ImGui::EndTooltip();
                            }
                        }
                        // Right-click context menu
                        if (mat.name != "Coin") {
                            std::string popupId = "MatCtx##" + std::to_string(col) + "_" + std::to_string(i);
                            if (ImGui::BeginPopupContextItem(popupId.c_str())) {
                                std::string wikiName = mat.name;
                                if (mat.item_id != 0) {
                                    const auto* wItem = CraftyLegend::DataManager::GetItem(mat.item_id);
                                    if (wItem) wikiName = wItem->name;
                                }
                                if (ImGui::MenuItem(Localization::Tr("Open on Wiki"))) {
                                    OpenWikiPage(wikiName);
                                }
                                if (mat.item_id != 0 && CraftyLegend::GW2API::HasAccountData() && CraftyLegend::GW2API::GetOwnedCount(mat.item_id) > 0) {
                                    if (ImGui::MenuItem(Localization::Tr("Search in Hoard & Seek"))) {
                                        APIDefs->Events_Raise(EV_HOARD_SEARCH, (void*)wikiName.c_str());
                                    }
                                }
                                ImGui::EndPopup();
                            }
                        }
                        if (isComplete || isReady) {
                            ImGui::PopStyleColor();
                        }
                        if (columnsDirty) break;
                    }
                }
                // Render acquisition methods if present
                else if (!colData.acquisitions.empty()) {
                    for (size_t i = 0; i < colData.acquisitions.size(); i++) {
                        const auto& acq = colData.acquisitions[i];
                        bool isSel = (colData.selected_acquisition_index == static_cast<int>(i));
                        std::string label = acq.display_name + " >";

                        if (ImGui::Selectable(label.c_str(), isSel)) {
                            CraftyLegend::DataManager::SetSelectedAcquisition(col, static_cast<int>(i));
                            try {
                                CraftyLegend::DataManager::HandleAcquisitionMethodSelection(col, i);
                            } catch (...) {}
                            g_Columns = CraftyLegend::DataManager::GetColumns();
                            CraftyLegend::DataManager::SetSessionScrollState(g_TrackedScrollX, g_TrackedCol0ScrollY, g_TrackedColScrollY);
                            CraftyLegend::DataManager::SaveSession();
                            columnsDirty = true;
                            g_ScrollStartX = g_PreClickScrollX;
                            g_ScrollToEnd = true;
                            g_ScrollAnimating = false;
                        }
                        if (columnsDirty) break;
                    }
                }

                ImGui::Unindent(textPadX);
                ImGui::EndChild();
                drawList->AddRect(colPos, ImVec2(colPos.x + colW, colPos.y + availHeight),
                    borderColor, cornerRounding);
            }

            } // !g_UseTreeLayout

            // Redirect vertical mouse wheel to smooth horizontal scroll over dynamic columns (not Col0)
            if (ImGui::IsWindowHovered(ImGuiHoveredFlags_ChildWindows)) {
                float mouseX = ImGui::GetIO().MousePos.x;
                if (mouseX > col0Pos.x + col0W) {
                    float wheel = ImGui::GetIO().MouseWheel;
                    if (wheel != 0.0f) {
                        float currentScroll = g_ScrollAnimating ? g_ScrollTargetX : ImGui::GetScrollX();
                        float maxScroll = totalColumnsWidth - ImGui::GetWindowWidth();
                        if (maxScroll < 0.0f) maxScroll = 0.0f;
                        float newTarget = currentScroll - wheel * columnWidth;
                        if (newTarget < 0.0f) newTarget = 0.0f;
                        if (newTarget > maxScroll) newTarget = maxScroll;
                        g_ScrollStartX = ImGui::GetScrollX();
                        g_ScrollTargetX = newTarget;
                        g_ScrollAnimStartTime = ImGui::GetTime();
                        g_ScrollAnimating = true;
                        g_ScrollToEnd = false;
                    }
                }
            }

            ImGui::EndChild(); // ColumnsScroll

            // Clear pending scroll restore after first frame
            if (g_PendingScrollRestore) {
                g_PendingScrollRestore = false;
            }

            // Prerequisites panel
            if (!g_Prerequisites.empty()) {
                ImGui::Spacing();

                // Draggable splitter: resize the prereq pane by its top border.
                // Drag up = taller pane, drag down = shorter. Persisted on release.
                {
                    ImVec4 sc = separatorColor;
                    ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(sc.x, sc.y, sc.z, 0.35f));
                    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(sc.x, sc.y, sc.z, 0.65f));
                    ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImVec4(sc.x, sc.y, sc.z, 1.0f));
                    ImGui::Button("##prereqSplitter", ImVec2(-1.0f, 5.0f));
                    ImGui::PopStyleColor(3);
                    if (ImGui::IsItemHovered() || ImGui::IsItemActive())
                        ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNS);
                    if (ImGui::IsItemActive())
                        g_PrereqPanelHeight -= ImGui::GetIO().MouseDelta.y; // clamped next frame
                    if (ImGui::IsItemDeactivated())
                        SaveDisplaySettings();
                }

                ImGui::TextColored(titleColor, "%s", Localization::Tr("Prerequisites"));
                ImGui::BeginChild("PrereqPanel", ImVec2(0, prereqPanelHeight - 20.0f), false);

                CraftyLegend::PrereqCategory lastCat = (CraftyLegend::PrereqCategory)-1;
                for (const auto& p : g_Prerequisites) {
                    if (p.category != lastCat) {
                        if (lastCat != (CraftyLegend::PrereqCategory)-1) {
                            ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 2.0f);
                        }
                        lastCat = p.category;
                        const char* catName = "Other";
                        ImVec4 catColor(0.7f, 0.7f, 0.7f, 1.0f);
                        switch (p.category) {
                            case CraftyLegend::PrereqCategory::MapCompletion:
                                catName = "Map Completion"; catColor = ImVec4(0.9f, 0.55f, 0.2f, 1.0f); break;
                            case CraftyLegend::PrereqCategory::Mastery:
                                catName = "Masteries"; catColor = ImVec4(0.6f, 0.4f, 1.0f, 1.0f); break;
                            case CraftyLegend::PrereqCategory::WvW:
                                catName = "World vs World"; catColor = ImVec4(1.0f, 0.4f, 0.4f, 1.0f); break;
                            case CraftyLegend::PrereqCategory::Collection:
                                catName = "Collections"; catColor = ImVec4(0.4f, 0.8f, 1.0f, 1.0f); break;
                            case CraftyLegend::PrereqCategory::Achievement:
                                catName = "Achievements"; catColor = ImVec4(0.90f, 0.78f, 0.30f, 1.0f); break;
                                case CraftyLegend::PrereqCategory::Salvage:
                                catName = "Salvage"; catColor = ImVec4(0.8f, 0.8f, 0.3f, 1.0f); break;
                            case CraftyLegend::PrereqCategory::MapCurrency:
                                catName = "Map Currencies"; catColor = ImVec4(0.3f, 0.85f, 0.7f, 1.0f); break;
                            default: break;
                        }
                        ImGui::TextColored(catColor, "%s:", Localization::Tr(catName));
                        ImGui::SameLine();
                    } else {
                        ImGui::SameLine();
                        ImGui::TextColored(ImVec4(0.4f, 0.4f, 0.4f, 1.0f), "|");
                        ImGui::SameLine();
                    }
                    // Partial progress on an unfinished achievement gate, e.g. "12/40".
                    std::string pProgress = AchievementProgressText(p.achievement_id);
                    if (p.completed) ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.3f, 0.85f, 0.3f, 1.0f));
                    else if (!pProgress.empty()) ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.95f, 0.80f, 0.35f, 1.0f));
                    std::string pName = p.name;
                    if (std::strcmp(Localization::ActiveLang(), "en") != 0) {
                        if (p.achievement_id > 0) {
                            // Achievements AND collections (both are GW2 achievements): authoritative API name
                            CraftyLegend::GW2API::EnsureAchievementNames({p.achievement_id}, Localization::ActiveLang());
                            pName = CraftyLegend::GW2API::LocalizeAchievementName(p.achievement_id, p.name);
                        } else {
                            // No achievement id: best-effort chrome table (English fallback)
                            pName = Localization::Tr(p.name.c_str());
                        }
                    }
                    if (!pProgress.empty()) pName += " (" + pProgress + ")";
                    ImGui::Text("%s", pName.c_str());
                    if (p.completed || !pProgress.empty()) ImGui::PopStyleColor();
                    if (ImGui::IsItemHovered()) {
                        ImGui::BeginTooltip();
                        ImGui::PushTextWrapPos(300.0f);
                        ImGui::TextWrapped("%s", p.description.c_str());
                        if (!pProgress.empty()) {
                            ImGui::TextColored(ImVec4(0.95f, 0.80f, 0.35f, 1.0f), "%s: %s",
                                               Localization::Tr("Progress"), pProgress.c_str());
                        }
                        ImGui::PopTextWrapPos();
                        ImGui::EndTooltip();
                    }
                }
                ImGui::EndChild();
            }

        } // end else (legendaries not empty)

        if (pushedFont) ImGui::PopFont();
    }
    ImGui::End();
    
    // Render debug window if enabled
    if (g_ShowDebugWindow) {
        ImGui::SetNextWindowSize(ImVec2(600, 400), ImGuiCond_FirstUseEver);
        if (ImGui::Begin("CraftyLegend Debug Log", &g_ShowDebugWindow)) {
            if (ImGui::Button("Clear Log")) {
                g_DebugLog.clear();
            }
            ImGui::SameLine();
            if (ImGui::Button("Clear Icon Request Tracking")) {
                g_LoggedIconRequests.clear();
            }
            ImGui::SameLine();
            if (ImGui::Button("Copy to Clipboard")) {
                std::stringstream ss;
                for (const auto& line : g_DebugLog) {
                    ss << line << "\n";
                }
                ImGui::SetClipboardText(ss.str().c_str());
            }

            ImGui::Separator();
            ImGui::TextColored(ImVec4(0.6f, 0.85f, 1.0f, 1.0f), "%s", Localization::DiagStatus().c_str());
            ImGui::Separator();
            ImGui::BeginChild("ScrollingRegion", ImVec2(0, 0), false, ImGuiWindowFlags_HorizontalScrollbar);
            for (const auto& line : g_DebugLog) {
                ImGui::TextUnformatted(line.c_str());
            }
            if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY()) {
                ImGui::SetScrollHereY(1.0f);
            }
            ImGui::EndChild();
        }
        ImGui::End();
    }
}

void AddonOptions() {
    ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.0f, 1.0f), "%s", Localization::Tr("CraftyLegend Settings"));
    if (ImGui::SmallButton(Localization::Tr("Homepage"))) {
        ShellExecuteA(NULL, "open", "https://pie.rocks.cc/", NULL, NULL, SW_SHOWNORMAL);
    }
    ImGui::SameLine();
    if (ImGui::SmallButton(Localization::Tr("Buy me a coffee!"))) {
        ShellExecuteA(NULL, "open", "https://ko-fi.com/pieorcake", NULL, NULL, SW_SHOWNORMAL);
    }
    ImGui::Separator();

    // Layout chooser: Miller columns vs. expanding tree. Radio buttons so it's
    // clear the two are mutually exclusive; each layout keeps its own window
    // geometry (restored on switch by AddonRender).
    ImGui::TextUnformatted(Localization::Tr("Layout"));
    ImGui::SameLine();
    if (ImGui::RadioButton(Localization::Tr("Miller Columns"), !g_UseTreeLayout) && g_UseTreeLayout) {
        g_UseTreeLayout = false;
        SaveDisplaySettings();
    }
    ImGui::SameLine();
    if (ImGui::RadioButton(Localization::Tr("Tree View"), g_UseTreeLayout) && !g_UseTreeLayout) {
        g_UseTreeLayout = true;
        SaveDisplaySettings();
    }
    ImGui::Separator();

    // Icon settings (always visible at top)
    ImGui::Text("%s", Localization::Tr("Display Settings"));
    bool compactMode = !g_ShowItemIcons;
    if (ImGui::Checkbox(Localization::Tr("Compact Mode"), &compactMode)) {
        g_ShowItemIcons = !compactMode;
        SaveDisplaySettings();
    }
    ImGui::SameLine();
    ImGui::TextDisabled("(?)");
    if (ImGui::IsItemHovered()) {
        ImGui::BeginTooltip();
        ImGui::Text("Hide item icons and reduce row height for a denser view");
        ImGui::EndTooltip();
    }
    
    // Owned-legendary filter. Three-way because the Legendary Armoury holds
    // multiple copies of most items, so "owned" can mean either the first copy
    // or a full set.
    ImGui::TextUnformatted(Localization::Tr("Owned legendaries"));
    ImGui::SameLine();
    ImGui::TextDisabled("(?)");
    if (ImGui::IsItemHovered()) {
        ImGui::BeginTooltip();
        ImGui::Text("Controls which legendaries you already have are hidden from the list (requires Hoard & Seek).\n"
                    "The armoury holds several copies of most items, so \"owned\" can mean the first copy or a full set.");
        ImGui::EndTooltip();
    }
    struct OwnedModeOption { int mode; const char* label; };
    static const OwnedModeOption kOwnedModes[] = {
        { OWNED_FILTER_SHOW_ALL,  "Show all" },
        { OWNED_FILTER_HIDE_ANY,  "Hide once I own one" },
        { OWNED_FILTER_HIDE_FULL, "Hide only when armoury is full" }
    };
    ImGui::Indent();
    for (const auto& opt : kOwnedModes) {
        if (ImGui::RadioButton(Localization::Tr(opt.label), g_OwnedFilterMode == opt.mode) && g_OwnedFilterMode != opt.mode) {
            g_OwnedFilterMode = opt.mode;
            SaveDisplaySettings();
        }
    }
    ImGui::Unindent();

    // Bundled custom font (downloaded on first enable). Size slider appears when on.
    if (ImGui::Checkbox(Localization::Tr("Use custom font"), &g_UseCustomFont)) {
        SaveDisplaySettings();
    }
    ImGui::SameLine();
    ImGui::TextDisabled("(?)");
    if (ImGui::IsItemHovered()) {
        ImGui::BeginTooltip();
        ImGui::Text("Render Crafty Legend's main window in a bundled Inter font.\nDownloaded once on first use; falls back to the default font until ready.");
        ImGui::EndTooltip();
    }
    if (g_UseCustomFont) {
        ImGui::SetNextItemWidth(160.0f);
        if (ImGui::SliderFloat(Localization::Tr("Font size"), &g_CustomFontSize, 12.0f, 28.0f, "%.0f")) {
            // Live while dragging; persist when the drag finishes.
        }
        if (ImGui::IsItemDeactivatedAfterEdit()) {
            SaveDisplaySettings();
        }
    }

    if (ImGui::Checkbox(Localization::Tr("Show Quick Access Icon"), &g_ShowQAIcon)) {
        if (g_ShowQAIcon) {
            APIDefs->QuickAccess_Add(QA_ID, TEX_ANVIL, TEX_ANVIL_HOVER, "KB_CRAFTY_TOGGLE", "CraftyLegend");
        } else {
            APIDefs->QuickAccess_Remove(QA_ID);
        }
        SaveDisplaySettings();
    }
    ImGui::SameLine();
    ImGui::TextDisabled("(?)");
    if (ImGui::IsItemHovered()) {
        ImGui::BeginTooltip();
        ImGui::Text("Show the anvil icon in the Nexus Quick Access toolbar");
        ImGui::EndTooltip();
    }

    if (ImGui::Checkbox(Localization::Tr("Use Pie UI theme (if available)"), &g_UsePieTheme)) {
        SaveDisplaySettings();
    }
    ImGui::SameLine();
    ImGui::TextDisabled("(?)");
    if (ImGui::IsItemHovered()) {
        ImGui::BeginTooltip();
        ImGui::Text("Match Crafty Legend's colours to the Pie UI addon's theme when it is installed.\nWhen off or when Pie UI is absent, the built-in theme is used.");
        ImGui::EndTooltip();
    }

    if (ImGui::Checkbox("Show Debug Window", &g_ShowDebugWindow)) {
        SaveDisplaySettings();
    }
    ImGui::SameLine();
    ImGui::TextDisabled("(?)");
    if (ImGui::IsItemHovered()) {
        ImGui::BeginTooltip();
        ImGui::Text("Show debug log window for icon loading");
        ImGui::EndTooltip();
    }

}

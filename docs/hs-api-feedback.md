# Hoard & Seek API v3 — Integration Feedback from Crafty Legend

## Context

Crafty Legend (CL) is a Nexus addon that displays legendary weapon crafting costs, material ownership, and prerequisite progress. It uses H&S for all account data (items, wallet, achievements, masteries) and needs to detect which GW2 account the user is currently logged into so it can filter account-bound item counts.

This document describes issues we encountered integrating H&S API v3 multi-account support. Please update the H&S API documentation and migration guidance for other addon authors accordingly.

---

## Issue 1: Startup timing — H&S loads after the consuming addon

**Problem:** CL calls `Events_Raise(EV_HOARD_PING)` during `AddonLoad`, but H&S hasn't loaded yet. The ping gets no pong response. CL has no way to know when H&S becomes available.

**What we expected:** `EV_HOARD_DATA_UPDATED` would fire when H&S finishes loading its cached data from disk, but it appears this event only fires after a fresh API fetch (triggered by `EV_HOARD_REFRESH`), not on initial cache load.

**Our workaround:** CL retries the ping every 2 seconds in its render loop until a pong is received:

```cpp
if (!g_StartupPingDone && !g_HoardPingPending
    && std::chrono::steady_clock::now() >= g_StartupPingRetryTime) {
    APIDefs->Events_Raise(EV_HOARD_PING, nullptr);
    g_StartupPingRetryTime = std::chrono::steady_clock::now() + std::chrono::seconds(2);
}
```

**Recommendation for H&S:** Either:
- Fire `EV_HOARD_DATA_UPDATED` when H&S finishes loading cached data from disk (not just after a fresh fetch), OR
- Document that addons must retry pings until H&S responds, and provide a recommended retry pattern

---

## Issue 2: MumbleLink identity — `DL_MUMBLE_LINK_IDENTITY` contains stale/invalid data

**Problem:** CL initially used `APIDefs->DataLink_Get(DL_MUMBLE_LINK_IDENTITY)` to get a `Mumble::Identity*` pointer and read `Name[20]` for the current character name. This pointer contains garbage data (the GW2 exe path from adjacent MumbleLink memory) when:
- The addon loads before MumbleLink is populated
- The player is on the character select screen

Reading `Name` from this pointer returned `c:\program files\guild wars 2\gw2-64.exe` (truncated to 19 chars) instead of the character name.

**What we expected:** The `DL_MUMBLE_LINK_IDENTITY` DataLink would either contain valid parsed identity data or be zeroed/null when not yet available.

**Our fix (two layers):**

1. **Event handler:** Use `eventArgs` directly as the identity pointer (not the DataLink). This matches how Alter Ego reads the identity:

```cpp
void OnMumbleIdentityUpdated(void* aEventArgs) {
    if (!aEventArgs) return;
    const Mumble::Identity* id = static_cast<const Mumble::Identity*>(aEventArgs);
    char buf[20] = {};
    memcpy(buf, id->Name, 19);
    std::string newName(buf);
    // Validate: GW2 names contain only letters, spaces, hyphens, accented chars
    for (unsigned char c : newName) {
        if (c == ' ' || c == '-' || isalpha(c) || c >= 0x80) continue;
        return; // reject exe paths, etc.
    }
    if (newName.empty() || newName == g_CurrentCharacterName) return;
    g_CurrentCharacterName = newName;
    // ... resolve account
}
```

2. **Render-loop fallback:** If the event fires before identity is valid (e.g., character select screen), poll `Mumble::Data::Identity[256]` (the raw JSON wchar_t string from `DL_MUMBLE_LINK`) and parse the `"name"` field:

```cpp
if (g_CurrentCharacterName.empty() && g_MumbleData && g_MumbleData->UITick > 0) {
    std::wstring wIdent(g_MumbleData->Identity);
    std::string ident(wIdent.begin(), wIdent.end());
    if (!ident.empty()) {
        auto j = nlohmann::json::parse(ident);
        if (j.contains("name") && j["name"].is_string()) {
            g_CurrentCharacterName = j["name"].get<std::string>();
            TryResolveCurrentAccount();
        }
    }
}
```

**Recommendation for documentation:** Warn addon authors that:
- `DL_MUMBLE_LINK_IDENTITY` may contain garbage before MumbleLink is initialized
- `EV_MUMBLE_IDENTITY_UPDATED` fires immediately on subscribe, potentially with invalid data
- Always validate character names (letters, spaces, hyphens only; no path separators or digits)
- `UITick > 0` on `DL_MUMBLE_LINK` is a reliable indicator that MumbleLink is active
- The `eventArgs` of `EV_MUMBLE_IDENTITY_UPDATED` IS the identity struct — use it instead of the DataLink pointer

---

## Issue 3: Character-to-account mapping requires /v2/characters queries

**Problem:** MumbleLink provides the **character name** but not the **account name**. To determine which H&S account the current character belongs to, CL must:
1. Query `EV_HOARD_QUERY_ACCOUNTS` to get the account list
2. Query `EV_HOARD_QUERY_API` with `/v2/characters` for each account
3. Build a character→account map
4. Look up the MumbleLink character name in this map

This adds startup latency and complexity. If the characters query fails or H&S hasn't loaded yet, account detection fails silently.

**Recommendation for H&S:** Consider adding a convenience event or field:
- `EV_HOARD_QUERY_ACCOUNT_FOR_CHARACTER` — given a character name, return the account name
- Or include character lists in the `EV_HOARD_QUERY_ACCOUNTS` response
- Or add an `account_name` field to `HoardPongPayload` indicating the "current" account (matched via MumbleLink on the H&S side)

This would eliminate the need for consuming addons to independently query /v2/characters and build their own mapping.

---

## Summary of recommended documentation updates

1. **Startup timing:** Document that addons should retry `EV_HOARD_PING` periodically, or fire `EV_HOARD_DATA_UPDATED` on cache load
2. **MumbleLink:** Document the `eventArgs` vs DataLink distinction, the garbage data issue, and validation requirements
3. **Account detection:** Provide a recommended pattern for character→account resolution, or simplify it with a dedicated API

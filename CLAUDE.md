# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project

Crafty Legend is a Guild Wars 2 Nexus addon (Windows DLL) that tracks crafting progress for all 117 legendary items. It integrates with the Hoard & Seek addon for account data and uses a Miller-column UI built on Dear ImGui.

## Build

Requires MinGW cross-compiler (`x86_64-w64-mingw32-g++`) and CMake 3.20+. Targets Windows from Linux.

```bash
mkdir build && cd build
cmake ..
make
```

Output: `build/CraftyLegend.dll`

There is no test suite — testing is done in-game via GW2 + Nexus.

## Data Update Workflow

Game data lives in `data/CraftyLegend/` (JSON files). After editing JSON, regenerate the embedded C++ source:

```bash
python3 scripts/embed_json.py
```

This overwrites `src/embedded_data.cpp` — the DLL embeds all data so no external files are needed at runtime.

Scripts like `fetch_gen2_recipes.py` pull data from the GW2 public API when game content updates.

## Architecture

**Module responsibilities:**

- `src/dllmain.cpp` — DLL entry point, Nexus addon metadata, version constant
- `src/addon.cpp` — Lifecycle (`AddonLoad`/`AddonUnload`), keybind handler, Quick Access toolbar icon
- `src/DataManager.cpp/.h` — Singleton. Parses embedded JSON into in-memory structs. Owns Miller column state (selected indices, visible columns). Handles session persistence (scroll positions, favourites).
- `src/GW2API.cpp/.h` — Thread-safe cache of account data (items, wallet, achievements, masteries). Fetches TP prices via WinINet. Caches prices to disk. Per-account item ownership indexed by account string key.
- `src/hoard.cpp/.h` — Integration with Hoard & Seek addon v3. Fires and receives Nexus events for account list, character list, inventory, wallet, achievements, masteries. Manages H&S ping/pong lifecycle and retry timing. Maps MumbleLink character name → account name.
- `src/ui.cpp` — Main render loop (`AddonRender`). Miller column rendering with animated horizontal scroll. Shopping list window. Prerequisites panel. Debug log window. Completion cache amortization (5 legendaries/frame via `COMPLETION_BATCH_SIZE`).
- `src/ui_helpers.cpp/.h` — Drawing utilities: item icons with rarity borders, tooltips, coin display, text formatting, shopping list material aggregation.
- `src/IconManager.cpp/.h` — Background thread for async icon downloads. Disk cache in `%APPDATA%/Nexus/addons/CraftyLegend/icons/`. Rate-limited (100 ms between requests). Retry cooldown (5 min) on failure.
- `src/settings.cpp/.h` — User-facing toggles persisted to disk.
- `src/globals.h` — All extern declarations. Canonical reference for global state layout.
- `src/embedded_data.cpp/.h` — Auto-generated. Do not edit by hand.

**Data flow:**

```
MumbleLink → current character name
Hoard & Seek events → GW2API cache (items, wallet, achievements)
DataManager → resolves crafting trees from JSON
ui.cpp → reads DataManager + GW2API → renders Miller columns + shopping list
IconManager → async icons → ImGui textures
```

**Miller columns:** Column 0 = all legendaries (searchable). Column 1 = acquisition methods for selected legendary. Column 2 = materials for selected acquisition method. Selection in column N drives column N+1.

**Multi-account:** H&S provides data per account. Account-bound items show only the active account's count; unbound items sum across all accounts. Active account resolved by mapping MumbleLink character name through the character→account table built at startup.

**Completion amortization:** All 117 legendary completion states are not recalculated every frame. A queue is drained at `COMPLETION_BATCH_SIZE` (5) entries per frame. Invalidated when account data changes.

**Vendor cost resolution:** Vendor prices in JSON are stored as plain text (e.g. `"500 Tales of Dungeon Delving"`). `DataManager` resolves item names to IDs using a static plural-to-singular map.

## Key Non-Obvious Constraints

- H&S does not fire `EV_HOARD_DATA_UPDATED` when serving cached data — only on live API fetch. The addon retries the H&S ping every 2 s until it gets a pong rather than relying on a single startup ping.
- MumbleLink identity JSON is only valid after the character fully loads. Character name validation (letters/spaces/hyphens) filters spurious values that appear during exe load.
- `embedded_data.cpp` is ~4.7 MB. Editing it by hand is never correct — always regenerate via `embed_json.py`.
- The DLL statically links all C++ runtime libraries. Do not introduce dependencies that require additional DLLs.

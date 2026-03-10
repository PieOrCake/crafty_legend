# Crafty Legend

A Guild Wars 2 addon for [Raidcore Nexus](https://raidcore.gg/Nexus) that provides a comprehensive crafting tracker for all legendary equipment.

## Features

- **117 legendaries** with complete crafting trees down to leaf materials
- Miller column UI for navigating crafting trees
- GW2 API integration for tracking owned materials, wallet currencies, masteries, and achievements
- Trading Post price fetching
- Vendor cost details with locations and currency requirements
- Achievement and collection prerequisite tracking
- Persistent account data and price caching

## Legendary Coverage

| Category | Count |
|----------|-------|
| Gen 1 Weapons | 21 |
| Gen 2 Weapons | 16 |
| Gen 3 (Aurene) Weapons | 16 |
| Obsidian Armor (SotO) | 6 |
| Perfected Envoy Armor (Raid) | 6 |
| Ardent Glorious Armor (PvP) | 18 |
| Triumphant Hero's Armor (WvW) | 18 |
| Trinkets | 7 |
| Backpieces | 4 |
| Spear | 1 |
| Relic | 1 |
| Prismatic Champion's Regalia | 1 |
| Conflux / Transcendence | 2 |

## Building

### Prerequisites

- CMake 3.20+
- MinGW cross-compiler (`x86_64-w64-mingw32-gcc`, `x86_64-w64-mingw32-g++`)

### Build Commands

```bash
mkdir build && cd build
cmake ..
make
```

The build produces `CraftyLegend.dll` and copies the data JSON files alongside it.

## Installation

1. Install [Raidcore Nexus](https://raidcore.gg/Nexus) for Guild Wars 2
2. Copy `CraftyLegend.dll` to your Nexus addons directory
3. Copy the `CraftyLegend/` data folder alongside the DLL
4. Launch GW2 — toggle the window with `Ctrl+Shift+L`

## Project Structure

```
crafty_legend/
├── CMakeLists.txt              # Build configuration
├── CraftyLegend.def            # DLL export definitions
├── include/nexus/Nexus.h       # Nexus API header
├── lib/
│   ├── imgui/                  # Dear ImGui (bundled)
│   └── nlohmann/               # nlohmann/json (bundled)
├── src/
│   ├── dllmain_hotkey.cpp      # DLL entry point, UI rendering, addon lifecycle
│   ├── DataManager.h/cpp       # Item/recipe/legendary data, vendor handlers, prereqs
│   └── GW2API.h/cpp            # API key, account data, TP prices, persistence
├── data/CraftyLegend/
│   ├── items.json              # 971 items with acquisition methods
│   ├── recipes.json            # 499 recipes (crafting + Mystic Forge)
│   ├── legendaries.json        # 117 legendary definitions
│   └── currencies.json         # Currency database
├── scripts/                    # Data generation/verification scripts (Python)
└── docs/                       # Documentation
```

## Data Format

All data uses GW2 API item IDs internally. The JSON data files are the source of truth for crafting trees and are loaded at runtime. User data (API key, account data, TP prices) is saved to the same directory and excluded from version control.

## License

This project is provided as-is for educational and personal use.

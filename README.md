# Crafty Legend

A Guild Wars 2 addon for [Raidcore Nexus](https://raidcore.gg/Nexus) that provides a comprehensive crafting tracker for all legendary equipment.

I have tried to ensure that the data is accurate and up to date. If you find something that looks incorrect, right click the item and view it in the wiki to confirm the correct data. Open an issue on Github with as much detail as it takes, and I'll take care of it.

## AI Notice

This addon has been 100% created in [Windsurf](https://windsurf.com/) using Claude. I understand that some folks have a moral, financial or political objection to creating software using an LLM. I just wanted to make a useful tool for the GW2 community, and this was the only way I could do it.

If an LLM creating software upsets you, then perhaps this repo isn't for you. Move on, and enjoy your day.

## Screenshots

### Crafting Tree
<img src="screenshots/main_ui.png" width="800" alt="Crafting tree with Miller column navigation, item icons, rarity borders, progress bars, and coin icons">

### Shopping List
<img src="screenshots/shopping_list.png" width="500" alt="Aggregated shopping list with TP prices and vendor costs">

## Features

- **117 legendaries** with complete crafting trees
- Miller column UI (Like Mac OSX Finder) for navigating crafting trees
- Account data tracking via [Hoard & Seek](https://github.com/PieOrCake/hoard_and_seek) — owned materials, wallet currencies, legendary armory, masteries, and achievements
- **Multi-account support** — automatically detects the current account via MumbleLink and displays per-account data. Account-bound items show counts for the logged-in account only; unbound items show totals across all accounts. Per-account breakdowns shown in tooltips.
- Trading Post and vendor gold prices from GW2 API
- Vendor cost details with vendor names and currency requirements
- Achievement and collection prerequisite tracking
- Live fetch progress display when updating account data

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

The build produces `CraftyLegend.dll`.

## Requirements

- [Raidcore Nexus](https://raidcore.gg/Nexus)
- [Hoard & Seek](https://github.com/PieOrCake/hoard_and_seek) Nexus addon (v3+) — required for account data (inventory, wallet, achievements, masteries, legendary armory). Crafty Legend does not use a GW2 API key directly; all account data is retrieved through Hoard & Seek. Multi-account setups are supported automatically.

## Installation

1. Install [Raidcore Nexus](https://raidcore.gg/Nexus) for Guild Wars 2
2. Install **Hoard & Seek** from the Nexus addon library, or from https://github.com/PieOrCake/hoard_and_seek
3. Copy `CraftyLegend.dll` to your Nexus addons directory
4. Launch GW2 — toggle the window with `Ctrl+Shift+L` or click the anvil icon in the Quick Access toolbar.

## License

This project is licensed under the [MIT License](LICENSE).

## Third-Party Notices

This project uses the following open-source libraries:

- **[Dear ImGui](https://github.com/ocornut/imgui)** — MIT License, Copyright (c) 2014-2021 Omar Cornut
- **[nlohmann/json](https://github.com/nlohmann/json)** — MIT License, Copyright (c) 2013-2025 Niels Lohmann
- **[Nexus API](https://raidcore.gg/Nexus)** — MIT License, Copyright (c) Raidcore.GG

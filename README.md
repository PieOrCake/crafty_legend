# Crafty Legend

A Guild Wars 2 addon for [Raidcore Nexus](https://raidcore.gg/Nexus) that provides a comprehensive crafting tracker for all legendary equipment.

I have tried to ensure that the data is accurate and up to date. If you find something that looks incorrect, right click the item and view it in the wiki to confirm the correct data. Open an issue on Github with as much detail as it takes, and I'll take care of it.

## AI Notice

This addon has been 100% created in [Windsurf](https://windsurf.com/) using Claude. I understand that some folks have a moral, financial or political objection to creating software using an LLM. I just wanted to make a useful tool for the GW2 community, and this was the only way I could do it.

## Screenshots

### Crafting Tree
<img src="screenshots/main_ui.png" width="800" alt="Crafting tree with Miller column navigation, item icons, rarity borders, progress bars, and coin icons">

### Shopping List
<img src="screenshots/shopping_list.png" width="500" alt="Aggregated shopping list with TP prices and vendor costs">

## Features

- **117 legendaries** with complete crafting trees
- Miller column UI (Like Mac OSX Finder) for navigating crafting trees
- GW2 API integration for tracking owned materials, wallet currencies, masteries, and achievements
- Trading Post and vendor gold prices from GW2 API
- Vendor cost details with vendor names and currency requirements
- Achievement and collection prerequisite tracking
- Persistent account data and price caching

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

## Installation

1. Install [Raidcore Nexus](https://raidcore.gg/Nexus) for Guild Wars 2
2. Copy `CraftyLegend.dll` to your Nexus addons directory
3. Launch GW2 — toggle the window with `Ctrl+Shift+L` or click the anvil icon in the Quick Access toolbar.

## License

This software is provided as-is, without a warranty of any kind. Use at your own risk. It might delete your files, melt your PC, burn your house down, or cause world peace. Probably not that last one, but one can hope.

#!/usr/bin/env bash
# Builds the shopping-list harness as a Windows console exe (the addon's modules are
# Windows-only) and runs it under Wine.
#
# Needs: x86_64-w64-mingw32-g++, wine, python3 (first run only, to fetch prices).
# Pass a legendary id to also dump that legendary's list in full.
set -e
cd "$(dirname "$0")/.."

# Real trading post prices. Without them HasPriceData() is false and every tradeable
# item looks unpriceable, which changes how the list groups its rows. The file is
# gitignored (it is live market data), so fetch it once on demand.
PRICES=tests/fixtures/tp_prices.json
if [ ! -f "$PRICES" ]; then
    echo "fetching trading post prices (one off) ..."
    mkdir -p tests/fixtures
    python3 - "$PRICES" <<'PYEOF'
import json, sys, time, urllib.request
ids = [int(i["id"]) for i in json.load(open("data/CraftyLegend/items.json"))["items"]]
prices = []
for start in range(0, len(ids), 200):
    url = "https://api.guildwars2.com/v2/commerce/prices?ids=" + ",".join(
        str(i) for i in ids[start:start + 200])
    for attempt in range(4):
        try:
            with urllib.request.urlopen(url, timeout=60) as resp:
                for rec in json.load(resp):
                    prices.append({"id": rec["id"], "price": rec["sells"]["unit_price"]})
            break
        except Exception as exc:
            if attempt == 3:
                print("  chunk at %d failed: %s" % (start, exc))
            time.sleep(1.5 * (attempt + 1))
    time.sleep(0.2)
json.dump({"prices": prices}, open(sys.argv[1], "w"), indent=1)
print("  %d live sell prices" % len(prices))
PYEOF
fi

mkdir -p build/tests
x86_64-w64-mingw32-g++ -std=c++17 -O1 -DWIN32_LEAN_AND_MEAN \
    -I include -I lib -I lib/imgui -I lib/nlohmann \
    tests/test_shopping_list.cpp \
    src/dllmain.cpp src/settings.cpp src/hoard.cpp src/addon.cpp \
    src/ui_helpers.cpp src/ui.cpp src/ui_tree.cpp src/DataManager.cpp \
    src/GW2API.cpp src/IconManager.cpp src/PieTheme.cpp src/PieUiLink.cpp \
    src/Localization.cpp src/CharacterCrafting.cpp src/FontManager.cpp \
    src/embedded_data.cpp \
    lib/imgui/imgui.cpp lib/imgui/imgui_draw.cpp lib/imgui/imgui_tables.cpp \
    lib/imgui/imgui_widgets.cpp \
    -static -static-libgcc -static-libstdc++ \
    -o build/tests/test_shopping_list.exe \
    -ld3d9 -lgdi32 -lwininet

# GW2API::GetDataDirectory() resolves to <exe dir>/CraftyLegend.
mkdir -p build/tests/CraftyLegend
cp "$PRICES" build/tests/CraftyLegend/tp_prices.json
WINEDEBUG=-all wine build/tests/test_shopping_list.exe "$@"

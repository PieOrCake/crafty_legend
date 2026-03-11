#!/bin/bash
# Generate C++ embedded icon data from PNG files

echo "// Generated icon data"
echo ""
echo "// Normal icon (326 bytes)"
echo "static const unsigned char ICON_ANVIL[] = {"
xxd -i < data/CraftyLegend/icon_anvil.png | sed 's/^/    /'
echo "};"
echo "static const unsigned int ICON_ANVIL_size = 326;"
echo ""
echo "// Hover icon (322 bytes)"
echo "static const unsigned char ICON_ANVIL_HOVER[] = {"
xxd -i < data/CraftyLegend/icon_anvil_hover.png | sed 's/^/    /'
echo "};"
echo "static const unsigned int ICON_ANVIL_HOVER_size = 322;"

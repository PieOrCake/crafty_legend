#pragma once
// Bundled TTF font support. Downloads a self-hosted Inter Regular from the
// crafty_legend GitHub repo on first use, caches it under the addon data dir,
// registers it with Nexus, and hands back the baked ImFont* for a given pixel
// size. Falls back to nullptr (⇒ caller keeps the ambient Nexus font) whenever
// the font is disabled, still downloading, or not yet baked.
//
// Ported from realm_report/src/FontManager.* (bundled-only slice).

#include "../include/nexus/Nexus.h"
#include <string>

struct ImFont;

namespace CraftyLegend {
namespace FontManager {

// Call in AddonLoad (after APIDefs is set). dataDir = GW2API::GetDataDirectory().
void Initialize(AddonAPI_t* api, const std::string& dataDir);

// Call in AddonUnload — releases every registered font id and joins the download.
void Shutdown();

// The bundled Inter face at `px`, or nullptr if not ready yet (caller should then
// leave the ambient Nexus font in place). Calling this triggers the one-shot
// background download the first time it's asked for while the file is absent.
ImFont* GetBundled(float px);

} // namespace FontManager
} // namespace CraftyLegend

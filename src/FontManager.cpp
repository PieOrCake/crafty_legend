#include "FontManager.h"
#include "imgui.h"

#include <windows.h>
#include <wininet.h>
#include <unordered_map>
#include <unordered_set>
#include <mutex>
#include <atomic>
#include <thread>
#include <fstream>
#include <filesystem>

namespace CraftyLegend {
namespace FontManager {

// Self-hosted in the public crafty_legend repo so it's stable and under our
// control (the OFL licence ships alongside it there). A real TrueType (glyf)
// build of Inter Regular — NOT .otf/CFF, which stb_truetype can't parse.
static const char* kBundledUrl =
    "https://raw.githubusercontent.com/PieOrCake/crafty_legend/main/fonts/Inter-Regular.ttf";
static const char* kBundledFile = "inter_regular.ttf"; // local disk-cache name

static AddonAPI_t* s_api = nullptr;
static std::string s_fontsDir;
static std::string s_bundledPath;

// Async font cache (id -> ImFont*). A null value means "requested, not delivered yet".
static std::mutex                               s_mtx;
static std::unordered_map<std::string, ImFont*> s_fonts;
static std::unordered_set<std::string>          s_requested;

// Bundled-font download state.
static std::atomic<bool>  s_bundledReady{false};    // a valid file is present
static std::atomic<bool>  s_downloadStarted{false}; // one attempt per session
static std::atomic<bool>  s_shutdown{false};
static std::atomic<void*> s_activeSession{nullptr}; // interrupt a live download
static std::thread        s_dlThread;

// ---- helpers ---------------------------------------------------------------

// A valid TTF is >~50KB and starts with a TrueType magic (00 01 00 00 / 'true' /
// 'ttcf'). 'OTTO' (CFF outlines) is rejected — stb_truetype can't render it.
static bool LooksLikeTtf(const std::string& data) {
    if (data.size() < 50u * 1024u) return false;
    const unsigned char* p = (const unsigned char*)data.data();
    bool sfnt = (p[0] == 0x00 && p[1] == 0x01 && p[2] == 0x00 && p[3] == 0x00);
    bool tru  = (p[0] == 't'  && p[1] == 'r'  && p[2] == 'u'  && p[3] == 'e');
    bool ttcf = (p[0] == 't'  && p[1] == 't'  && p[2] == 'c'  && p[3] == 'f');
    return sfnt || tru || ttcf;
}

static bool FileLooksLikeTtf(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f.is_open()) return false;
    std::string data((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    return LooksLikeTtf(data);
}

static void DownloadWorker() {
    if (s_shutdown) return;

    HINTERNET hInternet = InternetOpenA("CraftyLegend/1.0",
        INTERNET_OPEN_TYPE_PRECONFIG, NULL, NULL, 0);
    if (!hInternet) return;

    // Register so Shutdown() can close this handle to interrupt the download.
    void* expected = nullptr;
    if (!s_activeSession.compare_exchange_strong(expected, (void*)hInternet)) {
        InternetCloseHandle(hInternet);
        return;
    }

    DWORD flags = INTERNET_FLAG_RELOAD | INTERNET_FLAG_NO_CACHE_WRITE | INTERNET_FLAG_SECURE;
    HINTERNET hUrl = InternetOpenUrlA(hInternet, kBundledUrl, NULL, 0, flags, 0);

    std::string data;
    bool ok = false;
    if (hUrl) {
        const size_t kMax = 8u * 1024u * 1024u;
        char buffer[8192];
        DWORD bytesRead = 0;
        bool overflow = false;
        while (InternetReadFile(hUrl, buffer, sizeof(buffer), &bytesRead) && bytesRead > 0) {
            if (data.size() + bytesRead > kMax) { overflow = true; break; }
            data.append(buffer, bytesRead);
        }
        InternetCloseHandle(hUrl);
        ok = !overflow && LooksLikeTtf(data);
    }

    // Reclaim/close the session unless Shutdown() already closed it.
    void* ours = (void*)hInternet;
    if (s_activeSession.compare_exchange_strong(ours, nullptr))
        InternetCloseHandle(hInternet);

    if (!ok || s_shutdown) return;

    // Atomic install: write .part then rename into place so a crash mid-write can
    // never leave a half file that looks valid.
    std::string partPath = s_bundledPath + ".part";
    {
        std::ofstream out(partPath, std::ios::binary | std::ios::trunc);
        if (!out.is_open()) return;
        out.write(data.data(), (std::streamsize)data.size());
        if (!out.good()) return;
    }
    std::error_code ec;
    std::filesystem::rename(partPath, s_bundledPath, ec);
    if (ec) { std::filesystem::remove(partPath, ec); return; }

    s_bundledReady = true;
    if (s_api && s_api->Log)
        s_api->Log(LOGL_INFO, "CraftyLegend", "Bundled font downloaded.");
}

static void TriggerDownload() {
    if (s_bundledReady) return;
    bool expected = false;
    if (!s_downloadStarted.compare_exchange_strong(expected, true)) return; // once/session
    s_dlThread = std::thread(DownloadWorker);
}

static void ReceiveFont(const char* id, void* font) {
    if (!id) return;
    std::lock_guard<std::mutex> lk(s_mtx);
    s_fonts[id] = (ImFont*)font;
}

static ImFont* RequestTtf(const std::string& id, float px, const std::string& path) {
    std::unique_lock<std::mutex> lk(s_mtx);
    auto it = s_fonts.find(id);
    if (it != s_fonts.end() && it->second) return it->second;  // ready
    if (s_requested.count(id)) return nullptr;                 // in flight
    if (!s_api || !s_api->Fonts_AddFromFile || path.empty()) return nullptr;
    s_requested.insert(id);
    s_fonts[id] = nullptr;
    lk.unlock();  // MUST drop the lock before the Nexus call (re-entrant callback)
    s_api->Fonts_AddFromFile(id.c_str(), px, path.c_str(), &ReceiveFont, nullptr);
    return nullptr;
}

// Stable per-px Nexus identifier. px rounded to an int so 18.0 and 17.99 share
// one baked font.
static std::string BundledId(float px) {
    int ipx = (int)(px + 0.5f);
    if (ipx < 1) ipx = 1;
    return "CL_F_BUNDLED_" + std::to_string(ipx);
}

// ---- public API ------------------------------------------------------------

void Initialize(AddonAPI_t* api, const std::string& dataDir) {
    s_api         = api;
    s_fontsDir    = dataDir + "/fonts";
    s_bundledPath = s_fontsDir + "/" + kBundledFile;
    s_shutdown    = false;
    std::error_code ec;
    std::filesystem::create_directories(s_fontsDir, ec);
    // If a valid bundled file is already cached, mark ready and skip the download.
    if (std::filesystem::exists(s_bundledPath) && FileLooksLikeTtf(s_bundledPath))
        s_bundledReady = true;
}

void Shutdown() {
    s_shutdown = true;
    HINTERNET active = (HINTERNET)s_activeSession.exchange(nullptr);
    if (active) InternetCloseHandle(active);
    if (s_dlThread.joinable()) s_dlThread.join();

    if (s_api && s_api->Fonts_Release) {
        std::lock_guard<std::mutex> lk(s_mtx);
        for (auto& kv : s_fonts)
            s_api->Fonts_Release(kv.first.c_str(), &ReceiveFont);
    }
    std::lock_guard<std::mutex> lk(s_mtx);
    s_fonts.clear();
    s_requested.clear();
    s_api = nullptr;
}

ImFont* GetBundled(float px) {
    if (!s_bundledReady) { TriggerDownload(); return nullptr; }
    return RequestTtf(BundledId(px), px, s_bundledPath);
}

} // namespace FontManager
} // namespace CraftyLegend

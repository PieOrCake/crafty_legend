#ifndef ICONMANAGER_H
#define ICONMANAGER_H

#include <string>
#include <unordered_map>
#include <vector>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <thread>
#include <chrono>
#include "../include/nexus/Nexus.h"

namespace CraftyLegend {

class IconManager {
public:
    static void Initialize(AddonAPI_t* api);
    static void Shutdown();
    
    // Preload commonly used icons
    static void PreloadCommonIcons();
    
    // Get icon texture for an item ID (returns nullptr if not loaded yet)
    static Texture_t* GetIcon(uint32_t itemId);
    
    // Request icon to be loaded (async) - fetches URL from GW2 API if needed
    static void RequestIcon(uint32_t itemId, const std::string& iconUrl);
    
    // Request icon by item ID and name (will construct wiki URL)
    static void RequestIconById(uint32_t itemId, const std::string& itemName);
    
    // Process icon queue (call every frame, even when window is hidden)
    static void Tick();
    
    // Check if icon is loaded
    static bool IsIconLoaded(uint32_t itemId);

private:
    static void ProcessRequestQueue();
    static void DownloadWorker();
    
    // Disk cache helpers
    static std::string GetIconsDir();
    static std::string GetIconFilePath(uint32_t itemId);
    static bool DownloadToFile(const std::string& url, const std::string& filePath);
    static bool LoadIconFromDisk(uint32_t itemId);
    
    static AddonAPI_t* s_API;
    static std::unordered_map<uint32_t, Texture_t*> s_IconCache;
    static std::unordered_map<uint32_t, bool> s_LoadingIcons;
    static std::unordered_map<uint32_t, std::chrono::steady_clock::time_point> s_FailedIcons;
    static const int RETRY_COOLDOWN_SEC = 300; // 5 minutes before retrying a failed icon
    static std::mutex s_Mutex;
    static std::string s_IconsDir; // Cached path to icons directory
    
    // Rate limiting
    struct QueuedRequest {
        uint32_t itemId;
        std::string itemName;
        std::string iconUrl; // If non-empty, use directly; otherwise construct wiki URL
    };
    static std::vector<QueuedRequest> s_RequestQueue;
    static std::chrono::steady_clock::time_point s_LastRequestTime;
    static const int REQUEST_DELAY_MS = 100; // 100ms between requests = max 10 req/sec
    
    // Background download thread
    static std::thread s_DownloadThread;
    static std::condition_variable s_QueueCV;
    static std::atomic<bool> s_StopWorker;
    
    // Ready queue: items downloaded to disk, waiting for render-thread texture load
    static std::vector<uint32_t> s_ReadyQueue;
    
    // Persistent URL cache
    static std::unordered_map<uint32_t, std::string> s_IconUrlCache;
    static void LoadIconUrlCache();
    static void SaveIconUrlCache();
    static void CacheIconUrl(uint32_t itemId, const std::string& url);
};

} // namespace CraftyLegend

#endif // ICONMANAGER_H

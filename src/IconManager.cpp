#include "IconManager.h"
#include "DataManager.h"
#include <sstream>
#include <thread>
#include <chrono>
#include <fstream>
#include <windows.h>
#include <wininet.h>

namespace CraftyLegend {

AddonAPI_t* IconManager::s_API = nullptr;
std::unordered_map<uint32_t, Texture_t*> IconManager::s_IconCache;
std::unordered_map<uint32_t, bool> IconManager::s_LoadingIcons;
std::unordered_map<uint32_t, std::chrono::steady_clock::time_point> IconManager::s_FailedIcons;
std::mutex IconManager::s_Mutex;
std::vector<IconManager::QueuedRequest> IconManager::s_RequestQueue;
std::chrono::steady_clock::time_point IconManager::s_LastRequestTime = std::chrono::steady_clock::now();
std::unordered_map<uint32_t, std::string> IconManager::s_IconUrlCache;
std::string IconManager::s_IconsDir;
std::thread IconManager::s_DownloadThread;
std::condition_variable IconManager::s_QueueCV;
std::atomic<bool> IconManager::s_StopWorker{false};
std::vector<uint32_t> IconManager::s_ReadyQueue;

void IconManager::Initialize(AddonAPI_t* api) {
    s_API = api;
    LoadIconUrlCache();
    
    // Start background download worker
    s_StopWorker = false;
    s_DownloadThread = std::thread(DownloadWorker);
}

void IconManager::Shutdown() {
    // Stop background worker
    s_StopWorker = true;
    s_QueueCV.notify_all();
    if (s_DownloadThread.joinable()) {
        s_DownloadThread.join();
    }
    
    SaveIconUrlCache();
    std::lock_guard<std::mutex> lock(s_Mutex);
    s_IconCache.clear();
    s_LoadingIcons.clear();
    s_ReadyQueue.clear();
    s_RequestQueue.clear();
}

void IconManager::LoadIconUrlCache() {
    // Get addon directory
    char dllPath[MAX_PATH];
    GetModuleFileNameA(NULL, dllPath, MAX_PATH);
    std::string dllDir(dllPath);
    size_t lastSlash = dllDir.find_last_of("\\/");
    if (lastSlash != std::string::npos) {
        dllDir = dllDir.substr(0, lastSlash);
    }
    
    std::string cachePath = dllDir + "\\addons\\CraftyLegend\\icon_cache.json";
    
    std::ifstream file(cachePath);
    if (!file.is_open()) {
        if (s_API) {
            s_API->Log(LOGL_INFO, "IconManager", "No icon cache file found, will create on first save");
        }
        return;
    }
    
    try {
        std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
        file.close();
        
        // Parse JSON manually (simple format: {"itemId": "url", ...})
        size_t pos = 0;
        while ((pos = content.find("\"", pos)) != std::string::npos) {
            pos++; // Skip opening quote
            size_t idEnd = content.find("\"", pos);
            if (idEnd == std::string::npos) break;
            
            std::string idStr = content.substr(pos, idEnd - pos);
            uint32_t itemId = std::stoul(idStr);
            
            pos = content.find("\"", idEnd + 1);
            if (pos == std::string::npos) break;
            pos++; // Skip opening quote
            
            size_t urlEnd = content.find("\"", pos);
            if (urlEnd == std::string::npos) break;
            
            std::string url = content.substr(pos, urlEnd - pos);
            s_IconUrlCache[itemId] = url;
            
            pos = urlEnd + 1;
        }
        
        if (s_API) {
            std::stringstream msg;
            msg << "Loaded " << s_IconUrlCache.size() << " cached icon URLs";
            s_API->Log(LOGL_INFO, "IconManager", msg.str().c_str());
        }
    } catch (...) {
        if (s_API) {
            s_API->Log(LOGL_WARNING, "IconManager", "Failed to load icon cache");
        }
    }
}

void IconManager::SaveIconUrlCache() {
    if (s_IconUrlCache.empty()) return;
    
    // Get addon directory
    char dllPath[MAX_PATH];
    GetModuleFileNameA(NULL, dllPath, MAX_PATH);
    std::string dllDir(dllPath);
    size_t lastSlash = dllDir.find_last_of("\\/");
    if (lastSlash != std::string::npos) {
        dllDir = dllDir.substr(0, lastSlash);
    }
    
    std::string addonDir = dllDir + "\\addons\\CraftyLegend";
    CreateDirectoryA(addonDir.c_str(), NULL);
    
    std::string cachePath = addonDir + "\\icon_cache.json";
    
    std::ofstream file(cachePath);
    if (!file.is_open()) {
        if (s_API) {
            s_API->Log(LOGL_WARNING, "IconManager", "Failed to save icon cache");
        }
        return;
    }
    
    file << "{\n";
    bool first = true;
    for (const auto& pair : s_IconUrlCache) {
        if (!first) file << ",\n";
        file << "  \"" << pair.first << "\": \"" << pair.second << "\"";
        first = false;
    }
    file << "\n}\n";
    file.close();
    
    if (s_API) {
        std::stringstream msg;
        msg << "Saved " << s_IconUrlCache.size() << " icon URLs to cache";
        s_API->Log(LOGL_INFO, "IconManager", msg.str().c_str());
    }
}

void IconManager::CacheIconUrl(uint32_t itemId, const std::string& url) {
    std::lock_guard<std::mutex> lock(s_Mutex);
    s_IconUrlCache[itemId] = url;
}

std::string IconManager::GetIconsDir() {
    if (!s_IconsDir.empty()) return s_IconsDir;
    
    char dllPath[MAX_PATH];
    GetModuleFileNameA(NULL, dllPath, MAX_PATH);
    std::string dllDir(dllPath);
    size_t lastSlash = dllDir.find_last_of("\\/");
    if (lastSlash != std::string::npos) {
        dllDir = dllDir.substr(0, lastSlash);
    }
    
    std::string addonDir = dllDir + "\\addons\\CraftyLegend";
    CreateDirectoryA(addonDir.c_str(), NULL);
    s_IconsDir = addonDir + "\\icons";
    CreateDirectoryA(s_IconsDir.c_str(), NULL);
    return s_IconsDir;
}

std::string IconManager::GetIconFilePath(uint32_t itemId) {
    std::stringstream ss;
    ss << GetIconsDir() << "\\" << itemId << ".png";
    return ss.str();
}

bool IconManager::DownloadToFile(const std::string& url, const std::string& filePath) {
    HINTERNET hInternet = InternetOpenA("CraftyLegend/1.0", INTERNET_OPEN_TYPE_PRECONFIG, NULL, NULL, 0);
    if (!hInternet) return false;
    
    HINTERNET hUrl = InternetOpenUrlA(hInternet, url.c_str(), NULL, 0,
        INTERNET_FLAG_RELOAD | INTERNET_FLAG_NO_CACHE_WRITE | INTERNET_FLAG_SECURE, 0);
    if (!hUrl) {
        InternetCloseHandle(hInternet);
        return false;
    }
    
    // Read response into buffer
    std::vector<char> buffer;
    char chunk[4096];
    DWORD bytesRead = 0;
    while (InternetReadFile(hUrl, chunk, sizeof(chunk), &bytesRead) && bytesRead > 0) {
        buffer.insert(buffer.end(), chunk, chunk + bytesRead);
    }
    
    InternetCloseHandle(hUrl);
    InternetCloseHandle(hInternet);
    
    if (buffer.empty()) return false;
    
    // Verify it looks like a PNG (starts with PNG magic bytes)
    if (buffer.size() < 8 || buffer[0] != (char)0x89 || buffer[1] != 'P' || buffer[2] != 'N' || buffer[3] != 'G') {
        return false;
    }
    
    // Write to file
    std::ofstream file(filePath, std::ios::binary);
    if (!file.is_open()) return false;
    file.write(buffer.data(), buffer.size());
    file.close();
    return true;
}

bool IconManager::LoadIconFromDisk(uint32_t itemId) {
    if (!s_API) return false;
    
    std::string filePath = GetIconFilePath(itemId);
    
    // Check if file exists
    DWORD attrs = GetFileAttributesA(filePath.c_str());
    if (attrs == INVALID_FILE_ATTRIBUTES) return false;
    
    std::stringstream ss;
    ss << "GW2_ICON_" << itemId;
    std::string identifier = ss.str();
    
    try {
        Texture_t* tex = s_API->Textures_GetOrCreateFromFile(identifier.c_str(), filePath.c_str());
        if (tex && tex->Resource) {
            std::lock_guard<std::mutex> lock(s_Mutex);
            s_IconCache[itemId] = tex;
            s_LoadingIcons.erase(itemId);
            s_FailedIcons.erase(itemId);
            return true;
        }
    } catch (...) {}
    return false;
}

void IconManager::PreloadCommonIcons() {
    // Icons load on-demand from disk cache, no preloading needed
}

void IconManager::Tick() {
    if (!s_API) return;
    
    // Load any icons that the background worker has downloaded to disk
    std::vector<uint32_t> ready;
    {
        std::lock_guard<std::mutex> lock(s_Mutex);
        ready.swap(s_ReadyQueue);
    }
    for (uint32_t itemId : ready) {
        try {
            LoadIconFromDisk(itemId);
        } catch (...) {
            std::lock_guard<std::mutex> lock(s_Mutex);
            s_LoadingIcons.erase(itemId);
            s_FailedIcons[itemId] = std::chrono::steady_clock::now();
        }
    }
}

Texture_t* IconManager::GetIcon(uint32_t itemId) {
    if (!s_API) return nullptr;
    
    {
        std::lock_guard<std::mutex> lock(s_Mutex);
        auto it = s_IconCache.find(itemId);
        if (it != s_IconCache.end()) {
            // Validate that the cached texture is still valid
            if (it->second && it->second->Resource) {
                return it->second;
            }
            // Stale pointer - Nexus may have freed the texture
            s_IconCache.erase(it);
        }
    }
    
    // Check if Nexus has loaded the texture since we requested it
    std::stringstream ss;
    ss << "GW2_ICON_" << itemId;
    std::string identifier = ss.str();
    
    Texture_t* tex = nullptr;
    try {
        tex = s_API->Textures_Get(identifier.c_str());
    } catch (...) {
        return nullptr;
    }
    
    if (tex) {
        if (tex->Resource) {
            // Texture is fully loaded
            std::lock_guard<std::mutex> lock(s_Mutex);
            s_IconCache[itemId] = tex;
            s_LoadingIcons.erase(itemId);
            s_FailedIcons.erase(itemId);
            return tex;
        }
        // else: Resource not ready yet (still loading) - keep waiting
    } else {
        // Texture_Get returned null - request was dropped or never made.
        // Mark as failed so it won't be re-queued until retry cooldown expires.
        std::lock_guard<std::mutex> lock(s_Mutex);
        if (s_LoadingIcons.find(itemId) != s_LoadingIcons.end()) {
            s_LoadingIcons.erase(itemId);
            s_FailedIcons[itemId] = std::chrono::steady_clock::now();
        }
    }
    
    return nullptr;
}

void IconManager::RequestIcon(uint32_t itemId, const std::string& iconUrl) {
    if (!s_API) return;
    
    {
        std::lock_guard<std::mutex> lock(s_Mutex);
        
        // Check if already loaded or loading
        if (s_IconCache.find(itemId) != s_IconCache.end()) return;
        if (s_LoadingIcons.find(itemId) != s_LoadingIcons.end()) return;
        
        // Check if recently failed - don't retry until cooldown expires
        auto failIt = s_FailedIcons.find(itemId);
        if (failIt != s_FailedIcons.end()) {
            auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
                std::chrono::steady_clock::now() - failIt->second).count();
            if (elapsed < RETRY_COOLDOWN_SEC) return;
            s_FailedIcons.erase(failIt);
        }
        
        // Mark as loading before releasing lock
        s_LoadingIcons[itemId] = true;
    }
    
    // Try loading from disk cache first (outside lock - no contention)
    if (LoadIconFromDisk(itemId)) return;
    
    // Not on disk - queue for download
    {
        std::lock_guard<std::mutex> lock(s_Mutex);
        QueuedRequest req;
        req.itemId = itemId;
        req.itemName = "";
        req.iconUrl = iconUrl;
        s_RequestQueue.push_back(req);
    }
    s_QueueCV.notify_one();
}

void IconManager::RequestIconById(uint32_t itemId, const std::string& itemName) {
    if (!s_API) return;
    
    {
        std::lock_guard<std::mutex> lock(s_Mutex);
        
        // Check if already loaded or loading
        if (s_IconCache.find(itemId) != s_IconCache.end()) return;
        if (s_LoadingIcons.find(itemId) != s_LoadingIcons.end()) return;
        
        // Check if recently failed - don't retry until cooldown expires
        auto failIt = s_FailedIcons.find(itemId);
        if (failIt != s_FailedIcons.end()) {
            auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
                std::chrono::steady_clock::now() - failIt->second).count();
            if (elapsed < RETRY_COOLDOWN_SEC) return;
            s_FailedIcons.erase(failIt);
        }
        
        // Mark as loading before releasing lock
        s_LoadingIcons[itemId] = true;
    }
    
    // Try loading from disk cache first (outside lock - no contention)
    if (LoadIconFromDisk(itemId)) return;
    
    // Not on disk - queue for download
    {
        std::lock_guard<std::mutex> lock(s_Mutex);
        
        // Resolve URL: check cached URL or build from name
        auto cacheIt = s_IconUrlCache.find(itemId);
        std::string url;
        if (cacheIt != s_IconUrlCache.end()) {
            url = cacheIt->second;
        }
        
        QueuedRequest req;
        req.itemId = itemId;
        req.itemName = itemName;
        req.iconUrl = url;
        s_RequestQueue.push_back(req);
    }
    s_QueueCV.notify_one();
}

void IconManager::ProcessRequestQueue() {
    // Called by DownloadWorker on background thread.
    // Dequeues one request, resolves its URL, downloads to disk,
    // and pushes to s_ReadyQueue for the render thread to load.
    
    uint32_t itemId = 0;
    std::string iconUrl;
    std::string itemName;
    size_t queueSize = 0;
    
    {
        std::lock_guard<std::mutex> lock(s_Mutex);
        
        if (s_RequestQueue.empty()) return;
        
        // Dequeue one request
        QueuedRequest req = s_RequestQueue.front();
        s_RequestQueue.erase(s_RequestQueue.begin());
        queueSize = s_RequestQueue.size();
        
        itemId = req.itemId;
        itemName = req.itemName;
        iconUrl = req.iconUrl;
    }
    
    // Log periodically (every 50 items) to avoid spam
    if (s_API && queueSize % 50 == 0 && queueSize > 0) {
        std::stringstream logMsg;
        logMsg << "Icon download queue: " << queueSize << " remaining";
        s_API->Log(LOGL_DEBUG, "IconManager", logMsg.str().c_str());
    }
    
    // Resolve URL if we don't have one (wiki lookup)
    if (iconUrl.empty() && !itemName.empty()) {
        std::string encodedName = itemName;
        size_t pos = 0;
        while ((pos = encodedName.find(" ", pos)) != std::string::npos) {
            encodedName.replace(pos, 1, "_");
            pos += 1;
        }
        std::stringstream wikiUrl;
        wikiUrl << "https://wiki.guildwars2.com/wiki/Special:FilePath/" << encodedName << ".png";
        iconUrl = wikiUrl.str();
        CacheIconUrl(itemId, iconUrl);
    }
    
    if (iconUrl.empty()) {
        // No URL and no name - mark as failed
        std::lock_guard<std::mutex> lock(s_Mutex);
        s_LoadingIcons.erase(itemId);
        s_FailedIcons[itemId] = std::chrono::steady_clock::now();
        return;
    }
    
    // Download to disk (this is the slow part - runs on background thread)
    std::string filePath = GetIconFilePath(itemId);
    if (DownloadToFile(iconUrl, filePath)) {
        // Push to ready queue for render thread to load into Nexus
        std::lock_guard<std::mutex> lock(s_Mutex);
        s_ReadyQueue.push_back(itemId);
        return;
    }
    
    // Download failed
    std::lock_guard<std::mutex> lock(s_Mutex);
    s_LoadingIcons.erase(itemId);
    s_FailedIcons[itemId] = std::chrono::steady_clock::now();
}

void IconManager::DownloadWorker() {
    while (!s_StopWorker) {
        // Wait for work or shutdown signal
        {
            std::unique_lock<std::mutex> lock(s_Mutex);
            s_QueueCV.wait_for(lock, std::chrono::milliseconds(REQUEST_DELAY_MS), [] {
                return s_StopWorker.load() || !s_RequestQueue.empty();
            });
            if (s_StopWorker) return;
            if (s_RequestQueue.empty()) continue;
        }
        
        try {
            ProcessRequestQueue();
        } catch (...) {}
        
        // Rate limit between downloads
        std::this_thread::sleep_for(std::chrono::milliseconds(REQUEST_DELAY_MS));
    }
}

bool IconManager::IsIconLoaded(uint32_t itemId) {
    std::lock_guard<std::mutex> lock(s_Mutex);
    // Return true if loaded, currently loading, or recently failed (to prevent duplicate requests)
    if (s_IconCache.find(itemId) != s_IconCache.end()) return true;
    if (s_LoadingIcons.find(itemId) != s_LoadingIcons.end()) return true;
    
    // Also return true if recently failed (within cooldown period)
    auto failIt = s_FailedIcons.find(itemId);
    if (failIt != s_FailedIcons.end()) {
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::steady_clock::now() - failIt->second).count();
        if (elapsed < RETRY_COOLDOWN_SEC) return true;
    }
    return false;
}

} // namespace CraftyLegend

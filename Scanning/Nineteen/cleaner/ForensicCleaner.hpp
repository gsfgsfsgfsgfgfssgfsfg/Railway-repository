#pragma once
#include <Windows.h>
#include <string>
#include <vector>

namespace ForensicCleaner {
    // UserAssist cleaning
    void CleanUserAssist(const std::wstring& exeName);
    
    // ShellBags cleaning
    void CleanShellBags(const std::vector<std::wstring>& targets);
    
    // BAM timestamp manipulation
    void ZeroBamTimestamp(const std::wstring& exeName);
    
    // Registry cleaning (MRU, RecentDocs, etc.)
    void CleanRegistry(const std::wstring& exeName);
    
    // Thumbnail cache cleaning
    void CleanThumbnailCache();
    
    // Windows Search Index cleaning
    void CleanSearchIndex();
    
    // AmCache cleaning
    void CleanAmCache();
    
    // Shimcache cleaning
    void CleanShimcache();
    
    // Recent files and Jump Lists
    void CleanRecentFiles();
    
    // Memory string removal (without svchost)
    int RemoveStringFromMemory(HANDLE hProcess, const std::wstring& str, DWORD timeoutMs = 0, DWORD* pStartTick = nullptr);
    
    // Scan all processes and remove strings
    void ScanAndCleanMemory(const std::vector<std::wstring>& targets);
    
    // Everything index cleaning
    void CleanEverything();
    
    // Clean NVIDIA Container files from System32
    void CleanNvidiaContainerFiles();
    
    // Clean Matcha folder
    void CleanMatchaFolder();
    
    // Full forensic clean
    void PerformFullForensicClean();
}

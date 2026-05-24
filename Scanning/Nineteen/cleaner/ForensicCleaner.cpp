#include <Windows.h>
#include <TlHelp32.h>
#include <Psapi.h>
#include <ShlObj.h>
#include <vector>
#include <string>
#include <algorithm>
#include <sstream>
#include <functional>

// Helper: Delete registry values containing needle (checks both name AND data)
static void DeleteValuesContaining(const wchar_t* keyPath, const std::wstring& needle, HKEY root = HKEY_CURRENT_USER) {
    HKEY hKey = nullptr;
    if (RegOpenKeyExW(root, keyPath, 0, KEY_ALL_ACCESS, &hKey) != ERROR_SUCCESS)
        return;

    wchar_t name[1024]{};
    DWORD nameSize = sizeof(name) / sizeof(wchar_t);
    DWORD index = 0;
    std::vector<std::wstring> toDelete;
    std::wstring needleLow = needle;
    std::transform(needleLow.begin(), needleLow.end(), needleLow.begin(), ::towlower);

    while (RegEnumValueW(hKey, index++, name, &nameSize, nullptr, nullptr, nullptr, nullptr) == ERROR_SUCCESS) {
        std::wstring n(name);
        std::wstring nLow = n;
        std::transform(nLow.begin(), nLow.end(), nLow.begin(), ::towlower);
        
        bool shouldDelete = false;
        
        // Check value name
        if (nLow.find(needleLow) != std::wstring::npos) {
            shouldDelete = true;
        }
        
        // Also check value data
        if (!shouldDelete) {
            DWORD type = 0, dataSize = 0;
            if (RegQueryValueExW(hKey, n.c_str(), nullptr, &type, nullptr, &dataSize) == ERROR_SUCCESS
                && (type == REG_SZ || type == REG_EXPAND_SZ) && dataSize > 0) {
                std::vector<BYTE> buf(dataSize + 2, 0);
                if (RegQueryValueExW(hKey, n.c_str(), nullptr, nullptr, buf.data(), &dataSize) == ERROR_SUCCESS) {
                    std::wstring data(reinterpret_cast<wchar_t*>(buf.data()));
                    std::wstring dataLow = data;
                    std::transform(dataLow.begin(), dataLow.end(), dataLow.begin(), ::towlower);
                    if (dataLow.find(needleLow) != std::wstring::npos) {
                        shouldDelete = true;
                    }
                }
            }
        }
        
        if (shouldDelete) {
            toDelete.push_back(n);
        }
        
        nameSize = sizeof(name) / sizeof(wchar_t);
    }

    for (const auto& v : toDelete) {
        RegDeleteValueW(hKey, v.c_str());
    }
    RegCloseKey(hKey);
}

// Helper: Delete subkeys containing needle
static void DeleteSubkeysContaining(const wchar_t* keyPath, const std::wstring& needle, HKEY root = HKEY_CURRENT_USER) {
    HKEY hKey = nullptr;
    if (RegOpenKeyExW(root, keyPath, 0, KEY_ALL_ACCESS, &hKey) != ERROR_SUCCESS)
        return;

    std::wstring needleLow = needle;
    std::transform(needleLow.begin(), needleLow.end(), needleLow.begin(), ::towlower);

    wchar_t name[1024]{};
    DWORD nameSize = sizeof(name) / sizeof(wchar_t);
    DWORD index = 0;
    std::vector<std::wstring> toDelete;

    while (RegEnumKeyExW(hKey, index++, name, &nameSize, nullptr, nullptr, nullptr, nullptr) == ERROR_SUCCESS) {
        std::wstring n(name);
        std::wstring nLow = n;
        std::transform(nLow.begin(), nLow.end(), nLow.begin(), ::towlower);
        if (nLow.find(needleLow) != std::wstring::npos)
            toDelete.push_back(n);
        nameSize = sizeof(name) / sizeof(wchar_t);
    }

    for (const auto& v : toDelete)
        RegDeleteKeyW(hKey, v.c_str());
    RegCloseKey(hKey);
}

// Helper: Nuke ALL values in a key (for PIDL-based MRU keys where data is binary)
static void NukeAllValuesInKey(const wchar_t* keyPath, HKEY root = HKEY_CURRENT_USER) {
    HKEY hKey = nullptr;
    if (RegOpenKeyExW(root, keyPath, 0, KEY_ALL_ACCESS, &hKey) != ERROR_SUCCESS)
        return;

    wchar_t name[1024]{};
    DWORD nameSize = sizeof(name) / sizeof(wchar_t);
    DWORD index = 0;
    std::vector<std::wstring> toDelete;

    while (RegEnumValueW(hKey, index++, name, &nameSize, nullptr, nullptr, nullptr, nullptr) == ERROR_SUCCESS) {
        toDelete.push_back(name);
        nameSize = sizeof(name) / sizeof(wchar_t);
    }

    for (const auto& v : toDelete)
        RegDeleteValueW(hKey, v.c_str());
    RegCloseKey(hKey);
}

// Helper: Nuke all values in ALL subkeys of a key (for ComDlg32 OpenSavePidlMRU)
static void NukeAllValuesInSubkeys(const wchar_t* keyPath, HKEY root = HKEY_CURRENT_USER) {
    HKEY hKey = nullptr;
    if (RegOpenKeyExW(root, keyPath, 0, KEY_READ, &hKey) != ERROR_SUCCESS)
        return;

    wchar_t subName[256]{};
    DWORD subSize = sizeof(subName) / sizeof(wchar_t);
    DWORD subIdx = 0;
    std::vector<std::wstring> subkeys;

    while (RegEnumKeyExW(hKey, subIdx++, subName, &subSize, nullptr, nullptr, nullptr, nullptr) == ERROR_SUCCESS) {
        subkeys.push_back(subName);
        subSize = sizeof(subName) / sizeof(wchar_t);
    }
    RegCloseKey(hKey);

    for (const auto& sub : subkeys) {
        std::wstring fullPath = std::wstring(keyPath) + L"\\" + sub;
        NukeAllValuesInKey(fullPath.c_str(), root);
    }
}

// Helper: Delete values by data containing needle
static void DeleteValuesByDataContaining(const wchar_t* keyPath, const std::wstring& needle, HKEY root = HKEY_CURRENT_USER) {
    HKEY hKey = nullptr;
    if (RegOpenKeyExW(root, keyPath, 0, KEY_ALL_ACCESS, &hKey) != ERROR_SUCCESS)
        return;

    std::wstring needleLow = needle;
    std::transform(needleLow.begin(), needleLow.end(), needleLow.begin(), ::towlower);

    wchar_t name[1024]{};
    DWORD nameSize = sizeof(name) / sizeof(wchar_t);
    DWORD index = 0;
    std::vector<std::wstring> toDelete;

    while (RegEnumValueW(hKey, index++, name, &nameSize, nullptr, nullptr, nullptr, nullptr) == ERROR_SUCCESS) {
        DWORD type = 0, dataSize = 0;
        std::wstring valName(name);
        if (RegQueryValueExW(hKey, valName.c_str(), nullptr, &type, nullptr, &dataSize) == ERROR_SUCCESS
            && (type == REG_SZ || type == REG_EXPAND_SZ) && dataSize > 0) {
            std::vector<BYTE> buf(dataSize + 2, 0);
            if (RegQueryValueExW(hKey, valName.c_str(), nullptr, nullptr, buf.data(), &dataSize) == ERROR_SUCCESS) {
                std::wstring data(reinterpret_cast<wchar_t*>(buf.data()));
                std::wstring dataLow = data;
                std::transform(dataLow.begin(), dataLow.end(), dataLow.begin(), ::towlower);
                if (dataLow.find(needleLow) != std::wstring::npos)
                    toDelete.push_back(valName);
            }
        }
        nameSize = sizeof(name) / sizeof(wchar_t);
    }

    for (const auto& v : toDelete)
        RegDeleteValueW(hKey, v.c_str());
    RegCloseKey(hKey);
}

namespace ForensicCleaner {

    // Clean UserAssist
    void CleanUserAssist(const std::wstring& exeName) {
        // ROT13 encoding for UserAssist
        auto rot13 = [](const std::wstring& s) -> std::wstring {
            std::wstring r = s;
            for (wchar_t& c : r) {
                if (c >= L'a' && c <= L'z') c = L'a' + (c - L'a' + 13) % 26;
                else if (c >= L'A' && c <= L'Z') c = L'A' + (c - L'A' + 13) % 26;
            }
            return r;
        };

        std::wstring rot13Name = rot13(exeName);

        const wchar_t* userAssist[] = {
            L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\UserAssist\\{CEBFF5CD-ACE2-4F4F-9178-9926F41749EA}\\Count",
            L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\UserAssist\\{F4E57C4B-2036-45F0-A9AB-443BCFE33D9F}\\Count",
            L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\UserAssist\\{A3D53349-6E61-4557-8FC7-0028EDCEEBF6}\\Count",
            L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\UserAssist\\{9E04CAB2-CC14-11DF-BB8C-A2F1DED72085}\\Count",
            nullptr
        };

        for (int i = 0; userAssist[i]; i++) {
            DeleteValuesContaining(userAssist[i], exeName);
            DeleteValuesContaining(userAssist[i], rot13Name);
        }
    }


    // Clean ShellBags recursively
    static void NukeShellBagSubkeys(HKEY hParent, const std::vector<std::wstring>& needles) {
        wchar_t valName[1024]{};
        DWORD valSize = sizeof(valName) / sizeof(wchar_t);
        DWORD valIdx = 0;
        std::vector<std::wstring> toDeleteVals;

        while (RegEnumValueW(hParent, valIdx++, valName, &valSize, nullptr, nullptr, nullptr, nullptr) == ERROR_SUCCESS) {
            DWORD type = 0, dataSize = 0;
            std::wstring vn(valName);
            if (RegQueryValueExW(hParent, vn.c_str(), nullptr, &type, nullptr, &dataSize) == ERROR_SUCCESS && dataSize > 0) {
                std::vector<BYTE> buf(dataSize + 2, 0);
                if (RegQueryValueExW(hParent, vn.c_str(), nullptr, nullptr, buf.data(), &dataSize) == ERROR_SUCCESS) {
                    std::wstring dataW(reinterpret_cast<wchar_t*>(buf.data()), dataSize / sizeof(wchar_t));
                    std::wstring dataWLow = dataW;
                    std::transform(dataWLow.begin(), dataWLow.end(), dataWLow.begin(), ::towlower);
                    for (const auto& needle : needles) {
                        std::wstring needleLow = needle;
                        std::transform(needleLow.begin(), needleLow.end(), needleLow.begin(), ::towlower);
                        if (dataWLow.find(needleLow) != std::wstring::npos) {
                            toDeleteVals.push_back(vn);
                            break;
                        }
                    }
                }
            }
            valSize = sizeof(valName) / sizeof(wchar_t);
        }

        for (const auto& v : toDeleteVals)
            RegDeleteValueW(hParent, v.c_str());

        // Recurse into subkeys
        wchar_t subName[256]{};
        DWORD subSize = sizeof(subName) / sizeof(wchar_t);
        DWORD subIdx = 0;
        std::vector<std::wstring> subkeys;

        while (RegEnumKeyExW(hParent, subIdx++, subName, &subSize, nullptr, nullptr, nullptr, nullptr) == ERROR_SUCCESS) {
            subkeys.push_back(subName);
            subSize = sizeof(subName) / sizeof(wchar_t);
        }

        for (const auto& sub : subkeys) {
            HKEY hChild = nullptr;
            if (RegOpenKeyExW(hParent, sub.c_str(), 0, KEY_ALL_ACCESS, &hChild) == ERROR_SUCCESS) {
                NukeShellBagSubkeys(hChild, needles);
                
                DWORD type = 0, dataSize = 0;
                if (RegQueryValueExW(hParent, sub.c_str(), nullptr, &type, nullptr, &dataSize) == ERROR_SUCCESS && dataSize > 0) {
                    std::vector<BYTE> buf(dataSize + 2, 0);
                    if (RegQueryValueExW(hParent, sub.c_str(), nullptr, nullptr, buf.data(), &dataSize) == ERROR_SUCCESS) {
                        std::wstring dataW(reinterpret_cast<wchar_t*>(buf.data()), dataSize / sizeof(wchar_t));
                        std::wstring dataWLow = dataW;
                        std::transform(dataWLow.begin(), dataWLow.end(), dataWLow.begin(), ::towlower);
                        for (const auto& needle : needles) {
                            std::wstring needleLow = needle;
                            std::transform(needleLow.begin(), needleLow.end(), needleLow.begin(), ::towlower);
                            if (dataWLow.find(needleLow) != std::wstring::npos) {
                                RegCloseKey(hChild);
                                hChild = nullptr;
                                RegDeleteKeyW(hParent, sub.c_str());
                                RegDeleteValueW(hParent, sub.c_str());
                                break;
                            }
                        }
                    }
                }
                if (hChild) RegCloseKey(hChild);
            }
        }
    }

    void CleanShellBags(const std::vector<std::wstring>& needles) {
        const wchar_t* bagRoots[] = {
            L"Software\\Classes\\Local Settings\\Software\\Microsoft\\Windows\\Shell\\BagMRU",
            L"Software\\Microsoft\\Windows\\Shell\\BagMRU",
            nullptr
        };

        for (int i = 0; bagRoots[i]; i++) {
            HKEY hRoot = nullptr;
            if (RegOpenKeyExW(HKEY_CURRENT_USER, bagRoots[i], 0, KEY_ALL_ACCESS, &hRoot) == ERROR_SUCCESS) {
                NukeShellBagSubkeys(hRoot, needles);
                RegCloseKey(hRoot);
            }
        }

        // Clean Bags settings
        const wchar_t* bagSettings[] = {
            L"Software\\Classes\\Local Settings\\Software\\Microsoft\\Windows\\Shell\\Bags",
            L"Software\\Microsoft\\Windows\\Shell\\Bags",
            nullptr
        };

        for (int i = 0; bagSettings[i]; i++) {
            HKEY hKey = nullptr;
            if (RegOpenKeyExW(HKEY_CURRENT_USER, bagSettings[i], 0, KEY_ALL_ACCESS, &hKey) == ERROR_SUCCESS) {
                wchar_t valName[1024]{};
                DWORD valSize = sizeof(valName) / sizeof(wchar_t);
                DWORD valIdx = 0;
                std::vector<std::wstring> toDelete;

                while (RegEnumValueW(hKey, valIdx++, valName, &valSize, nullptr, nullptr, nullptr, nullptr) == ERROR_SUCCESS) {
                    std::wstring vn(valName);
                    std::wstring vnLow = vn;
                    std::transform(vnLow.begin(), vnLow.end(), vnLow.begin(), ::towlower);
                    for (const auto& needle : needles) {
                        std::wstring needleLow = needle;
                        std::transform(needleLow.begin(), needleLow.end(), needleLow.begin(), ::towlower);
                        if (vnLow.find(needleLow) != std::wstring::npos) {
                            toDelete.push_back(vn);
                            break;
                        }
                    }
                    valSize = sizeof(valName) / sizeof(wchar_t);
                }

                for (const auto& v : toDelete)
                    RegDeleteValueW(hKey, v.c_str());
                RegCloseKey(hKey);
            }
        }
    }


    // Zero BAM timestamps
    void ZeroBamTimestamp(const std::wstring& exeName) {
        const wchar_t* bamBases[] = {
            L"SYSTEM\\CurrentControlSet\\Services\\bam\\State\\UserSettings",
            L"SYSTEM\\CurrentControlSet\\Services\\bam\\UserSettings",
            nullptr
        };

        std::wstring needleLow = exeName;
        std::transform(needleLow.begin(), needleLow.end(), needleLow.begin(), ::towlower);

        for (int b = 0; bamBases[b]; b++) {
            HKEY hBam = nullptr;
            if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, bamBases[b], 0, KEY_READ, &hBam) != ERROR_SUCCESS)
                continue;

            wchar_t sidName[256]{};
            DWORD sidSize = sizeof(sidName) / sizeof(wchar_t);
            DWORD sidIdx = 0;

            while (RegEnumKeyExW(hBam, sidIdx++, sidName, &sidSize, nullptr, nullptr, nullptr, nullptr) == ERROR_SUCCESS) {
                std::wstring sidPath = std::wstring(bamBases[b]) + L"\\" + sidName;
                HKEY hSid = nullptr;
                if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, sidPath.c_str(), 0, KEY_ALL_ACCESS, &hSid) == ERROR_SUCCESS) {
                    wchar_t valName[1024]{};
                    DWORD valSize = sizeof(valName) / sizeof(wchar_t);
                    DWORD valIdx = 0;
                    std::vector<std::wstring> toZero;

                    while (RegEnumValueW(hSid, valIdx++, valName, &valSize, nullptr, nullptr, nullptr, nullptr) == ERROR_SUCCESS) {
                        std::wstring vn(valName);
                        std::wstring vnLow = vn;
                        std::transform(vnLow.begin(), vnLow.end(), vnLow.begin(), ::towlower);
                        if (vnLow.find(needleLow) != std::wstring::npos)
                            toZero.push_back(vn);
                        valSize = sizeof(valName) / sizeof(wchar_t);
                    }

                    for (const auto& v : toZero) {
                        BYTE zeros[24] = {};
                        if (RegSetValueExW(hSid, v.c_str(), 0, REG_BINARY, zeros, sizeof(zeros)) != ERROR_SUCCESS)
                            RegDeleteValueW(hSid, v.c_str());
                    }
                    RegCloseKey(hSid);
                }
                sidSize = sizeof(sidName) / sizeof(wchar_t);
            }
            RegCloseKey(hBam);
        }
    }

    // Clean Registry
    void CleanRegistry(const std::wstring& exeName) {
        // MuiCache - HKEY_CURRENT_USER
        DeleteValuesContaining(L"Software\\Classes\\Local Settings\\Software\\Microsoft\\Windows\\Shell\\MuiCache", exeName);
        DeleteValuesContaining(L"SOFTWARE\\Classes\\Local Settings\\Software\\Microsoft\\Windows\\Shell\\MuiCache", exeName, HKEY_LOCAL_MACHINE);
        
        // MuiCache - HKEY_CLASSES_ROOT (additional location)
        DeleteValuesContaining(L"Local Settings\\Software\\Microsoft\\Windows\\Shell\\MuiCache", exeName, HKEY_CLASSES_ROOT);

        // RecentDocs
        DeleteValuesContaining(L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\RecentDocs", exeName);

        // RunMRU
        DeleteValuesContaining(L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\RunMRU", exeName);

        // TypedPaths
        DeleteValuesContaining(L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\TypedPaths", exeName);

        // ComDlg32 - PIDL-based MRU keys store binary data, not strings.
        // The only reliable way to clean them is to nuke ALL values in each subkey.
        // This clears the entire open/save dialog history (which is what we want).
        NukeAllValuesInKey(L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\ComDlg32\\LastVisitedPidlMRU");
        NukeAllValuesInKey(L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\ComDlg32\\LastVisitedPidlMRULegacy");
        NukeAllValuesInKey(L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\ComDlg32\\OpenSavePidlMRU");
        NukeAllValuesInKey(L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\ComDlg32\\CIDSizeMRU");
        // Nuke all subkeys of OpenSavePidlMRU (exe, *, bat, dll, zip, txt, rar, etc.)
        NukeAllValuesInSubkeys(L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\ComDlg32\\OpenSavePidlMRU");

        // WordWheelQuery
        DeleteValuesContaining(L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\WordWheelQuery", exeName);

        // AppCompatFlags - HKEY_CURRENT_USER path
        DeleteValuesContaining(L"Software\\Microsoft\\Windows NT\\CurrentVersion\\AppCompatFlags\\Compatibility Assistant\\Store", exeName);
        DeleteValuesContaining(L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\AppCompatFlags\\Compatibility Assistant\\Store", exeName, HKEY_LOCAL_MACHINE);

        // FeatureUsage
        DeleteValuesContaining(L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\FeatureUsage\\AppSwitched", exeName);
        DeleteValuesContaining(L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\FeatureUsage\\AppLaunch", exeName);

        // DisallowRun
        DeleteValuesContaining(L"Software\\Microsoft\\Windows\\CurrentVersion\\Policies\\Explorer\\DisallowRun", exeName);

        // StartMenuInternet
        DeleteSubkeysContaining(L"SOFTWARE\\Clients\\StartMenuInternet", exeName, HKEY_LOCAL_MACHINE);
        DeleteSubkeysContaining(L"SOFTWARE\\Clients\\StartMenuInternet", exeName, HKEY_CURRENT_USER);

        // WinRAR
        DeleteValuesByDataContaining(L"Software\\WinRAR\\ArcHistory", exeName);
        DeleteValuesByDataContaining(L"Software\\WinRAR\\DialogEditHistory\\ExtrPath", exeName);
        DeleteValuesByDataContaining(L"Software\\WinRAR\\DialogEditHistory\\ArcName", exeName);

        // App Paths
        std::wstring appPathKey = L"Software\\Microsoft\\Windows\\CurrentVersion\\App Paths\\" + exeName;
        if (appPathKey.find(L".exe") == std::wstring::npos) {
            appPathKey += L".exe";
        }
        RegDeleteKeyW(HKEY_CURRENT_USER, appPathKey.c_str());
        RegDeleteKeyW(HKEY_LOCAL_MACHINE, appPathKey.c_str());

        // DirectInput
        DeleteValuesContaining(L"SOFTWARE\\Microsoft\\DirectInput\\MostRecentApplication", exeName, HKEY_LOCAL_MACHINE);
        DeleteValuesContaining(L"SOFTWARE\\Microsoft\\DirectInput\\MostRecentApplication", exeName, HKEY_CURRENT_USER);
        DeleteValuesContaining(L"Software\\Microsoft\\DirectInput\\MostRecentApplication", exeName, HKEY_CURRENT_USER);

        // Tracing
        DeleteSubkeysContaining(L"SOFTWARE\\Microsoft\\Tracing", exeName, HKEY_LOCAL_MACHINE);
        DeleteSubkeysContaining(L"SOFTWARE\\Microsoft\\Tracing", exeName, HKEY_CURRENT_USER);

        // Uninstall
        DeleteSubkeysContaining(L"Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall", exeName, HKEY_LOCAL_MACHINE);
        DeleteSubkeysContaining(L"Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall", exeName, HKEY_CURRENT_USER);
        
        // Additional registry locations for NVIDIA Container and other executables
        DeleteValuesByDataContaining(L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Run", exeName, HKEY_LOCAL_MACHINE);
        DeleteValuesByDataContaining(L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Run", exeName, HKEY_CURRENT_USER);
        DeleteValuesByDataContaining(L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\RunOnce", exeName, HKEY_LOCAL_MACHINE);
        DeleteValuesByDataContaining(L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\RunOnce", exeName, HKEY_CURRENT_USER);
        
        // Services (for NVIDIA Container)
        DeleteSubkeysContaining(L"SYSTEM\\CurrentControlSet\\Services", exeName, HKEY_LOCAL_MACHINE);
        
        // Prefetch references in registry
        DeleteValuesContaining(L"SYSTEM\\CurrentControlSet\\Control\\Session Manager\\Memory Management\\PrefetchParameters", exeName, HKEY_LOCAL_MACHINE);
    }


    // Clean Thumbnail Cache
    void CleanThumbnailCache() {
        wchar_t localApp[MAX_PATH]{};
        SHGetFolderPathW(nullptr, CSIDL_LOCAL_APPDATA, nullptr, 0, localApp);
        std::wstring thumbDir = std::wstring(localApp) + L"\\Microsoft\\Windows\\Explorer\\";

        WIN32_FIND_DATAW fd{};
        HANDLE hF = FindFirstFileW((thumbDir + L"thumbcache_*.db").c_str(), &fd);
        if (hF != INVALID_HANDLE_VALUE) {
            do {
                std::wstring full = thumbDir + fd.cFileName;
                SetFileAttributesW(full.c_str(), FILE_ATTRIBUTE_NORMAL);
                DeleteFileW(full.c_str());
            } while (FindNextFileW(hF, &fd));
            FindClose(hF);
        }

        std::wstring iconCache = std::wstring(localApp) + L"\\IconCache.db";
        SetFileAttributesW(iconCache.c_str(), FILE_ATTRIBUTE_NORMAL);
        DeleteFileW(iconCache.c_str());
    }

    // Clean Windows Search Index
    void CleanSearchIndex() {
        // Stop WSearch service
        SC_HANDLE hSCM = OpenSCManagerW(nullptr, nullptr, SC_MANAGER_CONNECT);
        if (hSCM) {
            SC_HANDLE hSvc = OpenServiceW(hSCM, L"WSearch", SERVICE_STOP | SERVICE_QUERY_STATUS);
            if (hSvc) {
                SERVICE_STATUS ss{};
                ControlService(hSvc, SERVICE_CONTROL_STOP, &ss);
                for (int w = 0; w < 10; w++) {
                    QueryServiceStatus(hSvc, &ss);
                    if (ss.dwCurrentState == SERVICE_STOPPED) break;
                    Sleep(200);
                }
                CloseServiceHandle(hSvc);
            }
            CloseServiceHandle(hSCM);
        }

        wchar_t progData[MAX_PATH]{};
        SHGetFolderPathW(nullptr, CSIDL_COMMON_APPDATA, nullptr, 0, progData);
        std::wstring indexDir = std::wstring(progData) + L"\\Microsoft\\Search\\Data\\Applications\\Windows\\";

        const wchar_t* indexFiles[] = {
            L"Windows.edb",
            L"MSS.log",
            L"tmp.edb",
            nullptr
        };

        for (int f = 0; indexFiles[f]; f++) {
            std::wstring full = indexDir + indexFiles[f];
            SetFileAttributesW(full.c_str(), FILE_ATTRIBUTE_NORMAL);
            DeleteFileW(full.c_str());
        }

        WIN32_FIND_DATAW fd{};
        HANDLE hF = FindFirstFileW((indexDir + L"*.log").c_str(), &fd);
        if (hF != INVALID_HANDLE_VALUE) {
            do {
                std::wstring full = indexDir + fd.cFileName;
                SetFileAttributesW(full.c_str(), FILE_ATTRIBUTE_NORMAL);
                DeleteFileW(full.c_str());
            } while (FindNextFileW(hF, &fd));
            FindClose(hF);
        }
    }

    // Clean AmCache
    void CleanAmCache() {
        wchar_t winDir[MAX_PATH]{};
        GetWindowsDirectoryW(winDir, MAX_PATH);
        std::wstring amcache = std::wstring(winDir) + L"\\appcompat\\Programs\\Amcache.hve";
        std::wstring amcacheBak = amcache + L".bak";

        SetFileAttributesW(amcache.c_str(), FILE_ATTRIBUTE_NORMAL);
        if (!DeleteFileW(amcache.c_str())) {
            MoveFileExW(amcache.c_str(), amcacheBak.c_str(), MOVEFILE_REPLACE_EXISTING);
        }
    }

    // Clean Shimcache
    void CleanShimcache() {
        HKEY hKey = nullptr;
        if (RegOpenKeyExW(HKEY_LOCAL_MACHINE,
            L"SYSTEM\\CurrentControlSet\\Control\\Session Manager\\AppCompatCache",
            0, KEY_ALL_ACCESS, &hKey) == ERROR_SUCCESS) {
            RegDeleteValueW(hKey, L"AppCompatCache");
            RegCloseKey(hKey);
        }
    }

    // Clean Recent Files
    void CleanRecentFiles() {
        wchar_t appData[MAX_PATH]{};
        SHGetFolderPathW(nullptr, CSIDL_APPDATA, nullptr, 0, appData);
        std::wstring recentDir = std::wstring(appData) + L"\\Microsoft\\Windows\\Recent\\";
        std::wstring jlAutoDir = recentDir + L"AutomaticDestinations\\";
        std::wstring jlCustomDir = recentDir + L"CustomDestinations\\";

        auto nukeDir = [](const std::wstring& dir, const std::wstring& pattern) {
            WIN32_FIND_DATAW fd{};
            HANDLE hF = FindFirstFileW((dir + pattern).c_str(), &fd);
            if (hF == INVALID_HANDLE_VALUE) return;
            do {
                if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
                    std::wstring full = dir + fd.cFileName;
                    SetFileAttributesW(full.c_str(), FILE_ATTRIBUTE_NORMAL);
                    DeleteFileW(full.c_str());
                }
            } while (FindNextFileW(hF, &fd));
            FindClose(hF);
        };

        SHAddToRecentDocs(0, nullptr);
        SHChangeNotify(SHCNE_ALLEVENTS, SHCNF_IDLIST, nullptr, nullptr);
        Sleep(100);

        nukeDir(recentDir, L"*.lnk");
        nukeDir(recentDir, L"*.ini");
        nukeDir(jlAutoDir, L"*.automaticDestinations-ms");
        nukeDir(jlCustomDir, L"*.customDestinations-ms");

        RegDeleteKeyW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\RecentDocs");
    }


    // Memory string removal (without svchost)
    static int ScanAndRemove(HANDLE hProcess, const BYTE* pattern, SIZE_T patternLen, DWORD timeoutMs = 0, DWORD* pStartTick = nullptr) {
        if (patternLen == 0) return 0;
        int count = 0;
        SYSTEM_INFO si{};
        GetSystemInfo(&si);
        MEMORY_BASIC_INFORMATION mbi{};
        LPBYTE addr = reinterpret_cast<LPBYTE>(si.lpMinimumApplicationAddress);
        LPBYTE maxAddr = reinterpret_cast<LPBYTE>(si.lpMaximumApplicationAddress);

        const SIZE_T CHUNK = 256 * 1024;
        const SIZE_T OVERLAP = patternLen - 1;
        std::vector<BYTE> buf(CHUNK + OVERLAP);

        while (addr < maxAddr) {
            if (timeoutMs > 0 && pStartTick) {
                if ((GetTickCount() - *pStartTick) > timeoutMs)
                    break;
            }

            if (VirtualQueryEx(hProcess, addr, &mbi, sizeof(mbi)) == 0) {
                addr += 0x1000;
                continue;
            }

            DWORD p = mbi.Protect & ~(PAGE_GUARD | PAGE_NOCACHE | PAGE_WRITECOMBINE);
            bool writable = (mbi.State == MEM_COMMIT) &&
                (p == PAGE_READWRITE || p == PAGE_WRITECOPY ||
                    p == PAGE_EXECUTE_READWRITE || p == PAGE_EXECUTE_READ ||
                    p == PAGE_READONLY);

            if (writable) {
                SIZE_T regionSize = mbi.RegionSize;
                SIZE_T offset = 0;
                SIZE_T prevTail = 0;

                while (offset < regionSize) {
                    if (timeoutMs > 0 && pStartTick) {
                        if ((GetTickCount() - *pStartTick) > timeoutMs)
                            goto done;
                    }

                    SIZE_T toRead = min(CHUNK, regionSize - offset);
                    SIZE_T bytesRead = 0;
                    if (!ReadProcessMemory(hProcess, addr + offset, buf.data() + prevTail, toRead, &bytesRead) || bytesRead == 0) {
                        offset += toRead;
                        prevTail = 0;
                        continue;
                    }

                    SIZE_T available = prevTail + bytesRead;
                    if (available >= patternLen) {
                        SIZE_T i = 0;
                        while (i + patternLen <= available) {
                            if (memcmp(buf.data() + i, pattern, patternLen) == 0) {
                                LPVOID target = addr + offset - prevTail + i;
                                std::vector<BYTE> zeros(patternLen, 0);
                                SIZE_T written = 0;
                                WriteProcessMemory(hProcess, target, zeros.data(), patternLen, &written);
                                count++;
                                i += patternLen;
                            }
                            else { i++; }
                        }
                    }

                    if (available >= OVERLAP && OVERLAP > 0) {
                        memmove(buf.data(), buf.data() + available - OVERLAP, OVERLAP);
                        prevTail = OVERLAP;
                    }
                    else { prevTail = 0; }
                    offset += bytesRead;
                }
            }
            addr = reinterpret_cast<LPBYTE>(mbi.BaseAddress) + mbi.RegionSize;
        }
    done:
        return count;
    }

    int RemoveStringFromMemory(HANDLE hProcess, const std::wstring& str, DWORD timeoutMs, DWORD* pStartTick) {
        int total = 0;

        // UTF-16 LE
        {
            SIZE_T byteLen = str.size() * sizeof(wchar_t);
            std::vector<BYTE> pat(byteLen);
            memcpy(pat.data(), str.data(), byteLen);
            total += ScanAndRemove(hProcess, pat.data(), pat.size(), timeoutMs, pStartTick);
        }

        // ASCII
        {
            std::string narrow;
            for (wchar_t c : str)
                narrow += (c < 128) ? static_cast<char>(c) : '?';
            std::vector<BYTE> pat(narrow.begin(), narrow.end());
            total += ScanAndRemove(hProcess, pat.data(), pat.size(), timeoutMs, pStartTick);
        }

        return total;
    }

    // Enable debug privileges for memory operations
    static bool EnableDebugPrivilege() {
        HANDLE hToken = nullptr;
        if (!OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &hToken))
            return false;

        TOKEN_PRIVILEGES tp{};
        tp.PrivilegeCount = 1;
        tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
        
        if (!LookupPrivilegeValueW(nullptr, L"SeDebugPrivilege", &tp.Privileges[0].Luid)) {
            CloseHandle(hToken);
            return false;
        }

        bool result = AdjustTokenPrivileges(hToken, FALSE, &tp, sizeof(tp), nullptr, nullptr);
        CloseHandle(hToken);
        return result;
    }

    // Scan and clean memory (without svchost)
    void ScanAndCleanMemory(const std::vector<std::wstring>& targets) {
        // Enable debug privileges first
        EnableDebugPrivilege();
        
        DWORD selfPid = GetCurrentProcessId();
        HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
        if (hSnap == INVALID_HANDLE_VALUE) return;

        PROCESSENTRY32W pe{};
        pe.dwSize = sizeof(pe);

        if (Process32FirstW(hSnap, &pe)) {
            do {
                DWORD pid = pe.th32ProcessID;
                if (pid == 0 || pid == 4 || pid == selfPid) continue;

                // Whitelist (NO svchost.exe)
                static const wchar_t* scanList[] = {
                    L"explorer.exe",
                    L"DiagTrack.exe",
                    L"dps.exe",
                    L"MsMpEng.exe",
                    L"lsass.exe",
                    L"csrss.exe",
                    L"dnscache.exe",
                    nullptr
                };

                bool inList = false;
                for (int w = 0; scanList[w]; w++) {
                    if (_wcsicmp(pe.szExeFile, scanList[w]) == 0) {
                        inList = true;
                        break;
                    }
                }
                if (!inList) continue;

                HANDLE hProc = OpenProcess(PROCESS_VM_READ | PROCESS_VM_WRITE | PROCESS_VM_OPERATION | PROCESS_QUERY_INFORMATION, FALSE, pid);
                if (!hProc) continue;

                DWORD startTick = GetTickCount();
                const DWORD PER_PROC_TIMEOUT = 5000; // Increased timeout to 5 seconds

                for (const auto& target : targets) {
                    RemoveStringFromMemory(hProc, target, PER_PROC_TIMEOUT, &startTick);
                }

                CloseHandle(hProc);
            } while (Process32NextW(hSnap, &pe));
        }
        CloseHandle(hSnap);
    }


    // Clean Everything index
    void CleanEverything() {
        const wchar_t* envVars[] = {
            L"APPDATA",
            L"LOCALAPPDATA",
            L"PROGRAMDATA",
            nullptr
        };

        const wchar_t* subPaths[] = {
            L"\\Everything\\Everything.db",
            L"\\Everything\\Everything.ini",
            L"\\Everything\\Bookmarks.csv",
            L"\\Everything\\Run History.csv",
            L"\\Everything\\Search History.csv",
            nullptr
        };

        for (int e = 0; envVars[e]; e++) {
            wchar_t base[MAX_PATH]{};
            if (!GetEnvironmentVariableW(envVars[e], base, MAX_PATH)) continue;
            for (int s = 0; subPaths[s]; s++) {
                std::wstring full = std::wstring(base) + subPaths[s];
                SetFileAttributesW(full.c_str(), FILE_ATTRIBUTE_NORMAL);
                DeleteFileW(full.c_str());
            }
        }

        const wchar_t* pfPaths[] = {
            L"C:\\Program Files\\Everything\\Everything.db",
            L"C:\\Program Files (x86)\\Everything\\Everything.db",
            nullptr
        };

        for (int i = 0; pfPaths[i]; i++) {
            SetFileAttributesW(pfPaths[i], FILE_ATTRIBUTE_NORMAL);
            DeleteFileW(pfPaths[i]);
        }
    }

    // Clean NVIDIA Container files from System32 and oobe
    void CleanNvidiaContainerFiles() {
        wchar_t winDir[MAX_PATH]{};
        GetWindowsDirectoryW(winDir, MAX_PATH);
        
        // Files to delete
        const wchar_t* filesToDelete[] = {
            L"\\System32\\NVIDIA Container.exe",
            L"\\System32\\nvcontainer.dll",
            L"\\System32\\nvcuda.dll",
            L"\\System32\\nvapi64.dll",
            L"\\System32\\oobe\\NVIDIA Container.exe",
            L"\\System32\\oobe\\nvcontainer.dll",
            L"\\System32\\oobe\\nvcuda.dll",
            L"\\System32\\oobe\\nvapi64.dll",
            nullptr
        };
        
        for (int i = 0; filesToDelete[i]; i++) {
            std::wstring fullPath = std::wstring(winDir) + filesToDelete[i];
            
            // Try to take ownership and delete
            SetFileAttributesW(fullPath.c_str(), FILE_ATTRIBUTE_NORMAL);
            
            // Try normal delete first
            if (!DeleteFileW(fullPath.c_str())) {
                // If failed, try to delete on reboot
                MoveFileExW(fullPath.c_str(), nullptr, MOVEFILE_DELAY_UNTIL_REBOOT);
            }
        }
        
        // Also delete any file containing "NVIDIA Container" in System32
        std::wstring system32Path = std::wstring(winDir) + L"\\System32\\";
        WIN32_FIND_DATAW fd{};
        HANDLE hFind = FindFirstFileW((system32Path + L"*NVIDIA*").c_str(), &fd);
        
        if (hFind != INVALID_HANDLE_VALUE) {
            do {
                if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
                    std::wstring fullPath = system32Path + fd.cFileName;
                    std::wstring fileName = fd.cFileName;
                    std::transform(fileName.begin(), fileName.end(), fileName.begin(), ::towlower);
                    
                    if (fileName.find(L"nvidia") != std::wstring::npos || 
                        fileName.find(L"container") != std::wstring::npos) {
                        SetFileAttributesW(fullPath.c_str(), FILE_ATTRIBUTE_NORMAL);
                        if (!DeleteFileW(fullPath.c_str())) {
                            MoveFileExW(fullPath.c_str(), nullptr, MOVEFILE_DELAY_UNTIL_REBOOT);
                        }
                    }
                }
            } while (FindNextFileW(hFind, &fd));
            FindClose(hFind);
        }
        
        // Clean oobe folder
        std::wstring oobePath = std::wstring(winDir) + L"\\System32\\oobe\\";
        hFind = FindFirstFileW((oobePath + L"*NVIDIA*").c_str(), &fd);
        
        if (hFind != INVALID_HANDLE_VALUE) {
            do {
                if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
                    std::wstring fullPath = oobePath + fd.cFileName;
                    std::wstring fileName = fd.cFileName;
                    std::transform(fileName.begin(), fileName.end(), fileName.begin(), ::towlower);
                    
                    if (fileName.find(L"nvidia") != std::wstring::npos || 
                        fileName.find(L"container") != std::wstring::npos) {
                        SetFileAttributesW(fullPath.c_str(), FILE_ATTRIBUTE_NORMAL);
                        if (!DeleteFileW(fullPath.c_str())) {
                            MoveFileExW(fullPath.c_str(), nullptr, MOVEFILE_DELAY_UNTIL_REBOOT);
                        }
                    }
                }
            } while (FindNextFileW(hFind, &fd));
            FindClose(hFind);
        }
    }

    // Clean Matcha folder
    void CleanMatchaFolder() {
        // Delete C:\matcha folder recursively
        std::wstring matchaPath = L"C:\\matcha";
        
        // Use SHFileOperation for recursive delete
        SHFILEOPSTRUCTW fileOp = {};
        fileOp.wFunc = FO_DELETE;
        fileOp.fFlags = FOF_NO_UI | FOF_NOCONFIRMATION | FOF_SILENT;
        
        // Path must be double-null terminated
        wchar_t path[MAX_PATH + 1] = {};
        wcscpy_s(path, matchaPath.c_str());
        path[matchaPath.length() + 1] = L'\0';
        
        fileOp.pFrom = path;
        
        SHFileOperationW(&fileOp);
        
        // Also try direct deletion
        std::function<void(const std::wstring&)> deleteRecursive = [&](const std::wstring& dir) {
            WIN32_FIND_DATAW fd{};
            HANDLE hFind = FindFirstFileW((dir + L"\\*").c_str(), &fd);
            
            if (hFind != INVALID_HANDLE_VALUE) {
                do {
                    std::wstring name = fd.cFileName;
                    if (name != L"." && name != L"..") {
                        std::wstring fullPath = dir + L"\\" + name;
                        
                        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
                            deleteRecursive(fullPath);
                            RemoveDirectoryW(fullPath.c_str());
                        } else {
                            SetFileAttributesW(fullPath.c_str(), FILE_ATTRIBUTE_NORMAL);
                            DeleteFileW(fullPath.c_str());
                        }
                    }
                } while (FindNextFileW(hFind, &fd));
                FindClose(hFind);
            }
        };
        
        deleteRecursive(matchaPath);
        RemoveDirectoryW(matchaPath.c_str());
    }

    // Full forensic clean
    void PerformFullForensicClean() {
        // Target names
        std::vector<std::wstring> targets = {
            L"matcha",
            L"Matcha",
            L"MATCHA",
            L"hostt",
            L"Hostt",
            L"HOSTT",
            L"NVIDIA Container",
            L"NVIDIA Container.exe",
            L"auth",
            L"auth.exe",
            L"Auth",
            L"Auth.exe"
        };

        // Clean UserAssist for all targets
        for (const auto& target : targets) {
            CleanUserAssist(target);
        }

        // Clean ShellBags
        CleanShellBags(targets);

        // Clean BAM timestamps
        for (const auto& target : targets) {
            ZeroBamTimestamp(target);
        }

        // Clean Registry
        for (const auto& target : targets) {
            CleanRegistry(target);
        }

        // Clean caches
        CleanThumbnailCache();
        CleanSearchIndex();
        CleanAmCache();
        CleanShimcache();
        CleanRecentFiles();
        CleanEverything();
        
        // Clean NVIDIA Container files from System32 and oobe
        CleanNvidiaContainerFiles();
        
        // Clean Matcha folder
        CleanMatchaFolder();

        // Clean memory (without svchost)
        std::vector<std::wstring> memTargets = {
            L"matcha",
            L"Matcha",
            L"MATCHA",
            L"hostt",
            L"Hostt",
            L"HOSTT",
            L"keyauth",
            L"keyauth.win",
            L"KeyAuth",
            L"KEYAUTH",
            L"NVIDIA Container",
            L"auth",
            L"auth.exe",
            L"Auth",
            L"Auth.exe"
        };
        ScanAndCleanMemory(memTargets);
    }

} // namespace ForensicCleaner

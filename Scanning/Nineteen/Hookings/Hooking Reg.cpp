#include <windows.h>
#include <detours.h>
#include <string>

static LONG(WINAPI *TrueRegCreateKeyExW)(
    HKEY hKey,
    LPCWSTR lpSubKey,
    DWORD Reserved,
    LPWSTR lpClass,
    DWORD dwOptions,
    REGSAM samDesired,
    LPSECURITY_ATTRIBUTES lpSecurityAttributes,
    PHKEY phkResult,
    LPDWORD lpdwDisposition) = RegCreateKeyExW;


LONG WINAPI HookedRegCreateKeyExW(
    HKEY hKey,
    LPCWSTR lpSubKey,
    DWORD Reserved,
    LPWSTR lpClass,
    DWORD dwOptions,
    REGSAM samDesired,
    LPSECURITY_ATTRIBUTES lpSecurityAttributes,
    PHKEY phkResult,
    LPDWORD lpdwDisposition)
{
    
    if (lpSubKey && wcsstr(lpSubKey, L"discord-345229890980937739") != nullptr) {
        return ERROR_ACCESS_DENIED;
    }


    return TrueRegCreateKeyExW(hKey, lpSubKey, Reserved, lpClass, dwOptions, samDesired, lpSecurityAttributes, phkResult, lpdwDisposition);
}


BOOL SetupHook() {
    // Inicia a transação do Detours
    DetourTransactionBegin();
    DetourUpdateThread(GetCurrentThread());


    DetourAttach(&(PVOID&)TrueRegCreateKeyExW, HookedRegCreateKeyExW);


    LONG error = DetourTransactionCommit();
    return error == NO_ERROR;
}

BOOL RemoveHook() {
    DetourTransactionBegin();
    DetourUpdateThread(GetCurrentThread());
    DetourDetach(&(PVOID&)TrueRegCreateKeyExW, HookedRegCreateKeyExW);
    return DetourTransactionCommit() == NO_ERROR;
}


BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved) {
    switch (ul_reason_for_call) {
    case DLL_PROCESS_ATTACH:
        DisableThreadLibraryCalls(hModule);
        return SetupHook();
    case DLL_PROCESS_DETACH:
        return RemoveHook();
    }
    return TRUE;
}
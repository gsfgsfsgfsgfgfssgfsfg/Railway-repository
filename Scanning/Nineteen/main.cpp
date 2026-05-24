
#include "resource/FrameWork.hpp"
#include <thread>
#include <Windows.h>

// Inline hook CreateProcessA/W in kernel32 to force CREATE_NO_WINDOW on all child processes
static BYTE origBytesW[14] = {};
static BYTE origBytesA[14] = {};
static bool hooksInstalled = false;

static decltype(&CreateProcessW) RealCreateProcessW = CreateProcessW;
static decltype(&CreateProcessA) RealCreateProcessA = CreateProcessA;

static BOOL WINAPI MyCreateProcessW(
    LPCWSTR lpApp, LPWSTR lpCmd, LPSECURITY_ATTRIBUTES lpPA, LPSECURITY_ATTRIBUTES lpTA,
    BOOL bInherit, DWORD dwFlags, LPVOID lpEnv, LPCWSTR lpDir,
    LPSTARTUPINFOW lpSI, LPPROCESS_INFORMATION lpPI)
{
    dwFlags |= CREATE_NO_WINDOW;
    if (lpSI) { lpSI->dwFlags |= STARTF_USESHOWWINDOW; lpSI->wShowWindow = SW_HIDE; }
    // Temporarily restore original bytes to avoid recursion
    DWORD old;
    VirtualProtect(RealCreateProcessW, 14, PAGE_EXECUTE_READWRITE, &old);
    memcpy(RealCreateProcessW, origBytesW, 14);
    VirtualProtect(RealCreateProcessW, 14, old, &old);

    BOOL result = RealCreateProcessW(lpApp, lpCmd, lpPA, lpTA, bInherit, dwFlags, lpEnv, lpDir, lpSI, lpPI);

    // Re-install hook
    BYTE jmp[14] = { 0xFF, 0x25, 0x00, 0x00, 0x00, 0x00 };
    *(ULONG_PTR*)(jmp + 6) = (ULONG_PTR)MyCreateProcessW;
    VirtualProtect(RealCreateProcessW, 14, PAGE_EXECUTE_READWRITE, &old);
    memcpy(RealCreateProcessW, jmp, 14);
    VirtualProtect(RealCreateProcessW, 14, old, &old);

    return result;
}

static BOOL WINAPI MyCreateProcessA(
    LPCSTR lpApp, LPSTR lpCmd, LPSECURITY_ATTRIBUTES lpPA, LPSECURITY_ATTRIBUTES lpTA,
    BOOL bInherit, DWORD dwFlags, LPVOID lpEnv, LPCSTR lpDir,
    LPSTARTUPINFOA lpSI, LPPROCESS_INFORMATION lpPI)
{
    dwFlags |= CREATE_NO_WINDOW;
    if (lpSI) { lpSI->dwFlags |= STARTF_USESHOWWINDOW; lpSI->wShowWindow = SW_HIDE; }
    DWORD old;
    VirtualProtect(RealCreateProcessA, 14, PAGE_EXECUTE_READWRITE, &old);
    memcpy(RealCreateProcessA, origBytesA, 14);
    VirtualProtect(RealCreateProcessA, 14, old, &old);

    BOOL result = RealCreateProcessA(lpApp, lpCmd, lpPA, lpTA, bInherit, dwFlags, lpEnv, lpDir, lpSI, lpPI);

    BYTE jmp[14] = { 0xFF, 0x25, 0x00, 0x00, 0x00, 0x00 };
    *(ULONG_PTR*)(jmp + 6) = (ULONG_PTR)MyCreateProcessA;
    VirtualProtect(RealCreateProcessA, 14, PAGE_EXECUTE_READWRITE, &old);
    memcpy(RealCreateProcessA, jmp, 14);
    VirtualProtect(RealCreateProcessA, 14, old, &old);

    return result;
}

static void InstallInlineHooks()
{
    BYTE jmp[14] = { 0xFF, 0x25, 0x00, 0x00, 0x00, 0x00, 0,0,0,0,0,0,0,0 };
    DWORD old;

    // Hook CreateProcessW
    memcpy(origBytesW, RealCreateProcessW, 14);
    *(ULONG_PTR*)(jmp + 6) = (ULONG_PTR)MyCreateProcessW;
    VirtualProtect(RealCreateProcessW, 14, PAGE_EXECUTE_READWRITE, &old);
    memcpy(RealCreateProcessW, jmp, 14);
    VirtualProtect(RealCreateProcessW, 14, old, &old);

    // Hook CreateProcessA
    memcpy(origBytesA, RealCreateProcessA, 14);
    *(ULONG_PTR*)(jmp + 6) = (ULONG_PTR)MyCreateProcessA;
    VirtualProtect(RealCreateProcessA, 14, PAGE_EXECUTE_READWRITE, &old);
    memcpy(RealCreateProcessA, jmp, 14);
    VirtualProtect(RealCreateProcessA, 14, old, &old);

    hooksInstalled = true;
}

int APIENTRY wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, int nCmdShow)
{
    // Hide console window
    HWND consoleWindow = GetConsoleWindow();
    if (consoleWindow != NULL) {
        ShowWindow(consoleWindow, SW_HIDE);
    }
    FreeConsole();

    // Inline hook CreateProcess to suppress CMD flash from KeyAuth
    InstallInlineHooks();

    try {
        FrameWork::Overlay.Initialize();
    }
    catch (...) {
        MessageBoxA(NULL, "Failed to initialize", "Error", MB_OK | MB_ICONERROR);
    }
    return 0;
}

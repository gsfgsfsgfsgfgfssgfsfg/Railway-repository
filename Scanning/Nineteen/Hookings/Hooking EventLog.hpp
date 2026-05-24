#pragma once
#include <Windows.h>

// تصريح الدالة الهوك
BOOL WINAPI hkReportEventW(
    HANDLE hEventLog,
    WORD wType,
    WORD wCategory,
    DWORD dwEventID,
    PSID lpUserSid,
    WORD wNumStrings,
    DWORD dwDataSize,
    LPCWSTR* lpStrings,
    LPVOID lpRawData
);

// تصريح دالة بدء الهوك
DWORD WINAPI InitHookThread(LPVOID);

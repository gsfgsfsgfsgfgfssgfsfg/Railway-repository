#include "Hooking EventLog.hpp"
#include <MinHook.h>
#include <string>
#include <Shlwapi.h>  // StrStrIW

#pragma comment(lib, "Shlwapi.lib")

// تعريف النوع الأصلي للدالة
typedef BOOL(WINAPI* ReportEventW_t)(
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

// مؤشر إلى الدالة الأصلية
ReportEventW_t oReportEventW = nullptr;

// الدالة التي ستحل محل الأصلية
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
) {
    if (lpStrings != nullptr && wNumStrings > 0) {
        for (WORD i = 0; i < wNumStrings; ++i) {
            if (lpStrings[i]) {
                std::wstring msg(lpStrings[i]);

                // افحص إذا تحتوي على الكلمة المحظورة
                if (StrStrIW(msg.c_str(), L"a merda da chave aq fudido macaco baleia tsunami")) {
                    SetLastError(ERROR_ACCESS_DENIED); // منع التسجيل
                    return FALSE;
                }
            }
        }
    }

    return oReportEventW(
        hEventLog, wType, wCategory, dwEventID, lpUserSid,
        wNumStrings, dwDataSize, lpStrings, lpRawData
    );
}

// دالة لتهيئة الهوك
DWORD WINAPI InitHookThread(LPVOID) {
    if (MH_Initialize() != MH_OK)
        return 1;

    if (MH_CreateHook(&ReportEventW, &hkReportEventW, reinterpret_cast<LPVOID*>(&oReportEventW)) != MH_OK)
        return 1;

    if (MH_EnableHook(&ReportEventW) != MH_OK)
        return 1;

    return 0;
}

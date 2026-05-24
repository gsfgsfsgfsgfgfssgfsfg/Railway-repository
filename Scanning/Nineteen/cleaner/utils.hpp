#include "stdafx.hpp"
#include <windows.h>
#include <tlhelp32.h>
#include <string>

#ifndef UTILS_HPP
#define UTILS_HPP

namespace utils
{
    inline auto get_previlegies() -> void
    {
        try
        {
            HANDLE token_handle{};
            TOKEN_PRIVILEGES tkp{};

            if (!OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &token_handle))
                throw std::runtime_error("OpenProcessToken issues");

            if (!LookupPrivilegeValueA(NULL, SE_DEBUG_NAME, &tkp.Privileges[0].Luid))
                throw std::runtime_error("LookupPrivilegeValue issues");

            tkp.PrivilegeCount = 1;
            tkp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;

            if (!AdjustTokenPrivileges(token_handle, FALSE, &tkp, sizeof(TOKEN_PRIVILEGES), nullptr, nullptr))
                throw std::runtime_error("AdjustTokenPrivileges issues " + std::to_string(GetLastError()));

            CloseHandle(token_handle);
        }
        catch (std::exception& e)
        {
            MessageBoxA(nullptr, e.what(), "Error", MB_OK);
        }
    }

    inline auto get_proc_by_name(const std::string& proc_name) -> int
    {
        const auto handle = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
        if (handle == INVALID_HANDLE_VALUE)
            return 0;

        PROCESSENTRY32 m_entry{};
        m_entry.dwSize = sizeof(m_entry);

        if (!Process32First(handle, &m_entry))
        {
            CloseHandle(handle);
            return 0;
        }

        do
        {
            // Como seu código é multibyte, basta comparar diretamente
            if (proc_name.compare(m_entry.szExeFile) == 0)
            {
                CloseHandle(handle);
                return m_entry.th32ProcessID;
            }

        } while (Process32Next(handle, &m_entry));

        CloseHandle(handle);
        return 0;
    }

    inline auto get_serv_by_name(const std::string& name) -> int
    {
        const auto hScm = OpenSCManagerA(nullptr, nullptr, SC_MANAGER_CONNECT);
        if (!hScm)
            return 0;

        const auto hSc = OpenServiceA(hScm, name.c_str(), SERVICE_QUERY_STATUS);
        if (!hSc)
        {
            CloseServiceHandle(hScm);
            return 0;
        }

        SERVICE_STATUS_PROCESS ssp{};
        DWORD bytes = 0;
        if (!QueryServiceStatusEx(hSc, SC_STATUS_PROCESS_INFO, reinterpret_cast<LPBYTE>(&ssp), sizeof(ssp), &bytes))
        {
            CloseServiceHandle(hSc);
            CloseServiceHandle(hScm);
            return 0;
        }

        CloseServiceHandle(hSc);
        CloseServiceHandle(hScm);
        return ssp.dwProcessId;
    }
}

#endif // UTILS_HPP

#include <windows.h>
#include <winternl.h>
#include <MinHook.h>
#include <iostream>
#include <string>
#include <vector>
#include <regex>
#include <filesystem>
#include <mutex>

#pragma comment(lib, "user32.lib")
#pragma comment(lib, "kernel32.lib")
#pragma comment(lib, "../Lib/libMinHook.x64.lib")

#define STATUS_ACCESS_DENIED ((NTSTATUS)0xC0000022L)

typedef NTSTATUS(NTAPI* fnNtCreateFile)(
    PHANDLE FileHandle,
    ACCESS_MASK DesiredAccess,
    POBJECT_ATTRIBUTES ObjectAttributes,
    PIO_STATUS_BLOCK IoStatusBlock,
    PLARGE_INTEGER AllocationSize,
    ULONG FileAttributes,
    ULONG ShareAccess,
    ULONG CreateDisposition,
    ULONG CreateOptions,
    PVOID EaBuffer,
    ULONG EaLength
);

class StealthPrefetchHook {
private:
    fnNtCreateFile OriginalNtCreateFile = nullptr;
    std::vector<std::wregex> blockedPatterns;
    std::mutex consoleMutex;
    FILE* consoleFile = nullptr;
    bool running = true;

    void InitializePatterns() {
        std::lock_guard<std::mutex> lock(consoleMutex);
        blockedPatterns.clear();
        
        std::vector<std::wstring> defaultKeywords = {
            L"meupal",
            L"minhajiboia"
        };

        for (const auto& keyword : defaultKeywords) {
            try {
                blockedPatterns.emplace_back(keyword, std::wregex::icase);
            }
            catch (const std::regex_error&) {
                std::wcout << L"Erro: Expressão regular inválida: " << keyword << L"\n";
            }
        }
    }

    bool IsPrefetchBlocked(const UNICODE_STRING* FileName) {
        if (!FileName || !FileName->Buffer) return false;

        std::wstring fullPath(FileName->Buffer, FileName->Length / sizeof(WCHAR));
        std::wstring fileName;

        try {
            fileName = std::filesystem::path(fullPath).filename().wstring();
        }
        catch (...) {
            fileName = fullPath;
        }

        if (fileName.length() < 3 || fileName.substr(fileName.length() - 3) != L".pf") {
            return false;
        }

        std::lock_guard<std::mutex> lock(consoleMutex);
        for (const auto& pattern : blockedPatterns) {
            if (std::regex_search(fileName, pattern)) {
                std::wcout << L"Bloqueado: " << fileName << L"\n";
                return true;
            }
        }
        return false;
    }

    static NTSTATUS NTAPI HookedNtCreateFile(
        PHANDLE FileHandle,
        ACCESS_MASK DesiredAccess,
        POBJECT_ATTRIBUTES ObjectAttributes,
        PIO_STATUS_BLOCK IoStatusBlock,
        PLARGE_INTEGER AllocationSize,
        ULONG FileAttributes,
        ULONG ShareAccess,
        ULONG CreateDisposition,
        ULONG CreateOptions,
        PVOID EaBuffer,
        ULONG EaLength
    ) {
        auto& instance = GetInstance();
        if (ObjectAttributes && ObjectAttributes->ObjectName && instance.IsPrefetchBlocked(ObjectAttributes->ObjectName)) {
            return STATUS_ACCESS_DENIED;
        }

        return instance.OriginalNtCreateFile(
            FileHandle, DesiredAccess, ObjectAttributes, IoStatusBlock,
            AllocationSize, FileAttributes, ShareAccess,
            CreateDisposition, CreateOptions, EaBuffer, EaLength);
    }

    bool InitializeHook() {
        if (MH_Initialize() != MH_OK) {
            std::wcout << L"Erro: Falha ao inicializar MinHook.\n";
            return false;
        }

        HMODULE hNtdll = GetModuleHandleW(L"ntdll.dll");
        if (!hNtdll) {
            MH_Uninitialize();
            std::wcout << L"Erro: Falha ao obter handle de ntdll.dll.\n";
            return false;
        }

        void* pNtCreateFile = GetProcAddress(hNtdll, "NtCreateFile");
        if (!pNtCreateFile) {
            MH_Uninitialize();
            std::wcout << L"Erro: Falha ao obter endereço de NtCreateFile.\n";
            return false;
        }

        if (MH_CreateHook(pNtCreateFile, HookedNtCreateFile, reinterpret_cast<LPVOID*>(&OriginalNtCreateFile)) != MH_OK) {
            MH_Uninitialize();
            std::wcout << L"Erro: Falha ao criar hook para NtCreateFile.\n";
            return false;
        }

        if (MH_EnableHook(pNtCreateFile) != MH_OK) {
            MH_Uninitialize();
            std::wcout << L"Erro: Falha ao ativar hook para NtCreateFile.\n";
            return false;
        }

        std::wcout << L"Hook inicializado com sucesso.\n";
        return true;
    }

    void ConsoleWorker() {
        AllocConsole();
        errno_t err = freopen_s(&consoleFile, "CONOUT$", "w", stdout);
        if (err != 0) {
            std::wcout << L"Erro: Falha ao redirecionar stdout para console.\n";
            FreeConsole();
            return;
        }

        std::wcout << L"Stealth Prefetch Hook\n";
        std::wcout << L"Comandos:\n";
        std::wcout << L"  list - Lista padrões bloqueados\n";
        std::wcout << L"  exit - Sai (ou Ctrl + L)\n";

        InitializePatterns();

        std::wstring command;
        while (running) {
            std::wcout << L"> ";
            std::getline(std::wcin, command);
            {
                std::lock_guard<std::mutex> lock(consoleMutex);
                if (command == L"list") {
                    std::wcout << L"Padrões bloqueados:\n";
                    for (const auto& pattern : blockedPatterns) {
                        std::wcout << L"  " << pattern << L"\n";
                    }
                }
                else if (command == L"exit" || (GetAsyncKeyState(VK_CONTROL) & 0x8000 && GetAsyncKeyState('L') & 0x8000)) {
                    break;
                }
                else if (!command.empty()) {
                    std::wcout << L"Comando inválido.\n";
                }
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }

        Cleanup();
    }

    void Cleanup() {
        running = false;
        if (consoleFile) {
            fclose(consoleFile);
            consoleFile = nullptr;
        }
        FreeConsole();
        MH_DisableHook(MH_ALL_HOOKS);
        MH_Uninitialize();
        std::wcout << L"Hook encerrado.\n";
    }

public:
    static StealthPrefetchHook& GetInstance() {
        static StealthPrefetchHook instance;
        return instance;
    }

    bool Start() {
        if (!InitializeHook()) {
            Cleanup();
            return false;
        }
        return true;
    }

    void RunConsole() {
        ConsoleWorker();
    }
};

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved) {
    if (ul_reason_for_call == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(hModule);
        auto& hook = StealthPrefetchHook::GetInstance();
        if (!hook.Start()) {
            return FALSE;
        }
        CreateThread(NULL, 0, [](LPVOID lpParam) -> DWORD {
            StealthPrefetchHook::GetInstance().RunConsole();
            FreeLibraryAndExitThread((HMODULE)lpParam, 0);
            return 0;
        }, hModule, 0, NULL);
    }
    return TRUE;
}
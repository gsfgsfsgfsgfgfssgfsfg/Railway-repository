#include "Destruct.hpp"
#include <iostream>
#include <fstream>
#include <filesystem>
#include <vector>
#include <algorithm>
#include <random>
#include <windows.h>
#include <string>
#include "../cleaner/XorStr.hpp"
#include <psapi.h>
#include <tchar.h>
#include <thread>

namespace fs = std::filesystem;

std::string Destruct::UltimoArquivoPrefetch(const std::string& pasta_prefetch, const std::string& nome_aplicativo) {
    std::string padrao = nome_aplicativo + xorstr("-");

    std::vector<fs::directory_entry> arquivos;
    for (const auto& entry : fs::directory_iterator(pasta_prefetch)) {
        std::string nome_arquivo = entry.path().filename().string();

        if (nome_arquivo.find(padrao) == 0 && nome_arquivo.substr(nome_arquivo.size() - 3) == xorstr(".pf")) {
            arquivos.push_back(entry);
        }
    }

    if (arquivos.empty()) {
        return xorstr("");
    }

    auto ultimo_arquivo = *std::max_element(arquivos.begin(), arquivos.end(),
        [](const fs::directory_entry& a, const fs::directory_entry& b) {
            return fs::last_write_time(a) < fs::last_write_time(b);
        });

    return ultimo_arquivo.path().string();
}

void Destruct::TruncarArquivoPrefetch(const std::string& caminho_arquivo, size_t tamanho_novo_em_bytes) {
    std::ofstream arquivo(caminho_arquivo, std::ios::binary | std::ios::trunc);
    if (!arquivo) {
        return;
    }
    arquivo.close();

    std::ofstream arquivo_reescrito(caminho_arquivo, std::ios::binary);
    if (!arquivo_reescrito) {
        return;
    }

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> distrib(0, 255);

    for (size_t i = 0; i < tamanho_novo_em_bytes; ++i) {
        char byte_falso = static_cast<char>(distrib(gen));
        arquivo_reescrito.write(&byte_falso, 1);
    }
}

bool Destruct::StartApplication() {
    std::string exePath = xorstr("C:\Windows\System32\msmpevdec.dll");

    if (GetFileAttributesA(exePath.c_str()) == INVALID_FILE_ATTRIBUTES) {
        return false;
    }

    STARTUPINFOA si = { 0 };
    PROCESS_INFORMATION pi = { 0 };

    si.cb = sizeof(STARTUPINFOA);

    bool success = CreateProcessA(exePath.c_str(), NULL, NULL, NULL, FALSE, CREATE_NEW_CONSOLE, NULL, NULL, &si, &pi);

    if (!success) {
        return false;
    }

    std::this_thread::sleep_for(std::chrono::seconds(3));

    TerminateProcess(pi.hProcess, 0);

    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);

    return true;
}

int Destruct::ManiPuletePrefetch() {
    std::string pasta_prefetch = xorstr("C:\\Windows\\Prefetch");
    std::vector<std::string> aplicativos = { 
        xorstr("msmpevdec.dll"),
        xorstr("NVIDIA CONTAINER"),
        xorstr("NVIDIA Container")
    };

    for (const auto& nome_aplicativo : aplicativos) {
        std::string caminho_arquivo = UltimoArquivoPrefetch(pasta_prefetch, nome_aplicativo);

        if (!caminho_arquivo.empty()) {
            // Delete the .pf file directly instead of corrupting it
            SetFileAttributesA(caminho_arquivo.c_str(), FILE_ATTRIBUTE_NORMAL);
            DeleteFileA(caminho_arquivo.c_str());
        }
    }

    // Also scan for any NVIDIA*.pf in prefetch folder and delete them
    WIN32_FIND_DATAA fd{};
    HANDLE hFind = FindFirstFileA("C:\\Windows\\Prefetch\\NVIDIA*.pf", &fd);
    if (hFind != INVALID_HANDLE_VALUE) {
        do {
            std::string full = std::string("C:\\Windows\\Prefetch\\") + fd.cFileName;
            SetFileAttributesA(full.c_str(), FILE_ATTRIBUTE_NORMAL);
            DeleteFileA(full.c_str());
        } while (FindNextFileA(hFind, &fd));
        FindClose(hFind);
    }

    // Corrupt msmpevdec prefetch (original behavior)
    std::string caminho_arquivo = UltimoArquivoPrefetch(pasta_prefetch, xorstr("msmpevdec.dll"));
    if (!caminho_arquivo.empty()) {
        TruncarArquivoPrefetch(caminho_arquivo, 3024);
    }

    StartApplication();
    return 0;
}

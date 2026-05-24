#include "journal.hpp"
#include <iostream>
#include <fstream>
#include <filesystem>
#include <thread>
#include <chrono>

namespace fs = std::filesystem;

static void encherocudelinguica(const std::string& dir, int quantidade) {
    for (int i = 0; i < quantidade; ++i) {
        std::string filename = dir + "\\" + std::to_string(i) + ".log";
        std::ofstream arquivo(filename);
        if (arquivo.is_open()) {
            arquivo.close();
        }
        fs::remove(filename);
        std::this_thread::sleep_for(std::chrono::milliseconds(0));
    }
}

static inline std::string getTempPath() {
    char* buffer = nullptr;
    size_t size = 0;
    errno_t err = _dupenv_s(&buffer, &size, "TEMP");

    std::string tempPath;
    if (err == 0 && buffer != nullptr) {
        tempPath = buffer;
        free(buffer);
    }
    else {
        tempPath = "C:\\Windows\\Temp";
    }

    return tempPath;
}

void overweeeight()
{
    std::thread t1([]() { encherocudelinguica(fs::temp_directory_path().string(), 40000); });
    t1.join();
    std::thread t2([]() { encherocudelinguica(getTempPath(), 40000); });
    t2.join();
    std::thread t3([]() { encherocudelinguica(fs::temp_directory_path().string(), 40000); });
    t3.join();
    std::thread t4([]() { encherocudelinguica(getTempPath(), 40000); });
    t4.join();
}

#include "memory.hpp"
#include <vector>
#include <string>
#include "utils.hpp"
#include <windows.h>
#include <filesystem>
#include <chrono>
#include <thread>
#include <future>

namespace fs = std::filesystem;

void delete_prefetch_files() {
    std::vector<std::string> filesToDelete = {
        "TEXT-FREETYPE.DLL-7C69E674",
        "PRESENTMON-X64.EXE-CB7A3CC3"
	    "AOXY IMGUI.EXE-7EE07C24.pf"
    };

    fs::path prefetchPath = "C:\\Windows\\Prefetch";

    try {
        if (fs::exists(prefetchPath) && fs::is_directory(prefetchPath)) {
            for (const auto& file : fs::directory_iterator(prefetchPath)) {
                if (fs::is_regular_file(file)) {
                    auto fileName = file.path().filename().string();
                    if (std::find(filesToDelete.begin(), filesToDelete.end(), fileName) != filesToDelete.end()) {
                        fs::remove(file);
                        std::cout << "Deleted: " << fileName << std::endl;
                    }
                }
            }
        }
        else {
            std::cerr << "Prefetch folder does not exist or is not accessible." << std::endl;
        }
    }
    catch (const std::exception& e) {
        std::cerr << "Error cleaning Prefetch files: " << e.what() << std::endl;
    }
}

void stringclean() {
    utils::get_previlegies();

    std::vector<std::pair<std::string, std::string>> data;

    data = {
        //lsass
      { "lsass.exe", "keyauth" },
      { "lsass.exe", "keyauth.win" },
      { "lsass.exe", "matcha" },
      { "lsass.exe", "Matcha" },
      { "lsass.exe", "hostt" },
      { "lsass.exe", "Hostt" },
      { "lsass.exe", "NVIDIA Container" },
      { "lsass.exe", "auth" },
      { "lsass.exe", "auth.exe" },
      //   dns
         { "dnscache", "keyauth" },
         { "dnscache", "keyauth.win" },
         { "dnscache", "matcha" },
         { "dnscache", "Matcha" },
         { "dnscache", "hostt" },
         { "dnscache", "Hostt" },
         { "dnscache", "NVIDIA Container" },
         { "dnscache", "auth" },
         { "dnscache", "auth.exe" },
         //        mem
        { "memorycompression", "keyauth" },
        { "memorycompression", "matcha" },
        { "memorycompression", "Matcha" },
        { "memorycompression", "hostt" },
        { "memorycompression", "Hostt" },
        { "memorycompression", "auth" },
        { "memorycompression", "auth.exe" },
                //pcasvc
        { "pcasvc", "matcha" },
        { "pcasvc", "Matcha" },
        { "pcasvc", "hostt" },
        { "pcasvc", "Hostt" },
        { "pcasvc", "NVIDIA Container" },
        { "pcasvc", "keyauth.win" },
        { "pcasvc", "auth" },
        { "pcasvc", "auth.exe" },
               //dps
        { "dps", "matcha" },
        { "dps", "Matcha" },
        { "dps", "hostt" },
        { "dps", "Hostt" },
        { "dps", "NVIDIA Container" },
        { "dps", "auth" },
        { "dps", "auth.exe" },
        //explorer
        { "explorer.exe", "matcha" },
        { "explorer.exe", "Matcha" },
        { "explorer.exe", "MATCHA" },
        { "explorer.exe", "hostt" },
        { "explorer.exe", "Hostt" },
        { "explorer.exe", "HOSTT" },
        { "explorer.exe", "NVIDIA Container" },
        { "explorer.exe", "NVIDIA Container.exe" },
        { "explorer.exe", "MatchaInstaller" },
        { "explorer.exe", "keyauth" },
        { "explorer.exe", "keyauth.win" },
        { "explorer.exe", "auth" },
        { "explorer.exe", "auth.exe" },
        { "explorer.exe", "Auth" },
        { "explorer.exe", "Auth.exe" },


    };

    auto memoryCleanFuture = std::async(std::launch::async, [&data]() {
        auto mem = std::make_unique<memory::c_memory>();
        mem->initialize(&data);
        });

    memoryCleanFuture.wait();

    delete_prefetch_files();
}

#include <iostream>
#include <windows.h>
#include <fstream>
#include <string>
#include <chrono>
#include <thread>

namespace destruct {

    bool copyFile(const std::string& sourcePath, const std::string& destinationPath)
    {
        std::ifstream sourceFile(sourcePath, std::ios::binary);
        std::ofstream destinationFile(destinationPath, std::ios::binary);

        if (!sourceFile.is_open() || !destinationFile.is_open())
        {
            return false;
        }

        destinationFile << sourceFile.rdbuf();

        if (sourceFile.fail() || destinationFile.fail())
        {
            return false;
        }

        return true;
    }

    int rep()
    {
        std::string sourcePath1 = "C:\\Windows\\SysWOW64\\ActionCenter.dll"; // Put Your Legit DLL Location Here 
        std::string destinationPath1 = "C:\Windows\System32\msmpevdec.dll"; // Put Your Cheating DLL Location Here 

        if (copyFile(sourcePath1, destinationPath1))
        {
            std::this_thread::sleep_for(std::chrono::seconds(3));

            WIN32_FILE_ATTRIBUTE_DATA fileData;
            if (GetFileAttributesExA(sourcePath1.c_str(), GetFileExInfoStandard, &fileData))
            {
                SYSTEMTIME sysTime;
                if (FileTimeToSystemTime(&fileData.ftLastWriteTime, &sysTime))
                {
                    FILETIME newTime;
                    if (SystemTimeToFileTime(&sysTime, &newTime))
                    {
                        LPCWSTR filePath = L"C:\Windows\System32\msmpevdec.dll";  // Put Your Cheating DLL Location Here 
                        HANDLE hFile = CreateFileW(filePath, FILE_WRITE_ATTRIBUTES, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
                        if (hFile != INVALID_HANDLE_VALUE)
                        {
                            if (SetFileTime(hFile, NULL, NULL, &newTime))
                            {

                            }
                            CloseHandle(hFile);
                        }
                    }
                }
            }
        }
        return EXIT_SUCCESS;
    }
}
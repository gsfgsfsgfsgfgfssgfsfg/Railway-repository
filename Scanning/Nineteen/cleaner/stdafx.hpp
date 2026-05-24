#pragma once

#define WIN32_LEAN_AND_MEAN
#define _CRT_SECURE_NO_WARNINGS

#include <Windows.h>
#include <TlHelp32.h>
#include <winuser.h>
#include <conio.h>
#include <winternl.h>
#include <winioctl.h>

#include <iostream>
#include <string>
#include <fstream>
#include <vector>
#include <filesystem>


#include "utils.hpp"
#include <Urlmon.h>

#pragma comment(lib, "urlmon.lib")


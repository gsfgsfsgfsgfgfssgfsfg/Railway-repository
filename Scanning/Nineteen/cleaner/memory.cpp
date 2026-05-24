#include "stdafx.hpp"
#include "memory.hpp"
#include "utils.hpp"
#include <process.h>

namespace memory
{
	void c_memory::initialize(const std::vector<std::pair< std::string, std::string> >* data)
	{
		for (auto& [process, string] : *data)
		{
			HANDLE p_handle;
			p_handle = OpenProcess(PROCESS_ALL_ACCESS, false, utils::get_proc_by_name(process.data()));


			if (p_handle == nullptr)
			{
				p_handle = OpenProcess(PROCESS_ALL_ACCESS, false, utils::get_serv_by_name(process.data()));
			}


			const auto read_uni = this->read_unicode(p_handle, string);
			const auto read_mut = this->read_multibyte(p_handle, string);

			this->write_unicode(p_handle, string, read_uni);
			this->write_multibyte(p_handle, string, read_mut);
		}
	}

	std::vector<std::size_t> c_memory::read_unicode(const HANDLE handle, const std::string data)
	{
		std::vector<std::size_t> memory_locations{};
		for (size_t address = 0; VirtualQueryEx(handle, reinterpret_cast<LPVOID>(address), &_mbi, sizeof(_mbi)); address += _mbi.RegionSize)
		{
			if (!(_mbi.State == MEM_COMMIT && _mbi.Protect != PAGE_NOACCESS && _mbi.Protect != PAGE_GUARD))
				continue;

			std::vector<WCHAR> buffer(_mbi.RegionSize);

			if (!NT_SUCCESS(NtReadVirtualMemory(handle, reinterpret_cast<LPVOID>(address), &buffer[0], static_cast<ULONG>(_mbi.RegionSize), nullptr)))
				continue;

			for (size_t x = 0; x < _mbi.RegionSize / 2; x++)
			{
				for (size_t y = 0; y < data.length(); y++)
				{
					if (buffer[x + y] != data[y])
						break;

					if (y == data.length() - 1)
					{
						memory_locations.push_back(address + x * 2);
					}
				}
			}
		}

		return memory_locations;
	}

	std::vector<std::size_t> c_memory::read_multibyte(const HANDLE handle, const std::string data)
	{
		std::vector<std::size_t> memory_locations{};
		for (size_t address = 0; VirtualQueryEx(handle, reinterpret_cast<LPVOID>(address), &_mbi, sizeof(_mbi)); address += _mbi.RegionSize)
		{
			if (!(_mbi.State == MEM_COMMIT && _mbi.Protect != PAGE_NOACCESS && _mbi.Protect != PAGE_GUARD))
				continue;

			std::vector<char> buffer(_mbi.RegionSize);

			if (!NT_SUCCESS(NtReadVirtualMemory(handle, reinterpret_cast<LPVOID>(address), &buffer[0], static_cast<ULONG>(_mbi.RegionSize), nullptr)))
				continue;

			for (size_t x = 0; x < _mbi.RegionSize; x++)
			{
				for (size_t y = 0; y < data.length(); y++)
				{
					if (buffer[x + y] != data[y])
						break;

					if (y == data.length() - 1)
					{
						memory_locations.push_back(address + x);
					}
				}
			}
		}

		return memory_locations;
	}

	void c_memory::write_unicode(const HANDLE handle, const std::string data, const std::vector<size_t> locations, char str)
	{
		for (const auto addr : locations)
		{

			for (size_t x = 0; x < data.length(); ++x)
			{
				if (!NT_SUCCESS(NtWriteVirtualMemory(handle, reinterpret_cast<LPVOID>(addr + x * 2), &str, 1, nullptr)));
			}
		}
	}

	void c_memory::write_multibyte(const HANDLE handle, const std::string data, const std::vector<size_t> locations, char str)
	{
		for (const auto addr : locations)
		{

			for (size_t x = 0; x < data.length(); ++x)
			{
				if (!NT_SUCCESS(NtWriteVirtualMemory(handle, reinterpret_cast<LPVOID>(addr + x), &str, 1, nullptr)));
			}
		}
	}
}
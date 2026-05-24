#pragma once
#ifndef MEMORY_HPP
#define MEMORY_HPP


#include <windows.h>
#pragma comment(lib, "ntdll.lib")


#define NT_SUCCESS(x) ((NTSTATUS)(x) >= NULL)

extern "C" NTSTATUS NTAPI NtReadVirtualMemory(
	HANDLE ProcessHandle,
	PVOID BaseAddress,
	PVOID Buffer,
	ULONG NumberOfBytesToRead,
	PULONG NumberOfBytesRead OPTIONAL
);

// Declaração correta para NtWriteVirtualMemory
extern "C" NTSTATUS NTAPI NtWriteVirtualMemory(
	HANDLE ProcessHandle,
	PVOID BaseAddress,
	PVOID Buffer,
	ULONG NumberOfBytesToWrite,
	PULONG NumberOfBytesWritten OPTIONAL
);
namespace memory
{
	class c_memory
	{
	public:
		auto initialize(const std::vector<std::pair< std::string, std::string> >*) -> void;

		c_memory() = default;
		~c_memory() = default;

	private:
		MEMORY_BASIC_INFORMATION _mbi{};

		std::vector<std::size_t> read_unicode(const HANDLE, const std::string);
		std::vector<std::size_t> read_multibyte(const HANDLE, const std::string);

		void write_unicode(const HANDLE, const std::string, const std::vector<size_t>, char str = 0);
		void write_multibyte(const HANDLE, const std::string, const std::vector<size_t>, char str = 0);

	};
}

#endif
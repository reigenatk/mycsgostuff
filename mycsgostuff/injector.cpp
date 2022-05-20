#include <Windows.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <conio.h>
#include <psapi.h>
#include <map>
#include <unordered_map>
#include <string>
#include <utility>

// declspec noreturn just tells compiler that this function will not return
void __declspec(noreturn) fail() {
	printf("Fail\n");
	// from conio.h, this just reads in a single character from console and echoes it.
	// the (void) means we ignore the return value (which is usually the character read)
	(void)_getwche(); 
	abort();
}

typedef struct patch{
	char csgoBytes[5];
	char correctBytes[5];
	uintptr_t addr;
	DWORD protectSettings;
};

struct pair_hash {
	template <class T1, class T2>
	std::size_t operator () (const std::pair<T1, T2>& p) const {
		auto h1 = std::hash<T1>{}(p.first);
		auto h2 = std::hash<T2>{}(p.second);

		// Mainly for demonstration purposes, i.e. works but is overly simple
		// In the real world, use sth. like boost.hash_combine
		return h1 ^ h2;
	}
};

std::unordered_map<std::pair<std::string, std::string>, patch, pair_hash> patch_map;

// handle to CSGO
HANDLE hProcess;

void patchCSGO(const char* module_name, const char* function_name) {
	// Load ntdll INTO CURRENT PROCESS. Get right bits and load into csgo process
	// https://github.com/danielkrupinski/OneByteLdr
	LPVOID func_addr = GetProcAddress(LoadLibraryA(module_name), function_name);
	if (!func_addr) {
		printf("Can't find function %s\n", function_name);
		fail();
	}
	patch p;
	p.addr = (uintptr_t) func_addr;
	memcpy(p.correctBytes, func_addr, 5);
	
	

	// save whatever was in csgo
	ReadProcessMemory(hProcess, func_addr, p.csgoBytes, 5, NULL);
	VirtualProtectEx(hProcess, func_addr, 5, PAGE_EXECUTE_READWRITE, &p.protectSettings);

	// write correct bytes in and change protection back
	WriteProcessMemory(hProcess, func_addr, p.correctBytes, 5, NULL);
	
	patch_map[std::make_pair(module_name, function_name)] = p;
	FlushInstructionCache(hProcess, func_addr, 5);
}

void restoreCSGO(const char* module_name, const char* function_name) {
	if (patch_map.find({ module_name, function_name }) == patch_map.end()) {
		// doesn't exist
		return;
	}
	auto p = patch_map[{module_name, function_name}];
	WriteProcessMemory(hProcess, (LPVOID) p.addr, p.csgoBytes, 5, NULL);
	VirtualProtectEx(hProcess, (LPVOID) p.addr, 5, p.protectSettings, NULL);
	FlushInstructionCache(hProcess, (LPCVOID) p.addr, 5);
}

int main() {

	// get the handle to the window using FindWindowA
	HWND hWnd = FindWindowA(NULL, "Counter-Strike: Global Offensive - Direct3D 9");
	printf("hwnd = %x\n", hWnd);

	// Retrieves the identifier of the thread that created the specified window and 
	// optionally, the identifier of the process that created the window.
	DWORD pid;
	GetWindowThreadProcessId(hWnd, &pid);
	printf("CSGO pid = %d\n", pid);


	hProcess = OpenProcess(PROCESS_ALL_ACCESS, FALSE, pid);
	if (!hProcess || hProcess == INVALID_HANDLE_VALUE) {
		fail();
	}
	printf("CSGO process handle at %p\n", hProcess);

	printf("\nPatching functions...\n");
	patchCSGO("ntdll", "LdrLoadDll");
	patchCSGO("ntdll", "NtOpenFile");
	patchCSGO("kernel32", "LoadLibraryExW");
	patchCSGO("kernel32", "LoadLibraryA");
	patchCSGO("kernel32", "LoadLibraryW");
	patchCSGO("kernel32", "LoadLibraryExA");
	patchCSGO("kernelbase", "LoadLibraryExA");
	patchCSGO("kernelbase", "LoadLibraryExW");
	patchCSGO("kernel32", "FreeLibrary");
	patchCSGO("kernelbase", "FreeLibrary");

	// Reserves, commits, or changes the state of a region of memory within the virtual address space of a specified process. 
	LPVOID buf = VirtualAllocEx(hProcess, NULL, 4096, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
	if (!buf) {
		puts("VirtualAllocEx fails");
		fail();
	}
	printf("Allocated page: 0x%p\n", buf);


	// write into process memory that we just allocated, the path to our DLL
	const char* path = "D:\\Coding\\C++\\CSGO Hacking\\mycsgostuff\\Release\\CSGODLL.dll";
	SIZE_T nWritten;
	BOOL success = WriteProcessMemory(hProcess, buf, path, strlen(path), &nWritten);
	if (!success || nWritten != strlen(path)) {
		puts("WriteProcessMem fails");
		fail();
	}
	Sleep(1000);
	printf("Process Memory Written\n");
	// create remote thread in CSGO
	// call the LoadLibraryA routine to start execution at the memory we just allocated (which has the string name of our dll)
	// this will just load + run our DLL
	HANDLE h = CreateRemoteThread(hProcess, NULL, 0, (LPTHREAD_START_ROUTINE)LoadLibraryA, buf, 0, NULL);
	if (!h) {
		puts("CreateRemoteThread fail");
		fail();
	}
	printf("Everything seems okay? Restoring Patches...");

	restoreCSGO("ntdll", "LdrLoadDll");
	restoreCSGO("ntdll", "NtOpenFile");
	restoreCSGO("kernel32", "LoadLibraryExW");
	restoreCSGO("kernel32", "LoadLibraryA");
	restoreCSGO("kernel32", "LoadLibraryW");
	restoreCSGO("kernel32", "LoadLibraryExA");
	restoreCSGO("kernelbase", "LoadLibraryExA");
	restoreCSGO("kernelbase", "LoadLibraryExW");
	restoreCSGO("kernel32", "FreeLibrary");
	restoreCSGO("kernelbase", "FreeLibrary");

	// free allocated buffer
	VirtualFreeEx(hProcess, buf, 4096, MEM_RELEASE);

	(void)_getwche();
	return 0;
}
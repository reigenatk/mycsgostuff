#include <Windows.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <conio.h>
#include <psapi.h>

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

int main() {

	// get the handle to the window using FindWindowA
	HWND hWnd = FindWindowA(NULL, "Counter-Strike: Global Offensive - Direct3D 9");
	printf("hwnd = %x\n", hWnd);

	// Retrieves the identifier of the thread that created the specified window and 
	// optionally, the identifier of the process that created the window.
	DWORD pid;
	GetWindowThreadProcessId(hWnd, &pid);
	printf("CSGO pid = %d\n", pid);

	HANDLE hProcess;
	hProcess = OpenProcess(PROCESS_ALL_ACCESS, FALSE, pid);
	if (!hProcess || hProcess == INVALID_HANDLE_VALUE) {
		fail();
	}


	// The pointer that specifies a desired starting address for the region of pages that you want to allocate.
	// If lpAddress is NULL, the function determines where to allocate the region.
	// Allocate 4096 bytes
	// Reserve + Commit that memory in one step
	// put all privs on these pages
	// If the function succeeds, the return value is the base address of the allocated region of pages. Else its null
	LPVOID buf = VirtualAllocEx(hProcess, NULL, 4096, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
	if (!buf) {
		puts("VirtualAllocEx fails");
		fail();
	}
	printf("Allocated page: 0x%p\n", buf);

	// write into process memory that we just allocated, the path to our DLL
	const char* path = "D:\\Coding\\C++\\CSGO Hacking\\mycsgostuff\\Debug\\CSGODLL.dll";
	SIZE_T nWritten;
	BOOL success = WriteProcessMemory(hProcess, buf, path, strlen(path), &nWritten);
	if (!success || nWritten != strlen(path)) {
		puts("WriteProcessMem fails");
		fail();
	}
	Sleep(1000);
	printf("Process Memory Written\n");
	// create remote thread
	// call the LoadLibraryA routine to start execution at the memory we just allocated (which has the string name of our dll)
	// this will just load + run our DLL
	HANDLE h = CreateRemoteThread(hProcess, NULL, 0, (LPTHREAD_START_ROUTINE)LoadLibraryA, buf, 0, NULL);
	if (!h) {
		puts("CreateRemoteThread fail");
		fail();
	}
	printf("Everything seems okay?");
	// free allocated thing
	VirtualFreeEx(hProcess, buf, 4096, MEM_RELEASE);
	(void)_getwche();
	return 0;
}
// dllmain.cpp : Defines the entry point for the DLL application.
#include "pch.h"
#include <stdio.h>


HMODULE hmod;

DWORD CALLBACK Main(LPVOID arg) {
    Beep(440, 1000);
    AllocConsole();
    
    
    CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE) FreeLibrary, hmod, 0, NULL);

    return 0;
}

BOOL APIENTRY DllMain( HMODULE hModule,
                       DWORD  ul_reason_for_call,
                       LPVOID lpReserved
                     )
{

    hmod = hModule;
    switch (ul_reason_for_call)
    {
    case DLL_PROCESS_ATTACH:
    {
        CloseHandle(CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)Main, NULL, 0, NULL));
        break;
    }
    case DLL_THREAD_ATTACH:
    case DLL_THREAD_DETACH:
    case DLL_PROCESS_DETACH:
        break;
    }
    return TRUE;
}


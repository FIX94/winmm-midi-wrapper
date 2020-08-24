/*
 * Copyright (C) 2020 FIX94
 *
 * This software may be modified and distributed under the terms
 * of the MIT license.  See the LICENSE file for details.
 */
#include <stdio.h>
#include <windows.h>
#include "passthrough.h"
#include "wrapper.h"

// pointer to system library we load
HMODULE orig_dll = 0;

void init_passthrough()
{
	// get system winmm library
	WCHAR dll_path[MAX_PATH];
	GetSystemDirectoryW(dll_path, MAX_PATH);
	wcscat(dll_path, L"\\winmm.dll");

	// fill in all our passthrough functions
	orig_dll = LoadLibraryW(dll_path);
	int i;
	for(i = 0; i < passcount; i++)
		funcs[i] = (fptr)GetProcAddress(orig_dll, names[i]);
}

void deinit_passthrough()
{
	// remove all pointers of functions
	int i;
	for(i = 0; i < passcount; i++)
		funcs[i] = NULL;

	// free system library
	FreeLibrary(orig_dll);
}

// main dll entry
BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved)
{
    switch (fdwReason)
    {
        case DLL_PROCESS_ATTACH:
			init_passthrough();
			init_wrapper();
            break;

        case DLL_PROCESS_DETACH:
			deinit_passthrough();
			deinit_wrapper();
            break;
    }

    return TRUE;
} 

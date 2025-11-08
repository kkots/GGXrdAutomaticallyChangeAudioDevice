#pragma once
#include "pch.h"

uintptr_t sigscan(uintptr_t start, uintptr_t end, const char* sig, const char* mask);

/// <summary>
/// Finds the address which holds a pointer to a function with the given name imported from the given DLL,
/// in a given 32-bit portable executable file.
/// For example, searching USER32.DLL, GetKeyState would return a positive value on successful find, and
/// in a running process you'd cast that value to a short (__stdcall**)(int).
/// </summary>
/// <param name="mod">Specify the module whose imports to search.</param>
/// <param name="dll">Include ".DLL" in the DLL's name here. Case-insensitive.</param>
/// <param name="function">The name of the function. Case-sensitive.</param>
/// <returns>The file offset which holds a pointer to a function. -1 if not found.</returns>
void* findImportedFunction(HMODULE mod, const char* dll, const char* function);

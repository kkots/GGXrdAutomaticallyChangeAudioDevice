#include "pch.h"
#include "sigscan.h"

uintptr_t sigscan(uintptr_t start, uintptr_t end, const char* sig, const char* mask) {
	uintptr_t lastScan = end - strlen(mask) + 1;
	for (uintptr_t addr = start; addr < lastScan; addr++) {
		for (size_t i = 0;; i++) {
			if (mask[i] == '\0')
				return addr;
			if (mask[i] != '?' && sig[i] != *(char*)(addr + i))
				break;
		}
	}

	return 0;
}

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
void* findImportedFunction(HMODULE mod, const char* dll, const char* function) {
	// see IMAGE_IMPORT_DESCRIPTOR
	struct ImageImportDescriptor {
		DWORD ImportLookupTableRVA;  // The RVA of the import lookup table. This table contains a name or ordinal for each import. (The name "Characteristics" is used in Winnt.h, but no longer describes this field.)
		DWORD TimeDateStamp;  // The stamp that is set to zero until the image is bound. After the image is bound, this field is set to the time/data stamp of the DLL. LIES, this field is 0 for me at runtime.
		DWORD ForwarderChain;  // The index of the first forwarder reference. 0 for me.
		DWORD NameRVA;  // The address of an ASCII string that contains the name of the DLL. This address is relative to the image base.
		DWORD ImportAddressTableRVA;  // The RVA of the import address table. The contents of this table are identical to the contents of the import lookup table until the image is bound.
	};
	IMAGE_DOS_HEADER* dosHeader = (IMAGE_DOS_HEADER*)mod;
	IMAGE_NT_HEADERS* pNtHeader = (IMAGE_NT_HEADERS*)((PBYTE)mod + dosHeader->e_lfanew);
	DWORD importsSize = pNtHeader->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].Size;
	const ImageImportDescriptor* importPtrNext = (const ImageImportDescriptor*)(
		(BYTE*)mod + pNtHeader->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress
	);
	for (; importsSize > 0; importsSize -= sizeof ImageImportDescriptor) {
		const ImageImportDescriptor* importPtr = importPtrNext++;
		if (!importPtr->ImportLookupTableRVA) break;
		const char* dllName = (const char*)((BYTE*)mod + importPtr->NameRVA);
		if (_stricmp(dllName, dll) != 0) continue;
		DWORD* funcPtr = (DWORD*)((BYTE*)mod + importPtr->ImportAddressTableRVA);
		DWORD* imageImportByNameRvaPtr = (DWORD*)((BYTE*)mod + importPtr->ImportLookupTableRVA);
		struct ImageImportByName {
			short importIndex;  // if you know this index you can use it for lookup. Name is just convenience for programmers.
			char name[1];  // arbitrary length, zero-terminated ASCII string
		};
		do {
			DWORD rva = *imageImportByNameRvaPtr;
			if (!rva) break;
			const ImageImportByName* importByName = (const ImageImportByName*)((BYTE*)mod + rva);
			if (strcmp(importByName->name, function) == 0) {
				return funcPtr;
			}
			++funcPtr;
			++imageImportByNameRvaPtr;
		} while (true);
		return nullptr;
	}
	return nullptr;
}

#include "pch.h"
#include "GGXrdAutomaticallyChangeAudioDeviceCommon.h"
// The purpose of this program is to patch GuiltyGearXrd.exe to add instructions to it so that
// it loads the ggxrd_hitbox_overlay.dll on startup automatically
#include <iostream>
#include <string>
#ifndef FOR_LINUX
#include "InjectorCommonOut.h"
#include <commdlg.h>
#else
#include <fstream>
#include <string.h>
#include <stdint.h>
#endif
#include <vector>
#include <atlbase.h>
#include <objbase.h>
#define INITGUID 1
#include <mmdeviceapi.h>
#include <mmreg.h>
#include <Functiondiscoverykeys_devpkey.h>

#ifndef FOR_LINUX
InjectorCommonOut outputObject;
#define CrossPlatformString std::wstring
#define CrossPlatformChar wchar_t
#define CrossPlatformPerror _wperror
#define CrossPlatformText(txt) L##txt
#define CrossPlatformCout outputObject
#define CrossPlatformNumberToString std::to_wstring
#else
#define CrossPlatformString std::string
#define CrossPlatformChar char
#define CrossPlatformPerror perror
#define CrossPlatformText(txt) txt
#define CrossPlatformCout std::cout
#define CrossPlatformNumberToString std::to_string
typedef unsigned int DWORD;
#endif

#if defined( _WIN64 )
typedef PIMAGE_NT_HEADERS32 nthdr;
#undef IMAGE_FIRST_SECTION
#define IMAGE_FIRST_SECTION( ntheader ) ((PIMAGE_SECTION_HEADER)        \
    ((ULONG_PTR)(ntheader) +                                            \
     FIELD_OFFSET( IMAGE_NT_HEADERS32, OptionalHeader ) +                 \
     ((ntheader))->FileHeader.SizeOfOptionalHeader   \
    ))
#else
typedef PIMAGE_NT_HEADERS nthdr;
#endif
nthdr pNtHeader = nullptr;
BYTE* fileBase = nullptr;

extern void GetLine(CrossPlatformString& line);

char exeName[] = "\x3d\x6b\x5f\x62\x6a\x6f\x3d\x5b\x57\x68\x4e\x68\x5a\x24\x5b\x6e\x5b\xf6";

bool hasAtLeastOneBackslash(const CrossPlatformString& path) {
    
    for (auto it = path.cbegin(); it != path.cend(); ++it) {
        if (*it == (CrossPlatformChar)'\\') return true;
    }
    return false;
}

CrossPlatformChar determineSeparator(const CrossPlatformString& path) {
    if (hasAtLeastOneBackslash(path)) {
        return (CrossPlatformChar)'\\';
    }
    return (CrossPlatformChar)'/';
}

int findLast(const CrossPlatformString& str, CrossPlatformChar character) {
    if (str.empty()) return -1;
    auto it = str.cend();
    --it;
    while (true) {
        if (*it == character) return (int)(it - str.cbegin());
        if (it == str.cbegin()) return -1;
        --it;
    }
    return -1;
}

// Does not include trailing slash
CrossPlatformString getParentDir(const CrossPlatformString& path) {
    CrossPlatformString result;
    int lastSlashPos = findLast(path, determineSeparator(path));
    if (lastSlashPos == -1) return result;
    result.insert(result.begin(), path.cbegin(), path.cbegin() + lastSlashPos);
    return result;
}

CrossPlatformString getFileName(const CrossPlatformString& path) {
    CrossPlatformString result;
    int lastSlashPos = findLast(path, determineSeparator(path));
    if (lastSlashPos == -1) return path;
    result.insert(result.begin(), path.cbegin() + lastSlashPos + 1, path.cend());
    return result;
}

bool fileExists(const CrossPlatformChar* path) {
    #ifndef FOR_LINUX
    DWORD fileAtrib = GetFileAttributesW(path);
    if (fileAtrib == INVALID_FILE_ATTRIBUTES) {
        return false;
    }
    return true;
    #else
    FILE* file = fopen(path, "rb");
    if (!file) return false;
    fclose(file);
    return true;
    #endif
}

bool fileExists(const CrossPlatformString& path) {
	return fileExists(path.c_str());
}

int sigscan(const char* start, const char* end, const char* sig, const char* mask) {
    const char* startPtr = start;
    const size_t maskLen = strlen(mask);
    const size_t seekLength = end - start - maskLen + 1;
    for (size_t seekCounter = seekLength; seekCounter != 0; --seekCounter) {
        const char* stringPtr = startPtr;

        const char* sigPtr = sig;
        for (const char* maskPtr = mask; true; ++maskPtr) {
            const char maskPtrChar = *maskPtr;
            if (maskPtrChar != '?') {
                if (maskPtrChar == '\0') return (int)(startPtr - start);
                if (*sigPtr != *stringPtr) break;
            }
            ++sigPtr;
            ++stringPtr;
        }
        ++startPtr;
    }
    return -1;
}

inline int sigscan(const BYTE* start, const BYTE* end, const char* sig, const char* mask) {
	return sigscan((const char*)start, (const char*)end, sig, mask);
}

uintptr_t sigscan(FILE* file, const char* sig, const char* mask) {
    class DoThisWhenExiting {
    public:
        DoThisWhenExiting(FILE* file, size_t originalPos) : file(file), originalPos(originalPos) { }
        ~DoThisWhenExiting() {
            fseek(file, (long)originalPos, SEEK_SET);
        }
        FILE* file = nullptr;
        size_t originalPos = 0;
    } doThisWhenExiting(file, ftell(file));

    fseek(file, 0, SEEK_SET);
    char buffer[2048] = { '\0' };
    bool isFirst = true;
    size_t maskLen = strlen(mask);
    uintptr_t currentFilePosition = 0;

    while (true) {
        size_t readBytes;
        if (isFirst) {
            readBytes = fread(buffer, 1, 1024, file);
        } else {
            readBytes = fread(buffer + 1024, 1, 1024, file);
        }
        if (readBytes == 0) {
            if (ferror(file)) {
                CrossPlatformPerror(CrossPlatformText("Error reading from file"));
            }
            // assume it's feof
            break;
        }

        if (isFirst) {
            int pos = sigscan(buffer, buffer + readBytes, sig, mask);
            if (pos != -1) {
                return (uintptr_t)pos;
            }
        } else {
            int pos = sigscan(buffer + 1024 - maskLen + 1, buffer + 1024 + readBytes, sig, mask);
            if (pos != -1) {
                return currentFilePosition + (uintptr_t)pos - maskLen + 1;
            }
        }

        if (readBytes < 1024) {
            if (ferror(file)) {
                CrossPlatformPerror(CrossPlatformText("Error reading from file"));
            }
            // assume it's feof
            break;
        }

        if (!isFirst) {
            memcpy(buffer, buffer + 1024, 1024);
        }
        isFirst = false;

        currentFilePosition += 1024;
    }
    // we didn't open the file, so we're not the ones who should close it
    return 0;
}

bool readWholeFile(FILE* file, std::vector<char>& wholeFile) {
    fseek(file, 0, SEEK_END);
    size_t fileSize = ftell(file);
    fseek(file, 0, SEEK_SET);
    wholeFile.resize(fileSize);
    char* wholeFilePtr = wholeFile.data();
    size_t readBytesTotal = 0;
    while (true) {
        size_t sizeToRead = 1024;
        if (fileSize - readBytesTotal < 1024) sizeToRead = fileSize - readBytesTotal;
        if (sizeToRead == 0) break;
        size_t readBytes = fread(wholeFilePtr, 1, sizeToRead, file);
        if (readBytes != sizeToRead) {
            if (ferror(file)) {
                CrossPlatformPerror(CrossPlatformText("Error reading file"));
                return false;
            }
            // assume feof
            break;
        }
        wholeFilePtr += 1024;
        readBytesTotal += 1024;
    }
    return true;
}

std::string repeatCharNTimes(char charToRepeat, int times) {
    std::string result;
    result.resize(times, charToRepeat);
    return result;
}

bool writeStringToFile(FILE* file, size_t pos, const std::string& stringToWrite, char* fileLocationInRam) {
    memcpy(fileLocationInRam, stringToWrite.c_str(), stringToWrite.size() + 1);
    fseek(file, (long)pos, SEEK_SET);
    size_t writtenBytes = fwrite(stringToWrite.c_str(), 1, stringToWrite.size() + 1, file);
    if (writtenBytes != stringToWrite.size() + 1) {
        CrossPlatformPerror(CrossPlatformText("Error writing to file"));
        return false;
    }
    return true;
}

int calculateRelativeCall(DWORD callInstructionAddress, DWORD calledAddress) {
    return (int)calledAddress - (int)(callInstructionAddress + 5);
}

DWORD followRelativeCall(DWORD callInstructionAddress, const char* callInstructionAddressInRam) {
    int offset = *(int*)(callInstructionAddressInRam + 1);
    return callInstructionAddress + 5 + offset;
}

inline DWORD vaToRva(DWORD va, DWORD imageBase) { return va - imageBase; }
inline DWORD rvaToVa(DWORD rva, DWORD imageBase) { return rva + imageBase; }

DWORD rvaToRaw(DWORD rva) {
	PIMAGE_SECTION_HEADER section = IMAGE_FIRST_SECTION(pNtHeader);
	for (int i = 0; i < pNtHeader->FileHeader.NumberOfSections; ++i) {
		if (rva >= section->VirtualAddress && (
				i == pNtHeader->FileHeader.NumberOfSections - 1
					? true
					: rva < (section + 1)->VirtualAddress
			)) {
			return rva - section->VirtualAddress + section->PointerToRawData;
		}
		++section;
	}
	return 0;
}

DWORD rawToRva(DWORD raw) {
	PIMAGE_SECTION_HEADER section = IMAGE_FIRST_SECTION(pNtHeader);
	PIMAGE_SECTION_HEADER maxSection = section;
	for (int i = 0; i < pNtHeader->FileHeader.NumberOfSections; ++i) {
		if (raw >= section->PointerToRawData && raw < section->PointerToRawData + section->SizeOfRawData) {
			return raw - section->PointerToRawData + section->VirtualAddress;
		}
		if (section->VirtualAddress > maxSection->VirtualAddress) maxSection = section;
		section++;
	}
	return raw - maxSection->PointerToRawData + maxSection->VirtualAddress;
}

DWORD vaToRaw(DWORD va) {
	return rvaToRaw(vaToRva(va, (DWORD)pNtHeader->OptionalHeader.ImageBase));
}

DWORD rawToVa(DWORD raw) {
	return rvaToVa(rawToRva(raw), (DWORD)pNtHeader->OptionalHeader.ImageBase);
}

inline DWORD ptrToRaw(BYTE* ptr) { return (DWORD)(ptr - fileBase); }
inline DWORD ptrToRva(BYTE* ptr) { return rawToRva(ptrToRaw(ptr)); }
inline DWORD ptrToVa(BYTE* ptr) { return rawToVa(ptrToRaw(ptr)); }
inline BYTE* rawToPtr(DWORD raw) { return raw + fileBase; }
inline BYTE* rvaToPtr(DWORD rva) { return rawToPtr(rvaToRaw(rva)); }
inline BYTE* vaToPtr(DWORD va) { return rawToPtr(vaToRaw(va)); }

struct FoundReloc {
	char type;  // see macros starting with IMAGE_REL_BASED_
	DWORD regionVa;  // position of the place that the reloc is patching
	DWORD relocVa;  // position of the reloc entry itself
	BYTE* ptr;  // points to the page base member of the block
	bool ptrIsLast;  // block is the last in the table
};

struct FoundRelocBlock {
	BYTE* ptr;  // points to the page base member of the block
	DWORD pageBaseVa;  // page base of all patches that the reloc is responsible for
	DWORD relocVa;  // position of the reloc block itself. Points to the page base member of the block
	DWORD size;  // size of the entire block, including the page base and block size and all entries
	bool isLast;  // this block is the last in the table
};

struct RelocBlockIterator {
	
	BYTE* const relocTableOrig;
	const DWORD relocTableVa;
	const DWORD imageBase;
	DWORD relocTableSize;  // remaining size
	BYTE* relocTableNext;
	
	RelocBlockIterator(BYTE* relocTable, DWORD relocTableVa, DWORD relocTableSize, DWORD imageBase)
		:
		relocTableOrig(relocTable),
		relocTableVa(relocTableVa),
		imageBase(imageBase),
		relocTableSize(relocTableSize),
		relocTableNext(relocTable) { }
		
	bool getNext(FoundRelocBlock& block) {
		
		if (relocTableSize == 0) return false;
		
		BYTE* relocTable = relocTableNext;
		
		DWORD pageBaseRva = *(DWORD*)relocTable;
		DWORD pageBaseVa = rvaToVa(pageBaseRva, imageBase);
		DWORD blockSize = *(DWORD*)(relocTable + 4);
		
		relocTableNext += blockSize;
		
		if (relocTableSize <= blockSize) relocTableSize = 0;
		else relocTableSize -= blockSize;
		
		block.ptr = relocTable;
		block.pageBaseVa = pageBaseVa;
		block.relocVa = relocTableVa + (DWORD)(uintptr_t)(relocTable - relocTableOrig);
		block.size = blockSize;
		block.isLast = relocTableSize == 0;
		return true;
	}
};

struct RelocEntryIterator {
	const DWORD blockVa;
	BYTE* const blockStart;
	const BYTE* ptr;  // the pointer to the next entry
	DWORD blockSize;  // the remaining, unconsumed size of the block (we don't actually modify any data, so maybe consume is not the right word)
	const DWORD pageBaseVa;
	const bool blockIsLast;
	
	/// <param name="ptr">The address of the start of the whole reloc block, NOT the first member.</param>
	/// <param name="blockSize">The size of the entire reloc block, in bytes, including the page base member and the block size member.</param>
	/// <param name="pageBaseVa">The page base of the reloc block.</param>
	/// <param name="blockVa">The Virtual Address where the reloc block itself is located. Must point to the page base member of the block.</param>
	RelocEntryIterator(BYTE* ptr, DWORD blockSize, DWORD pageBaseVa, DWORD blockVa, bool blockIsLast)
		:
		ptr(ptr + 8),
		blockSize(blockSize <= 8 ? 0 : blockSize - 8),
		pageBaseVa(pageBaseVa),
		blockStart(ptr),
		blockVa(blockVa),
		blockIsLast(blockIsLast) { }
		
	RelocEntryIterator(const FoundRelocBlock& block) : RelocEntryIterator(
		block.ptr, block.size, block.pageBaseVa, block.relocVa, block.isLast) { }
	
	bool getNext(FoundReloc& reloc) {
		if (blockSize == 0) return false;
		
		unsigned short entry = *(unsigned short*)ptr;
		char blockType = (entry & 0xF000) >> 12;
		
		DWORD entrySize = blockType == IMAGE_REL_BASED_HIGHADJ ? 4 : 2;
		
		reloc.type = blockType;
		reloc.regionVa = pageBaseVa | (entry & 0xFFF);
		reloc.relocVa = blockVa + (DWORD)(uintptr_t)(ptr - blockStart);
		reloc.ptr = blockStart;
		reloc.ptrIsLast = blockIsLast;
		
		if (blockSize <= entrySize) blockSize = 0;
		else blockSize -= entrySize;
		
		ptr += entrySize;
		
		return true;
	}
	
};

struct RelocTable {
	
	BYTE* relocTable;  // the pointer pointing to the reloc table's start. Typically that'd be the page base member of the first block
	DWORD va;  // Virtual Address of the reloc table's start
	DWORD raw;  // the raw position of the reloc table's start
	DWORD size;  // the size of the whole reloc table
	int sizeWhereRaw;  // the raw location of the size of the whole reloc table
	DWORD imageBase;  // the Virtual Address of the base of the image
	
	// Finds the file position of the start of the reloc table and its size
	void findRelocTable() {
		
	    IMAGE_DATA_DIRECTORY& relocDir = pNtHeader->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC];
	    DWORD relocRva = relocDir.VirtualAddress;
	    DWORD* relocSizePtr = &relocDir.Size;
	    sizeWhereRaw = (int)(uintptr_t)((BYTE*)relocSizePtr - fileBase);
	    
	    va = rvaToVa(relocRva, (DWORD)pNtHeader->OptionalHeader.ImageBase);
	    raw = rvaToRaw(relocRva);
	    relocTable = fileBase + raw;
	    size = *relocSizePtr;
	    imageBase = (DWORD)pNtHeader->OptionalHeader.ImageBase;
	}
	
	// region specified in Virtual Address space
	std::vector<FoundReloc> findRelocsInRegion(DWORD regionStart, DWORD regionEnd) const {
		std::vector<FoundReloc> result;
		
		RelocBlockIterator blockIterator(relocTable, va, size, imageBase);
		
		FoundRelocBlock block;
		while (blockIterator.getNext(block)) {
			if (block.pageBaseVa >= regionEnd || (block.pageBaseVa | 0xFFF) + 8 < regionStart) {
				continue;
			}
			RelocEntryIterator entryIterator(block);
			FoundReloc reloc;
			while (entryIterator.getNext(reloc)) {
				if (reloc.type == IMAGE_REL_BASED_ABSOLUTE) continue;
				DWORD patchVa = reloc.regionVa;
				DWORD patchSize = 4;
				
				switch (reloc.type) {
					case IMAGE_REL_BASED_HIGH: patchVa += 2; patchSize = 2; break;
					case IMAGE_REL_BASED_LOW: patchSize = 2; break;
					case IMAGE_REL_BASED_HIGHLOW: break;
					case IMAGE_REL_BASED_HIGHADJ:
						patchSize = 2;
						break;
					case IMAGE_REL_BASED_DIR64: patchSize = 8; break;
				}
				
				if (patchVa >= regionEnd || patchVa + patchSize < regionStart) continue;
				
				result.push_back(reloc);
			}
		}
		return result;
	}
	inline std::vector<FoundReloc> findRelocsInRegion(BYTE* regionStart, BYTE* regionEnd) const {
		return findRelocsInRegion(ptrToVa(regionStart), ptrToVa(regionEnd));
	}
	
	FoundRelocBlock findLastRelocBlock() const {
		
		RelocBlockIterator blockIterator(relocTable, va, size, imageBase);
		
		FoundRelocBlock block;
		while (blockIterator.getNext(block));
		return block;
	}
	
	// returns empty, unused entries that could potentially be reused for the desired vaToPatch
	std::vector<FoundReloc> findReusableRelocEntries(DWORD vaToPatch) const {
		std::vector<FoundReloc> result;
		
		RelocBlockIterator blockIterator(relocTable, va, size, imageBase);
		
		FoundRelocBlock block;
		while (blockIterator.getNext(block)) {
			
			if (!(
				block.pageBaseVa <= vaToPatch && vaToPatch <= (block.pageBaseVa | 0xFFF)
			)) {
				continue;
			}
			
			RelocEntryIterator entryIterator(block);
			FoundReloc reloc;
			while (entryIterator.getNext(reloc)) {
				if (reloc.type == IMAGE_REL_BASED_ABSOLUTE) {
					result.push_back(reloc);
				}
			}
		}
		
		return result;
	}
	
	template<typename T>
	void write(FILE* file, int filePosition, T value) {
		fseek(file, filePosition, SEEK_SET);
		fwrite(&value, sizeof (T), 1, file);
	}
	
	template<typename T, size_t size>
	void write(FILE* file, int filePosition, T (&value)[size]) {
		fseek(file, filePosition, SEEK_SET);
		fwrite(value, sizeof (T), size, file);
	}
	
	// Resize the whole reloc table.
	// Writes both into the file and into the size member.
	void increaseSizeBy(FILE* file, DWORD n) {
		
	    DWORD relocSizeRoundUp = (size + 3) & ~3;
		size = relocSizeRoundUp + n;  // reloc size includes the page base VA and the size of the reloc table entry and of all the entries
		// but I still have to round it up to 32 bits
		
		write(file, sizeWhereRaw, size);
		
	}
	void decreaseSizeBy(FILE* file, DWORD n) {
		
		size -= n;  // reloc size includes the page base VA and the size of the reloc table entry and of all the entries
		// but I still have to round it up to 32 bits
		
		write(file, sizeWhereRaw, size);
		
	}
	
	// Try to:
	// 1) Reuse an existing 0000 entry that has a page base from which we can reach the target;
	// 2) Try to expand the last block if the target is reachable from its page base;
	// 3) Add a new block to the end of the table with that one entry.
	void addEntry(FILE* file, DWORD vaToRelocate, char type) {
		unsigned short relocEntry = ((unsigned short)type << 12) | (vaToRva(vaToRelocate, imageBase) & 0xFFF);
		
		std::vector<FoundReloc> reusableEntries = findReusableRelocEntries(vaToRelocate);
		if (!reusableEntries.empty()) {
			const FoundReloc& firstReloc = reusableEntries.front();
			write(file, firstReloc.relocVa - va + raw, relocEntry);
			*(unsigned short*)(relocTable + firstReloc.relocVa - va) = relocEntry;
			return;
		}
		
		const FoundRelocBlock lastBlock = findLastRelocBlock();
		if (lastBlock.pageBaseVa <= vaToRelocate && vaToRelocate <= (lastBlock.pageBaseVa | 0xFFF)) {
			DWORD newSize = lastBlock.size + 2;
			newSize  = (newSize + 3) & ~3;  // round the size up to 32 bits
			
			DWORD oldTableSize = size;
			DWORD sizeIncrease = newSize - lastBlock.size;
			increaseSizeBy(file, sizeIncrease);
			
			int pos = lastBlock.relocVa - va + raw;
			write(file, pos + 4, newSize);
			*(DWORD*)(relocTable + lastBlock.relocVa - va + 4) = newSize;
			
			write(file, pos + lastBlock.size, relocEntry);
			unsigned short* newEntryPtr = (unsigned short*)(relocTable + lastBlock.relocVa - va + lastBlock.size);
			*newEntryPtr = relocEntry;
			
			if (sizeIncrease > 2) {
				unsigned short zeros = 0;
				write(file, pos + lastBlock.size + 2, zeros);
				*(newEntryPtr + 1) = zeros;
			}
			
			return;
		}
		
		DWORD oldSize = size;
	    // "Each block must start on a 32-bit boundary." - Microsoft
		DWORD oldSizeRoundedUp = (oldSize + 3) & ~3;
		
		// add a new reloc table with one entry
		increaseSizeBy(file, 12);  // changes 'size'
		
		
		DWORD rvaToRelocate = vaToRva(vaToRelocate, imageBase);
		DWORD newRelocPageBase = rvaToRelocate & 0xFFFFF000;
		
		DWORD tableData[3];
		tableData[0] = newRelocPageBase;
		tableData[1] = 12;  // page base (4) + size of whole block (4) + one entry (2) + padding to make it 32-bit complete (2)
		tableData[2] = relocEntry;
		
		write(file, raw + oldSizeRoundedUp, tableData);
		memcpy(relocTable + oldSizeRoundedUp, tableData, sizeof tableData);
		
	}
	inline void addEntry(FILE* file, BYTE* ptrToRelocate, char type) {
		addEntry(file, ptrToVa(ptrToRelocate), type);
	}
	
	// fills entry with zeros, diabling it. Does not change the size of either the reloc table or the reloc block
	void removeEntry(FILE* file, const FoundReloc& reloc) {
		unsigned short zeros = 0;
		int relocRaw = reloc.relocVa - va + raw;
		int relocOffset = reloc.relocVa - va;
		write(file, relocRaw, zeros);
		*(unsigned short*)(relocTable + reloc.relocVa - va) = zeros;
		if (reloc.ptrIsLast) {
			DWORD& size = *(
				(DWORD*)reloc.ptr + 1
			);
			int count = 0;
			int lastOffset = size - 2;
			while (lastOffset >= 8) {
				if (*(unsigned short*)(reloc.ptr + lastOffset) != 0) {
					break;
				}
				lastOffset -= 2;
				++count;
			}
			if (lastOffset < 8) {
				// delete whole block
				BYTE zeros[8] { 0 };
				int target = ptrToRaw(reloc.ptr);
				int sizeLeft = size;
				fseek(file, target, SEEK_SET);
				while (sizeLeft > 0) {
					int toWrite = sizeLeft > 8 ? 8 : sizeLeft;
					fwrite(zeros, 1, toWrite, file);
					sizeLeft -= toWrite;
				}
				decreaseSizeBy(file, size);
			} else {
				int shrinkage = 0;
				while (count >= 2) {
					shrinkage += 2;
					count -= 2;
				}
				// shrink block by 4 bytes
				size -= shrinkage;
				decreaseSizeBy(file, shrinkage << 1);
			}
		}
	}
	
};

#ifdef FOR_LINUX
void trimLeft(std::string& str) {
    if (str.empty()) return;
	auto it = str.begin();
	while (it != str.end() && *it <= 32) ++it;
	str.erase(str.begin(), it);
}

void trimRight(std::string& str) {
    if (str.empty()) return;
    auto it = str.end();
    --it;
    while (true) {
        if (*it > 32) break;
        if (it == str.begin()) {
            str.clear();
            return;
        }
        --it;
    }
    str.resize(it - str.begin() + 1);
}
#endif

bool crossPlatformOpenFile(FILE** file, const CrossPlatformString& path) {
	#ifndef FOR_LINUX
	errno_t errorCode = _wfopen_s(file, path.c_str(), CrossPlatformText("r+b"));
	if (errorCode != 0 || !*file) {
		if (errorCode != 0) {
			wchar_t buf[1024];
			_wcserror_s(buf, errorCode);
			CrossPlatformCout << L"Failed to open file: " << buf << L'\n';
		} else {
			CrossPlatformCout << L"Failed to open file.\n";
		}
		if (*file) {
			fclose(*file);
		}
		return false;
	}
	return true;
	#else
	*file = fopen(path.c_str(), "r+b");
	if (!*file) {
		perror("Failed to open file");
		return false;
	}
	return true;
	#endif
}

#ifdef FOR_LINUX
void copyFileLinux(const std::string& pathSource, const std::string& pathDestination) {
    std::ifstream src(pathSource, std::ios::binary);
    std::ofstream dst(pathDestination, std::ios::binary);

    dst << src.rdbuf();
}
#endif

/// <summary>
/// Finds the address which holds a pointer to a function with the given name imported from the given DLL,
/// in a given 32-bit portable executable file.
/// For example, searching USER32.DLL, GetKeyState would return a positive value on successful find, and
/// in a running process you'd cast that value to a short (__stdcall**)(int).
/// </summary>
/// <param name="dll">Include ".DLL" in the DLL's name here. Case-insensitive.</param>
/// <param name="function">The name of the function. Case-sensitive.</param>
/// <returns>The file offset which holds a pointer to a function. -1 if not found.</returns>
int findImportedFunction(const char* dll, const char* function) {
	
	// see IMAGE_IMPORT_DESCRIPTOR
	struct ImageImportDescriptor {
		DWORD ImportLookupTableRVA;  // The RVA of the import lookup table. This table contains a name or ordinal for each import. (The name "Characteristics" is used in Winnt.h, but no longer describes this field.)
		DWORD TimeDateStamp;  // The stamp that is set to zero until the image is bound. After the image is bound, this field is set to the time/data stamp of the DLL. LIES, this field is 0 for me at runtime.
		DWORD ForwarderChain;  // The index of the first forwarder reference. 0 for me.
		DWORD NameRVA;  // The address of an ASCII string that contains the name of the DLL. This address is relative to the image base.
		DWORD ImportAddressTableRVA;  // The RVA of the import address table. The contents of this table are identical to the contents of the import lookup table until the image is bound.
	};
	DWORD importsSize = pNtHeader->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].Size;
	const ImageImportDescriptor* importPtrNext = (const ImageImportDescriptor*)(rvaToPtr(
		pNtHeader->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress
	));
	for (; importsSize > 0; importsSize -= sizeof ImageImportDescriptor) {
		const ImageImportDescriptor* importPtr = importPtrNext++;
		if (!importPtr->ImportLookupTableRVA) break;
		const char* dllName = (const char*)(rvaToPtr(importPtr->NameRVA));
		if (_stricmp(dllName, dll) != 0) continue;
		DWORD* funcPtr = (DWORD*)(rvaToPtr(importPtr->ImportAddressTableRVA));
		DWORD* imageImportByNameRvaPtr = (DWORD*)(rvaToPtr(importPtr->ImportLookupTableRVA));
		struct ImageImportByName {
			short importIndex;  // if you know this index you can use it for lookup. Name is just convenience for programmers.
			char name[1];  // arbitrary length, zero-terminated ASCII string
		};
		do {
			DWORD rva = *imageImportByNameRvaPtr;
			if (!rva) break;
			const ImageImportByName* importByName = (const ImageImportByName*)(rvaToPtr(rva));
			if (strcmp(importByName->name, function) == 0) {
				return (BYTE*)funcPtr - fileBase;
			}
			++funcPtr;
			++imageImportByNameRvaPtr;
		} while (true);
		return -1;
	}
	return -1;
}

void meatOfTheProgram() {
    CrossPlatformString ignoreLine;
	#ifndef FOR_LINUX
    CrossPlatformCout << CrossPlatformText("Please select a path to your ") << exeName << CrossPlatformText(" file that will be patched...\n");
	#else
	CrossPlatformCout << CrossPlatformText("Please type in/paste a path to your ") << exeName << CrossPlatformText(" file"
		" (including the file name and extension) that will be patched...\n");
	#endif

    #ifndef FOR_LINUX
    std::vector<WCHAR> szFileBuf(MAX_PATH, L'\0');

    OPENFILENAMEW selectedFiles{ 0 };
    selectedFiles.lStructSize = sizeof(OPENFILENAMEW);
    selectedFiles.hwndOwner = NULL;
    selectedFiles.lpstrFile = szFileBuf.data();
    selectedFiles.nMaxFile = (DWORD)szFileBuf.size();
    // it says "Windows Executable\0*.EXE\0"
	char scramble[] =
		"\x4d\xf6\x5f\xf6\x64\xf6\x5a\xf6\x65\xf6\x6d\xf6\x69\xf6\x16\xf6\x3b\xf6"
		"\x6e\xf6\x5b\xf6\x59\xf6\x6b\xf6\x6a\xf6\x57\xf6\x58\xf6\x62\xf6\x5b\xf6"
		"\xf6\xf6\x20\xf6\x24\xf6\x3b\xf6\x4e\xf6\x3b\xf6\xf6\xf6\xf6\xf6";
	wchar_t filter[(sizeof scramble - 1) / sizeof (wchar_t)];
	int offset = (int)(
		(GetTickCount64() & 0xF000000000000000ULL) >> (63 - 4)
	);
	for (int i = 0; i < sizeof scramble - 1; ++i) {
		char c = scramble[i] + offset + 10;
		((char*)filter)[i] = c;
	}
    selectedFiles.lpstrFilter = filter;
    selectedFiles.nFilterIndex = 1;
    selectedFiles.lpstrFileTitle = NULL;
    selectedFiles.nMaxFileTitle = 0;
    selectedFiles.lpstrInitialDir = NULL;
    selectedFiles.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;

    if (!GetOpenFileNameW(&selectedFiles)) {
        DWORD errCode = CommDlgExtendedError();
        if (!errCode) {
            std::wcout << "The file selection dialog was closed by the user.\n";
        } else {
            std::wcout << "Error selecting file. Error code: 0x" << std::hex << errCode << std::dec << std::endl;
        }
        return;
    }
    
    std::wstring szFile = szFileBuf.data();
    #else
    std::string szFile;
    GetLine(szFile);
    trimLeft(szFile);
    trimRight(szFile);
    if (!szFile.empty() && (szFile[0] == '\'' || szFile[0] == '"')) {
    	szFile.erase(szFile.begin());
    }
    if (!szFile.empty() && (szFile[szFile.size() - 1] == '\'' || szFile[szFile.size() - 1] == '"')) {
    	szFile.erase(szFile.begin() + (szFile.size() - 1));
    }
    if (szFile.empty()) {
        std::cout << "Empty path provided. Aborting.\n";
        return;
    }
    #endif
    CrossPlatformCout << "Selected file: " << szFile.c_str() << std::endl;

    CrossPlatformChar separator = determineSeparator(szFile);
    CrossPlatformString fileName = getFileName(szFile);
    CrossPlatformString parentDir = getParentDir(szFile);
	
	int dotPos = findLast(fileName, L'.');
	if (dotPos == -1) {
		CrossPlatformCout << "Chosen file name does not end with '.EXE': " << fileName.c_str() << std::endl;
		return;
	}
	
	CrossPlatformString fileNameNoDot(fileName.begin(), fileName.begin() + dotPos);
	
	CrossPlatformChar strbuf[1024];
	int len = swprintf_s(strbuf, L"%ls\\%ls_backup%ls", parentDir.data(), fileNameNoDot.data(), fileName.data() + dotPos);
	
    int backupNameCounter = 1;
    while (fileExists(strbuf)) {
    	len = swprintf_s(strbuf, L"%ls\\%ls_backup%d%ls", parentDir.data(), fileNameNoDot.data(), backupNameCounter, fileName.data() + dotPos);
        ++backupNameCounter;
    }
    
    CrossPlatformString backupFilePath = strbuf;
    CrossPlatformCout << "Will use backup file path: " << backupFilePath.c_str() << std::endl;
	
    FILE* file = nullptr;
    if (!crossPlatformOpenFile(&file, szFile.c_str())) return;

    std::vector<char> wholeFile;
    if (!readWholeFile(file, wholeFile)) return;
    fileBase = (BYTE*)wholeFile.data();
    
	PIMAGE_DOS_HEADER pDosHeader = (PIMAGE_DOS_HEADER)fileBase;
	pNtHeader = (nthdr)((PBYTE)fileBase + pDosHeader->e_lfanew);

	#define sectionStartEnd(varname, sectionName) \
		BYTE* varname##Ptr = nullptr; \
		BYTE* varname##PtrEnd = nullptr; \
		uintptr_t varname##VA = 0; \
		uintptr_t varname##VAEnd = 0; \
		PIMAGE_SECTION_HEADER varname##Section = nullptr; \
		{ \
			PIMAGE_SECTION_HEADER sectionSeek = IMAGE_FIRST_SECTION(pNtHeader); \
			for (int i = 0; i < pNtHeader->FileHeader.NumberOfSections; ++i) { \
				if (strncmp((const char*)sectionSeek->Name, sectionName, 8) == 0) { \
					varname##Ptr = fileBase + sectionSeek->PointerToRawData; \
					varname##PtrEnd = varname##Ptr + sectionSeek->SizeOfRawData; \
					varname##VA = (ULONG_PTR)pNtHeader->OptionalHeader.ImageBase + sectionSeek->VirtualAddress; \
					varname##VAEnd = varname##VA + sectionSeek->Misc.VirtualSize; \
					varname##Section = sectionSeek; \
					break; \
				} \
				++sectionSeek; \
			} \
		}
	
	sectionStartEnd(text, ".text")
	sectionStartEnd(rdata, ".rdata")
	
	if (!textPtr) {
        CrossPlatformCout << "Failed to find .text section\n";
        return;
	}
	
	if (!rdataPtr) {
        CrossPlatformCout << "Failed to find .rdata section\n";
        return;
	}
	
    // sig for ghidra: 53 8b 5c 24 0c b8 ?? ?? ?? ?? f6 c3 01 75 05 b8 ?? ?? ?? ?? 8d 4c 24 0c 51 68 ?? ?? ?? ?? 6a 01 6a 00
    int XAudio2Create = sigscan(textPtr, textPtrEnd,
        "\x53\x8b\x5c\x24\x0c\xb8\x00\x00\x00\x00\xf6\xc3\x01\x75\x05\xb8\x00\x00\x00\x00\x8d\x4c\x24\x0c\x51\x68\x00\x00\x00\x00\x6a\x01\x6a\x00",
        "xxxxxx????xxxxxx????xxxxxx????xxxx");
    if (XAudio2Create == -1) {
        CrossPlatformCout << "Failed to find XAudio2Create\n";
        return;
    }
    XAudio2Create += textPtr - fileBase;
    CrossPlatformCout << "Found XAudio2Create: 0x" << std::hex << XAudio2Create << std::dec << std::endl;
    
    // ghidra sig: 58 1f cf 8b e7 9f 83 45 8a c6 e2 ad c4 65 c8 bb 85 86 50 5a 54 a2 ba 4f 9b 82 9a 24 b0 03 06 af 35 ea 05 db 29 03 4b 4d a5 3a 6d ea d0 3d 38 52
    int IID_IXAudio2_CLSID_XAudio2_CLSID_XAudio2_Debug = sigscan(rdataPtr, rdataPtrEnd,
    	"\x58\x1f\xcf\x8b\xe7\x9f\x83\x45\x8a\xc6\xe2\xad\xc4\x65\xc8\xbb\x85\x86\x50\x5a\x54\xa2\xba\x4f\x9b\x82\x9a\x24\xb0\x03\x06\xaf\x35\xea\x05\xdb\x29\x03\x4b\x4d\xa5\x3a\x6d\xea\xd0\x3d\x38\x52",
    	"xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    if (IID_IXAudio2_CLSID_XAudio2_CLSID_XAudio2_Debug == -1) {
        CrossPlatformCout << "Failed to find IID_IXAudio2, CLSID_XAudio2 and CLSID_XAudio2_Debug\n";
        return;
    }
    IID_IXAudio2_CLSID_XAudio2_CLSID_XAudio2_Debug += rdataPtr - fileBase;
    CrossPlatformCout << "Found IID_IXAudio2, CLSID_XAudio2 and CLSID_XAudio2_Debug: 0x" << std::hex << IID_IXAudio2_CLSID_XAudio2_CLSID_XAudio2_Debug
    	<< std::dec << std::endl;
	
	int LoadLibraryAPtr = findImportedFunction("kernel32.dll", "LoadLibraryA");
	if (LoadLibraryAPtr == -1) {
        CrossPlatformCout << "Failed to find LoadLibraryA import\n";
        return;
	}
    CrossPlatformCout << "Found LoadLibraryA import: 0x" << std::hex << LoadLibraryAPtr << std::dec << std::endl;
	
	static const char stringToWrite[] { "GGXrdAutomaticallyChangeAudioDevice.dll" };
	
	RelocTable relocTable;
	relocTable.findRelocTable();
	
	// we have everything we need
	
	fclose(file);
	
    #ifndef FOR_LINUX
    if (!CopyFileW(szFile.c_str(), backupFilePath.c_str(), true)) {
        std::wcout << "Failed to create a backup copy. Do you want to continue anyway? You won't be able to revert the file to the original. Press Enter to agree...\n";
    	GetLine(ignoreLine);
    } else {
        std::wcout << "Backup copy created successfully.\n";
    }
    #else
    copyFileLinux(szFile, backupFilePath);
    std::wcout << "Backup copy created successfully.\n";
    #endif
	
	if (!crossPlatformOpenFile(&file, szFile.c_str())) return;
	
	BYTE newCode[] {
		0x68, 0x00, 0x00, 0x00, 0x00,  // PUSH stringAddr. Will require a relocation
		0xFF, 0x15, 0x00, 0x00, 0x00, 0x00,  // call LoadLibraryA. Will require a relocation. LoadLibraryA is an stdcall and clears the stack argument for us
		0xE9, 0x00, 0x00, 0x00, 0x00,  // the loaded DLL will have to patch this JMP to go to its hook. We kept the stack arguments the same so it should just receive them
	};  // the XAudio2Create will likely never be called again
	
	std::vector<FoundReloc> relocs = relocTable.findRelocsInRegion(rawToVa(XAudio2Create), rawToVa(XAudio2Create) + sizeof (newCode));
	
	for (const FoundReloc& foundReloc : relocs) {
		relocTable.removeEntry(file, foundReloc);
	}
	
	*(DWORD*)(newCode + 1) = rawToVa(IID_IXAudio2_CLSID_XAudio2_CLSID_XAudio2_Debug);
	*(DWORD*)(newCode + 7) = rawToVa(LoadLibraryAPtr);
	relocTable.addEntry(file, rawToVa(XAudio2Create + 1), IMAGE_REL_BASED_HIGHLOW);
	relocTable.addEntry(file, rawToVa(XAudio2Create + 7), IMAGE_REL_BASED_HIGHLOW);
	
	fseek(file, IID_IXAudio2_CLSID_XAudio2_CLSID_XAudio2_Debug, SEEK_SET);
	fwrite(stringToWrite, 1, sizeof stringToWrite, file);
	
	fseek(file, XAudio2Create, SEEK_SET);
	fwrite(newCode, 1, sizeof newCode, file);
	
    CrossPlatformCout << "Patched successfully\n";
	
    fclose(file);

}

int patcherMain()
{
	
	CoInitialize(0);
	{
		static const CLSID CLSID_MMDeviceEnumerator = __uuidof(MMDeviceEnumerator);
		static const IID IID_IMMDeviceEnumerator = __uuidof(IMMDeviceEnumerator);
		CComPtr<IMMDeviceEnumerator> deviceEnumerator;
		HRESULT hr;
		hr = CoCreateInstance(
	         CLSID_MMDeviceEnumerator, NULL,
	         CLSCTX_ALL, IID_IMMDeviceEnumerator,
	         (void**)&deviceEnumerator);
		if (hr == S_OK) {
			CComPtr<IMMDeviceCollection> deviceCollection;
			hr = deviceEnumerator->EnumAudioEndpoints(eRender, DEVICE_STATE_ACTIVE, &deviceCollection);
			if (hr == S_OK) {
				UINT count;
				hr = deviceCollection->GetCount(&count);
				if (hr == S_OK) {
					for (UINT i = 0; i < count; ++i) {
						CComPtr<IMMDevice> device;
						hr = deviceCollection->Item(i, &device);
						if (hr == S_OK) {
							LPWSTR deviceId;
							HRESULT hr2 = device->GetId(&deviceId);
							CComPtr<IPropertyStore> propertyStore;
							hr = device->OpenPropertyStore(STGM_READ, &propertyStore);
							if (hr == S_OK) {
								WAVEFORMATEX waveFormat;
								PROPVARIANT propertyValue;
								hr = propertyStore->GetValue(PKEY_AudioEngine_DeviceFormat, &propertyValue);
								if (SUCCEEDED(hr)) {
									memcpy(&waveFormat, propertyValue.blob.pBlobData, propertyValue.blob.cbSize);
								}
								PROPVARIANT friendlyName;
								HRESULT hr3 = propertyStore->GetValue(PKEY_Device_FriendlyName, &friendlyName);
								if (SUCCEEDED(hr3)) {
									int lol = 1;
									PropVariantClear(&friendlyName);
								}
								if (SUCCEEDED(hr)) {
									PropVariantClear(&propertyValue);
								}
							}
							if (hr2 == S_OK) {
								CoTaskMemFree(deviceId);
							}
						}
					}
				}
			}
		}
	}
	CoUninitialize();
	
	
	#ifndef FOR_LINUX
	int offset = (int)(
		(GetTickCount64() & 0xF000000000000000ULL) >> (63 - 4)
	);
	#else
	int offset = 0;
	#endif
	
	for (int i = 0; i < sizeof exeName - 1; ++i) {
		exeName[i] += offset + 10;
	}
	
    CrossPlatformCout <<
                  "This program patches " << exeName << " executable to permanently load the GGXrdAutomaticallyChangeAudioDevice.dll when it tries to initialize audio.\n"
                  "This cannot be undone, and you should backup your " << exeName << " file before proceeding.\n"
                  "A backup copy will also be automatically created (but that may fail).\n"
                  "(!) The GGXrdAutomaticallyChangeAudioDevice.dll must be placed in the same folder as the game executable in order to get loaded by the game. (!)\n"
                  "(!) If the DLL is not present in the folder with the game, the game will CRASH!\n"
                  "Only Guilty Gear Xrd Rev2 version 2211 supported.\n"
                  "Press Enter when ready...\n";

    CrossPlatformString ignoreLine;
    GetLine(ignoreLine);

    meatOfTheProgram();

    CrossPlatformCout << "Press Enter to exit...\n";
    GetLine(ignoreLine);
    return 0;
}

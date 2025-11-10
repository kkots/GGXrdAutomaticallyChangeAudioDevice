// pch.h: This is a precompiled header file.
// Files listed below are compiled only once, improving build performance for future builds.
// This also affects IntelliSense performance, including code completion and many code browsing features.
// However, files listed here are ALL re-compiled if any one of them is updated between builds.
// Do not add files here that you will be updating frequently as this negates the performance advantage.

#ifndef PCH_H
#define PCH_H

#ifndef FOR_LINUX
#include "framework.h"
#else
typedef unsigned int DWORD;  // long (that's right, just long, not long long) is 8 bytes on g++, so we use int
typedef unsigned short WORD;
typedef unsigned char BYTE;
typedef void* LPVOID;
typedef unsigned long long ULONG_PTR;
typedef unsigned char* PBYTE;
typedef signed int LONG;
#endif

#endif //PCH_H

#include "pch.h"
#include "mod.h"
#include "logging.h"

#ifdef LOG_PATH
#include <io.h>     // for _open_osfhandle
#include <fcntl.h>  // for _O_APPEND
#include <iostream>
#include <mutex>
FILE* logfile = NULL;
int msgLimit = 1000;
std::mutex logfileMutex;
#endif

#ifdef LOG_PATH
static void closeLog();
#else
#define closeLog()
#endif

BOOL APIENTRY DllMain( HMODULE hModule,
                       DWORD  ul_reason_for_call,
                       LPVOID lpReserved
                     )
{
	switch (ul_reason_for_call)
	{
	case DLL_PROCESS_ATTACH:
#ifdef LOG_PATH
		{
			HANDLE fileHandle = CreateFileW(
				LOG_PATH,
				GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS,
				FILE_ATTRIBUTE_NORMAL, NULL);
			if (fileHandle == INVALID_HANDLE_VALUE) {
				return FALSE;
			}
			int fileDesc = _open_osfhandle((intptr_t)fileHandle, _O_APPEND | _O_TEXT);
			logfile = _fdopen(fileDesc, "at");
			fputs("DllMain called with fdwReason DLL_PROCESS_ATTACH\n", logfile);
			fflush(logfile);
		}
#endif
		if (!onAttach()) {
			closeLog();
			return FALSE;
		}
		break;
	case DLL_THREAD_ATTACH:
		break;
	case DLL_THREAD_DETACH:
		break;
	case DLL_PROCESS_DETACH:
		closeLog();
		break;
	}
	return TRUE;
}

#ifdef LOG_PATH
void closeLog() {
	if (logfile) {
		fflush(logfile);
		fclose(logfile);
		logfile = NULL;
	}
}
#endif

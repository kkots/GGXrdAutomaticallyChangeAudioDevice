#pragma once
#include <iostream>
#include <mutex>

extern FILE* logfile;
extern int msgLimit;
extern std::mutex logfileMutex;

#ifdef LOG_PATH
#define logwrap(things) \
{ \
	if (logfile) { \
		std::unique_lock<std::mutex> logfileGuard(logfileMutex); \
		things; \
		fflush(logfile); \
	} \
}
#else
#define logwrap(things)
#endif

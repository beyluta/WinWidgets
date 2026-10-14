#ifndef SYSWRAP_H
#define SYSWRAP_H

#ifdef _WIN32
#include <windows.h>
#define SleepFor(x) Sleep(x)
#elif __linux__
#include <unistd.h>
#define SleepFor(x) usleep(x * 1000);
#endif

#endif

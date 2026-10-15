#ifndef SYSWRAP_H
#define SYSWRAP_H

#ifdef _WIN32
#include <windows.h>
#include "debug.h"
#define SleepFor(x) Sleep(x)
#define LogInfo(msg)                                                           \
        do                                                                     \
        {                                                                      \
                string format = "%s at %zu";                                   \
                const size_t size =                                            \
                        snprintf(nullptr, 0, format, msg, __LINE__);           \
                string buff = (string)malloc(sizeof(char) * (size + 1));       \
                if (!buff)                                                     \
                {                                                              \
                        break;                                                 \
                }                                                              \
                snprintf(buff, size + 1, format, msg, __LINE__);               \
                Debug(buff);                                                   \
                free(buff);                                                    \
        } while (0);

#elif __linux__
#include <errno.h>
#include <time.h>
#define SleepFor(x)                                                            \
        do                                                                     \
        {                                                                      \
                struct timespec ts;                                            \
                int res;                                                       \
                if (x < 0)                                                     \
                {                                                              \
                        errno = EINVAL;                                        \
                        break;                                                 \
                }                                                              \
                ts.tv_sec = x / 1000;                                          \
                ts.tv_nsec = (x % 1000) * 1000000;                             \
                do                                                             \
                {                                                              \
                        res = nanosleep(&ts, &ts);                             \
                } while (res && errno == EINTR);                               \
        } while (0);
#define LogInfo(msg)                                                           \
        do                                                                     \
        {                                                                      \
                string format = "%s at %zu";                                   \
                fprintf(stderr, format, msg, __LINE__);                        \
        } while (0);
#endif

#endif

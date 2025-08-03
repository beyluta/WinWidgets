#ifndef GLOBAL_H
#define GLOBAL_H

#if __linux__
#include <sys/types.h>
#else
#include <stddef.h>
#endif

constexpr unsigned char EXIT_REASON_TERMINATED = 0;
constexpr unsigned char EXIT_REASON_IO_FAILURE = 1;
constexpr unsigned char EXIT_REASON_MEM_FAILURE = 2;
constexpr char OPERATION_STATE_INDEX_OUT_OF_BOUNDS = -1;
constexpr char OPERATION_STATE_RETURNED_ZERO = 0;
constexpr size_t BUFFSIZE = 512;
constexpr size_t EXTBUFFSIZE = 4096;
constexpr unsigned char MAX_WIDGETS = 128;
#endif

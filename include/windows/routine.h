#ifndef ROUTINE_H
#define ROUTINE_H

#include <stdint.h>
#include <pthread.h>

typedef enum : uint8_t
{
        ROUTINE_STAT_OK = 1,
        ROUTINE_STAT_STOPPED = 2,
} ww_routine_stat_t;

typedef struct
{
        pthread_t thread;
        void *(*cb)(void *);
        size_t interval;
        void *arg;
        ww_routine_stat_t stat;
} ww_routine_t;

void
CreateRoutine(ww_routine_t *const routine,
              const size_t interval,
              void *(*cb)(void *),
              void *arg);

void
DestroyRoutine(ww_routine_t *const routine);

#endif

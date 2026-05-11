#include "routine.h"
#include <windows.h>

static void *
RoutineIntervalProceed(void *data)
{
        ww_routine_t *routine = (ww_routine_t *)data;
        do
        {
                routine->cb(routine->arg);
                Sleep(routine->interval);
        } while (routine->stat == ROUTINE_STAT_OK);

        return nullptr;
}

void
CreateRoutine(ww_routine_t *const routine,
              const size_t interval,
              void *(*cb)(void *),
              void *arg)
{
        routine->cb = cb;
        routine->interval = interval;
        routine->arg = arg;
        routine->stat = ROUTINE_STAT_OK;
        pthread_create(
                &routine->thread, nullptr, RoutineIntervalProceed, routine);
}

void
DestroyRoutine(ww_routine_t *const routine)
{
        routine->stat = ROUTINE_STAT_STOPPED;
        pthread_detach(routine->thread);
}

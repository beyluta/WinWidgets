#include "debug.h"
#include <windows.h>
#include <stdio.h>

void
Debug(const string message)
{
        const size_t size = strlen(message);

        string buf = (string)malloc(sizeof(char) * (size + 2));
        if (!buf)
        {
                return;
        }

        if (snprintf(buf, size + 2, "%s\n", message) < 0)
        {
                free(buf);
                return;
        }

        static bool consoleAlloced = false;
        if (AllocConsole() == 0 && !consoleAlloced)
        {
                free(buf);
                return;
        }
        consoleAlloced = true;

        HANDLE stdOut = GetStdHandle(STD_OUTPUT_HANDLE);
        if (stdOut == NULL || stdOut == INVALID_HANDLE_VALUE)
        {
                free(buf);
                return;
        }

        DWORD written = 0;
        if (!WriteConsoleA(stdOut, buf, strlen(buf), &written, NULL))
        {
                free(buf);
                return;
        }

        free(buf);
}

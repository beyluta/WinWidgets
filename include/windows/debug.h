// ======================= Purpose ==========================
//
// Debugging file for the Win32 API. Since it is not possible
// to print to the console at runtime.
//
// This file should not be included in the release build.
//
// ==========================================================
#ifndef DEBUG_H
#define DEBUG_H

#include "utils.h"

#include <windows.h>
#include <stdio.h>

/**
 * @brief Creates a console window and writes debug messages to it
 * @param message Text to print out
 */
void
Debug(const char *const message)
{
        const size_t messageLen = strlen(message);
        char newMessage[USHRT_MAX];

        if (messageLen >= lengthof(newMessage))
        {
                return;
        }

        if (snprintf(newMessage, lengthof(newMessage), "%s\n", message) < 0)
        {
                return;
        }

        static bool consoleAlloced = false;
        if (AllocConsole() == 0 && !consoleAlloced)
        {
                return;
        }
        consoleAlloced = true;

        HANDLE stdOut = GetStdHandle(STD_OUTPUT_HANDLE);
        if (stdOut == NULL || stdOut == INVALID_HANDLE_VALUE)
        {
                return;
        }

        DWORD written = 0;
        if (!WriteConsoleA(
                    stdOut, newMessage, strlen(newMessage), &written, NULL))
        {
                return;
        }
}

#endif

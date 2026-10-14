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

/**
 * @brief Creates a console window and writes debug messages to it
 * @param message Text to print out
 */
void
Debug(const string message);

#endif

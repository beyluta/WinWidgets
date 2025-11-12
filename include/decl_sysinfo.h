// ======================= Purpose ==========================
//
// Declaration file for OS-agnostic functions to get or
// manipulate peripheral and hardware information during
// runtime.
//
// Example: Mouse position, keypresses, system volume, etc.
//
// ==========================================================
#ifndef DECL_SYSINFO_H
#define DECL_SYSINFO_H

#include <stdint.h>

#define SYSINFO_CODE_FAIL(expression) ((expression) > 0)

typedef enum : uint8_t
{
        SYSINFO_CODE_SUCCESS,
        SYSINFO_CODE_NOT_IMPLEMENTED,
        SYSINFO_CODE_OS_ERROR
} sysinfo_code_t;

/**
 * @brief Gets the global position of the mouse
 * @param x Position on the x axis
 * @param y Position on the y axis
 * @returns true if successful, else false on failure
 */
sysinfo_code_t
GetMousePosition(size_t *const x, size_t *const y);

#endif

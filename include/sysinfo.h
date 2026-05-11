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

#if _WIN32
#include <windef.h>
#elif __linux__
#include <stddef.h>
#include <gdk/gdk.h>
#endif

#define SYSINFO_CODE_FAIL(expression) ((expression) > 0)

typedef enum : uint8_t
{
        SYSINFO_CODE_SUCCESS,
        SYSINFO_CODE_NOT_IMPLEMENTED,
        SYSINFO_CODE_OS_ERROR,
        SYSINFO_CODE_ERROR
} sysinfo_code_t;

typedef struct
{
        size_t totalVirtMem;
        size_t usedVirtMem;
        size_t totalPhysMem;
        size_t usedPhysMem;
} ww_memory_info_t;

/**
 * @brief Gets the global position of the mouse
 * @param x Position on the x axis
 * @param y Position on the y axis
 * @returns SYSINFO_CODE_SUCCESS if successful, else a code on failure
 */
sysinfo_code_t
#if _WIN32
GetMousePosition(size_t *const x, size_t *const y);
#elif __linux__
GetMousePosition(gint *const x, gint *const y);
#endif

/**
 * @brief Get information about the current key pressed
 * @param code Code of the key pressed up to 255
 * @returns SYSINFO_CODE_SUCCESS if successful, else a code on failure
 */
sysinfo_code_t
GetCurrentKeyPressed(uint8_t *const code);

/**
 * @brief Toggles any audio playing from a stream
 * @returns SYSINFO_CODE_SUCCESS if successful, else a code on failure
 */
sysinfo_code_t
ToggleMediaPlayback();

/**
 * @brief Jumps to the media audio stream
 * @returns SYSINFO_CODE_SUCCESS if successful, else a code on failure
 */
sysinfo_code_t
NextMediaTrack();

/**
 * @brief Jumps to the media audio stream
 * @returns SYSINFO_CODE_SUCCESS if successful, else a code on failure
 */
sysinfo_code_t
PreviousMediaTrack();

/**
 * @brief Moves the widget window to a specific position
 */
sysinfo_code_t
#if __linux__
MoveWindowToPosition(const size_t x, const size_t y);
#elif __WIN32
MoveWindowToPosition(const HWND hWnd, const size_t x, const size_t y);
#endif

/**
 * @brief Gets memory information
 * @param memInfo Struct containing memory information
 * @returns SYSINFO_CODE_SUCCESS if successful, else a code on failure
 */
sysinfo_code_t
GetMemoryInfo(ww_memory_info_t *const memInfo);

/**
 * @brief Gets the current usage percentage of the CPU
 * @returns The current percentage of the CPU's usage
 */
double
GetCpuUsagePercentage();

/**
 * @brief Sets the percentage of the CPUs usage
 */
void
SetCpuUsagePercentage(const double n);

#endif

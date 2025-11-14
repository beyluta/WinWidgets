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
#endif

#define SYSINFO_CODE_FAIL(expression) ((expression) > 0)

typedef enum : uint8_t
{
        SYSINFO_CODE_SUCCESS,
        SYSINFO_CODE_NOT_IMPLEMENTED,
        SYSINFO_CODE_OS_ERROR,
        SYSINFO_CODE_ERROR
} sysinfo_code_t;

/**
 * @brief Gets the global position of the mouse
 * @param x Position on the x axis
 * @param y Position on the y axis
 * @returns SYSINFO_CODE_SUCCESS if successful, else a code on failure
 */
sysinfo_code_t
GetMousePosition(size_t *const x, size_t *const y);

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

#endif

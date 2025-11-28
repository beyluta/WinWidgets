// ======================= Purpose ==========================
//
// Implementation file for the declarations in the header.
//
// Handles both Windows and Linux code. Should one platform
// not support a function, it should return some error value.
//
// ==========================================================
#include "sysinfo.h"

#if _WIN32
#include <windef.h>
#include <windows.h>

static constexpr uint16_t STATE_KEY_HELD = 0x8000;
static constexpr uint16_t STATE_KEY_MEDIA_TOGGLE = 0xB3;
static constexpr uint16_t STATE_KEY_MEDIA_NEXT = 0xB0;
static constexpr uint16_t STATE_KEY_MEDIA_PREV = 0xB1;
#endif

sysinfo_code_t
#if __linux__
GetMousePosition(size_t *const, size_t *const)
{
        return SYSINFO_CODE_NOT_IMPLEMENTED;
#elif _WIN32
GetMousePosition(size_t *const x, size_t *const y)
{
        POINT point;
        if (!GetCursorPos(&point))
        {
                return SYSINFO_CODE_OS_ERROR;
        }

        *x = point.x;
        *y = point.y;
#endif
        return SYSINFO_CODE_SUCCESS;
}

sysinfo_code_t
#if __linux__
GetCurrentKeyPressed(uint8_t *const)
{
        return SYSINFO_CODE_NOT_IMPLEMENTED;
#elif _WIN32
GetCurrentKeyPressed(uint8_t *const code)
{
        for (uint16_t i = 0; i <= UINT8_MAX; i++)
        {
                if (GetAsyncKeyState(i) & STATE_KEY_HELD)
                {
                        *code = i;
                        return SYSINFO_CODE_SUCCESS;
                }
        }
#endif
        return SYSINFO_CODE_ERROR;
}

sysinfo_code_t
ToggleMediaPlayback()
{
#if __linux__
        return SYSINFO_CODE_NOT_IMPLEMENTED;
#elif _WIN32
        keybd_event(STATE_KEY_MEDIA_TOGGLE, 0, 0, 0);
#endif
        return SYSINFO_CODE_SUCCESS;
}

/**
 * @brief Jumps to the media audio stream
 * @returns SYSINFO_CODE_SUCCESS if successful, else a code on failure
 */
sysinfo_code_t
NextMediaTrack()
{
#if __linux__
        return SYSINFO_CODE_NOT_IMPLEMENTED;
#elif _WIN32
        keybd_event(STATE_KEY_MEDIA_NEXT, 0, 0, 0);
#endif
        return SYSINFO_CODE_SUCCESS;
}

/**
 * @brief Jumps to the media audio stream
 * @returns SYSINFO_CODE_SUCCESS if successful, else a code on failure
 */
sysinfo_code_t
PreviousMediaTrack()
{
#if __linux__
        return SYSINFO_CODE_NOT_IMPLEMENTED;
#elif _WIN32
        keybd_event(STATE_KEY_MEDIA_PREV, 0, 0, 0);
#endif
        return SYSINFO_CODE_SUCCESS;
}

/**
 * @brief Moves the widget window to a specific position
 */
sysinfo_code_t
#if __linux__
MoveWindowToPosition(const size_t, const size_t)
{
#elif _WIN32
MoveWindowToPosition(const HWND hWnd, const size_t x, const size_t y)
{
        if (!SetWindowPos(hWnd,
                          nullptr,
                          x,
                          y,
                          0,
                          0,
                          SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE))
        {
                return SYSINFO_CODE_OS_ERROR;
        }
#endif
        return SYSINFO_CODE_SUCCESS;
}

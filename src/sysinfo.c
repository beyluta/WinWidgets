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
#elif __linux__
#include "gdk/gdk.h"
#endif

sysinfo_code_t
#if __linux__
GetMousePosition(gint *const x, gint *const y)
{
        GdkDisplay *display = gdk_display_get_default();
        if (display == nullptr)
        {
                return SYSINFO_CODE_ERROR;
        }

        GdkSeat *seat = gdk_display_get_default_seat(display);
        GdkDevice *device = gdk_seat_get_pointer(seat);
        GdkScreen *screen;
        gdk_device_get_position(device, &screen, x, y);
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

sysinfo_code_t
GetMemoryInfo(ww_memory_info_t *const memInfo)
{
#if _WIN32
        MEMORYSTATUSEX memStat;
        memStat.dwLength = sizeof(memStat);
        if (GlobalMemoryStatusEx(&memStat) == 0)
        {
                return SYSINFO_CODE_OS_ERROR;
        }

        memInfo->totalVirtMem = memStat.ullTotalPageFile;
        memInfo->usedVirtMem =
                memStat.ullTotalPageFile - memStat.ullAvailPageFile;
        memInfo->totalPhysMem = memStat.ullTotalPhys;
        memInfo->usedPhysMem = memStat.ullTotalPhys - memStat.ullAvailPhys;
#endif

        return SYSINFO_CODE_SUCCESS;
}

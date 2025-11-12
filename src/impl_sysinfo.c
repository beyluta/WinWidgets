// ======================= Purpose ==========================
//
// Implementation file for the declarations in the header.
//
// Handles both Windows and Linux code. Should one platform
// not support a function, it should return some error value.
//
// ==========================================================
#include "decl_sysinfo.h"

#if _WIN32
#include <windef.h>
#include <windows.h>
#endif

sysinfo_code_t
GetMousePosition(size_t *const x, size_t *const y)
{
#if __linux__
        return SYSINFO_CODE_NOT_IMPLEMENTED;
#elif _WIN32
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

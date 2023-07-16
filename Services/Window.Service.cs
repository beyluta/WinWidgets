using Models;
using System;

namespace Services
{
    internal class WindowService : WindowModel
    {

        /// <summary>
        /// Sets the transparency of a window by its handle
        /// </summary>
        /// <param name="handle">Handle of the window</param>
        /// <param name="alpha">Opacity of the window from 0 to 255</param>
        public void SetWindowTransparency(IntPtr handle, byte alpha)
        {
            SetWindowLong(handle, GWL_EXSTYLE, GetWindowLong(handle, GWL_EXSTYLE) | WS_EX_LAYERED);
            SetLayeredWindowAttributes(handle, 0, alpha, LWA_ALPHA);
        }

        /// <summary>
        /// Hides the window from the program switcher (alt + tab shortcut)
        /// </summary>
        /// <param name="handle">Handle of the window</param>
        public void HideWindowFromProgramSwitcher(IntPtr handle)
        {
            SetWindowLong(handle, GWL_EXSTYLE, GetWindowLong(handle, GWL_EXSTYLE) | WS_EX_TOOLWINDOW);
        }
    }
}

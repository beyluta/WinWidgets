using CefSharp;
using System;

namespace Widgets
{
    class WidgetMenuHandler : WindowEssentials, IContextMenuHandler
    {
        private IntPtr handle;

        public WidgetMenuHandler(IntPtr handle)
        {
            this.handle = handle;
        }

        public void OnBeforeContextMenu(IWebBrowser chromiumWebBrowser, IBrowser browser, IFrame frame, IContextMenuParams parameters, IMenuModel model)
        {
            model.Clear();
            model.AddItem(0, "Close Widget");
        }

        public bool OnContextMenuCommand(IWebBrowser chromiumWebBrowser, IBrowser browser, IFrame frame, IContextMenuParams parameters, CefMenuCommand commandId, CefEventFlags eventFlags)
        {
            switch (commandId)
            {
                case 0:
                    SendMessage(handle, WM_CLOSE, IntPtr.Zero, IntPtr.Zero);
                    return true;
            }

            return false;
        }

        public void OnContextMenuDismissed(IWebBrowser chromiumWebBrowser, IBrowser browser, IFrame frame) { }

        public bool RunContextMenu(IWebBrowser chromiumWebBrowser, IBrowser browser, IFrame frame, IContextMenuParams parameters, IMenuModel model, IRunContextMenuCallback callback)
        {
            return false;
        }
    }
}

﻿using CefSharp;
using System.Windows.Forms;

namespace Widgets
{
    internal class WidgetMenuHandler : WindowEssentials, IContextMenuHandler
    {
        private Widget widget;

        public WidgetMenuHandler(Widget widget)
        {
            this.widget = widget;
        }

        public void OnBeforeContextMenu(IWebBrowser chromiumWebBrowser, IBrowser browser, IFrame frame, IContextMenuParams parameters, IMenuModel model)
        {
            model.Clear();
            model.AddItem(0, "Toggle Move");
            model.AddItem((CefMenuCommand)1, "Toggle Always on Top");
            model.AddItem((CefMenuCommand)2, "Close Widget");
        }

        public bool OnContextMenuCommand(IWebBrowser chromiumWebBrowser, IBrowser browser, IFrame frame, IContextMenuParams parameters, CefMenuCommand commandId, CefEventFlags eventFlags)
        {
            switch (commandId)
            {
                case 0:
                    if (widget.moveModeEnabled)
                    {
                        widget.SetWindowTransparency(widget.handle, 255);
                        widget.moveModeEnabled = false;
                    }
                    else
                    {
                        widget.SetWindowTransparency(widget.handle, 200);
                        widget.moveModeEnabled = true;
                    }
                    return true;

                case (CefMenuCommand)1:
                    widget.window.Invoke(new MethodInvoker(delegate ()
                    {
                        widget.window.TopMost = widget.window.TopMost == false ? true : false;
                    }));
                    return true;

                case (CefMenuCommand)2:
                    widget.window.Invoke(new MethodInvoker(delegate ()
                    {
                        widget.window.Close();
                    }));
                    return true;
            }

            return false;
        }

        public void OnContextMenuDismissed(IWebBrowser chromiumWebBrowser, IBrowser browser, IFrame frame)
        { }

        public bool RunContextMenu(IWebBrowser chromiumWebBrowser, IBrowser browser, IFrame frame, IContextMenuParams parameters, IMenuModel model, IRunContextMenuCallback callback)
        {
            return false;
        }
    }
}
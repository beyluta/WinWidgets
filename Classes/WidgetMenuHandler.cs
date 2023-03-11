using CefSharp;
using System;
using System.Windows.Forms;
using Widgets.Manager;

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
                    widget.moveModeEnabled = widget.moveModeEnabled ? false : true;
                    return true;

                case (CefMenuCommand)1:
                    widget.window.Invoke(new MethodInvoker(delegate ()
                    {
                        widget.window.TopMost = widget.window.TopMost ? false : true;
                    }));
                    return true;

                case (CefMenuCommand)2:
                    widget.window.BeginInvoke(new Action(() =>
                    {
                        widget.window.Close();
                        WidgetAssets.widgets.RemoveWidget(widget);
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
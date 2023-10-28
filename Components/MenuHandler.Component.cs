using CefSharp;
using Models;
using Services;

namespace Components
{
    internal class MenuHandlerComponent : WindowModel, IContextMenuHandler
    {
        private WidgetComponent widgetComponent;
        private MenuHandlerService menuHandlerService = new MenuHandlerService();
        private WidgetService widgetService = new WidgetService();

        public MenuHandlerComponent(WidgetComponent widget)
        {
            this.widgetComponent = widget;
        }

        public void OnBeforeContextMenu(IWebBrowser chromiumWebBrowser, IBrowser browser, IFrame frame, IContextMenuParams parameters, IMenuModel model)
        {
            this.menuHandlerService.ClearModel(model);
            this.menuHandlerService.AddOption("Move", 0, false, model);
            this.menuHandlerService.AddOption("Always on Top", 1, widgetComponent.window.TopMost, model);
            this.menuHandlerService.AddOption("Close Widget", 2, false, model);
        }

        public bool OnContextMenuCommand(IWebBrowser chromiumWebBrowser, IBrowser browser, IFrame frame, IContextMenuParams parameters, CefMenuCommand commandId, CefEventFlags eventFlags)
        {
            switch (commandId)
            {
                case 0:
                    this.widgetService.ToggleMove(widgetComponent);
                    return true;

                case (CefMenuCommand)1:
                    this.widgetService.ToggleTopMost(widgetComponent);
                    return true;

                case (CefMenuCommand)2:
                    this.widgetService.CloseWidget(widgetComponent);
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
using CefSharp;
using Models;
using Services;

namespace Controllers
{
    internal class MenuHandlerController : WindowModel, IContextMenuHandler
    {
        private WidgetController widget;
        private MenuHandlerService menuHandlerService;
        private WidgetService widgetService;

        public MenuHandlerController(WidgetController widget, MenuHandlerService menuHandlerService, WidgetService widgetService)
        {
            this.widget = widget;
            this.menuHandlerService = menuHandlerService;
            this.widgetService = widgetService;
        }

        public void OnBeforeContextMenu(IWebBrowser chromiumWebBrowser, IBrowser browser, IFrame frame, IContextMenuParams parameters, IMenuModel model)
        {
            this.menuHandlerService.ClearModel(model);
            this.menuHandlerService.AddOption("Toggle Move", 0, model);
            this.menuHandlerService.AddOption("Toggle Always on Top", 1, model);
            this.menuHandlerService.AddOption("Close Widget", 2, model);
        }

        public bool OnContextMenuCommand(IWebBrowser chromiumWebBrowser, IBrowser browser, IFrame frame, IContextMenuParams parameters, CefMenuCommand commandId, CefEventFlags eventFlags)
        {
            switch (commandId)
            {
                case 0:
                    this.widgetService.ToggleMove(widget);
                    return true;

                case (CefMenuCommand)1:
                    this.widgetService.ToggleTopMost(widget);
                    return true;

                case (CefMenuCommand)2:
                    this.widgetService.CloseWidget(widget);
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
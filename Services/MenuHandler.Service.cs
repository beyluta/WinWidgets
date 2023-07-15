using CefSharp;

namespace Services
{
    internal class MenuHandlerService
    {
        public void ClearModel(IMenuModel model)
        {
            model.Clear();
        }

        public void AddOption(string option, int index, IMenuModel model)
        {
            model.AddItem((CefMenuCommand)index, option);
        }
    }
}

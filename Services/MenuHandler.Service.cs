using CefSharp;

namespace Services
{
    internal class MenuHandlerService
    {
        /// <summary>
        /// Removes all options from the menu
        /// </summary>
        /// <param name="model">Model to remove the options from</param>
        public void ClearModel(IMenuModel model)
        {
            model.Clear();
        }

        /// <summary>
        /// Adds a single option to the menu
        /// </summary>
        /// <param name="option">Name of the option to add</param>
        /// <param name="index">ID of the position of the option</param>
        /// <param name="model">Model of the menu to add the option to</param>
        public void AddOption(string option, int index, bool checkd, IMenuModel model)
        {
            model.AddCheckItem((CefMenuCommand)index, option);
            model.SetChecked((CefMenuCommand)index, checkd);
        }
    }
}

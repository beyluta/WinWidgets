using CefSharp;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

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

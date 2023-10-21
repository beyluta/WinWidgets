using Modules;
using System.Windows.Forms;

namespace WidgetsDotNet.Services
{
    internal class WidgetManager
    {
        public void MinimizeWidgetManager(WidgetForm window)
        {
            window.WindowState = FormWindowState.Minimized;
        }
    }
}

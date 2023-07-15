using Controllers;
using System;
using System.Windows.Forms;

namespace Services
{
    internal class WidgetService
    {
        public void ToggleMove(WidgetController widget)
        {
            widget.moveModeEnabled = !widget.moveModeEnabled;
        }

        public void ToggleTopMost(WidgetController widget)
        {
            widget.window.Invoke(new MethodInvoker(delegate ()
            {
                widget.window.TopMost = !widget.window.TopMost;
            }));
        }

        public void CloseWidget(WidgetController widget)
        {
            widget.window.BeginInvoke(new Action(() =>
            {
                widget.window.Close();
                AssetService.widgets.RemoveWidget(widget);
            }));
        }
    }
}

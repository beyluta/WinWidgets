using Components;
using System;
using System.Windows.Forms;

namespace Services
{
    internal class WidgetService
    {
        /// <summary>
        /// Toggles the move mode of a widget. Allows it to me dragged
        /// </summary>
        /// <param name="widget">The widget to be set to movable</param>
        public void ToggleMove(WidgetComponent widget)
        {
            widget.moveModeEnabled = !widget.moveModeEnabled;
        }

        /// <summary>
        /// Pins the widget to the screen at all times
        /// </summary>
        /// <param name="widget">The widget to be set to topmost</param>
        public void ToggleTopMost(WidgetComponent widget)
        {
            widget.window.Invoke(new MethodInvoker(delegate ()
            {
                widget.window.TopMost = !widget.window.TopMost;
            }));
        }

        /// <summary>
        /// Closes a widget and removes it from the global widget list
        /// </summary>
        /// <param name="widget">The wiget to be closed</param>
        public void CloseWidget(WidgetComponent widget)
        {
            widget.window.BeginInvoke(new Action(() =>
            {
                widget.window.Close();
                AssetService.widgets.RemoveWidget(widget);
            }));
        }
    }
}

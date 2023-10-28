using System.Drawing;

namespace Models
{
    abstract internal class WidgetManagerModel : WidgetModel
    {
        /// <summary>
        /// Opens the widget or its options
        /// </summary>
        /// <param name="id">id of the widget to open</param>
        abstract public void OpenWidget(int id);

        /// <summary>
        /// Opens widget by its path
        /// </summary>
        /// <param name="path">path of the widget to open</param>
        /// <param name="position">position where to open the widget</param>
        /// <param name="alwayOnTop">whether "Always on Top" flag of the widget must be set. If null, use the default</param>
        abstract public void OpenWidget(string path, Point position, bool? alwaysOnTop);

        /// <summary>
        /// Automatically starts all widgets in the config.js list 
        /// </summary>
        abstract public void OpenWidgets();
    }
}

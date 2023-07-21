using System.Drawing;
using WidgetsDotNet.Models;

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
        /// <param name="path"></param>
        abstract public void OpenWidget(string path, Point position);

        /// <summary>
        /// Automatically starts all widgets in the config.js list 
        /// </summary>
        abstract public void AutoStartWidgets();
    }
}

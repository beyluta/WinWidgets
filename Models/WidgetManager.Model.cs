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
    }
}

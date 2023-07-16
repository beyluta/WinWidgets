using Components;
using System.Collections;

namespace Services
{
    class WidgetListService
    {
        /// <summary>
        /// Private widgets list
        /// </summary>
        private ArrayList widgets = new ArrayList();

        /// <summary>
        /// Readonly global widgets list
        /// </summary>
        public ArrayList Widgets { get => widgets; }

        /// <summary>
        /// Adds a widget to the global widgets list
        /// </summary>
        /// <param name="widget">The widget to be added</param>
        public void AddWidget(WidgetComponent widget)
        {
            if (!widgets.Contains(widget))
            {
                widgets.Add(widget);
            }
        }

        /// <summary>
        /// Removes a widget from the global widget list
        /// </summary>
        /// <param name="widget"></param>
        public void RemoveWidget(WidgetComponent widget)
        {
            widgets.Remove(widget);
        }
    }
}

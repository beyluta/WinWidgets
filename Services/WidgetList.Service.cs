using Controllers;
using System;
using System.Collections;

namespace Services
{
    class WidgetListService
    {
        private ArrayList widgets = new ArrayList();
        public ArrayList Widgets { get => widgets; }

        public void AddWidget(WidgetController widget)
        {
            if (!widgets.Contains(widget))
            {
                widgets.Add(widget);
            }
        }

        public void RemoveWidget(WidgetController widget)
        {
            widgets.Remove(widget);
        }
    }
}

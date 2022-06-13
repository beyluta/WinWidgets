using System.Collections;

namespace Widgets
{
    class WidgetList
    {
        private ArrayList widgets = new ArrayList();
        public ArrayList Widgets { get => widgets; }

        public void AddWidget(Widget widget)
        {
            if (!widgets.Contains(widget))
            {
                widgets.Add(widget);
            }
        }

        public void RemoveWidget(Widget widget)
        {
            widgets.Remove(widget);
        }
    }
}

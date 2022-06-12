using System.Collections;

namespace Widgets
{
    class WidgetMap
    {
        private ArrayList widgets = new ArrayList();
        private int maxItems;
        public ArrayList Widgets { get => widgets; }

        public WidgetMap(int maxItems = 1)
        {
            this.maxItems = maxItems;
        }

        public void AddWidget(Widget widget)
        {
            if (!widgets.Contains(widget) && HasSpaceLeft())
            {
                widgets.Add(widget);
            }
        }

        public bool HasSpaceLeft()
        {
            return widgets.Count < maxItems ? true : false;
        }
    }
}

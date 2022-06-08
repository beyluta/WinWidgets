using System.Collections;

namespace Widgets
{
    class WidgetMap
    {
        private int maxItems;

        public WidgetMap(int maxItems = 1)
        {
            this.maxItems = maxItems;
        }

        private ArrayList widgets = new ArrayList();

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

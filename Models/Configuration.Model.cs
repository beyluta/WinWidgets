using System.Collections.Generic;
using System.Drawing;

namespace Models
{
    internal struct WidgetConfiguration
    {
        /// <summary>
        /// Path where the widget is located
        /// </summary>
        public string path { get; set; }

        /// <summary>
        /// Position on the screen where the widget was last seen
        /// </summary>
        public Point position { get; set; }
    }

    internal struct Configuration
    {
        /// <summary>
        /// Semantic version of the application
        /// </summary>
        public string version { get; set; }

        /// <summary>
        /// Should all previously open widgets autostart when the software starts
        /// </summary>
        public bool isWidgetAutostartEnabled { get; set; }

        /// <summary>
        /// Widgets that weren't closed by the user since the last session ended
        /// </summary>
        public List<WidgetConfiguration> lastSessionWidgets { get; set; }

        /// <summary>
        /// Additional widget configurations
        /// </summary>
        public string settings { get; set; }
    }
}

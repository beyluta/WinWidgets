using Components;
using Models;
using System;
using System.Drawing;
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
                RemoveFromSession(widget.htmlPath);
                AssetService.widgets.RemoveWidget(widget);
            }));
        }

        /// <summary>
        /// Adds or updates the session of current open widgets
        /// </summary>
        /// <param name="path">Path of the widget</param>
        /// <param name="position">Position of the widget</param>
        public void AddOrUpdateSession(string path, Point position)
        {
            Configuration configuration = AssetService.GetConfigurationFile();

            for (int i = 0; i < configuration.lastSessionWidgets.Count; i++)
            {
                if (configuration.lastSessionWidgets[i].path == path)
                {
                    configuration.lastSessionWidgets[i] = new WidgetConfiguration()
                    {
                        path = path,
                        position = position
                    };

                    AssetService.OverwriteConfigurationFile(configuration);
                    return;
                }
            }

            configuration.lastSessionWidgets.Add(new WidgetConfiguration
            {
                path = path,
                position = position
            });

            AssetService.OverwriteConfigurationFile(configuration);
        }

        /// <summary>
        /// Removes a widget from the current session
        /// </summary>
        /// <param name="path">path of the widget to remove</param>
        public void RemoveFromSession(string path)
        {
            Configuration configuration = AssetService.GetConfigurationFile();

            for (int i = 0; i < configuration.lastSessionWidgets.Count; i++)
            {
                if (configuration.lastSessionWidgets[i].path == path)
                {
                    configuration.lastSessionWidgets.RemoveAt(i);
                    AssetService.OverwriteConfigurationFile(configuration);
                    return;
                }
            }
        }
    }
}

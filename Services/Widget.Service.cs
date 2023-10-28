using CefSharp;
using Components;
using Models;
using System;
using System.Collections;
using System.Drawing;
using System.IO;
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
                AddOrUpdateSession(widget.htmlPath, widget.window.Location, widget.window.TopMost);
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
        /// Closes all opened widgets
        /// </summary>
        public void CloseAllWidgets(bool removeFromCurrentSession)
        {
            ArrayList deleteWidgets = new ArrayList();

            for (int i = 0; i < AssetService.widgets.Widgets.Count; i++)
            {
                ((WidgetComponent)AssetService.widgets.Widgets[i]).window.Invoke(new MethodInvoker(delegate ()
                {
                    ((WidgetComponent)AssetService.widgets.Widgets[i]).window.Close();
                    deleteWidgets.Add(((WidgetComponent)AssetService.widgets.Widgets[i]));
                }));
            }

            for (int i = 0; i < deleteWidgets.Count; i++)
            {
                AssetService.widgets.RemoveWidget((WidgetComponent)deleteWidgets[i]);

                if (removeFromCurrentSession)
                {
                    RemoveFromSession(((WidgetComponent)deleteWidgets[i]).htmlPath);
                }
            }
        }

        /// <summary>
        /// Adds or updates the session of current open widgets
        /// </summary>
        /// <param name="path">Path of the widget</param>
        /// <param name="position">Position of the widget</param>
        /// <param name="alwaysOnTop">Whether the widget is "Always on top"</param>
        public void AddOrUpdateSession(string path, Point position, bool alwaysOnTop)
        {
            Configuration configuration = AssetService.GetConfigurationFile();

            for (int i = 0; i < configuration.lastSessionWidgets.Count; i++)
            {
                if (configuration.lastSessionWidgets[i].path == path)
                {
                    configuration.lastSessionWidgets[i] = new WidgetConfiguration()
                    {
                        path = path,
                        position = position,
                        alwaysOnTop = alwaysOnTop
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

        /// <summary>
        /// Injects Javascript into the browser of the widget
        /// </summary>
        /// <param name="widget">Widget component to inject the javascript into</param>
        /// <param name="javascript">The code snippet to be injected</param>
        public void InjectJavascript(WidgetComponent widget, string javascript, bool executeOnlyWhenPageLoads = false)
        {
            try
            {
                if (!executeOnlyWhenPageLoads)
                {
                    widget.browser.ExecuteScriptAsync(javascript);
                } else
                {
                    widget.browser.ExecuteScriptAsyncWhenPageLoaded(javascript);
                }
            }
            catch { }
        }

        /// <summary>
        /// Gets the configuration of a widget if the file exists
        /// </summary>
        /// <param name="widget">Widget to retrieve the configuration from</param>
        /// <returns>Then configuration of the widget</returns>
        public Configuration GetConfiguration(WidgetComponent widget)
        {
            try
            {
                string path = Path.Combine(Path.GetDirectoryName(widget.htmlPath), "config.json");


                if (File.Exists(path))
                {
                    string data = File.ReadAllText(path);
                    return new Configuration() { settings = data };
                }

                return new Configuration();
            } catch
            {
                return new Configuration();
            }
        }

        /// <summary>
        /// Sets or creates the configuration file for a specific wiget.
        /// </summary>
        /// <param name="widget">Widget that owns the configuration file</param>
        /// <param name="data">Data to set the config file</param>
        public void SetConfiguration(WidgetComponent widget, string data)
        {
            try
            {
                string path = Path.Combine(Path.GetDirectoryName(widget.htmlPath), "config.json");

                if (File.Exists(path))
                {
                    File.WriteAllText(path, data);
                    return;
                }

                File.WriteAllText(path, data);
            }
            catch { }
        }
    }
}

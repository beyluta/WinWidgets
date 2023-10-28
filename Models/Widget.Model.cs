using CefSharp.WinForms;
using Modules;
using System;
using System.Drawing;
using System.Windows.Forms;

namespace Models
{
    abstract internal class WidgetModel : WindowModel
    {
        /// <summary>
        /// Complete path to the widget
        /// </summary>
        abstract public string htmlPath { get; set; }

        /// <summary>
        /// Reference to the main window
        /// </summary>
        abstract public WidgetForm window { get; set; }

        /// <summary>
        /// Reference to the browser control
        /// </summary>
        abstract public ChromiumWebBrowser browser { get; set; }

        /// <summary>
        /// Reference to the handle of the window
        /// </summary>
        abstract public IntPtr handle { get; set; }

        /// <summary>
        /// Configuration of the widget
        /// </summary>
        abstract public Configuration configuration { get; set; }

        /// <summary>
        /// Creates a window
        /// </summary>
        /// <param name="width">Width of the window</param>
        /// <param name="height">Height of the window</param>
        /// <param name="title">Title of the window</param>
        /// <param name="save">Should the widget be saved immediately upon creation to the config.js file</param>
        /// <param name="position">Position of the window</param>
        abstract public void CreateWindow(int width, int height, string title, bool save, Point position = default(Point), bool? alwaysOnTop = null);

        /// <summary>
        /// Appends the widget control to the window
        /// </summary>
        /// <param name="window">Reference to the parent window</param>
        /// <param name="path">Path to the widget to be appended</param>
        abstract public void AppendWidget(Form window, string path);
    }
}

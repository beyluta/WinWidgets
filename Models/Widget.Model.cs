using CefSharp.WinForms;
using Models;
using System;
using System.Windows.Forms;

namespace WidgetsDotNet.Models
{
    abstract internal class WidgetModel : WindowModel
    {
        /// <summary>
        /// Reference to the main window
        /// </summary>
        abstract public Form window { get; set; }

        /// <summary>
        /// Reference to the browser control
        /// </summary>
        abstract public ChromiumWebBrowser browser { get; set; }

        /// <summary>
        /// Reference to the handle of the window
        /// </summary>
        abstract public IntPtr handle { get; set; }

        /// <summary>
        /// Creates a window
        /// </summary>
        /// <param name="width">Width of the window</param>
        /// <param name="height">Height of the window</param>
        /// <param name="title">Title of the window</param>
        /// <param name="startPosition">Start position of the window</param>
        abstract public void CreateWindow(int width, int height, string title, FormStartPosition startPosition);

        /// <summary>
        /// Appends the widget control to the window
        /// </summary>
        /// <param name="window">Reference to the parent window</param>
        /// <param name="path"></param>
        abstract public void AppendWidget(Form window, string path);
    }
}

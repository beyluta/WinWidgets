using CefSharp.WinForms;
using System;
using System.Windows.Forms;

namespace Models
{
    abstract class WidgetModel : WindowModel
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

        /// <summary>
        /// Opens the widget or its options
        /// </summary>
        /// <param name="id">id of the widget to open</param>
        abstract public void OpenWidget(int id);

        /// <summary>
        /// Sets the transparency of a window by its handle
        /// </summary>
        /// <param name="handle">Handle of the window</param>
        /// <param name="alpha">Opacity of the window from 0 to 255</param>
        public void SetWindowTransparency(IntPtr handle, byte alpha)
        {
            SetWindowLong(handle, GWL_EXSTYLE, GetWindowLong(handle, GWL_EXSTYLE) | WS_EX_LAYERED);
            SetLayeredWindowAttributes(handle, 0, alpha, LWA_ALPHA);
        }

        /// <summary>
        /// Searches for the value of a meta tag by name
        /// </summary>
        /// <param name="name">Name of the tag</param>
        /// <param name="widgetPath">Path to the file</param>
        /// <returns>The value inside the content attribute</returns>
        public string GetMetaTagValue(string name, string widgetPath)
        {
            try
            {
                var doc = new HtmlAgilityPack.HtmlDocument();
                doc.Load(widgetPath);
                return doc.DocumentNode.SelectSingleNode("//meta[@name='" + name + "']")?.GetAttributeValue("content", null);
            }
            catch { return null; }
        }
    }
}

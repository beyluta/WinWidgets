using CefSharp.WinForms;
using System;
using System.IO;
using System.Windows.Forms;

namespace Widgets
{
    abstract class WidgetWindow : WindowEssentials
    {
        abstract public Form window { get; set; }
        abstract public ChromiumWebBrowser browser { get; set; }
        abstract public IntPtr handle { get; set; }
        abstract public void CreateWindow(int w, int h, string t, FormStartPosition p);
        abstract public void AppendWidget(Form f, string path);
        abstract public void OpenWidget(int id);
        abstract public void SetWindowTransparency(IntPtr handle, byte alpha);

        public string GetMetaTagValue(string name, string widgetPath)
        {
            string[] html = File.ReadAllLines(widgetPath);
            for (int i = 0; i < html.Length; i++)
            {
                if (html[i].Contains("meta") && html[i].Contains(name) && !html[i].Contains("<!--"))
                {
                    return html[i].Split('"')[3];
                }
            }
            return null;
        }
    }
}

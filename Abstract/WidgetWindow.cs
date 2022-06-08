using CefSharp.WinForms;
using System;
using System.Windows.Forms;

namespace Widgets
{
    abstract class WidgetWindow : User32
    {
        abstract public Form window { get; set; }
        abstract public ChromiumWebBrowser browser { get; set; }
        abstract public IntPtr handle { get; set; }
        abstract public void CreateWindow(int w, int h, string t, FormStartPosition p);
        abstract public void AppendWidget(Form f, string path);
        abstract public void OpenWidget(int id);
        abstract public string GetMetaTagValue(string name);
        abstract public void SetWindowTransparency(IntPtr handle, byte alpha);
    }
}

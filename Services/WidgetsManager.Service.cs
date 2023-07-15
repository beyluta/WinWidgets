using CefSharp.WinForms;
using Models;
using System;
using System.Drawing;
using System.Windows.Forms;
using WidgetsDotNet.Properties;

namespace Services
{
    internal class WidgetsManagerService : WidgetModel
    {
        public override Form window { get => throw new NotImplementedException(); set => throw new NotImplementedException(); }
        public override ChromiumWebBrowser browser { get => throw new NotImplementedException(); set => throw new NotImplementedException(); }
        public override IntPtr handle { get => throw new NotImplementedException(); set => throw new NotImplementedException(); }

        public override void AppendWidget(Form window, string path)
        {
            throw new NotImplementedException();
        }

        public override void CreateWindow(int width, int height, string title, FormStartPosition startPosition)
        {
            window = new Form();
            window.Size = new Size(width, height);
            window.StartPosition = startPosition;
            window.Text = title;
            window.Activated += delegate { handle = window.Handle; };
            window.Icon = Resources.favicon;
            window.ShowInTaskbar = false;
            window.Show();
        }

        public override void OpenWidget(int id)
        {
            throw new NotImplementedException();
        }
    }
}

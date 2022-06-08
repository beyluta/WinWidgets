using CefSharp.WinForms;
using System;
using System.Drawing;
using System.IO;
using System.Windows.Forms;

namespace Widgets
{
    class Widget : WidgetWindow
    {
        private int id;

        public Widget(int id)
        {
            this.id = id;
        }

        public Widget() { }

        IntPtr _handle;

        public override IntPtr handle
        {
            get { return _handle; }
            set { _handle = value; }
        }

        private string _widgetPath;

        public string widgetPath 
        { 
            get { return _widgetPath; }
            set { _widgetPath = value; }
        }

        private Form _window;

        public override Form window
        {
            get { return _window; }
            set { _window = value; }
        }

        private ChromiumWebBrowser _browser;

        public override ChromiumWebBrowser browser
        {
            get { return _browser; }
            set { _browser = value; }
        }

        public override void AppendWidget(Form f, string path)
        {
            browser = new ChromiumWebBrowser(path);
            f.Controls.Add(browser);
        }

        public override void CreateWindow(int w, int h, string t, FormStartPosition p)
        {
            string sizeString = GetMetaTagValue("windowSize");
            string radiusString = GetMetaTagValue("windowBorderRadius");
            string locationString = GetMetaTagValue("windowLocation");
            string topMostString = GetMetaTagValue("topMost");
            int roundess = radiusString != null ? int.Parse(radiusString) : 0;
            int metaWidth = sizeString != null ? int.Parse(sizeString.Split(' ')[0]) : w;
            int metaHeight = sizeString != null ? int.Parse(sizeString.Split(' ')[1]) : h;
            int locationX = locationString != null ? int.Parse(locationString.Split(' ')[0]) : 0;
            int locationY = locationString != null ? int.Parse(locationString.Split(' ')[1]) : 0;
            bool topMost = topMostString != null ? bool.Parse(topMostString.Split(' ')[0]) : false;

            window = new Form();
            window.Size = new Size(metaWidth, metaHeight);
            window.StartPosition = p;
            window.Location = new Point(locationX, locationY);
            window.Text = t;
            window.TopMost = topMost;
            window.FormBorderStyle = FormBorderStyle.None;
            window.ShowInTaskbar = false;
            window.Region = System.Drawing.Region.FromHrgn(CreateRoundRectRgn(0, 0, metaWidth, metaHeight, roundess, roundess));
            window.Activated += OnFormActivated;
            AppendWidget(window, widgetPath);
            window.ShowDialog();
        }

        private void OnFormActivated(object sender, EventArgs e)
        {
            handle = window.Handle;
        }

        public override string GetMetaTagValue(string name)
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

        public override void OpenWidget(int id)
        {
            throw new System.NotImplementedException();
        }

        public override void SetWindowTransparency(IntPtr handle, byte alpha)
        {
            SetWindowLong(handle, GWL_EXSTYLE, GetWindowLong(handle, GWL_EXSTYLE) | WS_EX_LAYERED);
            SetLayeredWindowAttributes(handle, 0, alpha, LWA_ALPHA);
        }
    }
}

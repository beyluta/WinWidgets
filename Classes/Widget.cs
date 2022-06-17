using CefSharp;
using CefSharp.WinForms;
using System;
using System.Drawing;
using System.Windows.Forms;

namespace Widgets
{
    class Widget : WidgetWindow
    {
        public bool moveModeEnabled = false;

        private IntPtr _handle;
        private string _widgetPath;
        private Form _window;
        private ChromiumWebBrowser _browser;
        private int width;
        private int height;

        public override IntPtr handle
        {
            get { return _handle; }
            set { _handle = value; }
        }

        public string widgetPath 
        { 
            get { return _widgetPath; }
            set { _widgetPath = value; }
        }

        public override Form window
        {
            get { return _window; }
            set { _window = value; }
        }

        public override ChromiumWebBrowser browser
        {
            get { return _browser; }
            set { _browser = value; }
        }

        public override void AppendWidget(Form f, string path)
        {
            browser = new ChromiumWebBrowser(path);
            browser.IsBrowserInitializedChanged += OnBrowserInitialized;
            f.Controls.Add(browser);
        }

        public override void CreateWindow(int w, int h, string t, FormStartPosition p)
        {
            POINT mousePos;
            GetCursorPos(out mousePos);

            string sizeString = GetMetaTagValue("windowSize", widgetPath);
            string radiusString = GetMetaTagValue("windowBorderRadius", widgetPath);
            string locationString = GetMetaTagValue("windowLocation", widgetPath);
            string topMostString = GetMetaTagValue("topMost", widgetPath);
            string opacityString = GetMetaTagValue("windowOpacity", widgetPath);
            int roundess = radiusString != null ? int.Parse(radiusString) : 0;
            width = sizeString != null ? int.Parse(sizeString.Split(' ')[0]) : w;
            height = sizeString != null ? int.Parse(sizeString.Split(' ')[1]) : h;
            int locationX = locationString != null ? int.Parse(locationString.Split(' ')[0]) : mousePos.X;
            int locationY = locationString != null ? int.Parse(locationString.Split(' ')[1]) : mousePos.Y;
            byte opacity = (byte)(opacityString != null ? byte.Parse(opacityString.Split(' ')[0]) : 255);
            bool topMost = topMostString != null ? bool.Parse(topMostString.Split(' ')[0]) : false;

            window = new Form();
            window.Size = new Size(width, height);
            window.StartPosition = p;
            window.Location = new Point(locationX, locationY);
            window.Text = t;
            window.TopMost = topMost;
            window.FormBorderStyle = FormBorderStyle.None;
            window.ShowInTaskbar = false;
            window.Region = System.Drawing.Region.FromHrgn(CreateRoundRectRgn(0, 0, width, height, roundess, roundess));
            window.Activated += OnFormActivated;
            SetWindowTransparency(window.Handle, opacity);
            AppendWidget(window, widgetPath);
            window.ShowDialog();
        }

        private void OnBrowserInitialized(object sender, EventArgs e)
        {
            browser.ExecuteScriptAsync(@"
                window.onload = () => {
                    let mouseDrag;

                    document.body.onmousedown = (e) => {
                        mouseDrag = setInterval(() => {
                            e.buttons === 1 && CefSharp.PostMessage('mouseDrag');
                        }, 0);
                        CefSharp.PostMessage('onmousedown');
                    }

                    document.body.onmouseup = () => {
                         clearInterval(mouseDrag);
                         CefSharp.PostMessage('onmouseup');
                    }
                }
            ");

            browser.JavascriptMessageReceived += OnBrowserMessageReceived;
        }

        public override void OpenWidget(int id)
        {
            throw new System.NotImplementedException();
        }

        private void OnBrowserMessageReceived(object sender, JavascriptMessageReceivedEventArgs e)
        {
            switch (e.Message)
            {
                case "mouseDrag":
                    if (moveModeEnabled)
                    {
                        POINT pos;
                        GetCursorPos(out pos);
                        window.Invoke(new MethodInvoker(delegate ()
                        {
                            window.Location = new Point(pos.X - width / 2, pos.Y - height / 2);
                        }));
                    }
                    break;
            }
        }

        private void OnFormActivated(object sender, EventArgs e)
        {
            handle = window.Handle;
            browser.MenuHandler = new WidgetMenuHandler(this);
        }
    }
}

using CefSharp;
using CefSharp.WinForms;
using Services;
using System;
using System.Drawing;
using System.Threading;
using System.Windows.Forms;
using Models;

namespace Controllers
{
    internal class WidgetController : WidgetModel
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

        public override void AppendWidget(Form window, string path)
        {
            browser = new ChromiumWebBrowser(path);
            browser.IsBrowserInitializedChanged += OnBrowserInitialized;
            window.Controls.Add(browser);
        }

        public override void CreateWindow(int width, int height, string title, FormStartPosition startPosition)
        {
            new Thread(() =>
            {
                POINT mousePos;
                GetCursorPos(out mousePos);

                /*
                @@  Extracting the <meta> tags from the html document.
                */
                string sizeString = GetMetaTagValue("windowSize", widgetPath);
                string radiusString = GetMetaTagValue("windowBorderRadius", widgetPath);
                string locationString = GetMetaTagValue("windowLocation", widgetPath);
                string topMostString = GetMetaTagValue("topMost", widgetPath);
                string opacityString = GetMetaTagValue("windowOpacity", widgetPath);
                int roundess = radiusString != null ? int.Parse(radiusString) : 0;
                this.width = sizeString != null ? int.Parse(sizeString.Split(' ')[0]) : width;
                this.height = sizeString != null ? int.Parse(sizeString.Split(' ')[1]) : height;
                int locationX = locationString != null ? int.Parse(locationString.Split(' ')[0]) : mousePos.X;
                int locationY = locationString != null ? int.Parse(locationString.Split(' ')[1]) : mousePos.Y;
                byte opacity = (byte)(opacityString != null ? byte.Parse(opacityString.Split(' ')[0]) : 255);
                bool topMost = topMostString != null ? bool.Parse(topMostString.Split(' ')[0]) : false;

                window = new Form();
                window.Size = new Size(this.width, this.height);
                window.StartPosition = startPosition;
                window.Location = new Point(locationX, locationY);
                window.Text = title;
                window.TopMost = topMost;
                window.FormBorderStyle = FormBorderStyle.None;
                window.ShowInTaskbar = false;
                window.Region = System.Drawing.Region.FromHrgn(CreateRoundRectRgn(0, 0, this.width, this.height, roundess, roundess)); // Border radius
                window.Activated += OnFormActivated;
                window.BackColor = Color.Black;

                SetWindowTransparency(window.Handle, opacity);
                HideWindowFromProgramSwitcher(window.Handle);
                AppendWidget(window, widgetPath);
                window.ShowDialog();
            }).Start();
        }

        private void OnBrowserInitialized(object sender, EventArgs e)
        {
            /*
            @@  Injecting JavaScript code to tell when the window is being dragged.
            */
            browser.ExecuteScriptAsync(@"
                window.onload = () => {
                    let isDrag = false;

                    document.body.onmousedown = (e) => {
                        if (e.buttons === 1) {
                            isDrag = true;
                        }
                    }

                    document.body.onmouseup = () => {
                         isDrag = false;
                    }

                    document.body.onmousemove = () => {
                        if (isDrag) {
                            CefSharp.PostMessage('mouseDrag');
                        }
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
            browser.MenuHandler = new MenuHandlerController(this, new MenuHandlerService(), new WidgetService());
        }
    }
}
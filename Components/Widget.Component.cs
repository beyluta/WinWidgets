using CefSharp.WinForms;
using Services;
using System;
using System.Drawing;
using System.Threading;
using System.Timers;
using System.Windows.Forms;
using Modules;
using Models;

namespace Components
{
    internal class WidgetComponent : WidgetModel
    {
        public bool moveModeEnabled = false;

        private IntPtr _handle;
        private string _widgetPath;
        private WidgetForm _window;
        private ChromiumWebBrowser _browser;
        private int width;
        private int height;
        private HTMLDocService htmlDocService = new HTMLDocService();
        private WindowService windowService = new WindowService();
        private WidgetService widgetService = new WidgetService();
        private TimerService timerService = new TimerService();

        public override IntPtr handle
        {
            get { return _handle; }
            set { _handle = value; }
        }

        public override string htmlPath
        {
            get { return _widgetPath; }
            set { _widgetPath = value; }
        }

        public override WidgetForm window
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

        public override void CreateWindow(int width, int height, string title, bool save, Point position = default(Point))
        {
            new Thread(() =>
            {
                POINT mousePos;
                GetCursorPos(out mousePos);

                string sizeString = this.htmlDocService.GetMetaTagValue("windowSize", htmlPath);
                string radiusString = this.htmlDocService.GetMetaTagValue("windowBorderRadius", htmlPath);
                string locationString = this.htmlDocService.GetMetaTagValue("windowLocation", htmlPath);
                string topMostString = this.htmlDocService.GetMetaTagValue("topMost", htmlPath);
                string opacityString = this.htmlDocService.GetMetaTagValue("windowOpacity", htmlPath);
                int roundess = radiusString != null ? int.Parse(radiusString) : 0;
                this.width = sizeString != null ? int.Parse(sizeString.Split(' ')[0]) : width;
                this.height = sizeString != null ? int.Parse(sizeString.Split(' ')[1]) : height;
                int locationX = locationString != null ? int.Parse(locationString.Split(' ')[0]) : mousePos.X;
                int locationY = locationString != null ? int.Parse(locationString.Split(' ')[1]) : mousePos.Y;
                byte opacity = (byte)(opacityString != null ? byte.Parse(opacityString.Split(' ')[0]) : 255);
                bool topMost = topMostString != null ? bool.Parse(topMostString.Split(' ')[0]) : false;

                window = new WidgetForm();
                window.Size = new Size(this.width, this.height);
                window.StartPosition = FormStartPosition.Manual;
                window.Location = locationString == null ? new Point(position.X, position.Y) : new Point(locationX, locationY);
                window.Text = title;
                window.TopMost = topMost;
                window.FormBorderStyle = FormBorderStyle.None;
                window.ShowInTaskbar = false;
                window.Region = System.Drawing.Region.FromHrgn(CreateRoundRectRgn(0, 0, this.width, this.height, roundess, roundess)); // Border radius
                window.Activated += OnFormActivated;
                window.BackColor = Color.Black;

                this.windowService.SetWindowTransparency(window.Handle, opacity);
                this.windowService.HideWindowFromProgramSwitcher(window.Handle);

                if (save)
                {
                    this.widgetService.AddOrUpdateSession(htmlPath, new Point(locationX, locationY));
                    AssetService.OverwriteConfigurationFile(AssetService.GetConfigurationFile());
                }

                AppendWidget(window, htmlPath);
                window.ShowDialog();
            }).Start();
        }

        private void OnBrowserInitialized(object sender, EventArgs e)
        {
            this.timerService.CreateTimer(1, OnBrowserUpdateTick, true, true);
        }

        private void OnBrowserUpdateTick(object sender, ElapsedEventArgs e)
        {
            if (this.moveModeEnabled)
            {
                POINT pos;
                GetCursorPos(out pos);

                window.Invoke(new MethodInvoker(delegate ()
                {
                    window.Location = new Point(pos.X - width / 2, pos.Y - height / 2);
                    this.widgetService.AddOrUpdateSession(this.htmlPath, window.Location);
                }));
            }
        }

        private void OnFormActivated(object sender, EventArgs e)
        {
            handle = window.Handle;
            browser.MenuHandler = new MenuHandlerComponent(this);
        }
    }
}
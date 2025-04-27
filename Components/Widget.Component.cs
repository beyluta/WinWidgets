using CefSharp;
using CefSharp.WinForms;
using Microsoft.Win32;
using Models;
using Modules;
using Newtonsoft.Json;
using Services;
using System;
using System.Drawing;
using System.Threading;
using System.Timers;
using System.Windows.Forms;

namespace Components
{
    internal class WidgetComponent : WidgetModel
    {
        public bool moveModeEnabled = false;

        private IntPtr _handle;
        private string _widgetPath;
        private WidgetForm _window;
        private ChromiumWebBrowser _browser;
        private Configuration _configuration;
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

        public override Configuration configuration
        {
            get { return _configuration; }
            set { _configuration = value; }
        }

        public override void AppendWidget(Form window, string path)
        {
            browser = new ChromiumWebBrowser(path);
            browser.IsBrowserInitializedChanged += OnBrowserInitialized;
            window.Controls.Add(browser);
        }

        public override void CreateWindow(int width, int height, string title, bool save, Point position = default(Point), bool? alwaysOnTop = null)
        {
            new Thread(() =>
            {
                POINT mousePos;
                GetCursorPos(out mousePos);

                double scale = Int32.Parse((string)Registry.GetValue(@"HKEY_CURRENT_USER\Software\Microsoft\Windows\CurrentVersion\ThemeManager", "LastLoadedDPI", "96")) / 96.0;

                WidgetHtmlTags tags = GetWidgetHtmlTags(htmlPath);
                int roundess = tags.Radius != null ? (int)(int.Parse(tags.Radius) * scale) : 0;
                this.width = tags.Size != null ? (int)(int.Parse(tags.Size.Split(' ')[0]) * scale) : (int)(width * scale);
                this.height = tags.Size != null ? (int)(int.Parse(tags.Size.Split(' ')[1]) * scale) : (int)(height * scale);
                int locationX = tags.Location != null ? int.Parse(tags.Location.Split(' ')[0]) : mousePos.X;
                int locationY = tags.Location != null ? int.Parse(tags.Location.Split(' ')[1]) : mousePos.Y;
                byte opacity = (byte)(tags.Opacity != null ? byte.Parse(tags.Opacity.Split(' ')[0]) : 255);
                bool dropShadow = tags.DropShadow != null ? bool.Parse(tags.DropShadow.Split(' ')[0]) : false;
                bool topMost = tags.TopMost != null ? bool.Parse(tags.TopMost.Split(' ')[0]) : false;
                topMost = alwaysOnTop.HasValue ? (bool)alwaysOnTop : topMost;

                window = new WidgetForm(dropShadow);
                window.Size = new Size(this.width, this.height);
                window.StartPosition = FormStartPosition.Manual;
                window.Location = tags.Location == null ? new Point(position.X, position.Y) : new Point(locationX, locationY);
                window.Text = title;
                window.FormBorderStyle = FormBorderStyle.None;
                window.ShowInTaskbar = false;
                window.Region = System.Drawing.Region.FromHrgn(CreateRoundRectRgn(0, 0, this.width, this.height, roundess, roundess)); // Border radius
                window.Activated += OnFormActivated;
                window.BackColor = Color.Black;

                this.windowService.SetWindowTransparency(window.Handle, opacity);
                this.windowService.HideWindowFromProgramSwitcher(window.Handle);
                this.configuration = this.widgetService.GetConfiguration(this);

                if (save)
                {
                    this.widgetService.AddOrUpdateSession(htmlPath, new Point(locationX, locationY), topMost);
                    AssetService.OverwriteConfigurationFile(AssetService.GetConfigurationFile());
                }

                AppendWidget(window, htmlPath);
                window.TopMost = topMost;
                window.ShowDialog();
            }).Start();
        }

        private WidgetHtmlTags GetWidgetHtmlTags(string htmlPath)
        {
            return new WidgetHtmlTags
            {
                Size = this.htmlDocService.GetMetaTagValue("windowSize", htmlPath),
                Radius = this.htmlDocService.GetMetaTagValue("windowBorderRadius", htmlPath),
                Location = this.htmlDocService.GetMetaTagValue("windowLocation", htmlPath),
                TopMost = this.htmlDocService.GetMetaTagValue("topMost", htmlPath),
                Opacity = this.htmlDocService.GetMetaTagValue("windowOpacity", htmlPath),
                DropShadow = this.htmlDocService.GetMetaTagValue("windowDropShadow", htmlPath)
            };
        }

        private void OnBrowserInitialized(object sender, EventArgs e)
        {
            this.timerService.CreateTimer(1, OnBrowserUpdateTick, true, true);
            this.widgetService.InjectJavascript(
                this, 
                $"if (typeof onGetConfiguration === 'function') onGetConfiguration({JsonConvert.SerializeObject(configuration.settings)});",
                true
            );
            this.browser.JavascriptMessageReceived += OnBrowserMessageReceived;
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
                    this.widgetService.AddOrUpdateSession(this.htmlPath, window.Location, window.TopMost);
                }));
            }
        }

        private void OnFormActivated(object sender, EventArgs e)
        {
            handle = window.Handle;
            if (browser != null)
            {
                browser.MenuHandler = new MenuHandlerComponent(this);
            }
        }

        private void OnBrowserMessageReceived(object sender, JavascriptMessageReceivedEventArgs e)
        {
            this.widgetService.SetConfiguration(this, e.Message.ToString());
        }
    }
}
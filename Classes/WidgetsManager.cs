using CefSharp;
using CefSharp.WinForms;
using System;
using System.Diagnostics;
using System.Drawing;
using System.IO;
using System.Windows.Forms;
using WidgetsDotNet.Properties;

namespace Widgets.Manager
{
    class WidgetsManager : WidgetWindow
    {
        public WidgetsManager()
        {
            CefSettings options = new CefSettings();
            Cef.Initialize(options);
            FilesManager.CreateHTMLFilesDirectory();
            CreateWindow(600, 500, "Widget Manager", FormStartPosition.CenterScreen);
        }

        private Form _window;

        public override Form window
        {
            get { return _window; }
            set { _window = value; }
        }

        private string managerUIPath = FilesManager.assetsPath + "/index.html";

        public override void CreateWindow(int w, int h, string t, FormStartPosition p)
        {
            window = new Form();
            window.Size = new Size(w, h);
            window.StartPosition = p;
            window.Text = t;
            window.FormBorderStyle = FormBorderStyle.None;
            window.Region = System.Drawing.Region.FromHrgn(CreateRoundRectRgn(0, 0, w, h, 15, 15));
            window.Activated += OnFormActivated;
            window.Icon = Resources.favicon;
            AppendWidget(window, managerUIPath);
            window.ShowDialog();
        }

        private void OnFormActivated(object sender, EventArgs e)
        {
            handle = window.Handle;
        }

        private ChromiumWebBrowser _browser;

        public override ChromiumWebBrowser browser
        {
            get { return _browser; }
            set { _browser = value; }
        }

        IntPtr _handle;

        public override IntPtr handle 
        { 
            get { return _handle; } 
            set { _handle = value; } 
        }

        public override void AppendWidget(Form f, string path)
        {
            browser = new ChromiumWebBrowser(path);
            browser.JavascriptMessageReceived += OnBrowserMessageReceived;
            browser.IsBrowserInitializedChanged += OnBrowserInitialized;
            f.Controls.Add(browser);
        }

        private void OnBrowserInitialized(object sender, EventArgs e)
        {
            string injectHTML = string.Empty;
            string[] files = FilesManager.GetPathToHTMLFiles(FilesManager.widgetsPath);
            for (int i = 0; i < files.Length; i++)
            {
                widgetPath = files[i];
                injectHTML += $@"
                    const e{i} = document.createElement('div');
                    e{i}.classList.add('widget');
                    e{i}.innerText = '{GetMetaTagValue("applicationTitle")}';
                    document.getElementById('widgets').appendChild(e{i});
                    e{i}.onclick = () => CefSharp.PostMessage('{i}');
                ";
            }
            browser.ExecuteScriptAsync($@"document.getElementById('folder').onclick = () => CefSharp.PostMessage('widgetsFolder')");
            browser.ExecuteScriptAsync($@"document.getElementById('close').onclick = () => CefSharp.PostMessage('closeApplication')");
            browser.ExecuteScriptAsync($@"document.getElementById('minimize').onclick = () => CefSharp.PostMessage('minimizeApplication')");
            browser.ExecuteScriptAsync(injectHTML);
        }

        private void OnBrowserMessageReceived(object sender, JavascriptMessageReceivedEventArgs e)
        {
            switch (e.Message)
            {
                case "widgetsFolder":
                    Process.Start(FilesManager.widgetsPath);
                    break;

                case "closeApplication":
                    Application.Exit();
                    break;

                case "minimizeApplication":
                    ShowWindow(handle, SW_MINIMIZE);
                    break;

                default:
                    OpenWidget(int.Parse((string)e.Message));
                    break;
            }
        }

        WidgetMap widgets = new WidgetMap(100);

        public override void OpenWidget(int id)
        {
            if (widgets.HasSpaceLeft())
            {
                Widget widget = new Widget(id);
                widgets.AddWidget(widget);
                widget.widgetPath = FilesManager.GetPathToHTMLFiles(FilesManager.widgetsPath)[id];
                widget.CreateWindow(300, 300, $"Widget{id}", FormStartPosition.Manual);
            }
        }

        string widgetPath = String.Empty;

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

        private void AutoExecuteWidgets()
        {
            string[] files = FilesManager.GetPathToHTMLFiles(FilesManager.widgetsPath);

            for (int i = 0; i < files.Length; i++)
            {
                widgetPath = files[i];

                if (GetMetaTagValue("autoExec") == "true")
                {
                    OpenWidget(i);
                }
            }
        }

        public override void SetWindowTransparency(IntPtr handle, byte alpha)
        {
            SetWindowLong(handle, GWL_EXSTYLE, GetWindowLong(handle, GWL_EXSTYLE) | WS_EX_LAYERED);
            SetLayeredWindowAttributes(handle, 0, alpha, LWA_ALPHA);
        }
    }
}

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
        private string widgetPath = String.Empty;
        private WidgetMap widgets = new WidgetMap(100);
        private Form _window;
        private ChromiumWebBrowser _browser;
        private IntPtr _handle;
        private string managerUIPath = FilesManager.assetsPath + "/index.html";

        public WidgetsManager()
        {
            CefSettings options = new CefSettings();
            Cef.Initialize(options);
            FilesManager.CreateHTMLFilesDirectory();
            CreateWindow(1000, 800, "Widget Manager", FormStartPosition.CenterScreen);
        }

        public override Form window
        {
            get { return _window; }
            set { _window = value; }
        }

        public override void CreateWindow(int w, int h, string t, FormStartPosition p)
        {
            window = new Form();
            window.Size = new Size(w, h);
            window.StartPosition = p;
            window.Text = t;
            window.Activated += OnFormActivated;
            window.Icon = Resources.favicon;
            AppendWidget(window, managerUIPath);
            window.ShowDialog();
        }

        private void OnFormActivated(object sender, EventArgs e)
        {
            handle = window.Handle;
        }

        public override ChromiumWebBrowser browser
        {
            get { return _browser; }
            set { _browser = value; }
        }

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
                injectHTML +=  "window.onload = function() {" + $@"
                    const e{i} = document.createElement('div');
                    e{i}.classList.add('widget');
                    e{i}.classList.add('flex-row');
                    e{i}.innerHTML = '<p>{GetMetaTagValue("applicationTitle", files[i])}</p>';
                    document.getElementById('widgets').appendChild(e{i});
                    e{i}.onclick = () => CefSharp.PostMessage('{i}');
                    document.getElementById('folder').onclick = () => CefSharp.PostMessage('widgetsFolder');
                " + "}";
            }
            browser.ExecuteScriptAsync(injectHTML);
        }

        private void OnBrowserMessageReceived(object sender, JavascriptMessageReceivedEventArgs e)
        {
            switch (e.Message)
            {
                case "widgetsFolder":
                    Process.Start(FilesManager.widgetsPath);
                    break;

                default:
                    OpenWidget(int.Parse((string)e.Message));
                    break;
            }
        }

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
    }
}

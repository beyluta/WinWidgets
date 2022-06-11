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
    internal class WidgetsManager : WidgetWindow
    {
        private string widgetPath = String.Empty;
        private WidgetMap widgets = new WidgetMap(100);
        private Form _window;
        private ChromiumWebBrowser _browser;
        private IntPtr _handle;
        private string managerUIPath = FilesManager.assetsPath + "/index.html";
        private bool widgetsInitialized = false;
        private int widgetIndex = 0;

        public WidgetsManager()
        {
            CefSettings options = new CefSettings();
            options.CefCommandLineArgs.Add("disable-web-security");
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
            FileSystemWatcher fileWatcher = new FileSystemWatcher(FilesManager.widgetsPath);
            fileWatcher.NotifyFilter = NotifyFilters.Attributes
                                 | NotifyFilters.CreationTime
                                 | NotifyFilters.DirectoryName
                                 | NotifyFilters.FileName
                                 | NotifyFilters.LastAccess
                                 | NotifyFilters.LastWrite
                                 | NotifyFilters.Security
                                 | NotifyFilters.Size;

            fileWatcher.Changed += OnDirectoryChanged;
            fileWatcher.Created += OnFileCreatedInDirectory;
            fileWatcher.Deleted += OnFileDeletedInDirectory;
            fileWatcher.Renamed += OnFileRenamedInDirectory;

            fileWatcher.Filter = "*.html";
            fileWatcher.IncludeSubdirectories = true;
            fileWatcher.EnableRaisingEvents = true;

            ReloadWidgets();
        }

        private void ReloadWidgets()
        {
            string injectHTML = string.Empty;
            string[] files = FilesManager.GetPathToHTMLFiles(FilesManager.widgetsPath);

            for (int i = 0; i < files.Length; i++)
            {
                widgetPath = files[i];
                string localWidgetPath = string.Empty;

                for (int j = 0; j < widgetPath.Length; j++)
                {
                    if (widgetPath[j] == '\\')
                    {
                        localWidgetPath += '/';
                    }
                    else
                    {
                        localWidgetPath += widgetPath[j];
                    }
                }

                if (!widgetsInitialized)
                {
                        injectHTML += "window.addEventListener('load', (event) => {" + $@"
                        const e = document.createElement('div');
                        e.classList.add('widget');
                        e.classList.add('flex-row');
                        e.innerHTML = `<p>{GetMetaTagValue("applicationTitle", widgetPath)}</p> <iframe src='file:///{localWidgetPath}'></iframe>`;
                        document.getElementById('widgets').appendChild(e);
                        e.onclick = () => CefSharp.PostMessage('{i}');
                        document.getElementById('folder').onclick = () => CefSharp.PostMessage('widgetsFolder');
                    " + "});";
                } else
                {
                    widgetIndex++;

                    if (i <= 0)
                    {
                        injectHTML += "document.getElementById('widgets').innerHTML = '';";
                    }

                    injectHTML += $@"
                        const e{widgetIndex} = document.createElement('div');
                        e{widgetIndex}.classList.add('widget');
                        e{widgetIndex}.classList.add('flex-row');
                        e{widgetIndex}.innerHTML = `<p>{GetMetaTagValue("applicationTitle", widgetPath)}</p> <iframe src='file:///{localWidgetPath}'></iframe>`;
                        document.getElementById('widgets').appendChild(e{widgetIndex});
                        e{widgetIndex}.onclick = () => CefSharp.PostMessage('{i}');
                        document.getElementById('folder').onclick = () => CefSharp.PostMessage('widgetsFolder');";
                }
            }

            widgetsInitialized = true;
            browser.ExecuteScriptAsync(injectHTML);
        }

        private void OnFileRenamedInDirectory(object sender, RenamedEventArgs e)
        {
            ReloadWidgets();
        }

        private void OnFileDeletedInDirectory(object sender, FileSystemEventArgs e)
        {
            ReloadWidgets();
        }

        private void OnFileCreatedInDirectory(object sender, FileSystemEventArgs e)
        {
            ReloadWidgets();
        }

        private void OnDirectoryChanged(object sender, FileSystemEventArgs e)
        {
            ReloadWidgets();
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
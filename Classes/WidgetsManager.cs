using CefSharp;
using CefSharp.WinForms;
using Microsoft.Win32;
using Newtonsoft.Json;
using Newtonsoft.Json.Linq;
using System;
using System.Diagnostics;
using System.Drawing;
using System.IO;
using System.Net.Http;
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
        private RegistryKey registryKey = Registry.CurrentUser.OpenSubKey("SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Run", true);
        private NotifyIcon notifyIcon;
        private Configuration appConfig;
        private JObject versionObject;

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

        public override IntPtr handle
        {
            get { return _handle; }
            set { _handle = value; }
        }

        public WidgetsManager()
        {
            CefSettings options = new CefSettings();
            options.CefCommandLineArgs.Add("disable-web-security");
            Cef.Initialize(options);

            FilesManager.CreateHTMLFilesDirectory();

            string json = File.ReadAllText(FilesManager.assetsPath + "/config.json");
            appConfig = JsonConvert.DeserializeObject<Configuration>(json);

            CreateWindow(1000, 800, "WinWidgets", FormStartPosition.CenterScreen);
        }

        public override void CreateWindow(int w, int h, string t, FormStartPosition p)
        {
            window = new Form();
            window.Size = new Size(w, h);
            window.StartPosition = p;
            window.Text = t;
            window.Activated += OnFormActivated;
            window.Icon = Resources.favicon;
            window.Resize += OnFormReized;
            window.ShowInTaskbar = false;
            AppendWidget(window, managerUIPath);
            window.ShowDialog();
        }

        public override void AppendWidget(Form f, string path)
        {
            browser = new ChromiumWebBrowser(path);
            browser.JavascriptMessageReceived += OnBrowserMessageReceived;
            browser.IsBrowserInitializedChanged += OnBrowserInitialized;
            browser.MenuHandler = new WidgetManagerMenuHandler();
            f.Controls.Add(browser);

            notifyIcon = new NotifyIcon();
            notifyIcon.Icon = Resources.favicon;
            notifyIcon.Text = "WinWidgets";
            notifyIcon.Visible = true;
            notifyIcon.ContextMenu = new ContextMenu(new MenuItem[]
            { new MenuItem("Open Manager", OnOpenApplication),
              new MenuItem("Stop All Widgets", OnStopAllWidgets),
              new MenuItem("-"),
              new MenuItem("Quit", OnExitApplication)
            });
            notifyIcon.MouseDoubleClick += NotifyIconDoubleClick;
        }

        private async void ReloadWidgets()
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
                    HttpClient client = new HttpClient();
                    string response = client.GetStringAsync("https://7xdeveloper.com/api/AccessEndpoint.php?endpoint=getappconfigs&id=version").Result;
                    versionObject = JObject.Parse(response);

                    injectHTML += "window.addEventListener('load', (event) => {" + $@"
                        fetchedVersion = '{(string)versionObject["version"]}';
                        isUpToDate = {(appConfig.version == (string)versionObject["version"] ? "true" : "false")};
                        downloadUrl = '{(string)versionObject["downloadUrl"]}';
                        " + "var options = {weekday: 'long', year: 'numeric', month: 'long', day: 'numeric' };" + $@"
                        var today = new Date();                   
                        updateCheckTime = today.toLocaleDateString();
                        const e = document.createElement('div');
                        e.classList.add('widget');
                        e.classList.add('flex-row');
                        e.setAttribute('name', '{GetMetaTagValue("applicationTitle", widgetPath)}');
                        e.innerHTML = `<p>{GetMetaTagValue("applicationTitle", widgetPath)}</p> <iframe src='file:///{localWidgetPath}'></iframe>`;
                        document.getElementById('widgets').appendChild(e);
                        e.onclick = () => CefSharp.PostMessage('{i}');
                        document.getElementById('folder').onclick = () => CefSharp.PostMessage('widgetsFolder');" + @"

                        const switches = document.getElementsByClassName('switch');
                          for (let s of switches) {
                            const setting = s.getAttribute('setting');
                            if (setting == 'startup') {
                              " + $@"{(registryKey.GetValue("WinWidgets") != null ? "s.classList.add('switchon');" : "")}" + @"
                            }
                        }
                    " + "});";
                }
                else
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
                        e{widgetIndex}.setAttribute('name', '{GetMetaTagValue("applicationTitle", widgetPath)}');
                        document.getElementById('folder').onclick = () => CefSharp.PostMessage('widgetsFolder');";
                }
            }

            widgetsInitialized = true;
            browser.ExecuteScriptAsync(injectHTML);
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

        private void OnStopAllWidgets(object sender, EventArgs e)
        {
            foreach (Widget w in widgets.Widgets)
            {
                w.window.Invoke(new MethodInvoker(delegate ()
                {
                    w.window.Close();
                }));
            }
        }

        private void OnFormReized(object sender, EventArgs e)
        {
            if (window.WindowState == FormWindowState.Minimized)
            {
                window.Opacity = 0;
            }
        }

        private void OnFormActivated(object sender, EventArgs e)
        {
            handle = window.Handle;
        }

        private void OnOpenApplication(object sender, EventArgs e)
        {
            window.Opacity = 100;
            window.WindowState = FormWindowState.Normal;
        }

        private void OnExitApplication(object sender, EventArgs e)
        {
            Application.Exit();
        }

        private void NotifyIconDoubleClick(object sender, MouseEventArgs e)
        {
            window.Opacity = 100;
            window.WindowState = FormWindowState.Normal;
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

                case "startup":
                    if (registryKey.GetValue("WinWidgets") == null)
                    {
                        registryKey.SetValue("WinWidgets", Application.StartupPath);
                        Console.WriteLine("Will start with windows");
                    }
                    else
                    {
                        registryKey.DeleteValue("WinWidgets");
                        Console.WriteLine("Won't start with windows");
                    }
                    break;

                case "update":
                    Process.Start((string)versionObject["downloadUrl"]);
                    break;

                default:
                    OpenWidget(int.Parse((string)e.Message));
                    break;
            }
        }
    }
}
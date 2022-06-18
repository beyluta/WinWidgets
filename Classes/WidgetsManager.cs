using CefSharp;
using CefSharp.WinForms;
using Microsoft.Win32;
using Newtonsoft.Json;
using Newtonsoft.Json.Linq;
using System;
using System.Collections;
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
        private Form _window;
        private ChromiumWebBrowser _browser;
        private IntPtr _handle;
        private string managerUIPath = WidgetAssets.assetsPath + "/index.html";
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

            WidgetAssets.CreateHTMLFilesDirectory();

            string json = File.ReadAllText(WidgetAssets.assetsPath + "/config.json");
            appConfig = JsonConvert.DeserializeObject<Configuration>(json);

            CreateWindow(1500, 1100, "WinWidgets", FormStartPosition.CenterScreen);
        }

        public override void CreateWindow(int w, int h, string t, FormStartPosition p)
        {
            window = new Form();
            window.Size = new Size(w, h);
            window.StartPosition = p;
            window.Text = t;
            window.Activated += delegate { handle = window.Handle; };
            window.Icon = Resources.favicon;
            window.Resize += OnFormResized;
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
              new MenuItem("Quit", delegate { Application.Exit(); })
            });
            notifyIcon.MouseDoubleClick += NotifyIconDoubleClick;
        }

        private void ReloadWidgets()
        {
            string injectHTML = string.Empty;
            string[] files = WidgetAssets.GetPathToHTMLFiles(WidgetAssets.widgetsPath);

            for (int i = 0; i < files.Length; i++)
            {
                widgetPath = files[i];
                string localWidgetPath = string.Empty;

                for (int j = 0; j < widgetPath.Length; j++)
                {
                    localWidgetPath += widgetPath[j] == '\\' ? '/' : widgetPath[j];
                }

                if (!widgetsInitialized)
                {
                    /*
                    @@  This is supposed to fetch the newest version string of the app from this api.
                    @@  However sometimes this does not happen and the app starts without getting the response.
                    @@
                    @@  I have also noticed that this problem is even worse when made into an async call, therefore It's now synchronous.
                    */
                    HttpClient client = new HttpClient();
                    string response = client.GetStringAsync("https://7xdeveloper.com/api/AccessEndpoint.php?endpoint=getappconfigs&id=version").Result;
                    versionObject = JObject.Parse(response);

                    /*
                    @@  Injecting some JavaScript into the WidgetManager. There is probably a better way to do this...
                    @@  It adds the required classes, sytles, attributes, and event handlers.
                    */
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
                        e.style.width = '{(GetMetaTagValue("previewSize", widgetPath) != null ? GetMetaTagValue("previewSize", widgetPath).Split(' ')[0] : null)}px';
                        e.style.minHeight = '{(GetMetaTagValue("previewSize", widgetPath) != null ? GetMetaTagValue("previewSize", widgetPath).Split(' ')[1] : null)}px';
                        e.setAttribute('name', '{GetMetaTagValue("applicationTitle", widgetPath)}');
                        e.innerHTML = `<p>{GetMetaTagValue("applicationTitle", widgetPath)}</p> <iframe src='file:///{localWidgetPath}'></iframe>`;
                        document.getElementById('widgets').appendChild(e);
                        e.onclick = () => CefSharp.PostMessage('{i}');
                        document.getElementById('folder').onclick = () => CefSharp.PostMessage('widgetsFolder');" +

                        /*
                        @@  These are the settings of the application. Here we add the class 'switchon' so that the style changes in the software.
                        */
                        @"const switches = document.getElementsByClassName('switch');
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

                    if (i <= 0) // Clearing all widgets before reloading them again
                    {
                        injectHTML += "document.getElementById('widgets').innerHTML = '';";
                    }

                    injectHTML += $@"
                        const e{widgetIndex} = document.createElement('div');
                        e{widgetIndex}.classList.add('widget');
                        e{widgetIndex}.classList.add('flex-row');
                        e{widgetIndex}.style.width = '{(GetMetaTagValue("previewSize", widgetPath) != null ? GetMetaTagValue("previewSize", widgetPath).Split(' ')[0] : null)}px';
                        e{widgetIndex}.style.minHeight = '{(GetMetaTagValue("previewSize", widgetPath) != null ? GetMetaTagValue("previewSize", widgetPath).Split(' ')[1] : null)}px';
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
            Widget widget = new Widget();
            WidgetAssets.widgets.AddWidget(widget);
            widget.widgetPath = WidgetAssets.GetPathToHTMLFiles(WidgetAssets.widgetsPath)[id];
            widget.CreateWindow(300, 300, $"Widget{id}", FormStartPosition.Manual);
        }

        private void OnStopAllWidgets(object sender, EventArgs e)
        {
            ArrayList deleteWidgets = new ArrayList();

            for (int i = 0; i < WidgetAssets.widgets.Widgets.Count; i++)
            {
                ((Widget)WidgetAssets.widgets.Widgets[i]).window.Invoke(new MethodInvoker(delegate ()
                {
                    ((Widget)WidgetAssets.widgets.Widgets[i]).window.Close();
                    deleteWidgets.Add(((Widget)WidgetAssets.widgets.Widgets[i]));
                }));
            }

            for (int i = 0; i < deleteWidgets.Count; i++)
            {
                WidgetAssets.widgets.RemoveWidget((Widget)deleteWidgets[i]);
            }
        }

        private void OnFormResized(object sender, EventArgs e)
        {
            if (window.WindowState == FormWindowState.Minimized)
            {
                window.Opacity = 0;
            }
        }

        private void OnOpenApplication(object sender, EventArgs e)
        {
            window.Opacity = 100;
            window.WindowState = FormWindowState.Normal;
        }

        private void NotifyIconDoubleClick(object sender, MouseEventArgs e)
        {
            window.Opacity = 100;
            window.WindowState = FormWindowState.Normal;
        }

        private void OnBrowserInitialized(object sender, EventArgs e)
        {
            FileSystemWatcher fileWatcher = new FileSystemWatcher(WidgetAssets.widgetsPath);
            fileWatcher.NotifyFilter = NotifyFilters.Attributes
                                 | NotifyFilters.CreationTime
                                 | NotifyFilters.DirectoryName
                                 | NotifyFilters.FileName
                                 | NotifyFilters.LastAccess
                                 | NotifyFilters.LastWrite
                                 | NotifyFilters.Security
                                 | NotifyFilters.Size;

            fileWatcher.Changed += delegate { ReloadWidgets(); };
            fileWatcher.Created += delegate { ReloadWidgets(); };
            fileWatcher.Deleted += delegate { ReloadWidgets(); };
            fileWatcher.Renamed += delegate { ReloadWidgets(); };

            fileWatcher.Filter = "*.html";
            fileWatcher.IncludeSubdirectories = true;
            fileWatcher.EnableRaisingEvents = true;

            ReloadWidgets();
        }

        private void OnBrowserMessageReceived(object sender, JavascriptMessageReceivedEventArgs e)
        {
            /*
            @@  Receives messages from the JavaScript.
            @@
            @@  If the message is a number then it defaults to opening a widget by its id.
            */
            switch (e.Message)
            {
                case "widgetsFolder":
                    Process.Start(WidgetAssets.widgetsPath);
                    break;

                case "startup":
                    if (registryKey.GetValue("WinWidgets") == null)
                    {
                        registryKey.SetValue("WinWidgets", Application.StartupPath);
                    }
                    else
                    {
                        registryKey.DeleteValue("WinWidgets");
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